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
#include "link_delay_interval_setting_sm.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIALIZE,
	SET_INTERVAL,
	REACTION,
}link_delay_interval_setting_state_t;

struct link_delay_interval_setting_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	MDEntityGlobal *mdeg;
	link_delay_interval_setting_state_t state;
	link_delay_interval_setting_state_t last_state;
	LinkDelayIntervalSettingSM *thisSM;
	int portIndex;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define USE_MGTSETTABLE_LOG_PDELAY_REQ_INTERVAL \
	sm->ppg->forAllDomain->useMgtSettableLogPdelayReqInterval
#define CURRENT_LOG_PDELAY_REQ_INTERVAL sm->mdeg->forAllDomain->currentLogPdelayReqInterval
#define INITIAL_LOG_PDELAY_REQ_INTERVAL sm->mdeg->forAllDomain->initialLogPdelayReqInterval
#define RCVD_SIGNALING_PTR sm->thisSM->rcvdSignalingPtr
#define RCVD_SIGNALING_MSG1 sm->thisSM->rcvdSignalingMsg1

static link_delay_interval_setting_state_t allstate_condition(
	link_delay_interval_setting_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->thisSM->portEnabled3 ||
	   !PORT_OPER ||  USE_MGTSETTABLE_LOG_PDELAY_REQ_INTERVAL)
		return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(link_delay_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "link_delay_interval_setting:%s:portIndex=%d\n",
	       __func__, sm->portIndex);

	if(USE_MGTSETTABLE_LOG_PDELAY_REQ_INTERVAL) {
		CURRENT_LOG_PDELAY_REQ_INTERVAL =
			sm->ppg->forAllDomain->mgtSettableLogPdelayReqInterval;
		sm->mdeg->forAllDomain->pdelayReqInterval.nsec =
			LOG_TO_NSEC(CURRENT_LOG_PDELAY_REQ_INTERVAL);
	}
	return NULL;
}

static link_delay_interval_setting_state_t not_enabled_condition(
	link_delay_interval_setting_data_t *sm)
{
	if(PORT_OPER && sm->thisSM->portEnabled3 && !USE_MGTSETTABLE_LOG_PDELAY_REQ_INTERVAL)
		return INITIALIZE;
	return NOT_ENABLED;
}

static void *initialize_proc(link_delay_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "link_delay_interval_setting:%s:portIndex=%d\n",
	       __func__, sm->portIndex);

	CURRENT_LOG_PDELAY_REQ_INTERVAL = INITIAL_LOG_PDELAY_REQ_INTERVAL;
	sm->mdeg->forAllDomain->pdelayReqInterval.nsec =
		LOG_TO_NSEC(INITIAL_LOG_PDELAY_REQ_INTERVAL);
	sm->ppg->forAllDomain->computeNeighborRateRatio = true;
	sm->ppg->forAllDomain->computeNeighborPropDelay = true;
	RCVD_SIGNALING_MSG1 = false;
	return NULL;
}

static link_delay_interval_setting_state_t initialize_condition(
	link_delay_interval_setting_data_t *sm)
{
	if(RCVD_SIGNALING_MSG1) return SET_INTERVAL;
	return INITIALIZE;
}

static void *set_interval_proc(link_delay_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "link_delay_interval_setting:%s:portIndex=%d\n",
	       __func__, sm->portIndex);

	switch (RCVD_SIGNALING_PTR->linkDelayInterval) {
	case (-128): /* don't change the interval */
		break;
	case 126: /* set interval to initial value */
		CURRENT_LOG_PDELAY_REQ_INTERVAL = INITIAL_LOG_PDELAY_REQ_INTERVAL;
		sm->mdeg->forAllDomain->pdelayReqInterval.nsec =
			LOG_TO_NSEC(INITIAL_LOG_PDELAY_REQ_INTERVAL);
		break;
	default: /* use indicated value; note that the value of 127 instructs the Pdelay
		  * requester to stop sending, in accordance with Table 10-12. */
		sm->mdeg->forAllDomain->pdelayReqInterval.nsec =
			LOG_TO_NSEC(RCVD_SIGNALING_PTR->linkDelayInterval);
		CURRENT_LOG_PDELAY_REQ_INTERVAL = RCVD_SIGNALING_PTR ->linkDelayInterval;
		break;
	}
	sm->ppg->forAllDomain->computeNeighborRateRatio=
		GET_FLAG_BIT(RCVD_SIGNALING_PTR->flags,
			     COMPUTE_NEIGHBOR_RATE_RATIO_BIT)?true:false;
	sm->ppg->forAllDomain->computeNeighborPropDelay =
		GET_FLAG_BIT(RCVD_SIGNALING_PTR->flags,
			     COMPUTE_NEIGHBOR_PROP_DELAY_BIT)?true:false;
	RCVD_SIGNALING_MSG1 = false;
	return NULL;
}

static link_delay_interval_setting_state_t set_interval_condition(
	link_delay_interval_setting_data_t *sm)
{
	if(RCVD_SIGNALING_MSG1) sm->last_state=REACTION;
	return SET_INTERVAL;
}


void *link_delay_interval_setting_sm(link_delay_interval_setting_data_t *sm, uint64_t cts64)
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
		case SET_INTERVAL:
			if(state_change)
				retp=set_interval_proc(sm);
			sm->state = set_interval_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void link_delay_interval_setting_sm_init(link_delay_interval_setting_data_t **sm,
	int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	MDEntityGlobal *mdeg)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, portIndex);
	INIT_SM_DATA(link_delay_interval_setting_data_t, LinkDelayIntervalSettingSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->mdeg = mdeg;
	(*sm)->portIndex = portIndex;
}

int link_delay_interval_setting_sm_close(link_delay_interval_setting_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *link_delay_interval_setting_SignalingMsg1(
	link_delay_interval_setting_data_t *sm,
	PTPMsgIntervalRequestTLV *rcvdSignalingPtr, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:portIndex=%d\n", __func__, sm->portIndex);
	RCVD_SIGNALING_MSG1=true;
	RCVD_SIGNALING_PTR=rcvdSignalingPtr;
	return link_delay_interval_setting_sm(sm, cts64);
}
