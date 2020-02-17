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
#include "gm_stable_sm.h"

typedef enum {
	INIT,
	INITIALIZE,
	GM_LOST,
	GM_UNSTABLE,
	GM_STABLE,
	REACTION,
}gm_stable_state_t;

struct gm_stable_data{
	PerTimeAwareSystemGlobal *ptasg;
	gm_stable_state_t state;
	gm_stable_state_t last_state;
	int domainIndex;
	uint64_t gm_stable_time;
	uint64_t gm_stable_timer_time;
	ClockIdentity clockIdentity;
	bool gm_change;
};

static gm_stable_state_t allstate_condition(gm_stable_data_t *sm)
{
	if(sm->ptasg->BEGIN || !sm->ptasg->asCapableOrAll ) {
		return INITIALIZE;
	}
	return sm->state;
}

static void *initialize_proc(gm_stable_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "gm_stable:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	sm->gm_stable_time=0;
	sm->gm_stable_timer_time=gptpconf_get_intitem(CONF_INITIAL_GM_STABLE_TIME)*1000000LL;
	sm->ptasg->gm_stable_initdone=false;
	if(sm->ptasg->selectedState[0]!=SlavePort){
		// if this device is not GM, gmsync must be lost
		gptpclock_reset_gmsync(0, sm->domainIndex);
	}
	gptpclock_set_gmstable(sm->domainIndex, false);
	return NULL;
}

static gm_stable_state_t initialize_condition(gm_stable_data_t *sm)
{
	if(sm->ptasg->asCapableOrAll &&
	   gptpclock_get_gmsync(0, sm->ptasg->domainNumber)) return GM_UNSTABLE;
	return INITIALIZE;
}

static void *gm_lost_proc(gm_stable_data_t *sm)
{
	UB_LOG(UBL_DEBUGV, "gm_stable:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	sm->gm_stable_time=0;
	sm->gm_change=false;
	return NULL;
}

static gm_stable_state_t gm_lost_condition(gm_stable_data_t *sm)
{
	if(gptpclock_get_gmsync(0, sm->ptasg->domainNumber)) return GM_UNSTABLE;
	return GM_LOST;
}

static void *gm_unstable_proc(gm_stable_data_t *sm, uint64_t cts64)
{
	UB_TLOG(UBL_INFO, "gm_stable:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	sm->gm_stable_time=cts64+sm->gm_stable_timer_time;
	gptpclock_set_gmstable(sm->domainIndex, false);
	return NULL;
}

static gm_stable_state_t gm_unstable_condition(gm_stable_data_t *sm, uint64_t cts64)
{
	if(cts64>sm->gm_stable_time) return GM_STABLE;
	if(sm->gm_change) return GM_LOST;
	return GM_UNSTABLE;
}

static void *gm_stable_proc(gm_stable_data_t *sm)
{
	UB_TLOG(UBL_INFO, "gm_stable:%s:domainIndex=%d\n", __func__, sm->domainIndex);
	sm->gm_stable_time=0;
	sm->gm_stable_timer_time=gptpconf_get_intitem(CONF_NORMAL_GM_STABLE_TIME)*1000000LL;
	gptpclock_set_gmstable(sm->domainIndex, true);
	sm->ptasg->gm_stable_initdone=true;
	return NULL;
}

static gm_stable_state_t gm_stable_condition(gm_stable_data_t *sm)
{
	if(sm->gm_change) return GM_LOST;
	return GM_STABLE;
}


void *gm_stable_sm(gm_stable_data_t *sm, uint64_t cts64)
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
		case GM_LOST:
			if(state_change)
				retp=gm_lost_proc(sm);
			sm->state = gm_lost_condition(sm);
			break;
		case GM_UNSTABLE:
			if(state_change)
				retp=gm_unstable_proc(sm, cts64);
			sm->state = gm_unstable_condition(sm, cts64);
			break;
		case GM_STABLE:
			if(state_change)
				retp=gm_stable_proc(sm);
			sm->state = gm_stable_condition(sm);
			break;
		case REACTION:
			break;
		}
		if(retp) return retp;
		if(sm->last_state == sm->state) break;
	}
	return retp;
}

void gm_stable_sm_init(gm_stable_data_t **sm,
		       int domainIndex,
		       PerTimeAwareSystemGlobal *ptasg)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, domainIndex);
	if(!*sm){
		*sm=malloc(sizeof(gm_stable_data_t));
		ub_assert(*sm, __func__, "malloc");
	}
	memset(*sm, 0, sizeof(gm_stable_data_t));
	(*sm)->ptasg = ptasg;
	(*sm)->domainIndex = domainIndex;
}

int gm_stable_sm_close(gm_stable_data_t **sm)
{
	UB_LOG(UBL_DEBUGV, "%s:domainIndex=%d\n", __func__, (*sm)->domainIndex);
	if(!*sm) return 0;
	free(*sm);
	*sm=NULL;
	return 0;
}

void gm_stable_gm_change(gm_stable_data_t *sm, ClockIdentity clockIdentity, uint64_t cts64)
{
	if(!memcmp(sm->clockIdentity, clockIdentity, sizeof(ClockIdentity))) return;
	memcpy(sm->clockIdentity, clockIdentity, sizeof(ClockIdentity));
	sm->gm_change=true;
	gm_stable_sm(sm, cts64);
	return;
}
