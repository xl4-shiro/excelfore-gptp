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
#include "port_announce_information_sm.h"

#define SELECTED_STATE	    sm->ptasg->selectedState
#define RESELECT	    sm->bptasg->reselect
#define SELECTED	    sm->bptasg->selected
#define RCVD_ANNOUNCE_PTR   sm->bppg->rcvdAnnouncePtr
#define INFO_IS		    sm->bppg->infoIs
#define RCVD_MSG	    sm->bppg->rcvdMsg
#define PORT_PRIORITY	    sm->bppg->portPriority
#define MASTER_PRIORITY	    sm->bppg->masterPriority
#define UPDT_INFO	    sm->bppg->updtInfo
#define ANN_RECEIPT_TIMEOUT_TIME_INTERVAL sm->bppg->announceReceiptTimeoutTimeInterval
#define PORT_OPER	    sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED    sm->ppg->ptpPortEnabled
#define AS_CAPABLE	    sm->ppg->asCapable
#define MESSAGE_PRIORITY    sm->thisSM->messagePriority
#define ANN_RECEIPT_TIMEOUT_TIME sm->thisSM->announceReceiptTimeoutTime

typedef enum {
	INIT,
	DISABLED,
	AGED,
	UPDATE,
	CURRENT,
	RECEIVE,
	SUPERIOR_MASTER_PORT,
	REPEATED_MASTER_PORT,
	INFERIOR_MASTER_OR_OTHER_PORT,
	REACTION,
}port_announce_information_state_t;

struct port_announce_information_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	BmcsPerTimeAwareSystemGlobal *bptasg;
	BmcsPerPortGlobal *bppg;
	port_announce_information_state_t state;
	port_announce_information_state_t last_state;
	PortAnnounceInformationSM *thisSM;
	int domainIndex;
	int portIndex;

	UScaledNs syncReceiptTimeoutTime;
};

static uint8_t rcvdInfo(port_announce_information_data_t *sm)
{
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

	print_priority_vector( UBL_DEBUGV, "portPriority   ", &PORT_PRIORITY);
	print_priority_vector( UBL_DEBUGV, "messagePriority", &MESSAGE_PRIORITY);
	// ??? receipt of a Announce message conveys always conveys a MasterPort
	if (SELECTED_STATE[sm->ppg->thisPortIndex] != DisabledPort){
		if (memcmp(&PORT_PRIORITY, &MESSAGE_PRIORITY, sizeof(UInteger224))==0){
			UB_LOG(UBL_DEBUGV, "port_announce_information:%s:rcvdInfo=%s\n",
			       __func__, "RepeatedMasterInfo");
			return RepeatedMasterInfo;
		}

		if (SUPERIOR_PRIORITY == compare_priority_vectors(&MESSAGE_PRIORITY,
								  &PORT_PRIORITY)){
			UB_LOG(UBL_DEBUGV, "port_announce_information:%s:rcvdInfo=%s\n",
			       __func__, "SuperiorMasterInfo");
			return SuperiorMasterInfo;
		}
		else
		{
			UB_LOG(UBL_DEBUGV, "port_announce_information:%s:rcvdInfo=%s\n",
			       __func__, "InferiorMasterInfo");
			return InferiorMasterInfo;
		}
	}
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:rcvdInfo=%s\n", __func__, "OtherInfo");
	return OtherInfo;
}

static void recordOtherAnnounceInfo(port_announce_information_data_t *sm)
{
	sm->bppg->annLeap61 = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x01) == 0x01;
	sm->bppg->annLeap59 = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x02) == 0x02;
	sm->bppg->annCurrentUtcOffsetValid = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x04) == 0x04;
	sm->bppg->annPtpTimescale = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x08) == 0x08;
	sm->bppg->annTimeTraceable = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x10) == 0x10;
	sm->bppg->annFrequencyTraceable = (RCVD_ANNOUNCE_PTR->header.flags[1] & 0x20) == 0x20;
	sm->bppg->annCurrentUtcOffset = RCVD_ANNOUNCE_PTR->currentUtcOffset;
	sm->bppg->annTimeSource = RCVD_ANNOUNCE_PTR->timeSource;

	// ??? Global pathTrace is updated only when portState is known
	// to be SlavePort, in the case when system is grandmaster (no SlavePort)
	// and the Announce received may convey transition of portState to SlavePort.
	// A copy of the announce pathSequence should be used for global pathTrace
	// and a copy of the GM clockIdentity should be placed in global gmIdentity.
	sm->bppg->annPathSequenceCount = RCVD_ANNOUNCE_PTR->tlvLength / sizeof(ClockIdentity);
	memcpy(&sm->bppg->annPathSequence, &RCVD_ANNOUNCE_PTR->pathSequence,
	       RCVD_ANNOUNCE_PTR->tlvLength);
	memcpy(sm->ptasg->gmIdentity, RCVD_ANNOUNCE_PTR->grandmasterIdentity,
			sizeof(ClockIdentity));
}

static port_announce_information_state_t allstate_condition(port_announce_information_data_t *sm)
{
	if ((((!PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE) &&
	     (INFO_IS != Disabled)) || sm->ptasg->BEGIN ||
	     !sm->ptasg->instanceEnable) && (sm->bptasg->externalPortConfiguration == VALUE_DISABLED)) {
		return DISABLED;
	}
	return sm->state;
}

static void *disabled_proc(port_announce_information_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVD_MSG = false;
	// announceReceiptTimeoutTime = currentTime
	ANN_RECEIPT_TIMEOUT_TIME.nsec = cts64;
	INFO_IS = Disabled;
	RESELECT[sm->portIndex] = true;
	SELECTED[sm->portIndex] = false;

	return NULL;
}

static port_announce_information_state_t disabled_condition(port_announce_information_data_t *sm)
{
	if (PORT_OPER && PTP_PORT_ENABLED && AS_CAPABLE){
		return AGED;
	}
	/* Order is important since asCapable may not be notified prior to receiving
	   ANNOUNCE message, in such case, this state machine should chech for AGED first.
	 */
	if (RCVD_MSG){
		return DISABLED;
	}
	return sm->state;
}

static void *aged_proc(port_announce_information_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	INFO_IS = Aged;
	RESELECT[sm->portIndex] = true;
	SELECTED[sm->portIndex] = false;

	// Clear global copies of pathTrace and set gmIdentity to own clockIdentity
	sm->bppg->annPathSequenceCount = 0;
	memcpy(sm->ptasg->gmIdentity, sm->ptasg->thisClock, sizeof(ClockIdentity));

	return NULL;
}

static port_announce_information_state_t aged_condition(port_announce_information_data_t *sm)
{
	if (SELECTED[sm->portIndex] && UPDT_INFO){
		return UPDATE;
	}
	return AGED;
}

static void *update_proc(port_announce_information_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	memcpy(&PORT_PRIORITY, &MASTER_PRIORITY, sizeof(UInteger224));
	sm->bppg->portStepsRemoved = sm->bptasg->masterStepsRemoved;
	UPDT_INFO = false;
	INFO_IS = Mine;
	sm->bppg->newInfo = true;
	return NULL;
}

static port_announce_information_state_t update_condition(port_announce_information_data_t *sm)
{
	/* unconditional transfer (UCT) */
	return CURRENT;
}

static void *current_proc(port_announce_information_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	/* Do nothing */
	return NULL;
}

static port_announce_information_state_t current_condition(port_announce_information_data_t *sm,
							   uint64_t cts64)
{
	if (SELECTED[sm->portIndex] && UPDT_INFO){
		return UPDATE;
	}
	if ((INFO_IS == Received) &&
	    ((cts64 >= ANN_RECEIPT_TIMEOUT_TIME.nsec) ||
	     ((cts64 >= sm->syncReceiptTimeoutTime.nsec) &&
	      sm->ptasg->gmPresent)) && !UPDT_INFO && !RCVD_MSG){
		UB_LOG(UBL_INFO, "port_announce_information:%s:domainIndex=%d, portIndex=%d, "
		       "CT=%"PRIu64", ARTT=%"PRIu64", SRTT=%"PRIu64"\n",
		       __func__, sm->domainIndex, sm->portIndex,
		       cts64, ANN_RECEIPT_TIMEOUT_TIME.nsec, sm->syncReceiptTimeoutTime.nsec);
		return AGED;
	}
	if (RCVD_MSG && !UPDT_INFO){
		return RECEIVE;
	}
	return CURRENT;
}

static void *receive_proc(port_announce_information_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	sm->thisSM->rcvdInfo = rcvdInfo(sm);
	return NULL;
}

static port_announce_information_state_t receive_condition(port_announce_information_data_t *sm)
{
	if ((sm->thisSM->rcvdInfo==RepeatedMasterInfo) &&
	    !sm->ppg->forAllDomain->asymmetryMeasurementMode){
		return REPEATED_MASTER_PORT;
	}
	if (((sm->thisSM->rcvdInfo==InferiorMasterInfo) || (sm->thisSM->rcvdInfo==OtherInfo)) &&
	    !sm->ppg->forAllDomain->asymmetryMeasurementMode){
		return INFERIOR_MASTER_OR_OTHER_PORT;
	}
	if ((sm->thisSM->rcvdInfo==SuperiorMasterInfo) &&
	    !sm->ppg->forAllDomain->asymmetryMeasurementMode){
		return SUPERIOR_MASTER_PORT;
	}
	return RECEIVE;
}

static void *superior_master_port_proc(port_announce_information_data_t *sm, uint64_t cts64)
{
	int A,B;

	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	memcpy(&PORT_PRIORITY, &MESSAGE_PRIORITY, sizeof(UInteger224));
	sm->bppg->portStepsRemoved = RCVD_ANNOUNCE_PTR->stepsRemoved;
	recordOtherAnnounceInfo(sm);

	/* A = 16*logMessageInterval, but since we do not use subns, we remove 16 bit shift */
	A = RCVD_ANNOUNCE_PTR->header.logMessageInterval;
	ANN_RECEIPT_TIMEOUT_TIME_INTERVAL.nsec = sm->bppg->announceReceiptTimeout * LOG_TO_NSEC(A);
	ANN_RECEIPT_TIMEOUT_TIME.nsec = cts64 + ANN_RECEIPT_TIMEOUT_TIME_INTERVAL.nsec;

	if (sm->ppg->useMgtSettableLogSyncInterval){
		B = sm->ppg->mgtSettableLogSyncInterval;
	}
	else {
		B = sm->ppg->initialLogSyncInterval;
	}
	sm->ppg->syncReceiptTimeoutTimeInterval.nsec = sm->ppg->syncReceiptTimeout * LOG_TO_NSEC(B);
	sm->syncReceiptTimeoutTime.nsec = cts64 + sm->ppg->syncReceiptTimeoutTimeInterval.nsec;

	INFO_IS = Received;
	RESELECT[sm->portIndex] = true;
	SELECTED[sm->portIndex] = false;
	RCVD_MSG = false;
	RCVD_ANNOUNCE_PTR = NULL;
	return NULL;
}

static port_announce_information_state_t superior_master_port_condition(port_announce_information_data_t *sm)
{
	/* unconditional transfer (UCT) */
	return CURRENT;
}

static void *repeated_master_port_proc(port_announce_information_data_t *sm, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	ANN_RECEIPT_TIMEOUT_TIME.nsec = cts64 + ANN_RECEIPT_TIMEOUT_TIME_INTERVAL.nsec;
	RCVD_MSG = false;
	RCVD_ANNOUNCE_PTR = NULL;
	return NULL;
}

static port_announce_information_state_t repeated_master_port_condition(port_announce_information_data_t *sm)
{
	/* unconditional transfer (UCT) */
	return CURRENT;
}

static void *inferior_master_or_other_port_proc(port_announce_information_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "port_announce_information:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVD_MSG = false;
	RCVD_ANNOUNCE_PTR = NULL;
	return NULL;
}

static port_announce_information_state_t inferior_master_or_other_port_condition(port_announce_information_data_t *sm)
{
	// ?? Check if current portPriority conveys the same sourcePort as
	// the newly received inferior/other master
	if(memcmp(&MESSAGE_PRIORITY.sourcePortIdentity, &PORT_PRIORITY.sourcePortIdentity,
		  sizeof(PortIdentity))==0){
		UB_LOG(UBL_INFO, "%s: current master priority updated to InferiorMasterInfo/OtherInfo\n",
		       __func__);
		// When BMCS quick update is enabled, set the current portPriority as AGED.
		// This should restart the BMCS to try to find better master port.
		// Otherwise, follow standard with UTC to CURRENT.
		if(sm->bptasg->bmcsQuickUpdate){
			return AGED;
		}
	}
	/* unconditional transfer (UCT) */
	return CURRENT;
}

void *port_announce_information_sm(port_announce_information_data_t *sm, uint64_t cts64)
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
			sm->state = DISABLED;
			break;
		case DISABLED:
			if(state_change)
				retp=disabled_proc(sm, cts64);
			sm->state = disabled_condition(sm);
			break;
		case AGED:
			if(state_change)
				retp=aged_proc(sm);
			sm->state = aged_condition(sm);
			break;
		case UPDATE:
			if(state_change)
				retp=update_proc(sm);
			sm->state = update_condition(sm);
			break;
		case CURRENT:
			if(state_change)
				retp=current_proc(sm);
			sm->state = current_condition(sm, cts64);
			break;
		case RECEIVE:
			if(state_change)
				retp=receive_proc(sm);
			sm->state = receive_condition(sm);
			break;
		case SUPERIOR_MASTER_PORT:
			if(state_change)
				retp=superior_master_port_proc(sm, cts64);
			sm->state = superior_master_port_condition(sm);
			break;
		case REPEATED_MASTER_PORT:
			if(state_change)
				retp=repeated_master_port_proc(sm, cts64);
			sm->state = repeated_master_port_condition(sm);
			break;
		case INFERIOR_MASTER_OR_OTHER_PORT:
			if(state_change)
				retp=inferior_master_or_other_port_proc(sm);
			sm->state = inferior_master_or_other_port_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void port_announce_information_sm_init(port_announce_information_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal *bppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(port_announce_information_data_t, PortAnnounceInformationSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->bptasg = bptasg;
	(*sm)->bppg = bppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;

	(*sm)->syncReceiptTimeoutTime.nsec = 0xffffffffffffffff;
}

int port_announce_information_sm_close(port_announce_information_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void port_announce_information_sm_SyncReceiptTimeoutTime(port_announce_information_data_t *sm,
							  PortSyncSync *portSyncSync)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	memcpy(&sm->syncReceiptTimeoutTime, &portSyncSync->syncReceiptTimeoutTime, sizeof(UScaledNs));
	UB_LOG(UBL_DEBUGV, "%s:syncReceiptTimeoutTime=%"PRIu64"\n", __func__, sm->syncReceiptTimeoutTime.nsec);
}
