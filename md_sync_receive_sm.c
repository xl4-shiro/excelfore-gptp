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
#include "md_sync_receive_sm.h"
#include <math.h>

typedef enum {
	INIT,
	DISCARD,
	WAITING_FOR_FOLLOW_UP,
	WAITING_FOR_SYNC,
	REACTION,
}md_sync_receive_state_t;

struct md_sync_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	MDEntityGlobal *mdeg;
	md_sync_receive_state_t state;
	md_sync_receive_state_t last_state;
	MDSyncReceiveSM *thisSM;
	int domainIndex;
	int portIndex;
	MDSyncReceive mdSyncReceive;
	uint64_t rts;
	MDPTPMsgSyncOneStep recSync;
	MDPTPMsgFollowUp recFollowUp;
	uint64_t rsync_ts; // for debug use
	uint64_t rfup_ts; // for debug use
	md_sync_receive_stat_data_t statd;
};

#define RCVD_SYNC sm->thisSM->rcvdSync
#define RCVD_SYNC_PTR sm->thisSM->rcvdSyncPtr
#define RCVD_SYNC_ONESETP_PTR ((MDPTPMsgSyncOneStep *)sm->thisSM->rcvdSyncPtr)
#define RCVD_FOLLOWUP sm->thisSM->rcvdFollowUp
#define RCVD_FOLLOWUP_PTR sm->thisSM->rcvdFollowUpPtr
#define PORT_OPER sm->ppg->forAllDomain->portOper
#define ASYMMETRY_MEASUREMENT_MODE sm->ppg->forAllDomain->asymmetryMeasurementMode
#define ONE_STEP_RECEIVE sm->mdeg->oneStepReceive
#define AS_CAPABLE sm->ppg->asCapable
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define TWO_STEP_FLAG GET_TWO_STEP_FLAG(RCVD_SYNC_PTR->head)

static MDSyncReceive *setMDSyncReceive(md_sync_receive_data_t *sm)
{
	sm->mdSyncReceive.domainNumber = RCVD_SYNC_PTR->head.domainNumber;
	sm->mdSyncReceive.followUpCorrectionField.nsec =
		(UB_NTOHLL((uint64_t)RCVD_SYNC_PTR->head.correctionField_nll)>>16);

	if(TWO_STEP_FLAG){
		sm->mdSyncReceive.followUpCorrectionField.nsec +=
			(UB_NTOHLL((uint64_t)RCVD_FOLLOWUP_PTR->head.correctionField_nll)>>16);
		sm->mdSyncReceive.preciseOriginTimestamp.seconds.lsb =
			(uint64_t)ntohl(RCVD_FOLLOWUP_PTR->preciseOriginTimestamp.seconds_lsb_nl);
		sm->mdSyncReceive.preciseOriginTimestamp.seconds.msb =
			(uint64_t)ntohs(RCVD_FOLLOWUP_PTR->preciseOriginTimestamp.seconds_msb_ns);
		sm->mdSyncReceive.preciseOriginTimestamp.nanoseconds =
			(uint64_t)ntohl(RCVD_FOLLOWUP_PTR->preciseOriginTimestamp.nanoseconds_nl);

		sm->mdSyncReceive.rateRatio =
			1.0 + ldexp((int32_t)ntohl(RCVD_FOLLOWUP_PTR->
						  FUpInfoTLV.cumulativeScaledRateOffset_nl),
				    -41);
		sm->mdSyncReceive.gmTimeBaseIndicator =
			ntohs(RCVD_FOLLOWUP_PTR->FUpInfoTLV.gmTimeBaseIndicator_ns);

		/* 10.2.2.1.10 lastGmPhaseChange (ScaledNs
		 * The lastGmPhaseChange is the value of the lastGmPhaseChange member
		 * of the most recently received PortSyncSync structure...
		 * It is set equal to the lastGmPhaseChange of the received
		 * time-synchronization information.
		 * Both have MDScaledNs structure, copy the value as is. */
		sm->mdSyncReceive.lastGmPhaseChange.nsec_msb =
			ntohs(RCVD_FOLLOWUP_PTR->FUpInfoTLV.lastGmPhaseChange.nsec_msb);
		sm->mdSyncReceive.lastGmPhaseChange.nsec =
			(int64_t)UB_NTOHLL(
				(uint64_t)RCVD_FOLLOWUP_PTR->FUpInfoTLV.lastGmPhaseChange.nsec_nll);
		sm->mdSyncReceive.lastGmPhaseChange.subns =
			ntohs(RCVD_FOLLOWUP_PTR->FUpInfoTLV.lastGmPhaseChange.subns_ns);

		sm->mdSyncReceive.lastGmFreqChange =
			ldexp((int32_t)ntohl(
				      RCVD_FOLLOWUP_PTR->FUpInfoTLV.scaledLastGmFreqChange_nl),
			      -41);
	}else{
		sm->mdSyncReceive.preciseOriginTimestamp.seconds.lsb =
			(uint64_t)ntohl(RCVD_SYNC_ONESETP_PTR->originTimestamp.seconds_lsb_nl);
		sm->mdSyncReceive.preciseOriginTimestamp.seconds.msb =
			(uint64_t)ntohs(RCVD_SYNC_ONESETP_PTR->originTimestamp.seconds_msb_ns);
		sm->mdSyncReceive.preciseOriginTimestamp.nanoseconds =
			(uint64_t)ntohl(RCVD_SYNC_ONESETP_PTR->originTimestamp.nanoseconds_nl);

		sm->mdSyncReceive.rateRatio =
			1.0 + ldexp((int32_t)ntohl(RCVD_SYNC_ONESETP_PTR->
						  FUpInfoTLV.cumulativeScaledRateOffset_nl),
				    -41);
		sm->mdSyncReceive.gmTimeBaseIndicator =
			ntohs(RCVD_SYNC_ONESETP_PTR->FUpInfoTLV.gmTimeBaseIndicator_ns);
		sm->mdSyncReceive.lastGmPhaseChange.nsec_msb =
			ntohs(RCVD_FOLLOWUP_PTR->FUpInfoTLV.lastGmPhaseChange.nsec_msb);
		sm->mdSyncReceive.lastGmPhaseChange.nsec =
			(int64_t)UB_NTOHLL(
				(uint64_t)RCVD_FOLLOWUP_PTR->FUpInfoTLV.lastGmPhaseChange.nsec_nll);
		sm->mdSyncReceive.lastGmPhaseChange.subns =
			ntohs(RCVD_FOLLOWUP_PTR->FUpInfoTLV.lastGmPhaseChange.subns_ns);
		sm->mdSyncReceive.lastGmFreqChange =
			ldexp((int32_t)
			      ntohl(RCVD_SYNC_ONESETP_PTR->FUpInfoTLV.scaledLastGmFreqChange_nl),
			      -41);
	}

	memcpy(sm->mdSyncReceive.sourcePortIdentity.clockIdentity,
	       RCVD_SYNC_PTR->head.sourcePortIdentity.clockIdentity,
	       sizeof(ClockIdentity));
	sm->mdSyncReceive.sourcePortIdentity.portNumber =
		ntohs(RCVD_SYNC_PTR->head.sourcePortIdentity.portNumber_ns);
	sm->mdSyncReceive.logMessageInterval = RCVD_SYNC_PTR->head.logMessageInterval;

	sm->mdSyncReceive.upstreamTxTime.nsec = (sm->rts -
		((double)sm->ppg->forAllDomain->neighborPropDelay.nsec /
		 sm->ppg->forAllDomain->neighborRateRatio) -
		((double)sm->ppg->forAllDomain->delayAsymmetry.nsec /
		 sm->mdSyncReceive.rateRatio));
	return &sm->mdSyncReceive;
}

static md_sync_receive_state_t allstate_condition(md_sync_receive_data_t *sm)
{
	/*
	  from 'MDSyncReceiveSM state machine' in the standard,
	  (sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	  (RCVD_SYNC && (!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE)))
	  is the right condition, but we remove 'RCVD_SYNC &&' part to avoid some issues
	 */
	if (sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	    !PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE){
		sm->last_state=REACTION;
		return DISCARD;
	}
	return sm->state;
}

static void *discard_proc(md_sync_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_sync_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_SYNC = false;
	RCVD_FOLLOWUP = false;
	sm->rsync_ts = 0;
	sm->rfup_ts = 0;
	return NULL;
}

static md_sync_receive_state_t discard_condition(md_sync_receive_data_t *sm)
{
	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE && !ONE_STEP_RECEIVE) {
		sm->last_state=REACTION;
		return DISCARD;
	}

	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE) return WAITING_FOR_FOLLOW_UP;

	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE && ONE_STEP_RECEIVE)
		return WAITING_FOR_SYNC;

	return DISCARD;
}

static void *waiting_for_follow_up_proc(md_sync_receive_data_t *sm, uint64_t cts64)
{
	int64_t dts;
	UB_LOG(UBL_DEBUGV, "md_sync_receive:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	if(RCVD_SYNC) {
		dts=cts64-sm->rsync_ts;
		if(sm->rsync_ts && dts>175000000) {
			UB_TLOG(UBL_INFO, "%s:domainIndex=%d, portIndex=%d, sync gap=%"PRIi64"\n",
				 __func__, sm->domainIndex, sm->portIndex, dts);
		}
		sm->rsync_ts=cts64;
	}
	sm->statd.sync_rec_valid++;
	RCVD_SYNC = false;
	sm->thisSM->upstreamSyncInterval.nsec =
		LOG_TO_NSEC(RCVD_SYNC_PTR->head.logMessageInterval);
	sm->thisSM->followUpReceiptTimeoutTime.nsec =
		cts64 + sm->thisSM->upstreamSyncInterval.nsec;
	return NULL;
}

static md_sync_receive_state_t waiting_for_follow_up_condition(md_sync_receive_data_t *sm,
							       uint64_t cts64)
{
	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   TWO_STEP_FLAG){
		UB_TLOG(UBL_WARN, "%s:domainIndex=%d, portIndex=%d, missed one FUP\n",
			 __func__, sm->domainIndex, sm->portIndex);
		sm->last_state=REACTION;
		return WAITING_FOR_FOLLOW_UP;
	}

	if(RCVD_FOLLOWUP){
		if(RCVD_FOLLOWUP_PTR->head.sequenceId_ns == RCVD_SYNC_PTR->head.sequenceId_ns)
			return WAITING_FOR_SYNC;
		UB_TLOG(UBL_WARN, "%s:domainIndex=%d, portIndex=%d, Sync SqID=%d, FUP SqID=%d\n",
			 __func__, sm->domainIndex, sm->portIndex,
			ntohs(RCVD_SYNC_PTR->head.sequenceId_ns),
			ntohs(RCVD_FOLLOWUP_PTR->head.sequenceId_ns));
		RCVD_FOLLOWUP=false;
	}

	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !TWO_STEP_FLAG && ONE_STEP_RECEIVE) return WAITING_FOR_SYNC;

	if((cts64 >= sm->thisSM->followUpReceiptTimeoutTime.nsec &&
	    !ASYMMETRY_MEASUREMENT_MODE) ||
	   (RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	    !TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE && !ONE_STEP_RECEIVE )) {
		UB_TLOG(UBL_WARN, "%s:domainIndex=%d, portIndex=%d, timed out for FUP\n",
			 __func__, sm->domainIndex, sm->portIndex);
		return DISCARD;
	}

	return WAITING_FOR_FOLLOW_UP;
}

static void *waiting_for_sync_proc(md_sync_receive_data_t *sm, uint64_t cts64)
{
	int64_t dts;
	UB_LOG(UBL_DEBUGV, "md_sync_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_SYNC = false;
	if(RCVD_FOLLOWUP) {
		dts=cts64-sm->rfup_ts;
		if(sm->rfup_ts && dts>175000000) {
			UB_TLOG(UBL_INFO, "%s:domainIndex=%d, portIndex=%d, fup gap=%"PRIi64"\n",
				 __func__, sm->domainIndex, sm->portIndex, dts);
		}
		sm->rfup_ts=cts64;
	}
	sm->statd.sync_fup_rec_valid++;
	RCVD_FOLLOWUP = false;
	sm->thisSM->txMDSyncReceivePtr = setMDSyncReceive(sm);
	//UB_LOG(UBL_DEBUG, "%s:txMDSyncReceive\n", __func__);
	return sm->thisSM->txMDSyncReceivePtr; // txMDSyncReceive();
}

static md_sync_receive_state_t waiting_for_sync_condition(md_sync_receive_data_t *sm)
{
	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE && ONE_STEP_RECEIVE){
		sm->last_state=REACTION;
		return WAITING_FOR_SYNC;
	}

	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE
		) return WAITING_FOR_FOLLOW_UP;

	if(RCVD_SYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !TWO_STEP_FLAG && !ASYMMETRY_MEASUREMENT_MODE && !ONE_STEP_RECEIVE) return DISCARD;

	if(RCVD_FOLLOWUP){
		UB_TLOG(UBL_WARN, "%s:waiting Sync but received SyncFup, "
			"received.seqId=%d\n", __func__,
			ntohs(RCVD_FOLLOWUP_PTR->head.sequenceId_ns));
		RCVD_FOLLOWUP=false;
	}

	return WAITING_FOR_SYNC;
}


void *md_sync_receive_sm(md_sync_receive_data_t *sm, uint64_t cts64)
{
	bool state_change;
	void *retp=NULL;

	if(!sm) return NULL;
	sm->state = allstate_condition(sm);

	while(true){
		state_change=(sm->last_state != sm->state);
		sm->last_state = sm->state;
		switch(sm->state){
		case INIT:
			sm->state = DISCARD;
			break;
		case DISCARD:
			if(state_change)
				retp=discard_proc(sm);
			sm->state = discard_condition(sm);
			break;
		case WAITING_FOR_FOLLOW_UP:
			if(state_change)
				retp=waiting_for_follow_up_proc(sm, cts64);
			sm->state = waiting_for_follow_up_condition(sm, cts64);
			break;
		case WAITING_FOR_SYNC:
			if(state_change)
				retp=waiting_for_sync_proc(sm, cts64);
			sm->state = waiting_for_sync_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void md_sync_receive_sm_init(md_sync_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	MDEntityGlobal *mdeg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(md_sync_receive_data_t, MDSyncReceiveSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->mdeg = mdeg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int md_sync_receive_sm_close(md_sync_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void md_sync_receive_sm_recv_sync(md_sync_receive_data_t *sm, event_data_recv_t *edrecv,
				  uint64_t cts64)
{
	int size;
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_SYNC=true;
	size=GET_TWO_STEP_FLAG(*(MDPTPMsgHeader *)edrecv->recbptr)?
		sizeof(MDPTPMsgSync):sizeof(MDPTPMsgSyncOneStep);
	memcpy(&sm->recSync, edrecv->recbptr, size);
	RCVD_SYNC_PTR = (MDPTPMsgSync*)&sm->recSync;
	sm->rts = edrecv->ts64;
	sm->statd.sync_rec++;
	md_sync_receive_sm(sm, cts64);
}

void *md_sync_receive_sm_recv_fup(md_sync_receive_data_t *sm, event_data_recv_t *edrecv,
				  uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_FOLLOWUP=true;
	memcpy(&sm->recFollowUp, edrecv->recbptr, sizeof(MDPTPMsgFollowUp));
	RCVD_FOLLOWUP_PTR = &sm->recFollowUp;
	sm->statd.sync_fup_rec++;
	return md_sync_receive_sm(sm, cts64);
}

void md_sync_receive_stat_reset(md_sync_receive_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_sync_receive_stat_data_t));
}

md_sync_receive_stat_data_t *md_sync_receive_get_stat(md_sync_receive_data_t *sm)
{
	return &sm->statd;
}
