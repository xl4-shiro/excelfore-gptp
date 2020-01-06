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
#include "one_step_tx_oper_setting_sm.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIALIZE,
	SET_ONE_STEP_TX_OPER,
	REACTION,
}one_step_tx_oper_setting_state_t;

struct one_step_tx_oper_setting_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	MDEntityGlobal *mdeg;
	one_step_tx_oper_setting_state_t state;
	one_step_tx_oper_setting_state_t last_state;
	OneStepTxOperSettingSM *thisSM;
	int domainIndex;
	int portIndex;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define USE_MGT_SETTABLE_ONE_STEP_TX_OPER sm->ppg->useMgtSettableOneStepTxOper
#define MGT_SETTABLE_ONE_STEP_TX_OPER sm->ppg->mgtSettableOneStepTxOper
#define CURRENT_ONE_STEP_TX_OPER sm->ppg->currentOneStepTxOper
#define INITIAL_ONE_STEP_TX_OPER sm->ppg->initialOneStepTxOper
#define ONE_STEP_TX_OPER sm->mdeg->oneStepTxOper
#define ONE_STEP_TRANSMIT sm->mdeg->oneStepTransmit

static one_step_tx_oper_setting_state_t allstate_condition(one_step_tx_oper_setting_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   !PORT_OPER || !PTP_PORT_ENABLED || USE_MGT_SETTABLE_ONE_STEP_TX_OPER )
		return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(one_step_tx_oper_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "one_step_tx_oper_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);

	if(USE_MGT_SETTABLE_ONE_STEP_TX_OPER){
		CURRENT_ONE_STEP_TX_OPER = MGT_SETTABLE_ONE_STEP_TX_OPER;
		ONE_STEP_TX_OPER = CURRENT_ONE_STEP_TX_OPER && ONE_STEP_TRANSMIT;
	}

	return NULL;
}

static one_step_tx_oper_setting_state_t not_enabled_condition(one_step_tx_oper_setting_data_t *sm)
{
	if(PORT_OPER && PTP_PORT_ENABLED && !USE_MGT_SETTABLE_ONE_STEP_TX_OPER)
		return INITIALIZE;
	return NOT_ENABLED;
}

static void *initialize_proc(one_step_tx_oper_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "one_step_tx_oper_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	CURRENT_ONE_STEP_TX_OPER = INITIAL_ONE_STEP_TX_OPER;
	ONE_STEP_TX_OPER = CURRENT_ONE_STEP_TX_OPER && ONE_STEP_TRANSMIT;
	return NULL;
}

static one_step_tx_oper_setting_state_t initialize_condition(one_step_tx_oper_setting_data_t *sm)
{
	if(sm->thisSM->rcvdSignalingMsg4) return SET_ONE_STEP_TX_OPER;
	return INITIALIZE;
}

static void *set_one_step_tx_oper_proc(one_step_tx_oper_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "one_step_tx_oper_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);

	CURRENT_ONE_STEP_TX_OPER =
		GET_FLAG_BIT(sm->thisSM->rcvdSignalingPtr->flags,
			     ONE_STEP_RECEIVE_CAPABLE_BIT)?true:false;
	ONE_STEP_TX_OPER = CURRENT_ONE_STEP_TX_OPER && ONE_STEP_TRANSMIT;
	sm->thisSM->rcvdSignalingMsg4 = false;
	return NULL;
}

static one_step_tx_oper_setting_state_t set_one_step_tx_oper_condition(
	one_step_tx_oper_setting_data_t *sm)
{
	if(sm->thisSM->rcvdSignalingMsg4) sm->last_state=REACTION;
	return SET_ONE_STEP_TX_OPER;
}


void *one_step_tx_oper_setting_sm(one_step_tx_oper_setting_data_t *sm, uint64_t cts64)
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
		case SET_ONE_STEP_TX_OPER:
			if(state_change)
				retp=set_one_step_tx_oper_proc(sm);
			sm->state = set_one_step_tx_oper_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void one_step_tx_oper_setting_sm_init(one_step_tx_oper_setting_data_t **sm,
				      int domainIndex, int portIndex,
				      PerTimeAwareSystemGlobal *ptasg,
				      PerPortGlobal *ppg,
				      MDEntityGlobal *mdeg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(one_step_tx_oper_setting_data_t, OneStepTxOperSettingSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->mdeg = mdeg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int one_step_tx_oper_setting_sm_close(one_step_tx_oper_setting_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *one_step_tx_oper_setting_SignalingMsg4(one_step_tx_oper_setting_data_t *sm,
					     PTPMsgIntervalRequestTLV *rcvdSignalingPtr,
					     uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->thisSM->rcvdSignalingMsg4 = true;
	sm->thisSM->rcvdSignalingPtr = rcvdSignalingPtr;
	return one_step_tx_oper_setting_sm(sm, cts64);
}
