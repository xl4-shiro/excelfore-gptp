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
#include "clock_master_sync_send_sm.h"

typedef enum {
	INIT,
	INITIALIZING,
	SEND_SYNC_INDICATION,
	REACTION,
}clock_master_sync_send_state_t;

struct clock_master_sync_send_data{
	PerTimeAwareSystemGlobal *ptasg;
	clock_master_sync_send_state_t state;
	clock_master_sync_send_state_t last_state;
	ClockMasterSyncSendSM *thisSM;
	int domainIndex;
	PortSyncSync portSyncSync;
};

#define SYNC_SEND_TIME sm->thisSM->syncSendTime

static void *setPSSyncCMSS(clock_master_sync_send_data_t *sm)
{
	int64_t ts64;
	struct timespec ts;
	sm->portSyncSync.localPortNumber = 0;
	sm->portSyncSync.localPortIndex = 0;
	sm->portSyncSync.local_ppg = NULL;
	sm->portSyncSync.domainNumber = sm->ptasg->domainNumber;

	/* for upstreamTxTime, use sm->ptasg->thisClockIndex,
	   for preciseOriginTimestamp, use masterclock(clockIndex=0),
	   the both time must be exactly the same time, so that 'gptpclock_apply_offset'
	   is needed. */
	ts64=gptpclock_getts64(sm->ptasg->thisClockIndex, sm->ptasg->domainNumber);
	sm->portSyncSync.upstreamTxTime.nsec = ts64;
	gptpclock_apply_offset(&ts64, 0, sm->ptasg->domainNumber);
	UB_NSEC2TS(ts64, ts);
	sm->portSyncSync.preciseOriginTimestamp.seconds.lsb = ts.tv_sec;
	sm->portSyncSync.preciseOriginTimestamp.nanoseconds = ts.tv_nsec;

	sm->portSyncSync.followUpCorrectionField.subns = 0;
	// gmRateRatio * (currentTime – localTime) must be 0

	memcpy(sm->portSyncSync.sourcePortIdentity.clockIdentity, sm->ptasg->thisClock,
	       sizeof(ClockIdentity));
	sm->portSyncSync.sourcePortIdentity.portNumber = 0;
	sm->portSyncSync.logMessageInterval = sm->ptasg->clockMasterLogSyncInterval;
	sm->portSyncSync.syncReceiptTimeoutTime.nsec = 0xffffffffffffffff;
	sm->portSyncSync.rateRatio = sm->ptasg->gmRateRatio;
	sm->portSyncSync.gmTimeBaseIndicator = sm->ptasg->clockSourceTimeBaseIndicator;
	sm->portSyncSync.lastGmPhaseChange = sm->ptasg->clockSourceLastGmPhaseChange;
	sm->portSyncSync.lastGmFreqChange = sm->ptasg->clockSourceLastGmFreqChange;
	return &sm->portSyncSync;
}

static clock_master_sync_send_state_t allstate_condition(clock_master_sync_send_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ) {
		return INITIALIZING;
	}
	return sm->state;
}

static void *initializing_proc(clock_master_sync_send_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_send:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	SYNC_SEND_TIME.nsec = cts64 + sm->ptasg->clockMasterSyncInterval.nsec;
	return NULL;
}

static clock_master_sync_send_state_t initializing_condition(clock_master_sync_send_data_t *sm,
	uint64_t cts64)
{
	if(cts64 >= SYNC_SEND_TIME.nsec) return SEND_SYNC_INDICATION;
	return INITIALIZING;
}

static void *send_sync_indication_proc(clock_master_sync_send_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_send:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	SYNC_SEND_TIME.nsec = cts64 + sm->ptasg->clockMasterSyncInterval.nsec;
	// align time in 25msec
	SYNC_SEND_TIME.nsec = ((SYNC_SEND_TIME.nsec + 12500000)/25000000)*25000000;
	return setPSSyncCMSS(sm);
}

static clock_master_sync_send_state_t send_sync_indication_condition(
	clock_master_sync_send_data_t *sm, uint64_t cts64)
{
	if(cts64 >= SYNC_SEND_TIME.nsec){
		sm->last_state = REACTION;
	}
	return SEND_SYNC_INDICATION;
}


void *clock_master_sync_send_sm(clock_master_sync_send_data_t *sm, uint64_t cts64)
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
				retp=initializing_proc(sm, cts64);
			sm->state = initializing_condition(sm, cts64);
			break;
		case SEND_SYNC_INDICATION:
			if(state_change){
				retp=send_sync_indication_proc(sm, cts64);
				if(retp) break;
			}
			sm->state = send_sync_indication_condition(sm, cts64);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void clock_master_sync_send_sm_init(clock_master_sync_send_data_t **sm,
				    int domainIndex,
				    PerTimeAwareSystemGlobal *ptasg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, domainIndex);
	INIT_SM_DATA(clock_master_sync_send_data_t, ClockMasterSyncSendSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->domainIndex = domainIndex;
}

int clock_master_sync_send_sm_close(clock_master_sync_send_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, (*sm)->domainIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}
