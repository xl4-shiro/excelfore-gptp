/*
 * Excelfore gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of Excelfore-gptp.
 *
 * Excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
#include "mind.h"
#include "mdeth.h"
#include "gptpnet.h"
#include "gptpclock.h"
#include "md_pdelay_req_sm.h"
#include "md_abnormal_hooks.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIAL_SEND_PDELAY_REQ,
	RESET,
	SEND_PDELAY_REQ,
	WAITING_FOR_PDELAY_RESP,
	WAITING_FOR_PDELAY_RESP_FOLLOW_UP,
	WAITING_FOR_PDELAY_INTERVAL_TIMER,
	REACTION,
}md_pdelay_req_state_t;

struct md_pdelay_req_data{
	gptpnet_data_t *gpnetd;
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	MDEntityGlobal *mdeg;
	md_pdelay_req_state_t state;
	md_pdelay_req_state_t last_state;
	MDPdelayReqSM *thisSM;
	int portIndex;
	uint64_t t1ts64;
	uint64_t t2ts64;
	uint64_t t3ts64;
	uint64_t t4ts64;
	MDPTPMsgPdelayReq txPdeayReq;
	MDPTPMsgPdelayResp recPdelayResp;
	MDPTPMsgPdelayRespFollowUp recPdelayRespFup;
	int cmlds_mode;
	md_pdelay_req_stat_data_t statd;
	uint64_t mock_txts64;
	uint64_t prev_t1ts64;
	uint64_t prev_t2ts64;
};

#define RCVD_PDELAY_RESP sm->thisSM->rcvdPdelayResp
#define RCVD_PDELAY_RESP_PTR sm->thisSM->rcvdPdelayRespPtr
#define RCVD_PDELAY_RESP_FOLLOWUP sm->thisSM->rcvdPdelayRespFollowUp
#define RCVD_PDELAY_RESP_FOLLOWUP_PTR sm->thisSM->rcvdPdelayRespFollowUpPtr

static bool isPdelayIntervalTimerExpired(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	const int round = 1000000;
	uint64_t tsdiff = cts64 - sm->thisSM->pdelayIntervalTimer.nsec;
	// align time in @round so that we can accept the case that tsdiff is very closed
	// to the pdelayIntervalTimer
	tsdiff = ((tsdiff + (round/2))/round)*round;
	return (tsdiff >= sm->mdeg->forAllDomain->pdelayReqInterval.nsec);
}

static MDPTPMsgPdelayReq *setPdelayReq(md_pdelay_req_data_t *sm)
{
	MDPTPMsgPdelayReq *sdata;
	int ssize=sizeof(MDPTPMsgPdelayReq);

	sdata=md_header_compose(sm->gpnetd, sm->portIndex, PDELAY_REQ, ssize,
				sm->ptasg->thisClock, sm->ppg->thisPort,
				sm->thisSM->pdelayReqSequenceId,
				sm->mdeg->forAllDomain->currentLogPdelayReqInterval);
	if(sm->cmlds_mode && sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq!=1)
		sdata->head.majorSdoId_messageType =
			(sdata->head.majorSdoId_messageType & 0x0F) | 0x20;
	if(!sdata) return NULL;
	memcpy(&sm->txPdeayReq, sdata, sizeof(MDPTPMsgPdelayReq));
	return &sm->txPdeayReq;
}

static int txPdelayReq(gptpnet_data_t *gpnetd, int portIndex)
{
	int ssize=sizeof(MDPTPMsgPdelayReq);
	return gptpnet_send_whook(gpnetd, portIndex-1, ssize);
}

static double computePdelayRateRatio(md_pdelay_req_data_t *sm, double oldRateRatio)
{
	uint64_t dt1, dt2;
	uint64_t maxd, mind;
	double pDelayRateRatio = 1.0;

	dt1=sm->t1ts64-sm->prev_t1ts64;
	dt2=sm->t2ts64-sm->prev_t2ts64;
	// check in +/- 50% of the interval time
	mind=sm->mdeg->forAllDomain->pdelayReqInterval.nsec / 2;
	maxd=sm->mdeg->forAllDomain->pdelayReqInterval.nsec + mind;
	if(dt1 > mind && dt1 < maxd && dt2 > mind && dt2 < maxd){
		/* 802.1AS-2020 Section 10.2.5.7 neighborRateRatio
		 * The neighborRateRatio is defined as the ratio of "frequency of the
		 * LocalClock entity of this TAS at the other end of the link" to the
		 * "frequency of the LocalClock entity of this TAS."
		 * Therefore, the ratio should be computed as "delta of TS at peer" over
		 * "delta of TS at this TAS".
		 *
		 * Note that this is in relation to the formula used in computing the
		 * computePropTime() where the resulting neighborRateRatio is to be
		 * applied to the difference in pdelayRespEventIngressTimestamp (t4) -
		 * pdelayReqEventEgressTimestamp (t1), to illustrate:
		 *   D = [r x (t4 - t1)] - (t3 - t2) / 2
		 * In other words, converting the difference in the frequency of the peer(responder).
		 */
		pDelayRateRatio = (double)(sm->t2ts64-sm->prev_t2ts64)/(sm->t1ts64-sm->prev_t1ts64);
		// neighborRateValid should be set to TRUE only after 2 valid responses
		if(!sm->thisSM->neighborRateRatioValid){
			sm->thisSM->neighborRateRatioValid=true;
		}
	}
	UB_LOG(UBL_INFO, "%s: old pDelayRateRatio %.17g -> %.17g : ave=%.17g  min=%"PRIu64" max=%"PRIu64" dt1=%"PRIu64" dt2=%"PRIu64"\n",
			__func__, oldRateRatio, pDelayRateRatio,
			(double)(oldRateRatio+pDelayRateRatio)/2, mind, maxd, dt1, dt2);
	sm->prev_t1ts64 = sm->t1ts64;
	sm->prev_t2ts64 = sm->t2ts64;
	// do rolling average
	return (double)(oldRateRatio+pDelayRateRatio)/2;
}

#define COMPUTED_PROP_TIME_TOO_BIG (UB_SEC_NS/100)
static uint64_t computePropTime(md_pdelay_req_data_t *sm)
{
	int64_t rts;
	/* 802.1AS-2020 Section 11.2.19.3.4 computePropTime
	 * The suggested equation given in the standards will be used in order to
	 * keep the coherence of the implementation in reference with the standards.
	 * Therefore, the following equation will be applied:
	 *   D = [r x (t4 - t1)] - (t3 - t2) / 2
	 *
	 * Do note that the neighborRateRatio (r) above needs to correspond to the
	 * rate of "delta of TS at peer" over "delta of TS at this TAS", see
	 * computePdelayRateRatio().
	 */
	rts = ( sm->ppg->forAllDomain->neighborRateRatio*(int64_t)(sm->t4ts64 - sm->t1ts64) -
		(int64_t)(sm->t3ts64 - sm->t2ts64) )/2;

	UB_LOG(UBL_INFO, "%s: neighborRateRatio: %.17g\n", __func__, sm->ppg->forAllDomain->neighborRateRatio);
	UB_LOG(UBL_INFO, "%s: t4 - t1 = %"PRIi64" - %"PRIi64" = %"PRIi64"\n",
	       __func__, sm->t4ts64, sm->t1ts64, sm->t4ts64-sm->t1ts64);
	UB_LOG(UBL_INFO, "%s: t3 - t2 = %"PRIi64" - %"PRIi64" = %"PRIi64"\n",
	       __func__, sm->t3ts64, sm->t2ts64, sm->t3ts64-sm->t2ts64);
	if(rts<0 || rts>COMPUTED_PROP_TIME_TOO_BIG){
		UB_LOG(UBL_WARN, "%s: computed PropTime is out of range = %"PRIi64", set 0\n",
		       __func__, rts);
		rts=0;
	}else{
		UB_LOG(UBL_INFO, "%s: computed PropTime = %"PRIu64"\n", __func__, rts);
	}
	return rts;
}

static md_pdelay_req_state_t allstate_condition(md_pdelay_req_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ppg->forAllDomain->portOper || !sm->thisSM->portEnabled0)
		return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(md_pdelay_req_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	if(gptpconf_get_intitem(CONF_NEIGHBOR_PROP_DELAY)){
		sm->ppg->forAllDomain->neighborRateRatio = 1.0;
		sm->mdeg->forAllDomain->asCapableAcrossDomains = true;
		sm->ppg->forAllDomain->neighborPropDelay.nsec =
			gptpconf_get_intitem(CONF_NEIGHBOR_PROP_DELAY);
		// this mode works only for Domain 0
		sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq=1;
		return NULL;
	}
	return NULL;
}

static md_pdelay_req_state_t not_enabled_condition(md_pdelay_req_data_t *sm)
{
	if(gptpconf_get_intitem(CONF_NEIGHBOR_PROP_DELAY)) return NOT_ENABLED;
	if(sm->ppg->forAllDomain->portOper && sm->thisSM->portEnabled0)
		return INITIAL_SEND_PDELAY_REQ;
	return NOT_ENABLED;
}

static int initial_send_pdelay_req_proc(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	int res;
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	sm->thisSM->initPdelayRespReceived = false;
	RCVD_PDELAY_RESP = false;
	RCVD_PDELAY_RESP_FOLLOWUP = false;
	sm->ppg->forAllDomain->neighborRateRatio = 1.0;
	sm->prev_t1ts64 = 0;
	sm->prev_t2ts64 = 0;
	sm->thisSM->rcvdMDTimestampReceive = false;
	sm->thisSM->pdelayReqSequenceId = (uint16_t)(rand() & 0xffff);
	sm->thisSM->txPdelayReqPtr = setPdelayReq(sm);
	if(!sm->thisSM->txPdelayReqPtr) return -1;
	res=txPdelayReq(sm->gpnetd, sm->portIndex);
	if(res==-1) return -2;
	if(res<0) sm->mock_txts64=gptpclock_getts64(sm->ptasg->thisClockIndex,0);
	sm->statd.pdelay_req_send++;
	sm->thisSM->pdelayIntervalTimer.subns = 0;
	sm->thisSM->pdelayIntervalTimer.nsec = cts64;
	sm->thisSM->lostResponses = 0;
	sm->thisSM->multiResponses = 0;
	sm->thisSM->detectedFaults = 0;
	sm->mdeg->forAllDomain->isMeasuringDelay = false;
	sm->mdeg->forAllDomain->asCapableAcrossDomains = false;
	return 0;
}

static md_pdelay_req_state_t initial_send_pdelay_req_condition(md_pdelay_req_data_t *sm,
							       uint64_t cts64)
{
	if(sm->thisSM->rcvdMDTimestampReceive) return WAITING_FOR_PDELAY_RESP;
	if(cts64 - sm->thisSM->pdelayIntervalTimer.nsec >=
	   gptpnet_txtslost_time(sm->gpnetd, sm->portIndex-1)){
		if(sm->mock_txts64){
			sm->t1ts64=sm->mock_txts64;
			sm->mock_txts64=0;
			return WAITING_FOR_PDELAY_RESP;
		}
		UB_TLOG(UBL_WARN,"%s:missing TxTS, portIndex=%d, seqID=%d\n",
			 __func__, sm->portIndex, sm->thisSM->pdelayReqSequenceId);
		// repeat to send the initial PDelayReq
		sm->last_state=REACTION;
		return INITIAL_SEND_PDELAY_REQ;
	}
	return INITIAL_SEND_PDELAY_REQ;
}

static void *reset_proc(md_pdelay_req_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	sm->thisSM->initPdelayRespReceived = false;
	RCVD_PDELAY_RESP = false;
	/* 802.1AS-2020 Figure 11-9 MPDelayReq state machine
	 * MPDelayReqSM must keep count of consecutive lost responses to Pdelay_Req
	 * and compares it to allowedLostResponses.
	 * The figure uses the condition "lostResponses <= allowedLostResponses"
	 * and since RESET state is triggered only after pdelayIntervalTimer has
	 * elapsed, thus this will allow effectively 2 + allowedLostResponses.
	 *
	 * An interpretation of this is that asCapable must be set to false after
	 * consective lost of allowedLostResponses. Thus it is more proper in such
	 * case to use "<" rather than "<=".
	 * This interpretation follows the expectation of AVNU gptp test plan.
	 *
	 * Thus instead of:
	 *   if (sm->thisSM->lostResponses <= sm->mdeg->forAllDomain->allowedLostResponses)
	 * we will use the following:
	 */
	if (sm->thisSM->lostResponses < sm->mdeg->forAllDomain->allowedLostResponses){
		sm->thisSM->lostResponses += 1;
	}else{
		sm->mdeg->forAllDomain->isMeasuringDelay = false;
		if(!sm->mdeg->forAllDomain->asCapableAcrossDomains) return NULL;
		sm->mdeg->forAllDomain->asCapableAcrossDomains = false;
		sm->thisSM->neighborRateRatioValid=false;
		sm->ppg->forAllDomain->neighborRateRatio = 1;
		UB_LOG(UBL_INFO, "%s:reset asCapableAcrossDomains, portIndex=%d\n",
		       __func__, sm->portIndex);
	}

	/* AVnu specific behavior
	 * AVnu alliance requres in addition to the required behavior on RESET that
	 * the device cease to transmit PDelayReq messages after consecutive
	 * PDelay_Req exchanges which have been responded to with multiple responses.
	 * See AVnuSpecific: gPTP.com.c.18.1 */
	if((sm->ptasg->conformToAvnu)&&(sm->thisSM->multiResponses)){
		if (sm->thisSM->multiResponses >= sm->mdeg->forAllDomain->allowedLostResponses){
			UB_LOG(UBL_WARN, "%s:portIndex=%d ceasing transmits for 5 minutes...\n",
					__func__, sm->portIndex);
			sm->mdeg->forAllDomain->isMeasuringDelay = false;
			sm->mdeg->forAllDomain->asCapableAcrossDomains = false;
			sm->thisSM->neighborRateRatioValid=false;
			sm->ppg->forAllDomain->neighborRateRatio = 1;
		}
	}

	return NULL;
}

#define CEASETIME_AVNU_MULTIRESPOSE 5*60*UB_SEC_NS // 5 minutes cease time
static md_pdelay_req_state_t reset_condition(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	/* AVnu specific behavior
	 * AVnu alliance requres in addition to the required behavior on RESET that
	 * the device cease to transmit PDelayReq messages for 5 minutes after
	 * consecutive PDelay_Req exchanges which have been responded to with
	 * multiple responses. See AVnuSpecific: gPTP.com.c.18.1 */
	if(sm->ptasg->conformToAvnu){
		if(sm->thisSM->multiResponses>=sm->mdeg->forAllDomain->allowedLostResponses){
			if(cts64 - sm->thisSM->pdelayIntervalTimer.nsec < CEASETIME_AVNU_MULTIRESPOSE){
				return RESET;
			}else{
				// clear so as not to cease indefinitely
				sm->thisSM->multiResponses=0;
			}
		}
	}
	if(isPdelayIntervalTimerExpired(sm, cts64)){
		return SEND_PDELAY_REQ;
	}
	return RESET;
}

static int send_pdelay_req_proc(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	int res;
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	RCVD_PDELAY_RESP = false;
	RCVD_PDELAY_RESP_FOLLOWUP = false;
	sm->thisSM->pdelayReqSequenceId += 1;
	sm->thisSM->txPdelayReqPtr = setPdelayReq(sm);
	if(!sm->thisSM->txPdelayReqPtr) return -1;
	res=txPdelayReq(sm->gpnetd, sm->portIndex);
	if(res==-1) return -2;
	if(res<0) sm->mock_txts64=gptpclock_getts64(sm->ptasg->thisClockIndex,0);
	sm->statd.pdelay_req_send++;
	sm->thisSM->pdelayIntervalTimer.nsec = cts64;
	return 0;
}

static md_pdelay_req_state_t send_pdelay_req_condition(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	if(sm->thisSM->rcvdMDTimestampReceive) return WAITING_FOR_PDELAY_RESP;
	if(RCVD_PDELAY_RESP){
		// this could happen on a device with slow TxTs
		UB_TLOG(UBL_DEBUG,"%s:received PDelayResp before TxTs of PDelayReq, "
			 "portIndex=%d, seqID=%d\n",
			 __func__, sm->portIndex, sm->thisSM->pdelayReqSequenceId);
	}
	if(cts64 - sm->thisSM->pdelayIntervalTimer.nsec >=
	   gptpnet_txtslost_time(sm->gpnetd, sm->portIndex-1)){
		if(sm->mock_txts64){
			sm->t1ts64=sm->mock_txts64;
			sm->mock_txts64=0;
			return WAITING_FOR_PDELAY_RESP;
		}
		UB_TLOG(UBL_WARN,"%s:missing TxTS, portIndex=%d, seqID=%d\n",
			 __func__, sm->portIndex, sm->thisSM->pdelayReqSequenceId);
		// send PdelayReq with the same SequenceId again
		sm->thisSM->pdelayReqSequenceId-=1;
		sm->last_state=REACTION;
		return SEND_PDELAY_REQ;
	}
	return SEND_PDELAY_REQ;
}

static void *waiting_for_pdelay_resp_proc(md_pdelay_req_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	sm->thisSM->rcvdMDTimestampReceive = false;
	return NULL;
}

static md_pdelay_req_state_t waiting_for_pdelay_resp_condition(md_pdelay_req_data_t *sm,
							       uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, sm->portIndex);
	if(isPdelayIntervalTimerExpired(sm, cts64)){
		UB_LOG(UBL_DEBUGV, "%s:pdelayIntervalTimer timedout\n", __func__);
		return RESET;
	}

	if(!RCVD_PDELAY_RESP) {
		if(RCVD_PDELAY_RESP_FOLLOWUP){
			UB_TLOG(UBL_WARN, "%s:waiting PdelayResp but received PdelayRespFup, "
				"exp.seqId=%d, received.seqId=%d\n", __func__,
				ntohs(sm->thisSM->txPdelayReqPtr->head.sequenceId_ns),
				ntohs(RCVD_PDELAY_RESP_FOLLOWUP_PTR->head.sequenceId_ns));
			RCVD_PDELAY_RESP_FOLLOWUP=false;
		}
		return WAITING_FOR_PDELAY_RESP;
	}

	/* 802.1AS-2020 11.2.2 Determination of asCapable
	 * Responses which are deeemed from itself or another port of the same
	 * instance should be ignored.
	 */
	if(!memcmp(RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity.clockIdentity,
		sm->ptasg->thisClock, sizeof(ClockIdentity))) {
		UB_TLOG(UBL_WARN, "%s:RESP is from self thisClock="UB_PRIhexB8
			 ", received="UB_PRIhexB8"\n",
			 __func__,
			 UB_ARRAY_B8(sm->ptasg->thisClock),
			 UB_ARRAY_B8(RCVD_PDELAY_RESP_PTR->
				    requestingPortIdentity.clockIdentity));
		return RESET;
	}

	if(memcmp(RCVD_PDELAY_RESP_PTR->requestingPortIdentity.clockIdentity,
		  sm->ptasg->thisClock, sizeof(ClockIdentity))) {
		UB_TLOG(UBL_WARN, "%s:ClockId doesn't match, expected="UB_PRIhexB8
			 ", received="UB_PRIhexB8"\n",
			 __func__,
			 UB_ARRAY_B8(sm->ptasg->thisClock),
			 UB_ARRAY_B8(RCVD_PDELAY_RESP_PTR->
				    requestingPortIdentity.clockIdentity));
		return RESET;
	}

	if(ntohs(RCVD_PDELAY_RESP_PTR->requestingPortIdentity.portNumber_ns) !=
	   sm->ppg->thisPort) return RESET;

	if(!sm->cmlds_mode){
		// 802.1AS-2020 8.1 the value of majorSdoId for gPTP domain must be 0x1
		// When device is accepting message under CMLDS domain, allow values
		// other than 0x1
		if((RCVD_PDELAY_RESP_PTR->head.majorSdoId_messageType & 0xF0)!=0x10){
			UB_LOG(UBL_DEBUGV, "%s: recevied RESP (seqId=%d) with invalid majorSdoId, ignore\n",
					__func__, ntohs(RCVD_PDELAY_RESP_PTR->head.sequenceId_ns));
			return WAITING_FOR_PDELAY_RESP;
		}
	}

	if(RCVD_PDELAY_RESP_PTR->head.sequenceId_ns !=
	   sm->thisSM->txPdelayReqPtr->head.sequenceId_ns) {
		UB_TLOG(UBL_WARN, "%s:sequenceId doesn't match, expected=%d, received=%d\n",
			 __func__, ntohs(sm->thisSM->txPdelayReqPtr->head.sequenceId_ns),
			 ntohs(RCVD_PDELAY_RESP_PTR->head.sequenceId_ns));
		if( ntohs(sm->thisSM->txPdelayReqPtr->head.sequenceId_ns) >
		    ntohs(RCVD_PDELAY_RESP_PTR->head.sequenceId_ns) ) {
			UB_TLOG(UBL_WARN, "%s:Discard this RESP and wait for the next one, "
				 "this could be because of SM Reset. "
				 "SeqId: expected=%d, received=%d\n",
			 __func__, ntohs(sm->thisSM->txPdelayReqPtr->head.sequenceId_ns),
			 ntohs(RCVD_PDELAY_RESP_PTR->head.sequenceId_ns));
		}else{
			return RESET;
		}
	}

	return  WAITING_FOR_PDELAY_RESP_FOLLOW_UP;
}

static void *waiting_for_pdelay_resp_follow_up_proc(md_pdelay_req_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	RCVD_PDELAY_RESP = false;
	sm->statd.pdelay_resp_rec_valid++;
	return NULL;
}

static md_pdelay_req_state_t waiting_for_pdelay_resp_follow_up_condition(
	md_pdelay_req_data_t *sm, uint64_t cts64)
{
	if(isPdelayIntervalTimerExpired(sm, cts64)){
		UB_LOG(UBL_DEBUG, "%s:portIndex=%d, pdelayIntervalTimer timedout\n",
		       __func__, sm->portIndex);
		return RESET;
	}

	if(RCVD_PDELAY_RESP &&
	   (RCVD_PDELAY_RESP_PTR->head.sequenceId_ns ==
	    sm->thisSM->txPdelayReqPtr->head.sequenceId_ns)) {
		UB_TLOG(UBL_WARN, "%s:portIndex=%d, PdelayResp comes "
			 "twice for the same PdelayReq, sequenceId=%d\n",
			 __func__, sm->portIndex,
			 ntohs(RCVD_PDELAY_RESP_PTR->head.sequenceId_ns));
		sm->thisSM->multiResponses++;
		return RESET;
	}

	if(!RCVD_PDELAY_RESP_FOLLOWUP) return WAITING_FOR_PDELAY_RESP_FOLLOW_UP;

	if(!sm->cmlds_mode){
		// 802.1AS-2020 8.1 the value of majorSdoId for gPTP domain must be 0x1
		// When device is accepting message under CMLDS domain, allow values
		// other than 0x1
		if((RCVD_PDELAY_RESP_FOLLOWUP_PTR->head.majorSdoId_messageType & 0xF0)!=0x10){

			UB_LOG(UBL_DEBUGV, "%s: received PDFup (seqId=%d) with invalid majorSdoId, ignore\n",
					__func__, RCVD_PDELAY_RESP_FOLLOWUP_PTR->head.sequenceId_ns);
			return WAITING_FOR_PDELAY_RESP_FOLLOW_UP;
		}
	}

	/* 802.1AS-2020 11.2.2 Determination of asCapable
	 * Responses which are deeemed from itself or another port of the same
	 * instance should be ignored.
	 */
	if(!memcmp(RCVD_PDELAY_RESP_FOLLOWUP_PTR->head.sourcePortIdentity.clockIdentity,
		sm->ptasg->thisClock, sizeof(ClockIdentity))) {
		UB_TLOG(UBL_WARN, "%s:FUP is from self thisClock="UB_PRIhexB8
			 ", received="UB_PRIhexB8"\n",
			 __func__,
			 UB_ARRAY_B8(sm->ptasg->thisClock),
			 UB_ARRAY_B8(RCVD_PDELAY_RESP_FOLLOWUP_PTR->
				    requestingPortIdentity.clockIdentity));
		return RESET;
	}

	if(RCVD_PDELAY_RESP_FOLLOWUP_PTR->head.sequenceId_ns !=
	   sm->thisSM->txPdelayReqPtr->head.sequenceId_ns) {
		UB_TLOG(UBL_WARN, "%s:portIndex=%d, sequenceId doesn't match, expected=%d, "
			 "received=%d\n", __func__, sm->portIndex,
			 ntohs(sm->thisSM->txPdelayReqPtr->head.sequenceId_ns),
		       ntohs(RCVD_PDELAY_RESP_FOLLOWUP_PTR->head.sequenceId_ns));
		return WAITING_FOR_PDELAY_RESP_FOLLOW_UP;
	}

	if(!memcmp(&RCVD_PDELAY_RESP_FOLLOWUP_PTR->
		   head.sourcePortIdentity,
		   &RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity,
		   sizeof(MDPortIdentity)) ) return WAITING_FOR_PDELAY_INTERVAL_TIMER;

	if(memcmp(RCVD_PDELAY_RESP_PTR->
		  requestingPortIdentity.clockIdentity,
		  sm->ptasg->thisClock, sizeof(ClockIdentity))) {
		UB_LOG(UBL_WARN, "%s:portIndex=%d, ClockId doesn't match, expected="UB_PRIhexB8
		       ", received="UB_PRIhexB8"\n",
		       __func__, sm->portIndex,
		       UB_ARRAY_B8(sm->ptasg->thisClock),
		       UB_ARRAY_B8(RCVD_PDELAY_RESP_PTR->
				  requestingPortIdentity.clockIdentity));
		return WAITING_FOR_PDELAY_RESP_FOLLOW_UP;
	}

	if(ntohs(RCVD_PDELAY_RESP_PTR->
		 requestingPortIdentity.portNumber_ns) !=
	   sm->ppg->thisPort) {
		UB_TLOG(UBL_WARN, "%s:portIndex=%d, PortNumber doesn't match\n",
			 __func__, sm->portIndex);
		return WAITING_FOR_PDELAY_RESP_FOLLOW_UP;
	}

	return WAITING_FOR_PDELAY_INTERVAL_TIMER;
}

static void *waiting_for_pdelay_interval_timer_proc(md_pdelay_req_data_t *sm)
{
	uint64_t prev_neighborRateRatioNs=sm->ppg->forAllDomain->neighborPropDelay.nsec;
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	RCVD_PDELAY_RESP_FOLLOWUP = false;
	sm->thisSM->lostResponses = 0;
	sm->thisSM->multiResponses = 0;
	if(sm->ppg->forAllDomain->asymmetryMeasurementMode) return NULL;
	if(sm->ppg->forAllDomain->computeNeighborRateRatio){
		sm->ppg->forAllDomain->neighborRateRatio = computePdelayRateRatio(sm,
				sm->ppg->forAllDomain->neighborRateRatio);
	} else {
		// neighborRateRatio is deemed valid when computeNeighborRateRatio is false
		sm->thisSM->neighborRateRatioValid=true;
	}
	if(sm->ppg->forAllDomain->computeNeighborPropDelay)
		sm->ppg->forAllDomain->neighborPropDelay.nsec = computePropTime(sm);

	sm->mdeg->forAllDomain->isMeasuringDelay = true;

	// In addition to the conditions in 802.1AS-2020 Figure 11-9
	// WAITING_FOR_PDELAY_INTERVAL_TIMER, consider neighborPropDelay less then
	// CONF_NEIGHBOR_PROPDELAY_MINLIMIT as fault
	if((sm->ppg->forAllDomain->neighborPropDelay.nsec <
		sm->mdeg->forAllDomain->neighborPropDelayMinLimit.nsec) &&
	   (memcmp(RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity.clockIdentity,
		   sm->ptasg->thisClock, sizeof(ClockIdentity))
		   ) && sm->thisSM->neighborRateRatioValid) {
		UB_LOG(UBL_WARN, "%s:portIndex=%d, neighborPropDelay is below the min limit (%"PRIu64" < %"PRIu64")\n",
				__func__, sm->portIndex, sm->ppg->forAllDomain->neighborPropDelay.nsec,
				sm->mdeg->forAllDomain->neighborPropDelayMinLimit.nsec);
		goto detectedfault;
	}

	if((sm->ppg->forAllDomain->neighborPropDelay.nsec <=
	    sm->mdeg->forAllDomain->neighborPropDelayThresh.nsec) &&
	   (memcmp(RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity.clockIdentity,
		   sm->ptasg->thisClock, sizeof(ClockIdentity))
		   ) && sm->thisSM->neighborRateRatioValid) {
		sm->statd.pdelay_resp_fup_rec_valid++;
		sm->thisSM->detectedFaults = 0;
		if(sm->mdeg->forAllDomain->asCapableAcrossDomains) return NULL;
		sm->mdeg->forAllDomain->asCapableAcrossDomains = true;
		UB_LOG(UBL_INFO, "%s:set asCapableAcrossDomains, portIndex=%d\n",
		       __func__, sm->portIndex);
		return NULL;
	}

	if(memcmp(RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity.clockIdentity,
		  sm->ptasg->thisClock, sizeof(ClockIdentity))) {
		if(sm->thisSM->neighborRateRatioValid){
			// At this point the computed neighborPropDelay is likely erroneous
			// and that a valid neighborPropDelay has been computed prior, thus
			// this particulat neighborPropDelay should be considered as fault
			// and previous value should be used to prevent unlikely deviation.
			goto detectedfault;
		}else{
			// This block handles the case that the computeNeighborPropDelay may
			// or may not be valid (e.g. from first set of Pdelay messages)
			// this will not be considered fault but will retain non-asCapable.
			UB_TLOG(UBL_WARN, "%s:portIndex=%d, sourcePortIdentity="UB_PRIhexB8
					", thisClock="UB_PRIhexB8", neighborPropDelay=%"PRIu64"\n",
					__func__, sm->portIndex,
					UB_ARRAY_B8(RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity.clockIdentity),
					UB_ARRAY_B8(sm->ptasg->thisClock),
					sm->ppg->forAllDomain->neighborPropDelay.nsec);
			goto noascapable;
		}
	}

detectedfault:
	if(sm->thisSM->detectedFaults <= sm->mdeg->forAllDomain->allowedFaults){
		sm->thisSM->detectedFaults += 1;
		UB_LOG(UBL_DEBUG, "%s:portIndex=%d detected fault=%d/%d\n", __func__,
				sm->portIndex, sm->thisSM->detectedFaults,
				sm->mdeg->forAllDomain->allowedFaults);
		UB_LOG(UBL_WARN, "%s:portIndex=%d, likely erroneous neighborPropDelay, use previous value=%"PRIu64"\n",
		     __func__, sm->portIndex, prev_neighborRateRatioNs);
		sm->ppg->forAllDomain->neighborPropDelay.nsec = prev_neighborRateRatioNs;
		return NULL;
	}

noascapable:
	UB_LOG(UBL_INFO, "%s:portIndex=%d, not asCapable\n", __func__, sm->portIndex);
	sm->mdeg->forAllDomain->asCapableAcrossDomains = false;
	sm->mdeg->forAllDomain->isMeasuringDelay = false;
	sm->thisSM->detectedFaults = 0;
	return NULL;
}

static md_pdelay_req_state_t waiting_for_pdelay_interval_timer_condition(
	md_pdelay_req_data_t *sm, uint64_t cts64)
{
	if(isPdelayIntervalTimerExpired(sm, cts64)){return SEND_PDELAY_REQ;}
	return WAITING_FOR_PDELAY_INTERVAL_TIMER;
}

// return 1 when PdelayReq is sent, otherwise return 0
int md_pdelay_req_sm(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	bool state_change;
	int res;

	if(!sm) return 0;
	sm->state = allstate_condition(sm);

	while(true){
		state_change=(sm->last_state != sm->state);
		sm->last_state = sm->state;
		switch(sm->state){
		case INIT:
			sm->state = NOT_ENABLED;
			break;
		case NOT_ENABLED:
			if(state_change)
				not_enabled_proc(sm);
			sm->state = not_enabled_condition(sm);
			break;
		case INITIAL_SEND_PDELAY_REQ:
			if(state_change){
				res=initial_send_pdelay_req_proc(sm, cts64);
				if(res==-2){
					sm->last_state = REACTION;
				}else if(res==-1){
					sm->state = NOT_ENABLED;
					break;
				}
				return 1;
			}
			sm->state = initial_send_pdelay_req_condition(sm, cts64);
			break;
		case RESET:
			if(state_change)
				reset_proc(sm);
			sm->state = reset_condition(sm, cts64);
			break;
		case SEND_PDELAY_REQ:
			if(state_change){
				res=send_pdelay_req_proc(sm, cts64);
				if(res==-2){
					sm->thisSM->pdelayReqSequenceId -= 1;
					sm->last_state = REACTION;
				}else if(res==-1){
					sm->state = RESET;
					break;
				}
				return 1;
			}
			sm->state = send_pdelay_req_condition(sm, cts64);
			break;
		case WAITING_FOR_PDELAY_RESP:
			if(state_change)
				waiting_for_pdelay_resp_proc(sm);
			sm->state = waiting_for_pdelay_resp_condition(sm, cts64);
			break;
		case WAITING_FOR_PDELAY_RESP_FOLLOW_UP:
			if(state_change)
				waiting_for_pdelay_resp_follow_up_proc(sm);
			sm->state = waiting_for_pdelay_resp_follow_up_condition(sm, cts64);
			break;
		case WAITING_FOR_PDELAY_INTERVAL_TIMER:
			if(state_change)
				waiting_for_pdelay_interval_timer_proc(sm);
			sm->state = waiting_for_pdelay_interval_timer_condition(sm, cts64);
			break;
		case REACTION:
			break;
		}
		if(sm->last_state == sm->state) break;
	}
	return 0;
}

void md_pdelay_req_sm_init(md_pdelay_req_data_t **sm,
			   int portIndex,
			   gptpnet_data_t *gpnetd,
			   PerTimeAwareSystemGlobal *ptasg,
			   PerPortGlobal *ppg,
			   MDEntityGlobal *mdeg)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, portIndex);
	INIT_SM_DATA(md_pdelay_req_data_t, MDPdelayReqSM, sm);
	(*sm)->gpnetd = gpnetd;
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->mdeg = mdeg;
	(*sm)->portIndex = portIndex;
	(*sm)->cmlds_mode = gptpconf_get_intitem(CONF_CMLDS_MODE);

	// 11.2.17.2.13
	if((*sm)->cmlds_mode){
		// ??? cmldsLinkPortEnabled for CMLDS, but we use ptpPortEnabled here
		(*sm)->thisSM->portEnabled0 = (*sm)->ppg->ptpPortEnabled;
	}else{
		(*sm)->thisSM->portEnabled0 = (*sm)->ppg->ptpPortEnabled;
	}
}

int md_pdelay_req_sm_close(md_pdelay_req_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void md_pdelay_req_sm_txts(md_pdelay_req_data_t *sm, event_data_txts_t *edtxts,
			   uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d, received seqID=%d\n",
	       __func__, sm->portIndex, edtxts->seqid);
	if(md_abnormal_timestamp(PDELAY_REQ, sm->portIndex-1, -1)) return;
	if((sm->state!=SEND_PDELAY_REQ && sm->state!=INITIAL_SEND_PDELAY_REQ)){
		UB_LOG(UBL_WARN,"%s:TxTS is not expected, state=%d, received seqID=%d\n",
		       __func__, sm->state, edtxts->seqid);
		return;
	}
	if(edtxts->seqid != sm->thisSM->pdelayReqSequenceId){
		UB_LOG(UBL_WARN,"%s:mismatched TxTS seqID, expected=%d, received=%d\n",
		       __func__, sm->thisSM->pdelayReqSequenceId, edtxts->seqid);
		return;
	}
	sm->t1ts64=edtxts->ts64;
	sm->thisSM->rcvdMDTimestampReceive = true;
	md_pdelay_req_sm(sm, cts64);
}

void md_pdelay_req_sm_recv_resp(md_pdelay_req_data_t *sm, event_data_recv_t *edrecv,
				uint64_t cts64)
{
	uint32_t tsec, tns;
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n",__func__, sm->portIndex);
	RCVD_PDELAY_RESP = true;
	memcpy(&sm->recPdelayResp, (MDPTPMsgPdelayResp *)edrecv->recbptr,
	       sizeof(MDPTPMsgPdelayResp));
	RCVD_PDELAY_RESP_PTR = &sm->recPdelayResp;
	tsec = ntohl(RCVD_PDELAY_RESP_PTR->requestReceiptTimestamp.seconds_lsb_nl);
	tns = ntohl(RCVD_PDELAY_RESP_PTR->requestReceiptTimestamp.nanoseconds_nl);
	sm->t2ts64 = (uint64_t)tsec * UB_SEC_NS + (uint64_t)tns;
	sm->t4ts64 = edrecv->ts64;
	md_pdelay_req_sm(sm, cts64);
	sm->statd.pdelay_resp_rec++;
}

void md_pdelay_req_sm_recv_respfup(md_pdelay_req_data_t *sm, event_data_recv_t *edrecv,
				   uint64_t cts64)
{
	uint32_t tsec, tns;
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n",__func__, sm->portIndex);
	RCVD_PDELAY_RESP_FOLLOWUP = true;
	memcpy(&sm->recPdelayRespFup, (MDPTPMsgPdelayRespFollowUp *)edrecv->recbptr,
	       sizeof(MDPTPMsgPdelayRespFollowUp));
	RCVD_PDELAY_RESP_FOLLOWUP_PTR = &sm->recPdelayRespFup;
	tsec = ntohl(RCVD_PDELAY_RESP_FOLLOWUP_PTR->requestOriginTimestamp.seconds_lsb_nl);
	tns = ntohl(RCVD_PDELAY_RESP_FOLLOWUP_PTR->requestOriginTimestamp.nanoseconds_nl);
	sm->t3ts64 = (uint64_t)tsec * UB_SEC_NS + (uint64_t)tns;
	md_pdelay_req_sm(sm, cts64);
	sm->statd.pdelay_resp_fup_rec++;
}

void md_pdelay_req_stat_reset(md_pdelay_req_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_pdelay_req_stat_data_t));
}

md_pdelay_req_stat_data_t *md_pdelay_req_get_stat(md_pdelay_req_data_t *sm)
{
	return &sm->statd;
}
