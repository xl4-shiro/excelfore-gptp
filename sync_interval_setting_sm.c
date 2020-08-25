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
#include "sync_interval_setting_sm.h"

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIALIZE,
	SET_INTERVAL,
	REACTION,
}sync_interval_setting_state_t;

struct sync_interval_setting_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	sync_interval_setting_state_t state;
	sync_interval_setting_state_t last_state;
	SyncIntervalSettingSM *thisSM;
	int domainIndex;
	int portIndex;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled

static sync_interval_setting_state_t allstate_condition(sync_interval_setting_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable || !PORT_OPER ||
	   !PTP_PORT_ENABLED || sm->ppg->useMgtSettableLogSyncInterval)
		return NOT_ENABLED;
	return sm->state;
}

static void *not_enabled_proc(sync_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "sync_interval_setting:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);

	if (sm->ppg->useMgtSettableLogSyncInterval) {
		sm->ppg->currentLogSyncInterval = sm->ppg->mgtSettableLogSyncInterval;
		sm->ppg->syncInterval.nsec = LOG_TO_NSEC(sm->ppg->currentLogSyncInterval);
	}
	return NULL;
}

static sync_interval_setting_state_t not_enabled_condition(sync_interval_setting_data_t *sm)
{
	if(PORT_OPER && PTP_PORT_ENABLED &&
	   !sm->ppg->useMgtSettableLogSyncInterval) return INITIALIZE;
	return NOT_ENABLED;
}

static void *initialize_proc(sync_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "sync_interval_setting:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);

	sm->ppg->currentLogSyncInterval = sm->ppg->initialLogSyncInterval;
	sm->ppg->syncInterval.nsec = LOG_TO_NSEC(sm->ppg->initialLogSyncInterval);
	sm->thisSM->rcvdSignalingMsg1 = false;
	return NULL;
}

static sync_interval_setting_state_t initialize_condition(sync_interval_setting_data_t *sm)
{
	if(sm->thisSM->rcvdSignalingMsg1) return SET_INTERVAL;
	return INITIALIZE;
}

static void *set_interval_proc(sync_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "sync_interval_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);

	switch (sm->thisSM->rcvdSignalingPtr->timeSyncInterval) {
	case (-128): /* don’t change the interval */
		break;
	case 126: /* set interval to initial value */
		sm->ppg->currentLogSyncInterval = sm->ppg->initialLogSyncInterval;
		sm->ppg->syncInterval.nsec = LOG_TO_NSEC(sm->ppg->initialLogSyncInterval);
		break;
	default: /* use indicated value; note that the value of 127 instructs the sender
		  * to stop sending, in accordance with Table 10-13. */
		sm->ppg->syncInterval.nsec =
			LOG_TO_NSEC(sm->thisSM->rcvdSignalingPtr->timeSyncInterval);
		sm->ppg->currentLogSyncInterval = sm->thisSM->rcvdSignalingPtr->timeSyncInterval;
		break;
	}
	sm->thisSM->rcvdSignalingMsg1 = false;
	return NULL;
}

static sync_interval_setting_state_t set_interval_condition(sync_interval_setting_data_t *sm)
{
	if(sm->thisSM->rcvdSignalingMsg1) sm->last_state=REACTION;
	return SET_INTERVAL;
}


void *sync_interval_setting_sm(sync_interval_setting_data_t *sm, uint64_t cts64)
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
			if(state_change){
				retp=initialize_proc(sm);
				break;
			}
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

void sync_interval_setting_sm_init(sync_interval_setting_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(sync_interval_setting_data_t, SyncIntervalSettingSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
	sync_interval_setting_sm(*sm, 0);
}

int sync_interval_setting_sm_close(sync_interval_setting_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *sync_interval_setting_SignalingMsg1(sync_interval_setting_data_t *sm,
					  PTPMsgIntervalRequestTLV *rcvdSignalingPtr,
					  uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	sm->thisSM->rcvdSignalingMsg1 = true;
	sm->thisSM->rcvdSignalingPtr = rcvdSignalingPtr;
	return sync_interval_setting_sm(sm, cts64);
}
