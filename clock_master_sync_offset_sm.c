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
#include "clock_master_sync_offset_sm.h"

typedef enum {
	INIT,
	INITIALIZING,
	SEND_SYNC_INDICATION,
	REACTION,
}clock_master_sync_offset_state_t;

struct clock_master_sync_offset_data{
	PerTimeAwareSystemGlobal *ptasg;
	clock_master_sync_offset_state_t state;
	clock_master_sync_offset_state_t last_state;
	ClockMasterSyncOffsetSM *thisSM;
	int domainIndex;
};

#define RCVD_SYNC_RECEIPT_TIME sm->thisSM->rcvdSyncReceiptTime
#define SELECTED_STATE sm->ptasg->selectedState
#define SYNC_RECEIPT_TIME sm->ptasg->SyncReceiptTime
#define SOURCE_TIME sourceTime

static clock_master_sync_offset_state_t allstate_condition(clock_master_sync_offset_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ) {
		return INITIALIZING;
	}
	return sm->state;
}

static void *initializing_proc(clock_master_sync_offset_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_offset:%s:domainIndex=%d\n",
	       __func__, sm->domainIndex);
	RCVD_SYNC_RECEIPT_TIME = false;
	return NULL;
}

static clock_master_sync_offset_state_t initializing_condition(clock_master_sync_offset_data_t *sm)
{
	if(RCVD_SYNC_RECEIPT_TIME)
		return SEND_SYNC_INDICATION;
	return INITIALIZING;
}

static void *send_sync_indication_proc(clock_master_sync_offset_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_offset:%s:domainIndex=%d\n",
	       __func__, sm->domainIndex);
	RCVD_SYNC_RECEIPT_TIME = false;
	if(SELECTED_STATE[0] == PassivePort) {
		/* ??? sourcTime
		sm->ptasg->clockSourcePhaseOffset.nsec =
			(SOURCE_TIME.seconds.lsb – SYNC_RECEIPT_TIME.seconds.lsb)* UB_SEC_NS +
			(SOURCE_TIME.fractionalNanoseconds.msb –
			 SYNC_RECEIPT_TIME.fractionalNanoseconds);
		sm->ptasg->clockSourceFreqOffset = computeClockSourceFreqOffset();
		*/
	} else if(sm->ptasg->clockSourceTimeBaseIndicator !=
		  sm->ptasg->clockSourceTimeBaseIndicatorOld) {
		sm->ptasg->clockSourcePhaseOffset = sm->ptasg->clockSourceLastGmPhaseChange;
		sm->ptasg->clockSourceFreqOffset = sm->ptasg->clockSourceLastGmFreqChange;
	}
	return NULL;
}

static clock_master_sync_offset_state_t send_sync_indication_condition(
	clock_master_sync_offset_data_t *sm)
{
	if(RCVD_SYNC_RECEIPT_TIME)
		sm->last_state = REACTION;
	return SEND_SYNC_INDICATION;
}


void *clock_master_sync_offset_sm(clock_master_sync_offset_data_t *sm, uint64_t cts64)
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
			if(state_change){
				retp=initializing_proc(sm);
				break;
			}
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

void clock_master_sync_offset_sm_init(clock_master_sync_offset_data_t **sm,
	int domainIndex,
	PerTimeAwareSystemGlobal *ptasg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, domainIndex);
	INIT_SM_DATA(clock_master_sync_offset_data_t, ClockMasterSyncOffsetSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->domainIndex = domainIndex;
}

int clock_master_sync_offset_sm_close(clock_master_sync_offset_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, (*sm)->domainIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *clock_master_sync_offset_sm_SyncReceiptTime(clock_master_sync_offset_data_t *sm,
						  uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, sm->domainIndex);
	RCVD_SYNC_RECEIPT_TIME = true;
	sm->last_state = REACTION;
	return clock_master_sync_offset_sm(sm, cts64);
}
