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
};

#define RCVD_PDELAY_RESP sm->thisSM->rcvdPdelayResp
#define RCVD_PDELAY_RESP_PTR sm->thisSM->rcvdPdelayRespPtr
#define RCVD_PDELAY_RESP_FOLLOWUP sm->thisSM->rcvdPdelayRespFollowUp
#define RCVD_PDELAY_RESP_FOLLOWUP_PTR sm->thisSM->rcvdPdelayRespFollowUpPtr

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

static double computePdelayRateRatio(md_pdelay_req_data_t *sm)
{
	return 1.0;
}

#define COMPUTED_PROP_TIME_TOO_BIG (UB_SEC_NS/100)
static uint64_t computePropTime(md_pdelay_req_data_t *sm)
{
	int64_t rts;
	rts = (int64_t)((sm->t4ts64 - sm->t1ts64) - (sm->t3ts64 - sm->t2ts64))/2;
	sm->thisSM->neighborRateRatioValid=true;
	if(rts<0 || rts>COMPUTED_PROP_TIME_TOO_BIG){
		UB_LOG(UBL_WARN, "%s: computed PropTime is out of range = %"PRIi64", set 0\n",
		       __func__, rts);
		rts=0;
	}else{
		UB_LOG(UBL_DEBUGV, "%s: computed PropTime = %"PRIu64"\n", __func__, rts);
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
	if (sm->thisSM->lostResponses <= sm->mdeg->forAllDomain->allowedLostResponses){
		sm->thisSM->lostResponses += 1;
	}else{
		sm->mdeg->forAllDomain->isMeasuringDelay = false;
		if(!sm->mdeg->forAllDomain->asCapableAcrossDomains) return NULL;
		sm->mdeg->forAllDomain->asCapableAcrossDomains = false;
		UB_LOG(UBL_INFO, "%s:reset asCapableAcrossDomains, portIndex=%d\n",
		       __func__, sm->portIndex);
	}
	return NULL;
}

static md_pdelay_req_state_t reset_condition(md_pdelay_req_data_t *sm, uint64_t cts64)
{
	if((cts64 - sm->thisSM->pdelayIntervalTimer.nsec >=
	    sm->mdeg->forAllDomain->pdelayReqInterval.nsec)){
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
	if((cts64 - sm->thisSM->pdelayIntervalTimer.nsec >=
	    sm->mdeg->forAllDomain->pdelayReqInterval.nsec)) {
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
	if(cts64 - sm->thisSM->pdelayIntervalTimer.nsec >=
	   sm->mdeg->forAllDomain->pdelayReqInterval.nsec) {
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
		return RESET;
	}

	if(!RCVD_PDELAY_RESP_FOLLOWUP) return WAITING_FOR_PDELAY_RESP_FOLLOW_UP;

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
	UB_LOG(UBL_DEBUGV, "md_pdelay_req:%s:portIndex=%d\n", __func__, sm->portIndex);
	RCVD_PDELAY_RESP_FOLLOWUP = false;
	sm->thisSM->lostResponses = 0;
	if(sm->ppg->forAllDomain->asymmetryMeasurementMode) return NULL;
	if(sm->ppg->forAllDomain->computeNeighborRateRatio)
		sm->ppg->forAllDomain->neighborRateRatio = computePdelayRateRatio(sm);
	if(sm->ppg->forAllDomain->computeNeighborPropDelay)
		sm->ppg->forAllDomain->neighborPropDelay.nsec = computePropTime(sm);

	sm->mdeg->forAllDomain->isMeasuringDelay = true;

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
		UB_TLOG(UBL_INFO, "%s:portIndex=%d, sourcePortIdentity="UB_PRIhexB8
			 ", thisClock="UB_PRIhexB8", neighborPropDelay=%"PRIu64"\n",
			 __func__, sm->portIndex,
		       UB_ARRAY_B8(RCVD_PDELAY_RESP_PTR->head.sourcePortIdentity.clockIdentity),
		       UB_ARRAY_B8(sm->ptasg->thisClock),
		       sm->ppg->forAllDomain->neighborPropDelay.nsec);
		goto noascapable;
	}

	if(sm->thisSM->detectedFaults <= sm->mdeg->forAllDomain->allowedFaults){
		sm->thisSM->detectedFaults += 1;
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
	if((cts64 - sm->thisSM->pdelayIntervalTimer.nsec >=
	    sm->mdeg->forAllDomain->pdelayReqInterval.nsec))
		return SEND_PDELAY_REQ;
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
	md_pdelay_req_sm(sm, cts64);
	sm->t4ts64 = edrecv->ts64;
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
