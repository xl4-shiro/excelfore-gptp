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
#include "site_sync_sync_sm.h"

typedef enum {
	INIT,
	INITIALIZING,
	RECEIVING_SYNC,
	REACTION,
}site_sync_sync_state_t;

struct site_sync_sync_data{
	PerTimeAwareSystemGlobal *ptasg;
	site_sync_sync_state_t state;
	site_sync_sync_state_t last_state;
	SiteSyncSyncSM *thisSM;
	int domainIndex;
	PortSyncSync portSyncSync;
	uint64_t site_sync_timeout;
	uint64_t site_sync_sendtime;
};

#define RCVD_PSSYNC sm->thisSM->rcvdPSSync
#define RCVD_PSSYNC_PTR sm->thisSM->rcvdPSSyncPtr
#define TX_PSSYNC_PTR_SSS sm->thisSM->txPSSyncPtrSSS
#define SELECTED_STATE sm->ptasg->selectedState
#define GM_PRESENT sm->ptasg->gmPresent
#define PARENT_LOG_SYNC_INTERVAL sm->ptasg->parentLogSyncInterval

static site_sync_sync_state_t allstate_condition(site_sync_sync_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ) {
		sm->last_state=REACTION;
		return INITIALIZING;
	}
	return sm->state;
}

static void *initializing_proc(site_sync_sync_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "site_sync_sync:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	RCVD_PSSYNC = false;
	return NULL;
}

static site_sync_sync_state_t initializing_condition(site_sync_sync_data_t *sm)
{
	if(RCVD_PSSYNC &&
	   ((SELECTED_STATE[RCVD_PSSYNC_PTR->localPortIndex] == SlavePort &&
	     GM_PRESENT) || (gptpconf_get_intitem(CONF_TEST_SYNC_REC_PORT) ==
			     RCVD_PSSYNC_PTR->localPortIndex)))
		return RECEIVING_SYNC;
	return INITIALIZING;
}

static PortSyncSync *setPSSyncSend(site_sync_sync_data_t *sm, PortSyncSync *pssptr)
{
	/* 8021AS-2020 10.2.7.2.1 setPSSyncSend
	 * This function create a PortSyncSync structure (sm->portSyncSync) which
	 * will contain the Sync information to be transmitted.
	 * In our interpretation of the standards, the txPSSyncPtrSSS comes from the
	 * received PortSyncSync structure which in this case the same as
	 * rcvdPSSyncPtr */
	memcpy(&sm->portSyncSync, pssptr, sizeof(PortSyncSync));

	sm->site_sync_timeout=RCVD_PSSYNC_PTR->syncReceiptTimeoutTime.nsec;
	sm->site_sync_sendtime=RCVD_PSSYNC_PTR->syncNextSendTimeoutTime.nsec;
	return &sm->portSyncSync;
}

static void *receiving_sync_proc(site_sync_sync_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "site_sync_sync:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	RCVD_PSSYNC = false;
	TX_PSSYNC_PTR_SSS = setPSSyncSend(sm, RCVD_PSSYNC_PTR);
	PARENT_LOG_SYNC_INTERVAL = RCVD_PSSYNC_PTR->logMessageInterval;
	return &sm->portSyncSync;
}

static site_sync_sync_state_t receiving_sync_condition(site_sync_sync_data_t *sm,
						       uint64_t cts64)
{
	if(RCVD_PSSYNC &&
	   ((SELECTED_STATE[RCVD_PSSYNC_PTR->localPortIndex] == SlavePort &&
	     GM_PRESENT)  || (gptpconf_get_intitem(CONF_TEST_SYNC_REC_PORT) ==
						   RCVD_PSSYNC_PTR->localPortIndex)))
		sm->last_state=REACTION;

	if(RCVD_PSSYNC &&
		(SELECTED_STATE[TX_PSSYNC_PTR_SSS->localPortIndex] == SlavePort && GM_PRESENT) &&
		(sm->site_sync_timeout && !(cts64 > sm->site_sync_timeout)) &&
		sm->site_sync_sendtime && (cts64 > sm->site_sync_sendtime)){
		/* Lost Sync message from GM, force reaction and refer to txPSSyncPtrSSS
		 * as reference for sending the Sync and FollowUp messages. */
		UB_LOG(UBL_DEBUGV,"%s:domainIndex=%d, site_sync_sendtime\n", __func__, sm->domainIndex);
		if((RCVD_PSSYNC_PTR->syncReceiptTimeoutTime.nsec == (uint64_t)-1)||
			(sm->site_sync_sendtime>=RCVD_PSSYNC_PTR->syncReceiptTimeoutTime.nsec)){
			/* In the case that a PSSSync is received but not yet stored via
			 * setPSSyncSend, check if it comes from the PortSyncSyncReceiveSM
			 * via checking the value of syncReceiptTimeoutTime.
			 * In case an outstanding PSSSync is available, use it instead of
			 * the previously stored txPSSyncPtr.
			 */
			RCVD_PSSYNC_PTR = TX_PSSYNC_PTR_SSS;
		}
		sm->last_state=REACTION;
	}
	if(sm->site_sync_timeout && (cts64 > sm->site_sync_timeout)){
		sm->site_sync_timeout=0;
		if(gptpconf_get_intitem(CONF_STATIC_PORT_STATE_SLAVE_PORT)>0){
			/*
			'static port config' and 'this is not GM'
			syncReceiptTimeoutTime is not checked in port_announce_information_state
			gmsync should be reset here for a quick response
			*/
			UB_LOG(UBL_DEBUGV,"%s:domainIndex=%d, site_sync_timeout\n",
			       __func__, sm->domainIndex);
			gptpclock_reset_gmsync(0, sm->domainIndex);
			gptpclock_set_gmstable(sm->domainIndex, false);
		}
	}
	return RECEIVING_SYNC;
}


void *site_sync_sync_sm(site_sync_sync_data_t *sm, uint64_t cts64)
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
		case RECEIVING_SYNC:
			if(state_change)
				retp=receiving_sync_proc(sm);
			sm->state = receiving_sync_condition(sm, cts64);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void site_sync_sync_sm_init(site_sync_sync_data_t **sm,
	int domainIndex,
	PerTimeAwareSystemGlobal *ptasg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, domainIndex);
	INIT_SM_DATA(site_sync_sync_data_t, SiteSyncSyncSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->domainIndex = domainIndex;
	site_sync_sync_sm(*sm, 0);
}

int site_sync_sync_sm_close(site_sync_sync_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, (*sm)->domainIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *site_sync_sync_sm_portSyncSync(site_sync_sync_data_t *sm,
				     PortSyncSync *portSyncSync, uint64_t cts64)
{
	RCVD_PSSYNC = true;
	RCVD_PSSYNC_PTR = portSyncSync;
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, localPortIndex=%d\n", __func__, sm->domainIndex,
	       RCVD_PSSYNC_PTR->localPortIndex);
	return site_sync_sync_sm(sm, cts64);
}
