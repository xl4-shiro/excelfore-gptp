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
#include "port_announce_transmit_sm.h"

#define SELECTED_STATE	    sm->ptasg->selectedState
#define OLD_ANNOUNCE_INTERVAL sm->ppg->oldAnnounceInterval
#define ANNOUNCE_SLOWDOWN   sm->bppg->announceSlowdown
#define NEW_INFO	    sm->bppg->newInfo
#define UPDT_INFO	    sm->bppg->updtInfo
#define MASTER_PRIORITY	    sm->bppg->masterPriority
#define ANNOUNCE_RECEIPT_TIMEOUT sm->bppg->announceReceiptTimeout
#define ANNOUNCE_INTERVAL   sm->bppg->announceInterval
#define SELECTED	    sm->bptasg->selected
#define AS_CAPABLE sm->ppg->asCapable

typedef enum {
	INIT,
	TRANSMIT_INIT,
	TRANSMIT_PERIODIC,
	IDLE,
	TRANSMIT_ANNOUNCE,
	REACTION,
}port_announce_transmit_state_t;

struct port_announce_transmit_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	BmcsPerTimeAwareSystemGlobal *bptasg;
	BmcsPerPortGlobal *bppg;
	port_announce_transmit_state_t state;
	port_announce_transmit_state_t last_state;
	PortAnnounceTransmitSM *thisSM;
	int domainIndex;
	int portIndex;

	PTPMsgAnnounce announceTx;
};

static void txAnnounce(port_announce_transmit_data_t *sm)
{
	int N=0;
	sm->announceTx.header.sequenceId++;
	if(sm->bptasg->leap61) sm->announceTx.header.flags[1] |= 0x01;
	if(sm->bptasg->leap59) sm->announceTx.header.flags[1] |= 0x02;
	if(sm->bptasg->currentUtcOffsetValid) sm->announceTx.header.flags[1] |= 0x04;
	if(sm->bptasg->ptpTimescale) sm->announceTx.header.flags[1] |= 0x08;
	if(sm->bptasg->timeTraceable) sm->announceTx.header.flags[1] |= 0x10;
	if(sm->bptasg->frequencyTraceable) sm->announceTx.header.flags[1] |= 0x20;

	sm->announceTx.currentUtcOffset = sm->bptasg->currentUtcOffset;
	sm->announceTx.grandmasterPriority1 = MASTER_PRIORITY.rootSystemIdentity.priority1;
	sm->announceTx.grandmasterClockQuality.clockClass = MASTER_PRIORITY.rootSystemIdentity.clockClass;
	sm->announceTx.grandmasterClockQuality.clockAccuracy =
		MASTER_PRIORITY.rootSystemIdentity.clockAccuracy;
	sm->announceTx.grandmasterClockQuality.offsetScaledLogVariance =
		MASTER_PRIORITY.rootSystemIdentity.offsetScaledLogVariance;
	sm->announceTx.grandmasterPriority2 = MASTER_PRIORITY.rootSystemIdentity.priority2;
	memcpy(&sm->announceTx.grandmasterIdentity, &MASTER_PRIORITY.rootSystemIdentity.clockIdentity,
	       sizeof(ClockIdentity));
	sm->announceTx.stepsRemoved = sm->bptasg->masterStepsRemoved;
	sm->announceTx.timeSource = sm->bptasg->timeSource;
	sm->announceTx.tlvType = 0x8;
	N = sm->bptasg->pathTraceCount;
	if (N <= MAX_PATH_TRACE_N){
		sm->announceTx.tlvLength = sizeof(ClockIdentity)*N;
		memcpy(&sm->announceTx.pathSequence, &sm->bptasg->pathTrace,
		       sm->announceTx.tlvLength);
	}else{
		UB_LOG(UBL_WARN, "port_announce_transmit:%s:pathTrace=%d exceeds limit=%d\n",
		       __func__, N, MAX_PATH_TRACE_N);
		/* 10.3.8.23 ... a path trace TLV is not appended to an Announce message
		   and the pathTrace array is empty, once appending a clockIdentity
		   to the TLV would cause the frame carrying the Announce to exceed
		   its maximum size. */
		sm->announceTx.tlvLength = 0;
	}
}

static port_announce_transmit_state_t allstate_condition(port_announce_transmit_data_t *sm)
{
	/* 802.1AS-2020 PortAnnounceTransmitSM TRANSMIT_INIT does not
	 * require asCapable condition as per the specification.
	 * However, we opt to add it to minimize procedures done during IDLE when
	 * asCapable is false. */
	if (sm->ptasg->BEGIN || !sm->ptasg->instanceEnable || !AS_CAPABLE){
		return TRANSMIT_INIT;
	}
	return sm->state;
}

static void *transmit_init_proc(port_announce_transmit_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_transmit:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);

	NEW_INFO = true;
	ANNOUNCE_SLOWDOWN = false;
	sm->thisSM->numberAnnounceTransmissions = false;
	return NULL;
}

static port_announce_transmit_state_t transmit_init_condition(port_announce_transmit_data_t *sm)
{
	/* unconditional transfer (UCT) */
	return IDLE;
}

static void *transmit_periodic_proc(port_announce_transmit_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_transmit:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	NEW_INFO = NEW_INFO || (SELECTED_STATE[sm->portIndex] == MasterPort);
	return NULL;
}

static port_announce_transmit_state_t transmit_periodic_condition(port_announce_transmit_data_t *sm)
{
	/* unconditional transfer (UCT) */
	return IDLE;
}

static void *idle_proc(port_announce_transmit_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "port_announce_transmit:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	/* announceSendTime = currentTime + interval2 */
	sm->thisSM->announceSendTime.nsec = cts64 + sm->thisSM->interval2.nsec;
	// align announce time to 25ms
	sm->thisSM->announceSendTime.nsec = (sm->thisSM->announceSendTime.nsec/25000000)*25000000;
	return NULL;
}

static port_announce_transmit_state_t idle_condition(port_announce_transmit_data_t *sm, uint64_t cts64)
{
	if(NEW_INFO && (SELECTED_STATE[sm->portIndex]==MasterPort) &&
	   (cts64 < sm->thisSM->announceSendTime.nsec) &&
	   ((SELECTED[sm->portIndex] && !UPDT_INFO) ||
	    sm->bptasg->externalPortConfiguration == VALUE_ENABLED) &&
	   !sm->ppg->forAllDomain->asymmetryMeasurementMode){
		return TRANSMIT_ANNOUNCE;
	}
	if((cts64 >= sm->thisSM->announceSendTime.nsec) &&
	   ((SELECTED[sm->portIndex] && !UPDT_INFO) ||
	    sm->bptasg->externalPortConfiguration == VALUE_DISABLED)){
		return TRANSMIT_PERIODIC;
	}
	return sm->state;
}

static void *transmit_announce_proc(port_announce_transmit_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_transmit:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	NEW_INFO = false;
	txAnnounce(sm);

	if(ANNOUNCE_SLOWDOWN){
		if (sm->thisSM->numberAnnounceTransmissions >= ANNOUNCE_RECEIPT_TIMEOUT){
			memcpy(&sm->thisSM->interval2, &ANNOUNCE_INTERVAL, sizeof(UScaledNs));
			sm->thisSM->numberAnnounceTransmissions = 0;
			ANNOUNCE_SLOWDOWN = false;
		}else{
			memcpy(&sm->thisSM->interval2, &OLD_ANNOUNCE_INTERVAL, sizeof(UScaledNs));
			sm->thisSM->numberAnnounceTransmissions++;
		}
	}else{
		sm->thisSM->numberAnnounceTransmissions = 0;
		memcpy(&sm->thisSM->interval2, &ANNOUNCE_INTERVAL, sizeof(UScaledNs));
	}
	return &sm->announceTx;
}

static port_announce_transmit_state_t transmit_announce_condition(port_announce_transmit_data_t *sm)
{
	/* unconditional transfer (UCT) */
	return IDLE;
}


void *port_announce_transmit_sm(port_announce_transmit_data_t *sm, uint64_t cts64)
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
			sm->state = TRANSMIT_INIT;
			break;
		case TRANSMIT_INIT:
			if(state_change)
				retp=transmit_init_proc(sm);
			sm->state = transmit_init_condition(sm);
			break;
		case TRANSMIT_PERIODIC:
			if(state_change)
				retp=transmit_periodic_proc(sm);
			sm->state = transmit_periodic_condition(sm);
			break;
		case IDLE:
			if(state_change)
				retp=idle_proc(sm, cts64);
			sm->state = idle_condition(sm, cts64);
			break;
		case TRANSMIT_ANNOUNCE:
			if(state_change)
				retp=transmit_announce_proc(sm);
			sm->state = transmit_announce_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void port_announce_transmit_sm_init(port_announce_transmit_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal *bppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_announce_transmit_data_t, PortAnnounceTransmitSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->bptasg = bptasg;
	(*sm)->bppg = bppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;

	memset(&(*sm)->announceTx, 0, sizeof(PTPMsgAnnounce));
	(*sm)->thisSM->interval2.nsec = 1000000000LL;
}

int port_announce_transmit_sm_close(port_announce_transmit_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}
