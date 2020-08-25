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
#include "port_announce_information_ext_sm.h"

#define SELECTED_STATE	    sm->ptasg->selectedState
#define PATH_TRACE	    sm->bptasg->pathTrace
#define RCVD_ANNOUNCE_PTR   sm->bppg->rcvdAnnouncePtr
#define UPDT_INFO	    sm->bppg->updtInfo
#define PORT_OPER	    sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED    sm->ppg->ptpPortEnabled
#define AS_CAPABLE	    sm->ppg->asCapable
#define MESSAGE_PRIORITY    sm->thisSM->messagePriority

typedef enum {
	INIT,
	INITIALIZE,
	RECEIVE,
	REACTION,
}port_announce_information_ext_state_t;

struct port_announce_information_ext_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	BmcsPerTimeAwareSystemGlobal *bptasg;
	BmcsPerPortGlobal *bppg;
	port_announce_information_ext_state_t state;
	port_announce_information_ext_state_t last_state;
	PortAnnounceInformationExtSM *thisSM;
	int domainIndex;
	int portIndex;
};

static void* rcvdInfoExt(port_announce_information_ext_data_t *sm)
{
	uint16_t N;
	/* Store the messagePriorityVector and stepRemoved received */
	MESSAGE_PRIORITY.rootSystemIdentity.priority1 = RCVD_ANNOUNCE_PTR->grandmasterPriority1;
	MESSAGE_PRIORITY.rootSystemIdentity.clockClass =
		RCVD_ANNOUNCE_PTR->grandmasterClockQuality.clockClass;
	MESSAGE_PRIORITY.rootSystemIdentity.clockAccuracy = RCVD_ANNOUNCE_PTR->
		grandmasterClockQuality.clockAccuracy;
	MESSAGE_PRIORITY.rootSystemIdentity.offsetScaledLogVariance = RCVD_ANNOUNCE_PTR->
		grandmasterClockQuality.offsetScaledLogVariance;
	MESSAGE_PRIORITY.rootSystemIdentity.priority2 = RCVD_ANNOUNCE_PTR->grandmasterPriority2;
	memcpy(&MESSAGE_PRIORITY.rootSystemIdentity.clockIdentity,
	       RCVD_ANNOUNCE_PTR->grandmasterIdentity, sizeof(ClockIdentity));
	MESSAGE_PRIORITY.stepsRemoved = RCVD_ANNOUNCE_PTR->stepsRemoved;
	memcpy(&MESSAGE_PRIORITY.sourcePortIdentity, &RCVD_ANNOUNCE_PTR->header.sourcePortIdentity,
	       sizeof(PortIdentity));
	MESSAGE_PRIORITY.portNumber = sm->ppg->thisPort;
	sm->bppg->messageStepsRemoved = RCVD_ANNOUNCE_PTR->stepsRemoved;

	if (SELECTED_STATE[sm->ppg->thisPortIndex] == SlavePort){
		N = (RCVD_ANNOUNCE_PTR->stepsRemoved)+1 < MAX_PATH_TRACE_N ?
			(RCVD_ANNOUNCE_PTR->stepsRemoved)+1 : MAX_PATH_TRACE_N;
		if (N < MAX_PATH_TRACE_N) {
			/* path trace TLV copy to pathTrace */
			memcpy(PATH_TRACE, RCVD_ANNOUNCE_PTR->pathSequence,
			       sizeof(ClockIdentity) * N);
			/* append thisClock to pathTrace */
			memcpy(&(PATH_TRACE[N]), sm->ptasg->thisClock, sizeof(ClockIdentity));
		}
		else {
			UB_LOG(UBL_WARN, "port_announce_receive:%s:pathTrace=%d exceeds limit=%d\n", __func__,
			       N, MAX_PATH_TRACE_N);
			/* 10.3.8.23 ... a path trace TLV is not appended to an Announce message
			   and the pathTrace array is empty, once appending a clockIdentity
			   to the TLV would cause the frame carrying the Announce to exceed
			   its maximum size. */
			memset(PATH_TRACE, 0, sizeof(ClockIdentity) * MAX_PATH_TRACE_N);
		}
	}
	// return messagePriority PortStateSettingExtSM reference
	return &MESSAGE_PRIORITY;
}

static void recordOtherAnnounceInfo(port_announce_information_ext_data_t *sm)
{
	sm->bppg->annLeap61 = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x01) == 0x01;
	sm->bppg->annLeap59 = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x02) == 0x02;
	sm->bppg->annCurrentUtcOffsetValid = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x04) == 0x04;
	sm->bppg->annPtpTimescale = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x08) == 0x08;
	sm->bppg->annTimeTraceable = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x10) == 0x10;
	sm->bppg->annFrequencyTraceable = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x20) == 0x20;
	sm->bppg->annCurrentUtcOffset = RCVD_ANNOUNCE_PTR->currentUtcOffset;
	sm->bppg->annTimeSource = RCVD_ANNOUNCE_PTR->timeSource;
}

static port_announce_information_ext_state_t allstate_condition(port_announce_information_ext_data_t *sm)
{
	if (((!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE) ||
	     sm->ptasg->BEGIN || !sm->ptasg->instanceEnable) &&
	    (sm->bptasg->externalPortConfiguration == VALUE_ENABLED)){
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(port_announce_information_ext_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information_ext:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->thisSM->rcvdAnnounce = false;
	UPDT_INFO = false;
	return NULL;
}

static port_announce_information_ext_state_t initialize_condition(port_announce_information_ext_data_t *sm)
{
	if (PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE && sm->thisSM->rcvdAnnounce){
		return RECEIVE;
	}
	return INITIALIZE;
}

static void *receive_proc(port_announce_information_ext_data_t *sm)
{
	void *retv;
	UB_LOG(UBL_DEBUGV, "port_announce_information_ext:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	retv = rcvdInfoExt(sm);
	recordOtherAnnounceInfo(sm);
	sm->bppg->portStepsRemoved = sm->bppg->messageStepsRemoved + 1;
	// messageStepsRemoved is set by rcvInfoExt()
	return retv;
}

static port_announce_information_ext_state_t receive_condition(port_announce_information_ext_data_t *sm)
{
	if (PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE && sm->thisSM->rcvdAnnounce){
		return REACTION;
	}
	return RECEIVE;
}

void *port_announce_information_ext_sm(port_announce_information_ext_data_t *sm, uint64_t cts64)
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
		case RECEIVE:
			if(state_change)
				retp=receive_proc(sm);
			sm->state = receive_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void port_announce_information_ext_sm_init(port_announce_information_ext_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal *bppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_announce_information_ext_data_t, PortAnnounceInformationExtSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->bptasg = bptasg;
	(*sm)->bppg = bppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int port_announce_information_ext_sm_close(port_announce_information_ext_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}
