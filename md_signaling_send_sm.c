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
#include "md_signaling_send_sm.h"
#include "md_abnormal_hooks.h"

typedef enum {
	INIT,
	INITIALIZE,
	SEND_SIGNALING,
	REACTION,
}md_signaling_send_state_t;

typedef enum {
	MD_SIGNALING_NONE = 0,
	MD_SIGNALING_MSG_INTERVAL_REQ,
	MD_SIGNALING_GPTP_CAPABLE,
} md_signaling_type_t;

struct md_signaling_send_data{
	gptpnet_data_t *gpnetd;
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	md_signaling_send_state_t state;
	md_signaling_send_state_t last_state;
	int domainIndex;
	int portIndex;
	md_signaling_type_t stype;
	void *rcvd_txmsg;
	uint16_t sequenceId;
	md_signaling_send_stat_data_t statd;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled

static void setup_gptp_capable_tlv(md_signaling_send_data_t *sm, void *sdata)
{
	MDPTPMsgGPTPCapableTLV *gcmsg=(MDPTPMsgGPTPCapableTLV *)sdata;
	PTPMsgGPTPCapableTLV *gctlm=(PTPMsgGPTPCapableTLV *)sm->rcvd_txmsg;
	gcmsg->tlvType_ns = htons(gctlm->tlvType);
	gcmsg->lengthField_ns = htons(gctlm->lengthField);
	memcpy(gcmsg->organizationId, gctlm->organizationId, 3);
	gcmsg->organizationSubType_nb[0] = (gctlm->organizationSubType >> 16) & 0xff;
	gcmsg->organizationSubType_nb[1] = (gctlm->organizationSubType >> 8) & 0xff;
	gcmsg->organizationSubType_nb[2] = gctlm->organizationSubType & 0xff;
	gcmsg->logGptpCapableMessageInterval = gctlm->logGptpCapableMessageInterval;
	gcmsg->flags = gctlm->flags;
}

static void setup_msg_interval_req_tlv(md_signaling_send_data_t *sm, void *sdata)
{
	MDPTPMsgIntervalRequestTLV *mrmsg=(MDPTPMsgIntervalRequestTLV *)sdata;
	PTPMsgIntervalRequestTLV *mrtlm=(PTPMsgIntervalRequestTLV *)sm->rcvd_txmsg;
	mrmsg->tlvType_ns = htons(mrtlm->tlvType);
	mrmsg->lengthField_ns = htons(mrtlm->lengthField);
	memcpy(mrmsg->organizationId, mrtlm->organizationId, 3);
	mrmsg->organizationSubType_nb[0] = (mrtlm->organizationSubType >> 16) & 0xff;
	mrmsg->organizationSubType_nb[1] = (mrtlm->organizationSubType >> 8) & 0xff;
	mrmsg->organizationSubType_nb[2] = mrtlm->organizationSubType & 0xff;
	mrmsg->linkDelayInterval = mrtlm->linkDelayInterval;
	mrmsg->timeSyncInterval = mrtlm->timeSyncInterval;
	mrmsg->announceInterval = mrtlm->announceInterval;
	mrmsg->flags = mrtlm->flags;
}

static int sendSignaling(md_signaling_send_data_t *sm)
{
	int size;
	void *sdata;
	uint32_t *statp=NULL;
	switch(sm->stype){
	case MD_SIGNALING_MSG_INTERVAL_REQ:
		size=sizeof(MDPTPMsgIntervalRequestTLV);
		break;
	case MD_SIGNALING_GPTP_CAPABLE:
		size=sizeof(MDPTPMsgGPTPCapableTLV);
		break;
	case MD_SIGNALING_NONE:
	default:
		return -1;
	}

        sdata=md_header_compose(sm->gpnetd, sm->portIndex, SIGNALING, size,
                                sm->ptasg->thisClock,
                                sm->ppg->thisPort,
                                sm->sequenceId,
                                0x7f);
	// 10.6.4.2.1 targetPortIdentity is 0xff
	memset(sdata+sizeof(MDPTPMsgHeader), 0xff, sizeof(MDPortIdentity));
	((MDPTPMsgHeader*)sdata)->domainNumber=sm->ptasg->domainNumber;
	switch(sm->stype){
	case MD_SIGNALING_MSG_INTERVAL_REQ:
		setup_msg_interval_req_tlv(sm, sdata);
		statp=&sm->statd.signal_msg_interval_send;
		break;
	case MD_SIGNALING_GPTP_CAPABLE:
		setup_gptp_capable_tlv(sm, sdata);
		statp=&sm->statd.signal_gptp_capable_send;
		break;
	default:
		return -1;
	}
        if(gptpnet_send_whook(sm->gpnetd, sm->portIndex-1, size)==-1) return -2;
	sm->sequenceId++;
	if(statp) (*statp)++;
	return 0;
}

static md_signaling_send_state_t allstate_condition(md_signaling_send_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   !PORT_OPER || !PTP_PORT_ENABLED) {
		sm->last_state=REACTION;
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(md_signaling_send_data_t *sm)
{
	if(PORT_OPER) UB_LOG(UBL_DEBUGV, "md_signaling_send:%s:domainIndex=%d, portIndex=%d\n",
			     __func__, sm->domainIndex, sm->portIndex);
	sm->sequenceId=(uint16_t)(rand() & 0xffff);
	return NULL;
}

static md_signaling_send_state_t initialize_condition(md_signaling_send_data_t *sm)
{
	if(sm->stype) return SEND_SIGNALING;
	return INITIALIZE;
}

static int send_signaling_proc(md_signaling_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_signaling_send:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	if(sendSignaling(sm)==-2) return -1;
	sm->stype=MD_SIGNALING_NONE;
	return 0;
}

static md_signaling_send_state_t send_signaling_condition(md_signaling_send_data_t *sm)
{
	if(sm->stype) sm->last_state=REACTION;
	return SEND_SIGNALING;
}


void *md_signaling_send_sm(md_signaling_send_data_t *sm, uint64_t cts64)
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
		case SEND_SIGNALING:
			if(state_change){
				if(send_signaling_proc(sm)) {
					sm->last_state=REACTION;
					return NULL;
				}
				break;
			}
			sm->state = send_signaling_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void md_signaling_send_sm_init(md_signaling_send_data_t **sm,
	int domainIndex, int portIndex,
	gptpnet_data_t *gpnetd,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	if(!*sm){
		*sm=malloc(sizeof(md_signaling_send_data_t));
		ub_assert(*sm, __func__, "malloc");
	}
	memset(*sm, 0, sizeof(md_signaling_send_data_t));
	(*sm)->gpnetd = gpnetd;
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int md_signaling_send_sm_close(md_signaling_send_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	if(!*sm) return 0;
	free(*sm);
	*sm=NULL;
	return 0;
}

void *md_signaling_send_sm_mdSignalingSend(md_signaling_send_data_t *sm, void *msg,
					   uint64_t cts64)
{
	uint32_t stype;
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	stype=((PTPMsgGPTPCapableTLV *)msg)->organizationSubType;
	sm->stype=MD_SIGNALING_NONE;
	if(stype==2){
		sm->stype=MD_SIGNALING_MSG_INTERVAL_REQ;
	}else if(stype==4){
		sm->stype=MD_SIGNALING_GPTP_CAPABLE;
	}else{
		UB_LOG(UBL_WARN,
		       "%s:domainIndex=%d, portIndex=%d unknown organizationSubType=%d\n",
		       __func__, sm->domainIndex, sm->portIndex, stype);
		return NULL;
	}
	sm->rcvd_txmsg=msg;
	sm->last_state=REACTION;
	return md_signaling_send_sm(sm, cts64);
}

void md_signaling_send_stat_reset(md_signaling_send_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_signaling_send_stat_data_t));
}

md_signaling_send_stat_data_t *md_signaling_send_get_stat(md_signaling_send_data_t *sm)
{
	return &sm->statd;
}
