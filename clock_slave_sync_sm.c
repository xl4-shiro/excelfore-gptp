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
#include "clock_slave_sync_sm.h"

typedef enum {
	INIT,
	INITIALIZING,
	SEND_SYNC_INDICATION,
	REACTION,
}clock_slave_sync_state_t;

struct clock_slave_sync_data{
	PerTimeAwareSystemGlobal *ptasg;
	clock_slave_sync_state_t state;
	clock_slave_sync_state_t last_state;
	ClockSlaveSyncSM *thisSM;
	int domainIndex;
};

#define RCVD_PSSYNC sm->thisSM->rcvdPSSync
#define RCVD_PSSYNC_PTR sm->thisSM->rcvdPSSyncPtr
#define RCVD_LOCAL_CLOCK_TICK sm->thisSM->rcvdLocalClockTick

static void updateSlaveTime(clock_slave_sync_data_t *sm)
{
	int64_t ts64;
	struct timespec ts;
	UB_LOG(UBL_DEBUGV, "clock_slave_sync:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	// ??? regardless of sm->ptasg->gmPresent, do this way
	ts64=gptpclock_getts64(sm->ptasg->thisClockIndex, sm->ptasg->domainNumber);
	UB_NSEC2TS(ts64, ts);
	sm->ptasg->clockSlaveTime.seconds.lsb=ts.tv_sec;
	sm->ptasg->clockSlaveTime.fractionalNanoseconds.msb=ts.tv_nsec;
}

static clock_slave_sync_state_t allstate_condition(clock_slave_sync_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ) {
		return INITIALIZING;
	}
	return sm->state;
}

static void *initializing_proc(clock_slave_sync_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "clock_slave_sync:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	RCVD_PSSYNC = false;
	return NULL;
}

static clock_slave_sync_state_t initializing_condition(clock_slave_sync_data_t *sm)
{
	if(RCVD_PSSYNC || RCVD_LOCAL_CLOCK_TICK) return SEND_SYNC_INDICATION;
	return INITIALIZING;
}

static void *send_sync_indication_proc(clock_slave_sync_data_t *sm)
{
	uint64_t nsec;
	void *smret=NULL;
	UB_LOG(UBL_DEBUGV, "clock_slave_sync:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	// localPortNumber==0 means, sync by ClockMasterSyncSend,
	// and don't need to issue syncReciptTime event
	if (RCVD_PSSYNC && RCVD_PSSYNC_PTR->localPortNumber ) {
		nsec=((RCVD_PSSYNC_PTR->preciseOriginTimestamp.seconds.lsb * UB_SEC_NS) +
		      RCVD_PSSYNC_PTR->preciseOriginTimestamp.nanoseconds) +
			RCVD_PSSYNC_PTR->followUpCorrectionField.nsec;
		if(RCVD_PSSYNC_PTR->local_ppg)
			nsec += RCVD_PSSYNC_PTR->local_ppg->forAllDomain->neighborPropDelay.nsec *
				(RCVD_PSSYNC_PTR->rateRatio /
				 RCVD_PSSYNC_PTR->local_ppg->forAllDomain->neighborRateRatio) +
				RCVD_PSSYNC_PTR->local_ppg->forAllDomain->delayAsymmetry.nsec;

		sm->ptasg->syncReceiptTime.seconds.lsb = nsec / UB_SEC_NS;
		sm->ptasg->syncReceiptTime.fractionalNanoseconds.msb = nsec % UB_SEC_NS;

		sm->ptasg->syncReceiptLocalTime.nsec = RCVD_PSSYNC_PTR->upstreamTxTime.nsec;
		if(RCVD_PSSYNC_PTR->local_ppg)
			sm->ptasg->syncReceiptLocalTime.nsec +=
				RCVD_PSSYNC_PTR->local_ppg->forAllDomain->neighborPropDelay.nsec /
				RCVD_PSSYNC_PTR->local_ppg->forAllDomain->neighborRateRatio +
				RCVD_PSSYNC_PTR->local_ppg->forAllDomain->delayAsymmetry.nsec /
				RCVD_PSSYNC_PTR->rateRatio;

		sm->ptasg->gmTimeBaseIndicator = RCVD_PSSYNC_PTR->gmTimeBaseIndicator;
		sm->ptasg->lastGmPhaseChange = RCVD_PSSYNC_PTR->lastGmPhaseChange;
		sm->ptasg->lastGmFreqChange = RCVD_PSSYNC_PTR->lastGmFreqChange;

		// this may need IPC notice
		//invokeApplicationInterfaceFunction (ClockTargetPhaseDiscontinuity.result);
		UB_LOG(UBL_DEBUGV, "clock_slave_sync: syncReceiptTime\n");
		smret = &sm->ptasg->syncReceiptTime;
	}
	if(RCVD_LOCAL_CLOCK_TICK) updateSlaveTime(sm);
	RCVD_PSSYNC = false;
	RCVD_LOCAL_CLOCK_TICK = false;
	return smret;
}

static clock_slave_sync_state_t send_sync_indication_condition(clock_slave_sync_data_t *sm)
{
	if(RCVD_PSSYNC || RCVD_LOCAL_CLOCK_TICK) {
		sm->last_state = REACTION;
	}
	return SEND_SYNC_INDICATION;
}


void *clock_slave_sync_sm(clock_slave_sync_data_t *sm, uint64_t cts64)
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
			sm->state = INITIALIZING;
			break;
		case INITIALIZING:
			if(state_change)
				retp=initializing_proc(sm);
			sm->state = initializing_condition(sm);
			break;
		case SEND_SYNC_INDICATION:
			if(state_change)
				retp=send_sync_indication_proc(sm);
			sm->state = send_sync_indication_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void clock_slave_sync_sm_init(clock_slave_sync_data_t **sm,
			      int domainIndex,
			      PerTimeAwareSystemGlobal *ptasg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, domainIndex);
	INIT_SM_DATA(clock_slave_sync_data_t, ClockSlaveSyncSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->domainIndex = domainIndex;
}

int clock_slave_sync_sm_close(clock_slave_sync_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, (*sm)->domainIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *clock_slave_sync_sm_loclClockUpdate(clock_slave_sync_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, sm->domainIndex);
	RCVD_LOCAL_CLOCK_TICK = true;
	return clock_slave_sync_sm(sm, cts64);
}

void *clock_slave_sync_sm_portSyncSync(clock_slave_sync_data_t *sm,
				       PortSyncSync *portSyncSync, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, sm->domainIndex);
	RCVD_PSSYNC = true;
	RCVD_PSSYNC_PTR = portSyncSync;
	return clock_slave_sync_sm(sm, cts64);
}
