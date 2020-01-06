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
#include "md_announce_receive_sm.h"

typedef enum {
	INIT,
	INITIALIZE,
	RECV_ANNOUNCE,
	REACTION,
}md_announce_receive_state_t;

struct md_announce_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	md_announce_receive_state_t state;
	md_announce_receive_state_t last_state;
	MDAnnounceReceiveSM *thisSM;
	int domainIndex;
	int portIndex;
	PTPMsgAnnounce rcvdAnnounce;
	md_announce_receive_stat_data_t statd;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define AS_CAPABLE sm->ppg->asCapable
#define RXANN sm->thisSM->rxAnnouncePtr
#define RCVDRXANN sm->thisSM->rcvdRxAnnounce
#define IEEE_1588_ANNOUNCE_NO_TLV_SIZE 64
static void *recAnnounce(md_announce_receive_data_t *sm)
{
	MDPTPMsgAnnounce *md_announce=(MDPTPMsgAnnounce *)RXANN;
	md_decompose_head((MDPTPMsgHeader *)RXANN, &sm->rcvdAnnounce.header);
	sm->rcvdAnnounce.currentUtcOffset = ntohs(md_announce->currentUtcOffset_ns);
	sm->rcvdAnnounce.grandmasterPriority1 = md_announce->grandmasterPriority1;
	sm->rcvdAnnounce.grandmasterClockQuality.clockClass =
		md_announce->grandmasterClockQuality.clockClass;
	sm->rcvdAnnounce.grandmasterClockQuality.clockAccuracy =
		md_announce->grandmasterClockQuality.clockAccuracy;
	sm->rcvdAnnounce.grandmasterClockQuality.offsetScaledLogVariance =
		ntohs(md_announce->grandmasterClockQuality.offsetScaledLogVariance_ns);
	sm->rcvdAnnounce.grandmasterPriority2 = md_announce->grandmasterPriority2;
	memcpy(&sm->rcvdAnnounce.grandmasterIdentity,
	       &md_announce->grandmasterIdentity, sizeof(ClockIdentity));
	sm->rcvdAnnounce.stepsRemoved = ntohs(md_announce->stepsRemoved_ns);
	sm->rcvdAnnounce.timeSource = md_announce->timeSource;
	if(ntohs(md_announce->head.messageLength_ns) > IEEE_1588_ANNOUNCE_NO_TLV_SIZE){
		sm->rcvdAnnounce.tlvType = ntohs(md_announce->tlvType_ns);
		sm->rcvdAnnounce.tlvLength = ntohs(md_announce->tlvLength_ns);
		// ??? In the case where the master sent inconsistent value
		// where stepsRemoved does not corresponds
		// to the number (N) of elements in the pathSequence, set missing path TLV to all 1
		memset(&sm->rcvdAnnounce.pathSequence, 0xFF,
		       sizeof(ClockIdentity)*MAX_PATH_TRACE_N);
		memcpy(&sm->rcvdAnnounce.pathSequence, &md_announce->pathSequence,
		       sm->rcvdAnnounce.tlvLength);
	}
	return &sm->rcvdAnnounce;
}

static md_announce_receive_state_t allstate_condition(md_announce_receive_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   !PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE) {
		sm->last_state=REACTION;
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(md_announce_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_announce_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	return NULL;
}

static md_announce_receive_state_t initialize_condition(md_announce_receive_data_t *sm)
{
	if(RCVDRXANN) return RECV_ANNOUNCE;
	return INITIALIZE;
}

static void *recv_announce_proc(md_announce_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_announce_receive:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	sm->statd.announce_rec_valid++;
	RCVDRXANN=false;
	return recAnnounce(sm);
}

static md_announce_receive_state_t recv_announce_condition(md_announce_receive_data_t *sm)
{
	if(RCVDRXANN) sm->last_state=REACTION;
	return RECV_ANNOUNCE;
}


void *md_announce_receive_sm(md_announce_receive_data_t *sm, uint64_t cts64)
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
		case RECV_ANNOUNCE:
			if(state_change){
				retp=recv_announce_proc(sm);
				break;
			}
			sm->state = recv_announce_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void md_announce_receive_sm_init(md_announce_receive_data_t **sm,
				 int domainIndex, int portIndex,
				 PerTimeAwareSystemGlobal *ptasg,
				 PerPortGlobal *ppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(md_announce_receive_data_t, MDAnnounceReceiveSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int md_announce_receive_sm_close(md_announce_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *md_announce_receive_sm_mdAnnounceRec(md_announce_receive_data_t *sm,
					   event_data_recv_t *edrecv, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
	       __func__, sm->domainIndex, sm->portIndex);
	RCVDRXANN=true;
	sm->statd.announce_rec++;
	RXANN=edrecv->recbptr;
	sm->last_state=REACTION;
	return md_announce_receive_sm(sm, cts64);
}

void md_announce_receive_stat_reset(md_announce_receive_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_announce_receive_stat_data_t));
}

md_announce_receive_stat_data_t *md_announce_receive_get_stat(md_announce_receive_data_t *sm)
{
	return &sm->statd;
}
