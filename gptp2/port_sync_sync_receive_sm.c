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
#include "port_sync_sync_receive_sm.h"

#define RCVD_MDSYNC sm->thisSM->rcvdMDSync
#define RCVD_MDSYNC_PTR sm->thisSM->rcvdMDSyncPtr
#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define AS_CAPABLE sm->ppg->asCapable
#define ASYMMETRY_MEASUREMENT_MODE sm->ppg->forAllDomain->asymmetryMeasurementMode

typedef enum {
	INIT,
	DISCARD,
	RECEIVED_SYNC,
	REACTION,
}port_sync_sync_receive_state_t;

struct port_sync_sync_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	port_sync_sync_receive_state_t state;
	port_sync_sync_receive_state_t last_state;
	PortSyncSyncReceiveSM *thisSM;
	int domainIndex;
	int portIndex;
	PortSyncSync portSyncSync;
};

static void setPSSyncPSSR(port_sync_sync_receive_data_t *sm, double rateRatio, uint64_t cts64)
{
	sm->portSyncSync.localPortNumber = sm->ppg->thisPort;
	sm->portSyncSync.localPortIndex = sm->ppg->thisPortIndex;
	sm->portSyncSync.local_ppg = sm->ppg;
	sm->portSyncSync.domainNumber = RCVD_MDSYNC_PTR->domainNumber;
	sm->portSyncSync.followUpCorrectionField = RCVD_MDSYNC_PTR->followUpCorrectionField;
	sm->portSyncSync.sourcePortIdentity = RCVD_MDSYNC_PTR->sourcePortIdentity;
	sm->portSyncSync.logMessageInterval = RCVD_MDSYNC_PTR->logMessageInterval;
	sm->portSyncSync.preciseOriginTimestamp = RCVD_MDSYNC_PTR->preciseOriginTimestamp;
	sm->portSyncSync.upstreamTxTime = RCVD_MDSYNC_PTR->upstreamTxTime;
	sm->portSyncSync.syncReceiptTimeoutTime.nsec =
		cts64 + sm->ppg->syncReceiptTimeoutTimeInterval.nsec;
	sm->portSyncSync.rateRatio = rateRatio;
	sm->portSyncSync.gmTimeBaseIndicator = RCVD_MDSYNC_PTR->gmTimeBaseIndicator;
	sm->portSyncSync.lastGmPhaseChange = RCVD_MDSYNC_PTR->lastGmPhaseChange;
	sm->portSyncSync.lastGmFreqChange = RCVD_MDSYNC_PTR->lastGmFreqChange;
}

static void *received_sync(port_sync_sync_receive_data_t *sm, uint64_t cts64)
{
	double rateRatio;
	int a;
	RCVD_MDSYNC = false;
	rateRatio = RCVD_MDSYNC_PTR->rateRatio;
	rateRatio += (sm->ppg->forAllDomain->neighborRateRatio - 1.0);
	a = RCVD_MDSYNC_PTR->logMessageInterval;
	sm->ppg->syncReceiptTimeoutTimeInterval.nsec =
		sm->ppg->syncReceiptTimeout * LOG_TO_NSEC(a);

	setPSSyncPSSR(sm, rateRatio, cts64);
	sm->ptasg->parentLogSyncInterval = RCVD_MDSYNC_PTR->logMessageInterval;
	return &sm->portSyncSync;
}

static port_sync_sync_receive_state_t allstate_condition(port_sync_sync_receive_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   (RCVD_MDSYNC && (!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE))){
		sm->last_state=REACTION;
		return DISCARD;
	}
	return sm->state;
}

static void *discard_proc(port_sync_sync_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_sync_sync_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_MDSYNC = false;
	return NULL;
}

static port_sync_sync_receive_state_t discard_condition(port_sync_sync_receive_data_t *sm)
{
	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE)
		return RECEIVED_SYNC;
	return DISCARD;
}

static void *received_sync_proc(port_sync_sync_receive_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "port_sync_sync_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	//UB_LOG(UBL_DEBUG,"%s:txPSSyncPSSR\n", __func__);
	return received_sync(sm, cts64);
}

static port_sync_sync_receive_state_t received_sync_condition(port_sync_sync_receive_data_t *sm)
{
	if(RCVD_MDSYNC && PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE &&
	   !ASYMMETRY_MEASUREMENT_MODE) return REACTION;
	return RECEIVED_SYNC;
}


void *port_sync_sync_receive_sm(port_sync_sync_receive_data_t *sm, uint64_t cts64)
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
			if(state_change){
				retp=discard_proc(sm);
				sm->state = RECEIVED_SYNC;
			}else{
				sm->state = discard_condition(sm);
			}
			break;
		case RECEIVED_SYNC:
			if(state_change)
				retp=received_sync_proc(sm, cts64);
			sm->state = received_sync_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void port_sync_sync_receive_sm_init(port_sync_sync_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_sync_sync_receive_data_t, PortSyncSyncReceiveSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int port_sync_sync_receive_sm_close(port_sync_sync_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *port_sync_sync_receive_sm_recvMDSync(port_sync_sync_receive_data_t *sm,
					   MDSyncReceive *txMDSyncReceivePtr, uint64_t cts64)
{
	RCVD_MDSYNC = true;
	RCVD_MDSYNC_PTR = txMDSyncReceivePtr;
	sm->last_state = REACTION;
	return port_sync_sync_receive_sm(sm, cts64);
}
