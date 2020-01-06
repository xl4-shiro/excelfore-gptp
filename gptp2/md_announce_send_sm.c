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
#include "md_announce_send_sm.h"
#include "md_abnormal_hooks.h"

typedef enum {
	INIT,
	INITIALIZE,
	SEND_ANNOUNCE,
	REACTION,
}md_announce_send_state_t;

struct md_announce_send_data{
	gptpnet_data_t *gpnetd;
	PerTimeAwareSystemGlobal *ptasg;
	PerPortGlobal *ppg;
	BmcsPerPortGlobal *bppg;
	md_announce_send_state_t state;
	md_announce_send_state_t last_state;
	MDAnnounceSendSM *thisSM;
	int domainIndex;
	int portIndex;
	md_announce_send_stat_data_t statd;
};

#define PORT_OPER sm->ppg->forAllDomain->portOper
#define PTP_PORT_ENABLED sm->ppg->ptpPortEnabled
#define AS_CAPABLE sm->ppg->asCapable
#define TXANN sm->thisSM->txAnnouncePtr
#define RCVDTXANN sm->thisSM->rcvdTxAnnounce

static int sendAnnounce(md_announce_send_data_t *sm)
{
        MDPTPMsgAnnounce *sdata;
        int N;
        int ssize=sizeof(MDPTPMsgAnnounce);

        // truncate uncessary container */
        N = TXANN->tlvLength / sizeof(ClockIdentity);
        ssize=ssize-((MAX_PATH_TRACE_N-N)*sizeof(ClockIdentity));
        sdata=md_header_compose(sm->gpnetd, sm->portIndex, ANNOUNCE, ssize,
                                sm->ptasg->thisClock,
                                sm->ppg->thisPort,
                                TXANN->header.sequenceId,
                                sm->bppg->currentLogAnnounceInterval);
        if(!sdata) return -1;
	sdata->head.domainNumber=sm->ptasg->domainNumber;
        sdata->head.flags[0] = TXANN->header.flags[0];
        sdata->head.flags[1] = TXANN->header.flags[1];
        sdata->currentUtcOffset_ns = htons(TXANN->currentUtcOffset);
        sdata->grandmasterPriority1 = TXANN->grandmasterPriority1;
        sdata->grandmasterClockQuality.clockClass =
		TXANN->grandmasterClockQuality.clockClass;
        sdata->grandmasterClockQuality.clockAccuracy =
		TXANN->grandmasterClockQuality.clockAccuracy;
        sdata->grandmasterClockQuality.offsetScaledLogVariance_ns =
                htons(TXANN->grandmasterClockQuality.offsetScaledLogVariance);
        sdata->grandmasterPriority2 = TXANN->grandmasterPriority2;
        memcpy(&sdata->grandmasterIdentity, &TXANN->grandmasterIdentity,
	       sizeof(ClockIdentity));
        sdata->stepsRemoved_ns = htons(TXANN->stepsRemoved);
        sdata->timeSource = TXANN->timeSource;
        sdata->tlvType_ns = htons(TXANN->tlvType);
        sdata->tlvLength_ns = htons(TXANN->tlvLength);
        if (TXANN->tlvLength > 0){
                memcpy(&sdata->pathSequence, &TXANN->pathSequence, sizeof(ClockIdentity) * N);
        }

        if(gptpnet_send_whook(sm->gpnetd, sm->portIndex-1, ssize)==-1) return -2;
	sm->statd.announce_send++;
        return 0;
}

static md_announce_send_state_t allstate_condition(md_announce_send_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ||
	   !PORT_OPER || !PTP_PORT_ENABLED || !AS_CAPABLE) {
		sm->last_state=REACTION;
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(md_announce_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_announce_send:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	return NULL;
}

static md_announce_send_state_t initialize_condition(md_announce_send_data_t *sm)
{
	if(RCVDTXANN) return SEND_ANNOUNCE;
	return INITIALIZE;
}

static int send_announce_proc(md_announce_send_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "md_announce_send:%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVDTXANN=false;
	return sendAnnounce(sm);
}

static md_announce_send_state_t send_announce_condition(md_announce_send_data_t *sm)
{
	if(RCVDTXANN) sm->last_state=REACTION;
	return SEND_ANNOUNCE;
}


void *md_announce_send_sm(md_announce_send_data_t *sm, uint64_t cts64)
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
		case SEND_ANNOUNCE:
			if(state_change){
				if(send_announce_proc(sm)==-2) {
					sm->last_state=REACTION;
					return NULL;
				}
				break;
			}
			sm->state = send_announce_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void md_announce_send_sm_init(md_announce_send_data_t **sm,
	int domainIndex, int portIndex,
	gptpnet_data_t *gpnetd,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerPortGlobal *bppg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, domainIndex, portIndex);
	INIT_SM_DATA(md_announce_send_data_t, MDAnnounceSendSM, sm);
	(*sm)->gpnetd = gpnetd;
	(*sm)->ptasg = ptasg;
	(*sm)->ppg = ppg;
	(*sm)->bppg = bppg;
	(*sm)->domainIndex = domainIndex;
	(*sm)->portIndex = portIndex;
}

int md_announce_send_sm_close(md_announce_send_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, (*sm)->domainIndex, (*sm)->portIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *md_announce_send_sm_mdAnnouncSend(md_announce_send_data_t *sm,
					PTPMsgAnnounce *msgAnnounce, uint64_t cts64)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d, portIndex=%d\n",
		__func__, sm->domainIndex, sm->portIndex);
	RCVDTXANN=true;
	TXANN=msgAnnounce;
	sm->last_state=REACTION;
	return md_announce_send_sm(sm, cts64);
}

void md_announce_send_stat_reset(md_announce_send_data_t *sm)
{
	memset(&sm->statd, 0, sizeof(md_announce_send_stat_data_t));
}

md_announce_send_stat_data_t *md_announce_send_get_stat(md_announce_send_data_t *sm)
{
	return &sm->statd;
}
