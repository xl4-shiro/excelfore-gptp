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
#include "announce_interval_setting_sm.h"

#define PTP_PORT_ENABLED    sm->ppg->ptpPortEnabled
#define PORT_OPER           sm->ppg->forAllDomain->portOper

typedef enum {
	INIT,
	NOT_ENABLED,
	INITIALIZE,
	SET_INTERVALS,
	REACTION,
}announce_interval_setting_state_t;

struct announce_interval_setting_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	BmcsPerPortGlobal *bppg;
	announce_interval_setting_state_t state;
	announce_interval_setting_state_t last_state;
	AnnounceIntervalSettingSM *thisSM;
	int domainIndex;
	int portIndex;
};

static announce_interval_setting_state_t allstate_condition(announce_interval_setting_data_t *sm)
{
        if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable || !PORT_OPER ||
            !PTP_PORT_ENABLED ||
            sm->ppg->forAllDomain->useMgtSettableLogAnnounceInterval){
                   return NOT_ENABLED;
        }
        return sm->state;
}

static void *not_enabled_proc(announce_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "announce_interval_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
        if(sm->ppg->forAllDomain->useMgtSettableLogAnnounceInterval){
                sm->bppg->currentLogAnnounceInterval =
                        sm->ppg->forAllDomain->mgtSettableLogAnnounceInterval;
                sm->bppg->announceInterval.nsec = LOG_TO_NSEC(sm->bppg->currentLogAnnounceInterval);
        }
	return NULL;
}

static announce_interval_setting_state_t not_enabled_condition(announce_interval_setting_data_t *sm)
{
        if(PORT_OPER && PTP_PORT_ENABLED &&
           !sm->ppg->forAllDomain->useMgtSettableLogAnnounceInterval){
                return INITIALIZE;
        }
        return NOT_ENABLED;
}

static void *initialize_proc(announce_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "announce_interval_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
        sm->bppg->currentLogAnnounceInterval = sm->bppg->initialLogAnnounceInterval;
        sm->bppg->announceInterval.nsec = LOG_TO_NSEC(sm->bppg->initialLogAnnounceInterval);
        sm->thisSM->rcvdSignalingMsg2 = false;
        memcpy(&sm->bppg->oldAnnounceInterval, &sm->bppg->announceInterval, sizeof(UScaledNs));
        sm->bppg->announceSlowdown = false;
	UB_LOG(UBL_DEBUGV, "announce_interval_setting:%s:announceInterval=%"PRIu64"\n",
               __func__, sm->bppg->announceInterval.nsec);
	return NULL;
}

static announce_interval_setting_state_t initialize_condition(announce_interval_setting_data_t *sm)
{
        if(sm->thisSM->rcvdSignalingMsg2){
                return SET_INTERVALS;
        }
	return INITIALIZE;
}

static void *set_intervals_proc(announce_interval_setting_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "announce_interval_setting:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
        switch(sm->thisSM->rcvdSignalingPtr->announceInterval){
        case (-128): /* don't change the interval */
                break;
        case 126: /* set interval to initial value */
                sm->bppg->currentLogAnnounceInterval = sm->bppg->initialLogAnnounceInterval;
                sm->bppg->announceInterval.nsec = LOG_TO_NSEC(sm->bppg->initialLogAnnounceInterval);
                break;
        default: /* use indicated value */
                sm->bppg->announceInterval.nsec = LOG_TO_NSEC(sm->thisSM->rcvdSignalingPtr->announceInterval);
                sm->bppg->currentLogAnnounceInterval = sm->thisSM->rcvdSignalingPtr->announceInterval;
                break;
        }
        sm->thisSM->rcvdSignalingMsg2 = false;
        if(sm->bppg->announceInterval.nsec < sm->bppg->oldAnnounceInterval.nsec){
                sm->bppg->announceSlowdown = true;
        }else{
                sm->bppg->announceSlowdown = false;
        }
	return NULL;
}

static announce_interval_setting_state_t set_intervals_condition(announce_interval_setting_data_t *sm)
{
        if(sm->thisSM->rcvdSignalingMsg2){
                return REACTION;
        }
        return SET_INTERVALS;
}


void *announce_interval_setting_sm(announce_interval_setting_data_t *sm, uint64_t cts64)
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
		case SET_INTERVALS:
			if(state_change)
				retp=set_intervals_proc(sm);
			sm->state = set_intervals_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void announce_interval_setting_sm_init(announce_interval_setting_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerPortGlobal *bppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(announce_interval_setting_data_t, AnnounceIntervalSettingSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->bppg = bppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int announce_interval_setting_sm_close(announce_interval_setting_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *announce_interval_setting_sm_SignalingMsg2(announce_interval_setting_data_t *sm,
                                                PTPMsgIntervalRequestTLV *rcvdSignalingPtr,
                                                uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, ":%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
        sm->thisSM->rcvdSignalingPtr = rcvdSignalingPtr;
        sm->thisSM->rcvdSignalingMsg2 = true;
        return announce_interval_setting_sm(sm, cts64);
}
