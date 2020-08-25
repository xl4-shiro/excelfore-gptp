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
#include "md_pdelay_resp_sm.h"
#include "md_abnormal_hooks.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIAL_WAITING_FOR_PDELAY_REQ,
	WAITING_FOR_PDELAY_REQ,
	SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP,
	REACTION,
}md_pdelay_resp_state_t;

struct md_pdelay_resp_data{
	gptpnet_data_t *gpnetd;
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	md_pdelay_resp_state_t state;
	md_pdelay_resp_state_t last_state;
	MDPdelayRespSM *thisSM;
	int portIndex;
	MDPTPMsgPdelayReq rcvdPdelayReq;
	MDPTPMsgPdelayResp txPdelayResp;
	MDPTPMsgPdelayRespFollowUp txPdelayRespFollowUp;
	int64_t ts2;
	int64_t ts3;
	uint64_t txPdelayResp_time;
	int cmlds_mode;
	int last_seqid;
	md_pdelay_resp_stat_data_t statd;
	uint64_t mock_txts64;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper

static MDPTPMsgPdelayRespFollowUp *setPdelayRespFollowUp(md_pdelay_resp_data_t *sm)
{
	MDPTPMsgPdelayRespFollowUp *sdata;
	int ssize=sizeof(MDPTPMsgPdelayRespFollowUp);
	struct timespec ts;

	sdata=md_header_compose(sm->gpnetd, sm->portIndex, PDELAY_RESP_FOLLOW_UP, ssize,
				sm->ptasg->thisClock, sm->ppg->thisPort,
				ntohs(sm->thisSM->rcvdPdelayReqPtr->head.sequenceId_ns),
				0x7f);
	if(!sdata) return NULL;
	if(sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq==-1 && sm->cmlds_mode){
		// PdelayReq came with CMLDS and we are also in CMLDS
		sdata->head.majorSdoId_messageType =
			(sdata->head.majorSdoId_messageType & 0x0F) | 0x20;
	}
	// As the fractional ns portion can't be measure, leave the 'correctionField' as 0
	UB_NSEC2TS(sm->ts3, ts);
	sdata->requestOriginTimestamp.seconds_lsb_nl=htonl(ts.tv_sec);
	sdata->requestOriginTimestamp.nanoseconds_nl=htonl(ts.tv_nsec);
	memcpy(&sdata->requestingPortIdentity,
	       &sm->thisSM->rcvdPdelayReqPtr->head.sourcePortIdentity,
	       sizeof(MDPortIdentity));
	memcpy(&sm->txPdelayRespFollowUp, sdata, sizeof(MDPTPMsgPdelayRespFollowUp));
	return &sm->txPdelayRespFollowUp;
}

static int txPdelayRespFollowUp(gptpnet_data_t *gpnetd, int portIndex)
{
	int ssize=sizeof(MDPTPMsgPdelayRespFollowUp);
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n",__func__, portIndex);
	return gptpnet_send_whook(gpnetd, portIndex-1, ssize);
}

static MDPTPMsgPdelayResp *setPdelayResp(md_pdelay_resp_data_t *sm)
{
	MDPTPMsgPdelayResp *sdata;
	int ssize=sizeof(MDPTPMsgPdelayResp);
	struct timespec ts;

	sdata=md_header_compose(sm->gpnetd, sm->portIndex, PDELAY_RESP, ssize,
				sm->ptasg->thisClock, sm->ppg->thisPort,
				ntohs(sm->thisSM->rcvdPdelayReqPtr->head.sequenceId_ns),
				0x7f);
	if(!sdata) return NULL;
	if(sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq==-1 && sm->cmlds_mode){
		// PdelayReq came with CMLDS and we are also in CMLDS
		sdata->head.majorSdoId_messageType =
			(sdata->head.majorSdoId_messageType & 0x0F) | 0x20;
	}
	// As the fractional ns portion can't be measure, leave the 'correctionField' as 0
	UB_NSEC2TS(sm->ts2, ts);
	sdata->requestReceiptTimestamp.seconds_lsb_nl=htonl(ts.tv_sec);
	sdata->requestReceiptTimestamp.nanoseconds_nl=htonl(ts.tv_nsec);
	memcpy(&sdata->requestingPortIdentity,
	       &sm->thisSM->rcvdPdelayReqPtr->head.sourcePortIdentity,
	       sizeof(MDPortIdentity));
	memcpy(&sm->txPdelayResp, sdata, sizeof(MDPTPMsgPdelayResp));
	return &sm->txPdelayResp;
}

static int txPdelayResp(gptpnet_data_t *gpnetd, int portIndex)
{
	int ssize=sizeof(MDPTPMsgPdelayReq);
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n",__func__, portIndex);
	return gptpnet_send_whook(gpnetd, portIndex-1, ssize);
}

static md_pdelay_resp_state_t allstate_condition(md_pdelay_resp_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ppg->forAllDomain->portOper || !sm->thisSM->portEnabled1)
		return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(md_pdelay_resp_data_t *sm)
{
	if(PORT_OPER)
		UB_LOG(UBL_DEBUGV, "md_pdelay_resp:%s:portIndex=%d\n", __func__, sm->portIndex);
	return NULL;
}

static md_pdelay_resp_state_t not_enabled_condition(md_pdelay_resp_data_t *sm)
{
	if(sm->thisSM->portEnabled1) return INITIAL_WAITING_FOR_PDELAY_REQ;
	return NOT_ENABLED;
}

static void *initial_waiting_for_pdelay_req_proc(md_pdelay_resp_data_t *sm)
{
	if(PORT_OPER)
		UB_LOG(UBL_DEBUGV, "md_pdelay_resp:%s:portIndex=%d\n", __func__, sm->portIndex);
	sm->thisSM->rcvdPdelayReq = false;
	sm->last_seqid = -1;
	return NULL;
}

static md_pdelay_resp_state_t initial_waiting_for_pdelay_req_condition(md_pdelay_resp_data_t *sm)
{
	if(sm->thisSM->rcvdPdelayReq) return SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP;
	return INITIAL_WAITING_FOR_PDELAY_REQ;
}

static int waiting_for_pdelay_req_proc(md_pdelay_resp_data_t *sm)
{
	if(PORT_OPER)
		UB_LOG(UBL_DEBUGV, "md_pdelay_resp:%s:portIndex=%d\n", __func__, sm->portIndex);
	sm->thisSM->txPdelayRespFollowUpPtr =
		setPdelayRespFollowUp(sm);
	if(txPdelayRespFollowUp(sm->gpnetd, sm->portIndex)==-1) return -1;
	sm->statd.pdelay_resp_fup_send++;
	return 0;
}

static md_pdelay_resp_state_t waiting_for_pdelay_req_condition(md_pdelay_resp_data_t *sm)
{
	if(sm->thisSM->rcvdPdelayReq) return SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP;
	return WAITING_FOR_PDELAY_REQ;
}

static int sent_pdelay_resp_waiting_for_timestamp_proc(md_pdelay_resp_data_t *sm,
						       uint64_t cts64)
{
	int res;
	UB_LOG(UBL_DEBUGV, "md_pdelay_resp:%s:portIndex=%d\n", __func__, sm->portIndex);
	sm->thisSM->rcvdPdelayReq = false;
	sm->thisSM->txPdelayRespPtr = setPdelayResp(sm);
	sm->thisSM->rcvdMDTimestampReceive = false;
	res=txPdelayResp(sm->gpnetd, sm->portIndex);
	if(res==-1) return -1;
	sm->txPdelayResp_time=cts64;
	sm->statd.pdelay_resp_send++;
	if(res<0){
		sm->mock_txts64=gptpclock_getts64(sm->ptasg->thisClockIndex,0);
		res=-res;
	}
	return res;
}

static md_pdelay_resp_state_t sent_pdelay_resp_waiting_for_timestamp_condition(
	md_pdelay_resp_data_t *sm, uint64_t cts64)
{
	if(sm->thisSM->rcvdMDTimestampReceive) return WAITING_FOR_PDELAY_REQ;
	if(cts64 - sm->txPdelayResp_time >=
	   gptpnet_txtslost_time(sm->gpnetd, sm->portIndex-1)){
		if(sm->mock_txts64){
			sm->ts3=sm->mock_txts64;
			sm->mock_txts64=0;
			return WAITING_FOR_PDELAY_REQ;
		}
		UB_TLOG(UBL_WARN, "md_pdelay_resp:%s:missing TxTS, portIndex=%d, seqID=%d\n",
			 __func__, sm->portIndex,
			 ntohs(sm->thisSM->rcvdPdelayReqPtr->head.sequenceId_ns));
		// send again the same PdelayResp
		sm->last_state=REACTION;
		return SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP;
		// Or ignore this PdelayReq, and wait next one
		//return INITIAL_WAITING_FOR_PDELAY_REQ
	}
	return SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP;
}

// return 1 when PdelayResp is sent, otherwise return 0;
int md_pdelay_resp_sm(md_pdelay_resp_data_t *sm, uint64_t cts64)
{
	bool state_change;

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
		case INITIAL_WAITING_FOR_PDELAY_REQ:
			if(state_change)
				initial_waiting_for_pdelay_req_proc(sm);
			sm->state = initial_waiting_for_pdelay_req_condition(sm);
			break;
		case WAITING_FOR_PDELAY_REQ:
			if(state_change)
				if(waiting_for_pdelay_req_proc(sm)){
					sm->last_state=REACTION;
					return 0;
				}
			sm->state = waiting_for_pdelay_req_condition(sm);
			break;
		case SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP:
			if(state_change){
				if(sent_pdelay_resp_waiting_for_timestamp_proc(sm, cts64)<0){
					sm->last_state=REACTION;
					return 0;
				}
				return 1;
			}
			sm->state = sent_pdelay_resp_waiting_for_timestamp_condition(sm, cts64);
			break;
		case REACTION:
			break;
		}
		if(sm->last_state == sm->state) break;
	}
	return 0;
}

void md_pdelay_resp_sm_init(md_pdelay_resp_data_t **sm,
	int portIndex,
	gptpnet_data_t *gpnetd,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, portIndex);
	INIT_SM_DATA(md_pdelay_resp_data_t, MDPdelayRespSM, sm);
	(*sm)->gpnetd = gpnetd;
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->portIndex = portIndex;
	(*sm)->cmlds_mode = gptpconf_get_intitem(CONF_CMLDS_MODE);
	// 11.2.18.2.5
	if((*sm)->cmlds_mode){
		// ??? cmldsLinkPortEnabled for CMLDS, but we use ptpPortEnabled here
		(*sm)->thisSM->portEnabled1 = (*sm)->ppg->ptpPortEnabled;
	}else{
		(*sm)->thisSM->portEnabled1 = (*sm)->ppg->ptpPortEnabled;
	}
}

int md_pdelay_resp_sm_close(md_pdelay_resp_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

int md_pdelay_resp_sm_recv_req(md_pdelay_resp_data_t *sm, event_data_recv_t *edrecv,
				uint64_t cts64)
{
	uint16_t recsqid;
	UB_TLOG(UBL_DEBUGV, "%s:port=%d, state=%d\n",__func__, sm->portIndex, sm->state);
	sm->statd.pdelay_req_rec++;
	memcpy(&sm->rcvdPdelayReq, edrecv->recbptr, sizeof(MDPTPMsgPdelayReq));
	sm->thisSM->rcvdPdelayReqPtr = &sm->rcvdPdelayReq;
	recsqid=ntohs(sm->thisSM->rcvdPdelayReqPtr->head.sequenceId_ns);
	if(sm->last_seqid >= 0){
		uint16_t expsqid;
		expsqid=(sm->last_seqid+1)&0xffff;
		if(expsqid != recsqid){
			UB_TLOG(UBL_WARN, "%s: port=%d, expected SeqID=%u, but received=%u\n",
			       __func__, sm->portIndex, expsqid, recsqid);
			sm->last_seqid=-1;
			return 0;
		}
	}
	sm->statd.pdelay_req_rec_valid++;
	sm->last_seqid=recsqid;
	sm->thisSM->rcvdPdelayReq = true;
	sm->ts2=edrecv->ts64;
	if((sm->thisSM->rcvdPdelayReqPtr->head.majorSdoId_messageType & 0xF0)==0x20){
		if(sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq!=-1){
			UB_LOG(UBL_INFO, "%s:port=%d, set receivedNonCMLDSPdelayReq=-1\n",
			       __func__, sm->portIndex);
			sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq=-1;
		}
	}
	if((sm->thisSM->rcvdPdelayReqPtr->head.majorSdoId_messageType & 0xF0)==0x10){
		if(sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq!=1){
			UB_LOG(UBL_INFO, "%s:port=%d, set receivedNonCMLDSPdelayReq=1\n",
			       __func__, sm->portIndex);
			sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq=1;
		}
	}
	return md_pdelay_resp_sm(sm, cts64);
}

void md_pdelay_resp_sm_txts(md_pdelay_resp_data_t *sm, event_data_txts_t *edtxts,
			    uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d, received seqID=%d\n",
	       __func__, sm->portIndex, edtxts->seqid);
	if(md_abnormal_timestamp(PDELAY_RESP, sm->portIndex-1, -1)) return;
	if(sm->state!=SENT_PDELAY_RESP_WAITING_FOR_TIMESTAMP ||
	   edtxts->seqid != ntohs(sm->thisSM->rcvdPdelayReqPtr->head.sequenceId_ns)){
		UB_TLOG(UBL_WARN, "%s:TxTS is not expected, seqid expected=%d, received=%d\n",
			 __func__, ntohs(sm->thisSM->rcvdPdelayReqPtr->head.sequenceId_ns),
			 edtxts->seqid);
		return;
	}
	sm->ts3=edtxts->ts64;
	sm->thisSM->rcvdMDTimestampReceive = true;
	md_pdelay_resp_sm(sm, cts64);
}

void md_pdelay_resp_stat_reset(md_pdelay_resp_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_pdelay_resp_stat_data_t));
}

md_pdelay_resp_stat_data_t *md_pdelay_resp_get_stat(md_pdelay_resp_data_t *sm)
{
	return &sm->statd;
}
