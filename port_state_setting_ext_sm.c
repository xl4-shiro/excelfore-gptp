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
#include "port_state_setting_ext_sm.h"

#define SELECTED_STATE	    sm->ptasg->selectedState
#define PATH_TRACE	    sm->bptasg->pathTrace
#define GM_PRIORITY	    sm->bptasg->gmPriority
#define SYSTEM_PRIORITY	    sm->bptasg->systemPriority

typedef enum {
	INIT,
	INITIALIZE,
	STATE_SETTING,
	REACTION,
}port_state_setting_ext_state_t;

struct port_state_setting_ext_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal **ppgl;
	BmcsPerTimeAwareSystemGlobal *bptasg;
	BmcsPerPortGlobal **bppgl;
	port_state_setting_ext_state_t state;
	port_state_setting_ext_state_t last_state;
	PortStateSettingExtSM *thisSM;
	int domainIndex;
	int portIndex;
	int max_ports;

	UInteger224 messagePriority;
};

void resetStateDisabledTree(port_state_setting_ext_data_t *sm)
{
	int i;
	bool slavePortAvail = false;
	SELECTED_STATE[sm->portIndex] = DisabledPort;
	for(i=0;i<sm->max_ports;i++){
		if (SELECTED_STATE[i]==SlavePort){
			slavePortAvail = true;
			break;
		}
	}
	if(!slavePortAvail){
		memset(PATH_TRACE, 0, sizeof(ClockIdentity) * MAX_PATH_TRACE_N);
		memcpy(&PATH_TRACE[0], sm->ptasg->thisClock, sizeof(ClockIdentity));
	}
}

void updtPortState(port_state_setting_ext_data_t *sm)
{
	int i,slaveIndex=-1;
	bool slavePortAvail = false;
	for(i=1;i<sm->max_ports;i++){
		if (SELECTED_STATE[i]==SlavePort){
			slavePortAvail = true;
			slaveIndex=i;
			break;
		}
	}
	if(slavePortAvail){
		// use announce information
		sm->bptasg->leap61 = sm->bppgl[sm->portIndex]->annLeap61;
		sm->bptasg->leap59 = sm->bppgl[sm->portIndex]->annLeap59;
		sm->bptasg->currentUtcOffsetValid = sm->bppgl[sm->portIndex]->annCurrentUtcOffsetValid;
		sm->bptasg->ptpTimescale = sm->bppgl[sm->portIndex]->annPtpTimescale;
		sm->bptasg->timeTraceable = sm->bppgl[sm->portIndex]->annTimeTraceable;
		sm->bptasg->frequencyTraceable = sm->bppgl[sm->portIndex]->annFrequencyTraceable;
		sm->bptasg->currentUtcOffset = sm->bppgl[sm->portIndex]->annCurrentUtcOffset;
		sm->bptasg->timeSource = sm->bppgl[sm->portIndex]->annTimeSource;

		sm->bptasg->masterStepsRemoved = sm->bppgl[slaveIndex]->portStepsRemoved;

		if(sm->messagePriority.rootSystemIdentity.priority1 < 255){
			sm->ptasg->gmPresent = true;
		}else{
			sm->ptasg->gmPresent = false;
		}
	} else {
		// use system information
		memcpy(&GM_PRIORITY, &SYSTEM_PRIORITY, sizeof(UInteger224));
		sm->bptasg->leap61 = sm->bptasg->sysLeap61;
		sm->bptasg->leap59 = sm->bptasg->sysLeap59;
		sm->bptasg->currentUtcOffsetValid = sm->bptasg->sysCurrentUTCOffsetValid;
		sm->bptasg->ptpTimescale = sm->bptasg->sysPtpTimescale;
		sm->bptasg->timeTraceable = sm->bptasg->sysTimeTraceable;
		sm->bptasg->frequencyTraceable = sm->bptasg->sysFrequencyTraceable;
		sm->bptasg->currentUtcOffset = sm->bptasg->sysCurrentUtcOffset;
		sm->bptasg->timeSource = sm->bptasg->sysTimeSource;

		sm->bptasg->masterStepsRemoved = 0;

		if(sm->bptasg->systemPriority.rootSystemIdentity.priority1 < 255){
			sm->ptasg->gmPresent = true;
		}else{
			sm->ptasg->gmPresent = false;
		}
	}
	// assign port state
	if(sm->thisSM->disabledExt){
		SELECTED_STATE[sm->portIndex] = DisabledPort;
	}else if(sm->ppgl[sm->portIndex]->forAllDomain->asymmetryMeasurementMode){
		SELECTED_STATE[sm->portIndex] = PassivePort;
	}else{
		SELECTED_STATE[sm->portIndex] = sm->thisSM->portStateInd;
	}
	UB_LOG(UBL_DEBUGV, "port_state_setting_ext:%s: portIndex=%d selectedState=%d\n",
	       __func__, sm->portIndex, SELECTED_STATE[sm->portIndex]);

	// assign portState for port 0
	if(SELECTED_STATE[sm->portIndex]==SlavePort){
		SELECTED_STATE[0] = PassivePort;
	}else if(!slavePortAvail){
		SELECTED_STATE[0] = SlavePort;
	}

	// compute gmPriority Vector
	if(SELECTED_STATE[sm->portIndex]==SlavePort){
		memcpy(&GM_PRIORITY, &sm->messagePriority, sizeof(UInteger224));
	}
	if(!slavePortAvail){
		memcpy(&GM_PRIORITY, &SYSTEM_PRIORITY, sizeof(UInteger224));
	}

	// compute masterPriority vector
	// masterPriority Vector = { SS : 0: { CS: PNQ}: PNQ} or
	// masterPriority Vector = { RM : SRM+1: { CS: PNQ}: PNQ} or
	memcpy(&sm->bppgl[sm->portIndex]->masterPriority, &GM_PRIORITY, sizeof(UInteger224));
	memcpy(&sm->bppgl[sm->portIndex]->masterPriority.sourcePortIdentity.clockIdentity,
	       &sm->ptasg->thisClock, sizeof(ClockIdentity));
	sm->bppgl[sm->portIndex]->masterPriority.sourcePortIdentity.portNumber =
		sm->ppgl[sm->portIndex]->thisPort;
	sm->bppgl[sm->portIndex]->masterPriority.portNumber = sm->ppgl[sm->portIndex]->thisPort;

	// set pathTrace
	if(!slavePortAvail){
		memset(PATH_TRACE, 0, sizeof(ClockIdentity) * MAX_PATH_TRACE_N);
		memcpy(&PATH_TRACE[0], sm->ptasg->thisClock, sizeof(ClockIdentity));
	}

}

static port_state_setting_ext_state_t allstate_condition(port_state_setting_ext_data_t *sm)
{
	if((sm->ptasg->BEGIN || !sm->ptasg->instanceEnable) &&
	   (sm->bptasg->externalPortConfiguration == VALUE_ENABLED)) {
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(port_state_setting_ext_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_state_setting_ext:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	resetStateDisabledTree(sm);
	return NULL;
}

static port_state_setting_ext_state_t initialize_condition(port_state_setting_ext_data_t *sm)
{
	if(sm->thisSM->rcvdPortStateInd ||
	   sm->thisSM->forAllDomain->asymmetryMeasurementModeChangeThisPort){
		return STATE_SETTING;
	}
	return INITIALIZE;
}

static void *state_setting_proc(port_state_setting_ext_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_state_setting_ext:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	updtPortState(sm);
	sm->thisSM->forAllDomain->asymmetryMeasurementModeChangeThisPort = false;
	sm->thisSM->rcvdPortStateInd = false;
	return NULL;
}

static port_state_setting_ext_state_t state_setting_condition(port_state_setting_ext_data_t *sm)
{
	if(sm->thisSM->rcvdPortStateInd ||
	   sm->thisSM->disabledExt ||
	   sm->thisSM->forAllDomain->asymmetryMeasurementModeChangeThisPort){
		sm->last_state = REACTION;
		return STATE_SETTING;
	}
	return STATE_SETTING;
}


void *port_state_setting_ext_sm(port_state_setting_ext_data_t *sm, uint64_t cts64)
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
		case STATE_SETTING:
			if(state_change)
				retp=state_setting_proc(sm);
			sm->state = state_setting_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void port_state_setting_ext_sm_init(port_state_setting_ext_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal **ppgl,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal **bppgl,
	int max_ports,
	port_state_setting_ext_data_t **forAllDomainSM)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_state_setting_ext_data_t, PortStateSettingExtSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppgl = ppgl;
	(*sm)->bptasg = bptasg;
	(*sm)->bppgl = bppgl;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
	(*sm)->max_ports = max_ports;
	memset(&(*sm)->messagePriority, 0xFF, sizeof(UInteger224));

	if(domainIndex==0){
		ub_assert((*sm)==(*forAllDomainSM), __func__, NULL);
		//initialize forAllDomain
		(*sm)->thisSM->forAllDomain=malloc(sizeof(PortStateSettingExtSMforAllDomain));
		ub_assert((*sm)->thisSM->forAllDomain, __func__, "malloc");
		memset((*sm)->thisSM->forAllDomain, 0, sizeof(PortStateSettingExtSMforAllDomain));
		(*sm)->thisSM->forAllDomain->asymmetryMeasurementModeChangeThisPort = false;
	}else{
		ub_assert(*forAllDomainSM && ((*forAllDomainSM)->thisSM->forAllDomain),
			  __func__, "forAllDomain");
		(*sm)->thisSM->forAllDomain = (*forAllDomainSM)->thisSM->forAllDomain;
	}
}

int port_state_setting_ext_sm_close(port_state_setting_ext_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	if((*sm)->domainIndex==0) free((*sm)->thisSM->forAllDomain);
	CLOSE_SM_DATA(sm);
	return 0;
}

void port_state_setting_ext_sm_messagePriority(port_state_setting_ext_data_t *sm,
					       UInteger224 *messagePriority)
{	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	memcpy(&sm->messagePriority, messagePriority, sizeof(UInteger224));
}
