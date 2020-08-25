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
#include "clock_master_sync_receive_sm.h"

typedef enum {
	INIT,
	INITIALIZING,
	WAITING,
	RECEIVE_SOURCE_TIME,
	REACTION,
}clock_master_sync_receive_state_t;

typedef enum {
	OFFSET_NOT_ADJ=-1,
	OFFSET_START_ADJ,
	OFFSET_UNSTABLE_ADJ,
	OFFSET_STABLE_ADJ,
}offset_state_t;

struct clock_master_sync_receive_data{
	PerTimeAwareSystemGlobal *ptasg;
	clock_master_sync_receive_state_t state;
	clock_master_sync_receive_state_t last_state;
	ClockMasterSyncReceiveSM *thisSM;
	int domainIndex;
	double mrate;
	uint64_t last_lts;
	uint64_t last_mts;
	int gmadjppb;
	double alpha;
	int rate_stable;
	int64_t offsetGM;
	offset_state_t offsetGM_stable;
	int gmchange_ind;
};

#define RCVD_CLOCK_SOURCE_REQ sm->thisSM->rcvdClockSourceReq
#define RCVD_CLOCK_SOURCE_REQ_PTR sm->thisSM->rcvdClockSourceReqPtr
#define	RCVD_LOCAL_CLOCK_TICK sm->thisSM->rcvdLocalClockTick

#define CMSR_UPDATE_DTS (1*UB_SEC_NS) // compute phase and freq, every this time,
//if passing time between GM and thisClock, no way to calculate the freq offset
#define CMSR_TOO_BIG_PASSTIME_GAP (UB_SEC_NS/10)

/* the following FREQ_* and PHASE_* constants may be configurable.
 * But because the current values have been well tuned,
 * and changing is not so easy, we use hardcoded numbers at this time.
 */

// stable if delta of adj rate is less then this
#define FREQ_OFFSET_STABLE_PPB 100
// move to stable condition if the FREQ_OFFSET_STABLE_PPB passed this time consecutively
#define FREQ_OFFSET_STABLE_TRNS 3
// unstable if delta of adj rate is bigger than this
#define FREQ_OFFSET_UNSTABLE_PPB 1000
// update freq offset only when the diff to the new rate is bigger than this
#define FREQ_OFFSET_UPDATE_MRATE_PPB 10

#define PHASE_NEWGM_CRITERION 1000000 // 1msec
#define PHASE_STABLE_CRITERION 10000 // 10usec
#define PHASE_UNSTABLE_CRITERION 30000 // 30usec
// after stable, adjust phase when the detected gap between GM and thisClock exceeds this value
#define PHASE_OFFSET_ADJUST_TARGET 10000 // nsec
#define PHASE_OFFSET_ADJUST_BY_FREQ 100000 // 100usec

static void debug_show_diff_to_GM(clock_master_sync_receive_data_t *sm,
				  int64_t lts, int64_t mts) __attribute__((unused));
static void debug_show_diff_to_GM(clock_master_sync_receive_data_t *sm,
				  int64_t lts, int64_t mts)
{
	gptpclock_tsconv(&lts, sm->ptasg->thisClockIndex, sm->ptasg->domainNumber,
			 0, sm->ptasg->domainNumber);
	lts=mts-lts;
	UB_LOG(UBL_INFO,"domainNumber=%d, %"PRIi64"nsec, offset=%"PRIi64"\n",
	       sm->ptasg->domainNumber, lts, sm->offsetGM);
}

static int set_phase_offsetGM(clock_master_sync_receive_data_t *sm, int64_t dts)
{
	uint64_t dofg;
	int alpha=1;
	int64_t od;
	int64_t offsetGM=0;
	int padj_clockindex, poabf;

	dofg=abs(dts-sm->offsetGM);
	if(dofg>=PHASE_NEWGM_CRITERION){
		if(sm->gmchange_ind &&
		   sm->gmchange_ind==gptpclock_get_gmchange_ind(sm->ptasg->domainNumber)){
			if(sm->offsetGM_stable>=OFFSET_START_ADJ) {
				UB_LOG(UBL_INFO, "%s:domainNumber=%d, big offset Jump=%"PRIu64"\n",
				       __func__, sm->ptasg->domainNumber, dofg);
				sm->offsetGM_stable=OFFSET_NOT_ADJ;
				// don't update 'sm->offsetGM' by the first 'big jump'
				// if the value at the next time is in PHASE_NEWGM_CRITERION,
				// this is treated as 'OFFSET_START_ADJ'
				return 0;
			}
			UB_LOG(UBL_INFO, "%s:domainNumber=%d, big offset Jump=%"PRIu64
			       "second time, update with the big jump\n",
			       __func__, sm->ptasg->domainNumber, dofg);
			// second 'big jump', start over from 'OFFSET_START_ADJ'
		}
		sm->offsetGM_stable=OFFSET_START_ADJ;
	}

	switch(sm->offsetGM_stable){
	case OFFSET_NOT_ADJ:
	case OFFSET_START_ADJ:
		offsetGM = dts;
		sm->offsetGM_stable=OFFSET_UNSTABLE_ADJ;
		UB_LOG(UBL_INFO, "%s:domainNumber=%d, New adjustment(New GM?)\n",
		       __func__, sm->ptasg->domainNumber);
		break;
	case OFFSET_UNSTABLE_ADJ:
		alpha=gptpconf_get_intitem(CONF_PHASE_OFFSET_IIR_ALPHA_START_VALUE);
		offsetGM = dts/alpha + (alpha-1) * (sm->offsetGM / alpha);
		if(dofg<PHASE_STABLE_CRITERION){
			UB_LOG(UBL_INFO, "%s:domainNumber=%d, stable\n",
			       __func__, sm->ptasg->domainNumber);
			sm->offsetGM_stable=OFFSET_STABLE_ADJ;
			sm->gmchange_ind=gptpclock_get_gmchange_ind(sm->ptasg->domainNumber);
			UB_LOG(UBL_DEBUG, "%s:gmchange_ind=%d\n",
			       __func__, sm->gmchange_ind);
		}
		break;
	case OFFSET_STABLE_ADJ:
		if(sm->gmchange_ind!=gptpclock_get_gmchange_ind(sm->ptasg->domainNumber)){
			UB_LOG(UBL_INFO, "%s:domainNumber=%d, GM changed. start over.\n",
			       __func__, sm->ptasg->domainNumber);
			sm->offsetGM_stable=OFFSET_START_ADJ;
			return 0;
		}
		alpha=gptpconf_get_intitem(CONF_PHASE_OFFSET_IIR_ALPHA_STABLE_VALUE);
		offsetGM = dts/alpha + (alpha-1) * (sm->offsetGM / alpha);
		if(dofg>PHASE_UNSTABLE_CRITERION){
			UB_LOG(UBL_INFO, "%s:domainNumber=%d, unstable\n",
			       __func__, sm->ptasg->domainNumber);
			sm->offsetGM_stable=OFFSET_UNSTABLE_ADJ;
		}
		break;
	}

	od=offsetGM-sm->offsetGM;
	if(gptpconf_get_intitem(CONF_USE_HW_PHASE_ADJUSTMENT) && sm->ptasg->domainNumber==0){
		// the range to be adjusted by freq must be wider than normal,
		// then gptpclock_setoffset64 is called less frequently
		poabf=PHASE_OFFSET_ADJUST_BY_FREQ*10;
		padj_clockindex=sm->ptasg->thisClockIndex;
	}else{
		poabf=PHASE_OFFSET_ADJUST_BY_FREQ;
		padj_clockindex=0;
	}
	if(abs(od)<poabf){
		if(abs(od)<PHASE_OFFSET_ADJUST_TARGET) return od/10;
		UB_LOG(UBL_INFO, "%s:domainNumber=%d, offset adjustment by Freq., diff=%"PRIi64
		       "\n", __func__, sm->ptasg->domainNumber, od/10);
		return od/10;
	}
	UB_LOG(UBL_INFO, "%s:domainNumber=%d, offset adjustment, diff=%"PRIi64"\n",
	       __func__, sm->ptasg->domainNumber, od);
	gptpclock_setoffset64(offsetGM, padj_clockindex, sm->ptasg->domainNumber);
	sm->offsetGM=offsetGM;
	return 0;
}

static int computeGmRateRatio(clock_master_sync_receive_data_t *sm,
			      uint64_t lts, uint64_t mts)
{
	uint64_t dlts, dmts;
	int64_t	dts;
	double nrate;
	int ppb;
	int offset_comp;
	dlts = lts - sm->last_lts;
	if(dlts < CMSR_UPDATE_DTS) return -1;
	dts=mts-lts;
	offset_comp=set_phase_offsetGM(sm, dts);
	//debug_show_diff_to_GM(sm, lts, mts);

	dmts = mts - sm->last_mts;
	sm->last_lts = lts;
	sm->last_mts = mts;
	if(abs(dmts-dlts) > CMSR_TOO_BIG_PASSTIME_GAP) return -1;
	// IIR filter, M(n) = a*R(n) + (1-a)*M(n-1), a=CMSR_IIR_COEFF
	nrate = sm->alpha * ((double)dmts/(double)dlts) +
		(1-sm->alpha)*sm->mrate;
	ppb = (int)((nrate-1.0)*1.0E9);
	if(sm->rate_stable < FREQ_OFFSET_STABLE_TRNS && abs(ppb) < FREQ_OFFSET_STABLE_PPB){
		sm->rate_stable++;
		if(sm->rate_stable >= FREQ_OFFSET_STABLE_TRNS){
			sm->alpha = 1.0/gptpconf_get_intitem(
				CONF_FREQ_OFFSET_IIR_ALPHA_STABLE_VALUE);
			UB_LOG(UBL_INFO, "domainNumber=%d, clock_master_sync_receive:stable rate\n",
				sm->ptasg->domainNumber);
		}
	}
	if(abs(ppb) > FREQ_OFFSET_UNSTABLE_PPB) {
		sm->rate_stable=0;
		if(sm->rate_stable >= FREQ_OFFSET_STABLE_TRNS){
			sm->alpha = 1.0/gptpconf_get_intitem(
				CONF_FREQ_OFFSET_IIR_ALPHA_START_VALUE);
			UB_LOG(UBL_INFO,
			       "domainNumber=%d, clock_master_sync_receive:unstable rate\n",
				sm->ptasg->domainNumber);
		}
	}

	UB_LOG(UBL_DEBUGV, "clock_master_sync_receive:%s:domainNumber=%d rate=%dppb\n",
	       __func__, sm->ptasg->domainNumber, ppb);

	ppb+=offset_comp;
	if(abs(ppb) > FREQ_OFFSET_UPDATE_MRATE_PPB){
		int maxadj=gptpconf_get_intitem(CONF_MAX_ADJUST_RATE_ON_CLOCK);
		sm->gmadjppb += ppb;
		if(sm->gmadjppb > maxadj){
			sm->gmadjppb = maxadj;
		}else if(sm->gmadjppb < -maxadj){
			sm->gmadjppb = -maxadj;
		}
		gptpclock_setadj(sm->gmadjppb,
				 sm->ptasg->thisClockIndex, sm->ptasg->domainNumber);
		UB_LOG(UBL_INFO, "domainNumber=%d, clock_master_sync_receive:"
		       "the master clock rate to %dppb\n",
		       sm->ptasg->domainNumber, sm->gmadjppb);
		// the master must be synchronized and the rate becomes 1.0
		sm->mrate = 1.0;
		sm->ptasg->gmRateRatio = 1.0;
	}

	return 0;
}

static clock_master_sync_receive_state_t allstate_condition(clock_master_sync_receive_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->instanceEnable ) {
		return INITIALIZING;
	}
	return sm->state;
}

static void *initializing_proc(clock_master_sync_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_receive:%s:domainIndex=%d\n",
	       __func__, sm->domainIndex);
	sm->ptasg->clockSourceTimeBaseIndicatorOld = 0;
	sm->mrate = 1.0;
	sm->alpha = 1.0/gptpconf_get_intitem(CONF_FREQ_OFFSET_IIR_ALPHA_START_VALUE);
	sm->last_lts = 0;
	sm->last_mts = 0;
	sm->offsetGM = 0;
	RCVD_CLOCK_SOURCE_REQ = false;
	RCVD_LOCAL_CLOCK_TICK = false;
	return NULL;
}

static clock_master_sync_receive_state_t initializing_condition(
	clock_master_sync_receive_data_t *sm)
{
	return WAITING;
}

static void *waiting_proc(clock_master_sync_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_receive:%s:domainIndex=%d\n",
	       __func__, sm->domainIndex);
	return NULL;
}

static clock_master_sync_receive_state_t waiting_condition(clock_master_sync_receive_data_t *sm)
{
	if(RCVD_CLOCK_SOURCE_REQ || RCVD_LOCAL_CLOCK_TICK)
		return RECEIVE_SOURCE_TIME;
	return WAITING;
}

static void *receive_source_time_proc(clock_master_sync_receive_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "clock_master_sync_receive:%s:domainIndex=%d\n",
	       __func__, sm->domainIndex);


	//updateMasterTime(sm);
	//sm->ptasg->localTime = currentTime;
	if (RCVD_CLOCK_SOURCE_REQ) {
		uint64_t lts, mts;
		mts = sm->ptasg->syncReceiptTime.seconds.lsb * UB_SEC_NS +
			sm->ptasg->syncReceiptTime.fractionalNanoseconds.msb;
		lts = sm->ptasg->syncReceiptLocalTime.nsec;
		if(!computeGmRateRatio(sm, lts, mts)){
			sm->ptasg->clockSourceTimeBaseIndicatorOld =
				sm->ptasg->clockSourceTimeBaseIndicator;
			gptpclock_get_clock_params(0, sm->ptasg->domainNumber,
						   &sm->ptasg->clockSourceTimeBaseIndicator,
						   &sm->ptasg->clockSourceLastGmPhaseChange,
						   &sm->ptasg->clockSourceLastGmFreqChange);
			gptpclock_set_gmsync(0, sm->ptasg->domainNumber, false);
		}
	}
	RCVD_CLOCK_SOURCE_REQ = false;
	RCVD_LOCAL_CLOCK_TICK = false;
	return NULL;
}

static clock_master_sync_receive_state_t receive_source_time_condition(
	clock_master_sync_receive_data_t *sm)
{
	return WAITING;
}


void *clock_master_sync_receive_sm(clock_master_sync_receive_data_t *sm, uint64_t cts64)
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
		case WAITING:
			if(state_change)
				retp=waiting_proc(sm);
			sm->state = waiting_condition(sm);
			break;
		case RECEIVE_SOURCE_TIME:
			if(state_change)
				retp=receive_source_time_proc(sm);
			sm->state = receive_source_time_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void clock_master_sync_receive_sm_init(clock_master_sync_receive_data_t **sm,
	int domainIndex,
	PerTimeAwareSystemGlobal *ptasg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, domainIndex);
	INIT_SM_DATA(clock_master_sync_receive_data_t, ClockMasterSyncReceiveSM, sm);
	(*sm)->ptasg = ptasg;
	(*sm)->domainIndex = domainIndex;
	clock_master_sync_receive_sm(*sm, 0);
}

int clock_master_sync_receive_sm_close(clock_master_sync_receive_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, (*sm)->domainIndex);
	CLOSE_SM_DATA(sm);
	return 0;
}

void *clock_master_sync_receive_sm_ClockSourceReq(clock_master_sync_receive_data_t *sm,
						  uint64_t cts64)
{
	RCVD_CLOCK_SOURCE_REQ = true;
	sm->last_state = REACTION;
	return clock_master_sync_receive_sm(sm, cts64);
}
