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
#include "md_signaling_receive_sm.h"

typedef enum {
	INIT,
	INITIALIZE,
	RECV_SIGNALING,
	REACTION,
}md_signaling_receive_state_t;

struct md_signaling_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	md_signaling_receive_state_t state;
	md_signaling_receive_state_t last_state;
	int domainIndex;
	int portIndex;
	bool recv;
	void *rcvd_rxmsg;
	PTPMsgGPTPCapableTLV gctlm;
	PTPMsgIntervalRequestTLV mrtlm;
	md_signaling_receive_stat_data_t statd;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled

static void *recv_gptp_capable(md_signaling_receive_data_t *sm)
{
	MDPTPMsgGPTPCapableTLV *gcmsg=(MDPTPMsgGPTPCapableTLV *)sm->rcvd_rxmsg;
	//PTPMsgHeader header;
	//md_decompose_head((MDPTPMsgHeader *)sm->rcvd_rxmsg, &header);
	sm->gctlm.tlvType = ntohs(gcmsg->tlvType_ns);
	sm->gctlm.lengthField = ntohs(gcmsg->lengthField_ns);
	memcpy(&sm->gctlm.organizationId, gcmsg->organizationId, 3);
	sm->gctlm.organizationSubType =
		(gcmsg->organizationSubType_nb[0] << 16) |
		(gcmsg->organizationSubType_nb[1] << 8) |
		gcmsg->organizationSubType_nb[2];
	sm->gctlm.logGptpCapableMessageInterval=gcmsg->logGptpCapableMessageInterval;
	sm->gctlm.flags = gcmsg->flags;
	return &sm->gctlm;
}

static void *recv_msg_interval_req(md_signaling_receive_data_t *sm)
{
	MDPTPMsgIntervalRequestTLV *mrmsg=(MDPTPMsgIntervalRequestTLV *)sm->rcvd_rxmsg;
	//PTPMsgHeader header;
	//md_decompose_head((MDPTPMsgHeader *)sm->rcvd_rxmsg, &header);
	sm->mrtlm.tlvType = ntohs(mrmsg->tlvType_ns);
	sm->mrtlm.lengthField = ntohs(mrmsg->lengthField_ns);
	memcpy(&sm->mrtlm.organizationId, mrmsg->organizationId, 3);
	sm->mrtlm.organizationSubType =
		(mrmsg->organizationSubType_nb[0] << 16) |
		(mrmsg->organizationSubType_nb[1] << 8) |
		mrmsg->organizationSubType_nb[2];
	sm->mrtlm.linkDelayInterval = mrmsg->linkDelayInterval;
	sm->mrtlm.timeSyncInterval = mrmsg->timeSyncInterval;
	sm->mrtlm.announceInterval = mrmsg->announceInterval;
	sm->mrtlm.flags = mrmsg->flags;
	return &sm->mrtlm;
}

static md_signaling_receive_state_t allstate_condition(md_signaling_receive_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   !PORT_OPER || !PTP_PORT_ENABLED) {
		sm->last_state=REACTION;
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(md_signaling_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_signaling_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	return NULL;
}

static md_signaling_receive_state_t initialize_condition(md_signaling_receive_data_t *sm)
{
	if(sm->recv) return RECV_SIGNALING;
	return INITIALIZE;
}

static void *recv_signaling_proc(md_signaling_receive_data_t *sm)
{
	uint8_t stype;
	UB_LOG(UBL_DEBUGV, "md_signaling_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->recv=false;
	stype=((MDPTPMsgGPTPCapableTLV*)(sm->rcvd_rxmsg))->organizationSubType_nb[2];
	if(stype==2){
		sm->statd.signal_msg_interval_rec++;
		return recv_msg_interval_req(sm);
	}
	else if(stype==4){
		sm->statd.signal_gptp_capable_rec++;
		return recv_gptp_capable(sm);
	}
	UB_LOG(UBL_WARN, "%s:unknown tlv type = %d\n", __func__, stype);
	return NULL;
}

static md_signaling_receive_state_t recv_signaling_condition(md_signaling_receive_data_t *sm)
{
	if(sm->recv) sm->last_state=REACTION;
	return RECV_SIGNALING;
}


void *md_signaling_receive_sm(md_signaling_receive_data_t *sm, uint64_t cts64)
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
			sm->state = INITIALIZE;
			break;
		case INITIALIZE:
			if(state_change)
				retp=initialize_proc(sm);
			sm->state = initialize_condition(sm);
			break;
		case RECV_SIGNALING:
			if(state_change){
				retp=recv_signaling_proc(sm);
				break;
			}
			sm->state = recv_signaling_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void md_signaling_receive_sm_init(md_signaling_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	if(!*sm){
		*sm=malloc(sizeof(md_signaling_receive_data_t));
		ub_assert(*sm, __func__, "malloc");
	}
	memset(*sm, 0, sizeof(md_signaling_receive_data_t));
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int md_signaling_receive_sm_close(md_signaling_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	if(!*sm) return 0;
	free(*sm);
	*sm=NULL;
	return 0;
}

void *md_signaling_receive_sm_mdSignalingRec(md_signaling_receive_data_t *sm,
					     event_data_recv_t *edrecv, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->recv=true;
	sm->rcvd_rxmsg=edrecv->recbptr;
	sm->last_state=REACTION;
	sm->statd.signal_rec++;
	return md_signaling_receive_sm(sm, cts64);
}

void md_signaling_receive_stat_reset(md_signaling_receive_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_signaling_receive_stat_data_t));
}

md_signaling_receive_stat_data_t *md_signaling_receive_get_stat(md_signaling_receive_data_t *sm)
{
	return &sm->statd;
}
