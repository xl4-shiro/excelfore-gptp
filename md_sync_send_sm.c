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
#include "md_sync_send_sm.h"
#include "md_abnormal_hooks.h"
#include <math.h>

typedef enum {
	INIT,
	INITIALIZING,
	SEND_SYNC_TWO_STEP,
	SEND_FOLLOW_UP,
	SEND_SYNC_ONE_STEP,
	SET_CORRECTION_FIELD,
	REACTION,
}md_sync_send_state_t;

struct md_sync_send_data{
	gptpnet_data_t *gpnetd;
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	MDEntityGlobal *mdeg;
	md_sync_send_state_t state;
	md_sync_send_state_t last_state;
	MDSyncSendSM *thisSM;
	int domainIndex;
	int portIndex;
	MDSyncSend *rcvdMDSyncPtr;
	uint64_t txSync_time;
	uint64_t sync_ts;
	uint64_t tsync_ts; // for debug use
	uint64_t pgap_ts; // for debug use
	uint64_t tfup_ts; // for debug use
	md_sync_send_stat_data_t statd;
	uint64_t mock_txts64;
};

#define RCVD_MDSYNC sm->thisSM->rcvdMDSync
#define RCVD_MDSYNC_PTR sm->rcvdMDSyncPtr
#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define AS_CAPABLE sm->ppg->asCapable
#define ASYMMETRY_MEASUREMENT_MODE sm->ppg->forAllDomain->asymmetryMeasurementMode
#define ONESTEP_TX_OPER sm->mdeg->oneStepTxOper
#define SYNC_SEQUENCE_ID sm->mdeg->syncSequenceId
#define RCVD_MDTIMESTAMP_RECEIVE sm->thisSM->rcvdMDTimestampReceive

static int setSyncTwoStep_txSync(md_sync_send_data_t *sm, uint64_t cts64)
{
	MDPTPMsgSync *sdata;
	int ssize=sizeof(MDPTPMsgSync);
	int64_t dts;
	int res;

	sdata=md_header_compose(sm->gpnetd, sm->portIndex, SYNC, ssize,
				RCVD_MDSYNC_PTR->sourcePortIdentity.clockIdentity,
				RCVD_MDSYNC_PTR->sourcePortIdentity.portNumber,
				SYNC_SEQUENCE_ID, sm->ppg->currentLogSyncInterval);
	if(!sdata) return -1;
	sdata->head.logMessageInterval=RCVD_MDSYNC_PTR->logMessageInterval;
	sdata->head.domainNumber=RCVD_MDSYNC_PTR->domainNumber;
	UB_LOG(UBL_DEBUGV, "md_sync_send:txSync domainIndex=%d, portIndex=%d\n",
	       sm->domainIndex, sm->portIndex);

	if(sm->pgap_ts==0) sm->pgap_ts=cts64;
	res=gptpnet_send_whook(sm->gpnetd, sm->portIndex-1, ssize);
	if(res==-1) return res;
	if(res<0) {
		sm->mock_txts64=gptpclock_getts64(sm->ptasg->thisClockIndex,
						  sm->ptasg->domainNumber);
		res=-res;
	}
	sm->statd.sync_send++;
	dts=cts64-sm->tsync_ts;
	if(dts>175000000) {
		UB_TLOG(UBL_INFO, "%s:domainIndex=%d, portIndex=%d, sync gap=%dmsec\n",
			__func__, sm->domainIndex, sm->portIndex, (int)(dts/1000000));
	}
	sm->tsync_ts=cts64;

	dts=cts64-sm->pgap_ts;
	if(dts>60000000){
		UB_TLOG(UBL_INFO, "%s:domainIndex=%d, portIndex=%d, sync blocked=%dmsec\n",
			__func__, sm->domainIndex, sm->portIndex,
			(int)(dts/1000000));
	}
	sm->pgap_ts=0;
	return res;
}

static int setFollowUp_txFollowUp(md_sync_send_data_t *sm, uint64_t cts64)
{
	MDPTPMsgFollowUp *sdata;
	int ssize=sizeof(MDPTPMsgFollowUp);
	PTPMsgHeader head;
	int64_t dts;
	int64_t cf;
	uint32_t tsns, tslsb;
	uint16_t tsmsb;

	sdata=(MDPTPMsgFollowUp *)gptpnet_get_sendbuf(sm->gpnetd, sm->portIndex-1);
	memset(sdata, 0, ssize);
	md_header_template(&head, FOLLOW_UP, ssize,
			   &RCVD_MDSYNC_PTR->sourcePortIdentity, SYNC_SEQUENCE_ID,
			   sm->ppg->currentLogSyncInterval);
	if(gptpclock_we_are_gm(sm->domainIndex)){
		head.correctionField=0;
		cf = sm->sync_ts - RCVD_MDSYNC_PTR->upstreamTxTime.nsec;
		// we assume cf<1sec
	}else{
		head.correctionField = (RCVD_MDSYNC_PTR->followUpCorrectionField.nsec<<16);
		head.correctionField += (RCVD_MDSYNC_PTR->rateRatio *
					 ((sm->sync_ts -
					   RCVD_MDSYNC_PTR->upstreamTxTime.nsec)<<16));
		cf=0;
	}

	head.domainNumber = RCVD_MDSYNC_PTR->domainNumber;

	md_compose_head(&head, (MDPTPMsgHeader *)sdata);

	tsmsb=RCVD_MDSYNC_PTR->preciseOriginTimestamp.seconds.msb;
	tslsb=RCVD_MDSYNC_PTR->preciseOriginTimestamp.seconds.lsb;
	tsns=RCVD_MDSYNC_PTR->preciseOriginTimestamp.nanoseconds+cf;
	if(tsns>=UB_SEC_NS){
		tsns-=UB_SEC_NS;
		if(++tslsb==0) tsmsb++;
	}
	sdata->preciseOriginTimestamp.seconds_msb_ns = htons(tsmsb);
	sdata->preciseOriginTimestamp.seconds_lsb_nl = htonl(tslsb);
	sdata->preciseOriginTimestamp.nanoseconds_nl = htonl(tsns);

	md_followup_information_tlv_compose(&sdata->FUpInfoTLV,
					    RCVD_MDSYNC_PTR->rateRatio,
					    RCVD_MDSYNC_PTR->gmTimeBaseIndicator,
					    RCVD_MDSYNC_PTR->lastGmPhaseChange,
					    RCVD_MDSYNC_PTR->lastGmFreqChange);

	UB_LOG(UBL_DEBUGV, "md_sync_send:txFollowUp domainIndex=%d, portIndex=%d\n",
	       sm->domainIndex, sm->portIndex);

	dts=cts64-sm->tfup_ts;
	if(dts>175000000) {
		UB_TLOG(UBL_INFO, "%s:domainIndex=%d, portIndex=%d, fup gap=%dmsec\n",
			 __func__, sm->domainIndex, sm->portIndex, (int)(dts/1000000));
	}
	if(gptpnet_send_whook(sm->gpnetd, sm->portIndex-1, ssize)==-1) return -1;
	sm->statd.sync_fup_send++;
	sm->tfup_ts=cts64;
	return 0;
}

static md_sync_send_state_t allstate_condition(md_sync_send_data_t *sm)
{
	/*
	  from 'MDSyncSendSM state machine' in the standard,
	  (sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	  (RCVD_MDSYNC && (!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE)))
	  is the right condition, but we remove 'RCVD_MDSYNC &&' part to avoid some issues
	 */
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   !PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE) {
		sm->last_state=REACTION;
		return INITIALIZING;
	}
	return sm->state;
}

static void *initializing_proc(md_sync_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_sync_send:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MDSYNC = false;
	RCVD_MDTIMESTAMP_RECEIVE = false;
	SYNC_SEQUENCE_ID = rand() & 0xffff;
	return NULL;
}

static md_sync_send_state_t initializing_condition(md_sync_send_data_t *sm)
{
	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE && !ONESTEP_TX_OPER)
		return SEND_SYNC_TWO_STEP;

	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE && ONESTEP_TX_OPER)
		return SEND_SYNC_ONE_STEP;

	return INITIALIZING;
}

static int send_sync_two_step_proc(md_sync_send_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "md_sync_send:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MDSYNC = false;
	if(setSyncTwoStep_txSync(sm, cts64)<0) return -1;
	sm->txSync_time = cts64;
	return 0;
}

static md_sync_send_state_t send_sync_two_step_condition(md_sync_send_data_t *sm, uint64_t cts64)
{
	if(RCVD_MDTIMESTAMP_RECEIVE) return SEND_FOLLOW_UP;
	if(sm->mock_txts64){
		sm->sync_ts=sm->mock_txts64;
		sm->mock_txts64=0;
		return SEND_FOLLOW_UP;
	}
	if((cts64 - sm->txSync_time >=
	    gptpnet_txtslost_time(sm->gpnetd, sm->portIndex-1))){
		UB_TLOG(UBL_WARN,"%s:missing TxTS, domainIndex=%d, portIndex=%d, seqID=%d\n",
			 __func__, sm->domainIndex, sm->portIndex, SYNC_SEQUENCE_ID);
		// send SYNC with the same SequenceID again
		sm->last_state=REACTION;
		return SEND_SYNC_TWO_STEP;
	}
	return SEND_SYNC_TWO_STEP;
}

static int send_follow_up_proc(md_sync_send_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "md_sync_send:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MDTIMESTAMP_RECEIVE = false;
	if(setFollowUp_txFollowUp(sm, cts64)) return -1;
	SYNC_SEQUENCE_ID += 1;
	return 0;
}

static md_sync_send_state_t send_follow_up_condition(md_sync_send_data_t *sm)
{
	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE && !ONESTEP_TX_OPER ) return SEND_SYNC_TWO_STEP;

	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE && ONESTEP_TX_OPER ) return SEND_SYNC_ONE_STEP;

	return SEND_FOLLOW_UP;
}

static void *send_sync_one_step_proc(md_sync_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_sync_send:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MDSYNC = false;
	//txSyncPtr = setSyncOneStep ();
	//txSync (txSyncPtr);
	SYNC_SEQUENCE_ID += 1;
	return NULL;
}

static md_sync_send_state_t send_sync_one_step_condition(md_sync_send_data_t *sm)
{
	if(RCVD_MDTIMESTAMP_RECEIVE) return SET_CORRECTION_FIELD;
	return SEND_SYNC_ONE_STEP;
}

static void *set_correction_field_proc(md_sync_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_sync_send:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MDTIMESTAMP_RECEIVE = false;
	//modifySync();
	return NULL;
}

static md_sync_send_state_t set_correction_field_condition(md_sync_send_data_t *sm)
{
	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE && !ONESTEP_TX_OPER ) return SEND_SYNC_TWO_STEP;

	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE && !ONESTEP_TX_OPER ) return SEND_SYNC_ONE_STEP;

	return SET_CORRECTION_FIELD;
}

// return 1 when SYNC(two step) is sent, oterwise return 0
int md_sync_send_sm(md_sync_send_data_t *sm, uint64_t cts64)
{
	bool state_change;

	if(!sm) return 0;
	sm->state = allstate_condition(sm);

	while(true){
		state_change=(sm->last_state != sm->state);
		sm->last_state = sm->state;
		switch(sm->state){
		case INIT:
			sm->state = INITIALIZING;
			break;
		case INITIALIZING:
			if(state_change)
				initializing_proc(sm);
			sm->state = initializing_condition(sm);
			break;
		case SEND_SYNC_TWO_STEP:
			if(state_change){
				if(send_sync_two_step_proc(sm, cts64)<0){
					sm->last_state = REACTION;
					return 0;
				}
				return 1;
			}
			sm->state = send_sync_two_step_condition(sm, cts64);
			break;
		case SEND_FOLLOW_UP:
			if(state_change){
				if(send_follow_up_proc(sm, cts64)<0){
					sm->last_state = REACTION;
					return 0;
				}
				break;
			}
			sm->state = send_follow_up_condition(sm);
			break;
		case SEND_SYNC_ONE_STEP:
			if(state_change)
				send_sync_one_step_proc(sm);
			sm->state = send_sync_one_step_condition(sm);
			break;
		case SET_CORRECTION_FIELD:
			if(state_change)
				set_correction_field_proc(sm);
			sm->state = set_correction_field_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(sm->last_state == sm->state) break;
	}
	return 0;
}

void md_sync_send_sm_init(md_sync_send_data_t **sm,
			  int domainIndex, int portIndex,
			  gptpnet_data_t *gpnetd,
			  PerTimeAwareSystemGlobal *ptasg,
			  PerPortGlobal *ppg,
			  MDEntityGlobal *mdeg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, domainIndex, portIndex);
	INIT_SM_DATA(md_sync_send_data_t, MDSyncSendSM, sm);
	(*sm)->gpnetd = gpnetd;
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->mdeg = mdeg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
	md_sync_send_sm((*sm),0);
}

int md_sync_send_sm_close(md_sync_send_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

int md_sync_send_sm_mdSyncSend(md_sync_send_data_t *sm,
			       MDSyncSend *mdSyncSend, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MDSYNC = true;
	RCVD_MDSYNC_PTR = mdSyncSend;
	return md_sync_send_sm(sm, cts64);
}

void md_sync_send_sm_txts(md_sync_send_data_t *sm, event_data_txts_t *edtxts,
			  uint64_t cts64)
{
	int pi;
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d, seqID=%d\n",
	       __func__, sm->domainIndex, sm->portIndex, edtxts->seqid);
	if(md_abnormal_timestamp(SYNC, sm->portIndex-1, sm->ptasg->domainNumber)) return;
	RCVD_MDTIMESTAMP_RECEIVE = true;
	pi=gptpconf_get_intitem(CONF_SINGLE_CLOCK_MODE)?1:sm->portIndex;
	gptpclock_tsconv(&edtxts->ts64, pi, 0,
			 sm->ptasg->thisClockIndex, sm->ptasg->domainNumber);
	sm->sync_ts=edtxts->ts64;
	md_sync_send_sm(sm, cts64);
}

void md_sync_send_stat_reset(md_sync_send_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_sync_send_stat_data_t));
}

md_sync_send_stat_data_t *md_sync_send_get_stat(md_sync_send_data_t *sm)
{
	return &sm->statd;
}
