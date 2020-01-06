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
#include "gptp_capable_transmit_sm.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIALIZE,
	TRANSMIT_TLV,
	REACTION,
}gptp_capable_transmit_state_t;

struct gptp_capable_transmit_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	gptp_capable_transmit_state_t state;
	gptp_capable_transmit_state_t last_state;
	gPtpCapableTransmitSM *thisSM;
	int domainIndex;
	int portIndex;
	PTPMsgGPTPCapableTLV signalingMsg;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
// ??? domainEnabled, Figure 10-19—gPtpCapableTransmit state machine
// we set domainEnabled by receiving CMLDS mode PdelayReq
#define DOMAIN_ENABLED (sm->ppg->forAllDomain->receivedNonCMLDSPdelayReq==-1)

static void *setGptpCapableTlv(gptp_capable_transmit_data_t *sm)
{
	sm->signalingMsg.tlvType=0x03;
	sm->signalingMsg.lengthField=12;
	sm->signalingMsg.organizationId[0]=0x00;
	sm->signalingMsg.organizationId[1]=0x80;
	sm->signalingMsg.organizationId[2]=0xC2;
	sm->signalingMsg.organizationSubType=4;
	sm->signalingMsg.logGptpCapableMessageInterval = sm->ppg->logGptpCapableMessageInterval;
	// Table 10-15 - Definitions of bits of flags field of message interval request TLV
	SET_RESET_FLAG_BIT(sm->ppg->forAllDomain->computeNeighborPropDelay,
			   sm->signalingMsg.flags, COMPUTE_NEIGHBOR_PROP_DELAY_BIT);
	SET_RESET_FLAG_BIT(sm->ppg->forAllDomain->computeNeighborRateRatio,
			   sm->signalingMsg.flags, COMPUTE_NEIGHBOR_RATE_RATIO_BIT);
	// ??? bit2 of flags,  oneStepReceiveCapable
	return &sm->signalingMsg;
}

static gptp_capable_transmit_state_t allstate_condition(gptp_capable_transmit_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable || !DOMAIN_ENABLED ||
	   !PORT_OPER || !PTP_PORT_ENABLED) return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(gptp_capable_transmit_data_t *sm)
{
	if(PORT_OPER) UB_LOG(UBL_DEBUGV, "gptp_capable_transmit:%s:domainIndex=%d, portIndex=%d\n",
			     __func__, sm->domainIndex, sm->portIndex);
	return NULL;
}

static gptp_capable_transmit_state_t not_enabled_condition(gptp_capable_transmit_data_t *sm)
{
	if(PORT_OPER && PTP_PORT_ENABLED && DOMAIN_ENABLED) return INITIALIZE;
	return NOT_ENABLED;
}

static void *initialize_proc(gptp_capable_transmit_data_t *sm, uint64_t cts64)
{
	if(PORT_OPER) UB_LOG(UBL_DEBUGV, "gptp_capable_transmit:%s:domainIndex=%d, portIndex=%d\n",
			     __func__, sm->domainIndex, sm->portIndex);
	// the default of logGptpCapableMessageInterval is TBD
	sm->thisSM->signalingMsgTimeInterval.nsec =
		UB_SEC_NS * (1 << sm->ppg->logGptpCapableMessageInterval);
	sm->thisSM->intervalTimer.nsec = cts64;
	return NULL;
}

static gptp_capable_transmit_state_t initialize_condition(gptp_capable_transmit_data_t *sm)
{
	return TRANSMIT_TLV;
}

static void *transmit_tlv_proc(gptp_capable_transmit_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "gptp_capable_transmit:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->thisSM->intervalTimer.nsec += sm->thisSM->signalingMsgTimeInterval.nsec;
	//txGptpCapableSignalingMsg (&txSignalingMsgPtr);
	//UB_LOG(UBL_DEBUG, "gptp_capable_transmit:txGptpCapableSignalingMsg\n");
	return setGptpCapableTlv(sm);
}

static gptp_capable_transmit_state_t transmit_tlv_condition(gptp_capable_transmit_data_t *sm,
							    uint64_t cts64)
{
	if(cts64 >= sm->thisSM->intervalTimer.nsec) sm->last_state=REACTION;
	return TRANSMIT_TLV;
}


void *gptp_capable_transmit_sm(gptp_capable_transmit_data_t *sm, uint64_t cts64)
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
				retp=initialize_proc(sm, cts64);
			sm->state = initialize_condition(sm);
			break;
		case TRANSMIT_TLV:
			if(state_change){
				retp=transmit_tlv_proc(sm);
				break;
			}
			sm->state = transmit_tlv_condition(sm, cts64);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void gptp_capable_transmit_sm_init(gptp_capable_transmit_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(gptp_capable_transmit_data_t, gPtpCapableTransmitSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int gptp_capable_transmit_sm_close(gptp_capable_transmit_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}
