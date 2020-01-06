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
#include "port_sync_sync_send_sm.h"

typedef enum {
	INIT,
	TRANSMIT_INIT,
	SEND_MD_SYNC,
	SYNC_RECEIPT_TIMEOUT,
	REACTION,
}port_sync_sync_send_state_t;

struct port_sync_sync_send_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	port_sync_sync_send_state_t state;
	port_sync_sync_send_state_t last_state;
	PortSyncSyncSendSM *thisSM;
	int domainIndex;
	int portIndex;
	MDSyncSend mdSyncSend;
};

#define RCVD_PSSYNC sm->thisSM->rcvdPSSync
#define RCVD_PSSYNC_PTR sm->thisSM->rcvdPSSyncPtr
#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define AS_CAPABLE sm->ppg->asCapable
#define SYNC_SLOWDOWN sm->ppg->syncSlowdown
#define NUMBER_SYNC_TRANSMISSIONS sm->thisSM->numberSyncTransmissions
#define THIS_PORT sm->ppg->thisPort
#define SELECTED_STATE sm->ptasg->selectedState
#define INTERVAL1 sm->thisSM->interval1
#define SYNC_LOCKED sm->ppg->syncLocked
// oldSyncInterval is not defined in 802.1AS-rev-d5
//#define OLD_SYNC_INTERVAL oldSyncInterval
#define OLD_SYNC_INTERVAL sm->ppg->syncInterval;
#define LAST_SYNC_SENT_TIME sm->thisSM->lastSyncSentTime

static void setMDSync(port_sync_sync_send_data_t *sm)
{
	sm->mdSyncSend.domainNumber = sm->ptasg->domainNumber;
	sm->mdSyncSend.followUpCorrectionField = sm->thisSM->lastFollowUpCorrectionField;
	sm->mdSyncSend.sourcePortIdentity.portNumber = THIS_PORT ;
	memcpy(sm->mdSyncSend.sourcePortIdentity.clockIdentity, sm->ptasg->thisClock,
	       sizeof(ClockIdentity));
	sm->mdSyncSend.logMessageInterval = sm->ppg->currentLogSyncInterval;
	sm->mdSyncSend.preciseOriginTimestamp = sm->thisSM->lastPreciseOriginTimestamp;
	sm->mdSyncSend.upstreamTxTime = sm->thisSM->lastUpstreamTxTime;
	sm->mdSyncSend.rateRatio = sm->thisSM->lastRateRatio;
	sm->mdSyncSend.gmTimeBaseIndicator = sm->thisSM->lastGmTimeBaseIndicator;
	sm->mdSyncSend.lastGmPhaseChange = sm->thisSM->lastGmPhaseChange;
	sm->mdSyncSend.lastGmFreqChange = sm->thisSM->lastGmFreqChange;
}

static port_sync_sync_send_state_t allstate_condition(port_sync_sync_send_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   (RCVD_PSSYNC &&  (!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE))){
		sm->last_state=REACTION;
		return TRANSMIT_INIT;
	}
	return sm->state;
}

static void *transmit_init_proc(port_sync_sync_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_sync_sync_send:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_PSSYNC = false;
	SYNC_SLOWDOWN = false;
	NUMBER_SYNC_TRANSMISSIONS = 0;
	return NULL;
}

static port_sync_sync_send_state_t transmit_init_condition(port_sync_sync_send_data_t *sm)
{
	if(RCVD_PSSYNC &&
	   (RCVD_PSSYNC_PTR->localPortNumber != THIS_PORT) &&
	   PORT_OPER && PTP_PORT_ENABLED &&
	   AS_CAPABLE &&
	   (SELECTED_STATE[sm->portIndex] == MasterPort ||
	    gptpconf_get_intitem(CONF_TEST_SYNC_SEND_PORT) == sm->portIndex))
		return SEND_MD_SYNC;

	return TRANSMIT_INIT;
}

static void *send_md_sync_proc(port_sync_sync_send_data_t *sm, uint64_t cts64)
{
	void *retv = NULL;
	UB_LOG(UBL_DEBUGV, "port_sync_sync_send:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	if(RCVD_PSSYNC) {
		sm->thisSM->lastRcvdPortNum = RCVD_PSSYNC_PTR->localPortNumber;
		sm->thisSM->lastPreciseOriginTimestamp = RCVD_PSSYNC_PTR->preciseOriginTimestamp;
		sm->thisSM->lastFollowUpCorrectionField = RCVD_PSSYNC_PTR->followUpCorrectionField;
		sm->thisSM->lastRateRatio = RCVD_PSSYNC_PTR->rateRatio;
		sm->thisSM->lastUpstreamTxTime = RCVD_PSSYNC_PTR->upstreamTxTime;
		sm->thisSM->lastGmTimeBaseIndicator = RCVD_PSSYNC_PTR->gmTimeBaseIndicator;
		sm->thisSM->lastGmPhaseChange = RCVD_PSSYNC_PTR->lastGmPhaseChange;
		sm->thisSM->lastGmFreqChange = RCVD_PSSYNC_PTR->lastGmFreqChange;
		sm->thisSM->syncReceiptTimeoutTime = RCVD_PSSYNC_PTR->syncReceiptTimeoutTime;
		SYNC_LOCKED = (sm->ptasg->parentLogSyncInterval == sm->ppg->currentLogSyncInterval);
	}
	RCVD_PSSYNC = false;
	LAST_SYNC_SENT_TIME.nsec = cts64;
	setMDSync(sm);
	//UB_LOG(UBL_DEBUG, "%s:txMDSync\n", __func__);
	retv = &sm->mdSyncSend;
	if (SYNC_SLOWDOWN) {
		if (NUMBER_SYNC_TRANSMISSIONS >= sm->ppg->syncReceiptTimeout) {
			INTERVAL1 = sm->ppg->syncInterval;
			NUMBER_SYNC_TRANSMISSIONS = 0;
			SYNC_SLOWDOWN = false;
		} else {
			INTERVAL1 = OLD_SYNC_INTERVAL;
			NUMBER_SYNC_TRANSMISSIONS++;
		}
	} else {
		NUMBER_SYNC_TRANSMISSIONS = 0;
		INTERVAL1 = sm->ppg->syncInterval;
	}
	return retv;
}

static port_sync_sync_send_state_t send_md_sync_condition(port_sync_sync_send_data_t *sm,
							  uint64_t cts64)
{
	if(cts64 >= sm->thisSM->syncReceiptTimeoutTime.nsec && !SYNC_LOCKED)
		return SYNC_RECEIPT_TIMEOUT;

	if( ( (RCVD_PSSYNC && SYNC_LOCKED &&
	       RCVD_PSSYNC_PTR->localPortNumber != THIS_PORT) ||
	      (!SYNC_LOCKED && (cts64 - LAST_SYNC_SENT_TIME.nsec >= INTERVAL1.nsec) &&
	       (sm->thisSM->lastRcvdPortNum != THIS_PORT )) ) &&
	    PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	    (SELECTED_STATE[sm->portIndex] == MasterPort ||
	     gptpconf_get_intitem(CONF_TEST_SYNC_SEND_PORT) == sm->portIndex))
		sm->last_state = REACTION;

	return SEND_MD_SYNC;
}

static void *sync_receipt_timeout_proc(port_sync_sync_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_sync_sync_send:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_PSSYNC = false;
	return NULL;
}

static port_sync_sync_send_state_t sync_receipt_timeout_condition(port_sync_sync_send_data_t *sm)
{
	if(RCVD_PSSYNC &&
	   (RCVD_PSSYNC_PTR->localPortNumber != THIS_PORT) &&
	   PORT_OPER && PTP_PORT_ENABLED &&
	   AS_CAPABLE &&
	   (SELECTED_STATE[sm->portIndex] == MasterPort ||
	    gptpconf_get_intitem(CONF_TEST_SYNC_SEND_PORT) == sm->portIndex))
		return SEND_MD_SYNC;
	return SYNC_RECEIPT_TIMEOUT;
}


void *port_sync_sync_send_sm(port_sync_sync_send_data_t *sm, uint64_t cts64)
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
		case SEND_MD_SYNC:
			if(state_change){
				retp=send_md_sync_proc(sm, cts64);
				break;
			}
			sm->state = send_md_sync_condition(sm, cts64);
			break;
		case SYNC_RECEIPT_TIMEOUT:
			if(state_change)
				retp=sync_receipt_timeout_proc(sm);
			sm->state = sync_receipt_timeout_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void port_sync_sync_send_sm_init(port_sync_sync_send_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_sync_sync_send_data_t, PortSyncSyncSendSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int port_sync_sync_send_sm_close(port_sync_sync_send_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *port_sync_sync_send_sm_portSyncSync(port_sync_sync_send_data_t *sm,
					  PortSyncSync *portSyncSync, uint64_t cts64)
{
	RCVD_PSSYNC = true;
	RCVD_PSSYNC_PTR = portSyncSync;
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d, localPortNumber=%d\n",
	       __func__, sm->domainIndex, sm->portIndex, RCVD_PSSYNC_PTR->localPortNumber);
	return port_sync_sync_send_sm(sm, cts64);
}
