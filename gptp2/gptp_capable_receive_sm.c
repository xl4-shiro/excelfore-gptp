/*
 * excelfore-gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of excelfore-gptp.
 *
 * excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
#include "mind.h"
#include "mdeth.h"
#include "gptpnet.h"
#include "gptpclock.h"
#include "gptp_capable_receive_sm.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIALIZE,
	RECEIVED_TLV,
	REACTION,
}gptp_capable_receive_state_t;

struct gptp_capable_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	gptp_capable_receive_state_t state;
	gptp_capable_receive_state_t last_state;
	gPtpCapableReceiveSM *thisSM;
	int domainIndex;
	int portIndex;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
// ??? !domainEnabled, Figure 10-20—gPtpCapableReceive state machine
// we set domainEnabled by receiving CMLDS mode PdelayReq
#define DOMAIN_ENABLED (sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq==-1)

static gptp_capable_receive_state_t allstate_condition(gptp_capable_receive_data_t *sm)
{

	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable || !DOMAIN_ENABLED ||
	   !PORT_OPER || !PTP_PORT_ENABLED) return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(gptp_capable_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "gptp_capable_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	return NULL;
}

static gptp_capable_receive_state_t not_enabled_condition(gptp_capable_receive_data_t *sm)
{
	if(PORT_OPER && PTP_PORT_ENABLED && DOMAIN_ENABLED) return INITIALIZE;
	return NOT_ENABLED;
}

static void *initialize_proc(gptp_capable_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "gptp_capable_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	if(!sm->ppg->neighborGptpCapable) return NULL;
	UB_LOG(UBL_INFO, "reset neighborGptpCapable, domainIndex=%d, portIndex=%d\n",
	       sm->domainIndex, sm->portIndex);
	sm->ppg->neighborGptpCapable = false;
	return NULL;
}

static gptp_capable_receive_state_t initialize_condition(gptp_capable_receive_data_t *sm)
{
	if(sm->thisSM->rcvdGptpCapableTlv) return RECEIVED_TLV;
	return INITIALIZE;
}

static void *received_tlv_proc(gptp_capable_receive_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "gptp_capable_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);

	sm->thisSM->rcvdGptpCapableTlv = false;
	sm->thisSM->gPtpCapableReceiptTimeoutInterval.nsec =
		sm->ppg->gPtpCapableReceiptTimeout *
		LOG_TO_NSEC(sm->thisSM->rcvdSignalingMsgPtr->logGptpCapableMessageInterval);
	sm->thisSM->timeoutTime.nsec = cts64 +
		sm->thisSM->gPtpCapableReceiptTimeoutInterval.nsec;
	if(sm->ppg->neighborGptpCapable) return NULL;
	UB_LOG(UBL_INFO, "set neighborGptpCapable, domainIndex=%d, portIndex=%d\n",
	       sm->domainIndex, sm->portIndex);
	sm->ppg->neighborGptpCapable = true;
	return NULL;
}

static gptp_capable_receive_state_t received_tlv_condition(gptp_capable_receive_data_t *sm,
							   uint64_t cts64)
{
	if(cts64 >= sm->thisSM->timeoutTime.nsec) return INITIALIZE;
	if(sm->thisSM->rcvdGptpCapableTlv) sm->last_state=REACTION;
	return RECEIVED_TLV;
}


void *gptp_capable_receive_sm(gptp_capable_receive_data_t *sm, uint64_t cts64)
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
			sm->state = NOT_ENABLED;
			break;
		case NOT_ENABLED:
			if(state_change)
				retp=not_enabled_proc(sm);
			sm->state = not_enabled_condition(sm);
			break;
		case INITIALIZE:
			if(state_change)
				retp=initialize_proc(sm);
			sm->state = initialize_condition(sm);
			break;
		case RECEIVED_TLV:
			if(state_change)
				retp=received_tlv_proc(sm, cts64);
			sm->state = received_tlv_condition(sm, cts64);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void gptp_capable_receive_sm_init(gptp_capable_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(gptp_capable_receive_data_t, gPtpCapableReceiveSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
	gptp_capable_receive_sm(*sm, 0);
}

int gptp_capable_receive_sm_close(gptp_capable_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *gptp_capable_receive_rcvdSignalingMsg(gptp_capable_receive_data_t *sm,
					    PTPMsgGPTPCapableTLV *gGPTPCapableTLV,
					    uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "gptp_capable_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->thisSM->rcvdGptpCapableTlv = true;
	sm->thisSM->rcvdSignalingMsgPtr = gGPTPCapableTLV;
	return gptp_capable_receive_sm(sm, cts64);
}
