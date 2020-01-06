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
#include "port_announce_receive_sm.h"

#define SELECTED_STATE	    sm->ptasg->selectedState
#define PATH_TRACE	    sm->bptasg->pathTrace
#define RCVD_ANNOUNCE_PTR   sm->bppg->rcvdAnnouncePtr
#define RCVD_MSG	    sm->bppg->rcvdMsg
#define PORT_OPER	    sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED    sm->ppg->ptpPortEnabled
#define AS_CAPABLE	    sm->ppg->asCapable
#define RCVD_ANNOUNCE	    sm->thisSM->rcvdAnnounce

typedef enum {
	INIT,
	DISCARD,
	RECEIVE,
	REACTION,
}port_announce_receive_state_t;

struct port_announce_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	BmcsPerTimeAwareSystemGlobal *bptasg;
	BmcsPerPortGlobal *bppg;
	port_announce_receive_state_t state;
	port_announce_receive_state_t last_state;
	PortAnnounceReceiveSM *thisSM;
	int domainIndex;
	int portIndex;
	PTPMsgAnnounce rcvdAnnounce;
};

static bool qualifyAnnounce(port_announce_receive_data_t *sm)
{
	uint16_t i, N;

	if(memcmp(RCVD_ANNOUNCE_PTR->header.sourcePortIdentity.clockIdentity,
		   sm->ptasg->thisClock, sizeof(ClockIdentity))==0){
		UB_LOG(UBL_DEBUGV, "port_announce_receive:%s:Announce sent by self\n", __func__);
		return false;
	}
	if(RCVD_ANNOUNCE_PTR->stepsRemoved >= 255){
		UB_LOG(UBL_DEBUGV, "port_announce_receive:%s:stepsRemoved=%d\n", __func__,
		       RCVD_ANNOUNCE_PTR->stepsRemoved);
		return false;
	}
	if (RCVD_ANNOUNCE_PTR->tlvType == 0x8){
		/* 10.6.3.3.4 pathSequence (ClockIdentify[N]
		   N = stepsRemoved+1 (pathSequence increases by 1 for each system traversed.
		   We also limit the pathSequence to MAX_. */
		// ??? Ideally we use stepsRemoved+1 to determine the length of the TLV
		// however, this is not always the case, some MasterPort implementation when
		// grandMaster priority has changed and the candidate GM is in the same pathTrace
		// the clockPriority vectors are not updated (resulting to Repeated or Inferior)
		// but pathTrace will be updated and is not equal to step remove anymore
		N = (RCVD_ANNOUNCE_PTR->tlvLength/sizeof(ClockIdentity)) < MAX_PATH_TRACE_N ?
			(RCVD_ANNOUNCE_PTR->tlvLength/sizeof(ClockIdentity)) : MAX_PATH_TRACE_N;
		for(i=0;i<N;i++){
			if(memcmp(RCVD_ANNOUNCE_PTR->pathSequence[i], sm->ptasg->thisClock,
				  sizeof(ClockIdentity))==0){
				UB_LOG(UBL_DEBUGV, "port_announce_receive:%s:Announce already traversed\n",
				       __func__);
				return false;
			}
		}
		if(SELECTED_STATE[sm->ppg->thisPortIndex] == SlavePort){
			if (N+1 < MAX_PATH_TRACE_N){
				/* path trace TLV copy to pathTrace */
				memcpy(PATH_TRACE, RCVD_ANNOUNCE_PTR->pathSequence,
				       sizeof(ClockIdentity) * N);
				/* append thisClock to pathTrace */
				memcpy(&(PATH_TRACE[N]), sm->ptasg->thisClock, sizeof(ClockIdentity));
				sm->bptasg->pathTraceCount = N+1;
			}else{
				UB_LOG(UBL_WARN, "port_announce_receive:%s:pathTrace=%d exceeds limit=%d\n",
				       __func__, N, MAX_PATH_TRACE_N);
				/* 10.3.8.23 ... a path trace TLV is not appended to an Announce message
				   and the pathTrace array is empty, once appending a clockIdentity
				   to the TLV would cause the frame carrying the Announce to exceed
				   its maximum size. */
				sm->bptasg->pathTraceCount = 0;
			}
		}
	}else {
		/* path trace TLV not present, empty pathTrace */
		memset(PATH_TRACE, 0, sizeof(ClockIdentity) * MAX_PATH_TRACE_N);
	}
	UB_LOG(UBL_DEBUGV, "port_announce_receive:%s:announce qualified\n", __func__);
	return true;
}

static port_announce_receive_state_t allstate_condition(port_announce_receive_data_t *sm)
{
	if((sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	    (RCVD_ANNOUNCE && (!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE))) &&
	    (sm->bptasg->externalPortConfiguration == VALUE_DISABLED)){
			return DISCARD;
	}
	return sm->state;
}

static void *discard_proc(port_announce_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_ANNOUNCE = false;
	RCVD_MSG = false;
	return NULL;
}

static port_announce_receive_state_t discard_condition(port_announce_receive_data_t *sm)
{
	if(RCVD_ANNOUNCE && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE)
		return RECEIVE;
	return DISCARD;
}

static void *receive_proc(port_announce_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_ANNOUNCE = false;
	RCVD_MSG = qualifyAnnounce(sm);
	return NULL;
}

static port_announce_receive_state_t receive_condition(port_announce_receive_data_t *sm)
{
	if(RCVD_ANNOUNCE && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE && !RCVD_MSG)
		return RECEIVE;
	return sm->state;
}

void *port_announce_receive_sm(port_announce_receive_data_t *sm, uint64_t cts64)
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
			sm->state = DISCARD;
			break;
		case DISCARD:
			if(state_change)
				retp=discard_proc(sm);
			sm->state = discard_condition(sm);
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

void port_announce_receive_sm_init(port_announce_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal *bppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_announce_receive_data_t, PortAnnounceReceiveSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->bptasg = bptasg;
	(*sm)->bppg = bppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;

	port_announce_receive_sm(*sm, 0);
}

int port_announce_receive_sm_close(port_announce_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void port_announce_receive_sm_recv_announce(port_announce_receive_data_t *sm,
					    PTPMsgAnnounce *rcvdAnnounce,
					    uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d, state=%d\n",
	       __func__, sm->domainIndex, sm->portIndex, sm->state);
	RCVD_ANNOUNCE = true;
	memcpy(&sm->rcvdAnnounce, rcvdAnnounce, sizeof(PTPMsgAnnounce));
	RCVD_ANNOUNCE_PTR = &sm->rcvdAnnounce;
	sm->last_state = DISCARD;
	port_announce_receive_sm(sm, cts64);
}
