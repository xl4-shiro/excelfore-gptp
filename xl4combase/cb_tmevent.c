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
/*
 * cb_tmevent.c
 * timer and event functions
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Shiro Ninomiya (shiro@excelfore.com)
 */

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include "combase_private.h"

struct cb_timer_object{
	timer_t timerid;
	char name[16];
	int timer_expire;
};

static int alarm_signal;

static void catch_alarm(int sig, siginfo_t *si, void *uc)
{
	cb_timer_object_t *mtmo;
	mtmo=(cb_timer_object_t *)si->si_value.sival_ptr;
	if(mtmo->timer_expire>=0) mtmo->timer_expire++;
	UB_LOG(UBL_DEBUG,"timer:%s expired\n",mtmo->name);
}

int cb_timer_init(int signal)
{
	struct sigaction sa;
	alarm_signal=signal?signal:SIGALRM;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = catch_alarm;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, alarm_signal);
	if(sigaction(alarm_signal, &sa, NULL) == -1){
		UB_LOG(UBL_ERROR,"%s:sigaction failed, %s\n",__func__, strerror(errno));
		return -1;
	}
	return 0;
}

cb_timer_object_t *cb_timer_create(char *tname)
{
	cb_timer_object_t *mtmo;
	struct sigevent sev;

	if(!alarm_signal) cb_timer_init(0);
	mtmo=malloc(sizeof(cb_timer_object_t));
	ub_assert(mtmo, __func__, NULL);
	memset(mtmo, 0, sizeof(cb_timer_object_t));
	if(tname) strncpy(mtmo->name, tname, sizeof(mtmo->name));
	sev.sigev_notify=SIGEV_SIGNAL;
	sev.sigev_signo=alarm_signal;
	sev.sigev_value.sival_ptr=mtmo;
	if(timer_create(CLOCK_REALTIME, &sev, &mtmo->timerid)){
		UB_LOG(UBL_ERROR,"%s:%s\n", __func__, strerror(errno));
		free(mtmo);
		return NULL;
	}
	return mtmo;
}

void cb_timer_close(cb_timer_object_t *mtmo)
{
	timer_delete(mtmo->timerid);
	free(mtmo);
	return;
}

int cb_timer_start_interval(cb_timer_object_t *mtmo,
			      uint32_t value_ms, uint32_t interval_ms)
{
	struct itimerspec tmts;

	tmts.it_interval.tv_sec=interval_ms/1000;
	tmts.it_interval.tv_nsec=(interval_ms%1000)*1000000;
	tmts.it_value.tv_sec=value_ms/1000;
	tmts.it_value.tv_nsec=(value_ms%1000)*1000000;
	mtmo->timer_expire=0;
	return timer_settime(mtmo->timerid, 0, &tmts, NULL);
}

int cb_timer_stop(cb_timer_object_t *mtmo)
{
	struct itimerspec tmts;

	memset(&tmts, 0, sizeof(tmts));
	mtmo->timer_expire=-1;
	return timer_settime(mtmo->timerid, 0, &tmts, NULL);
}

bool cb_timer_expired(cb_timer_object_t *mtmo, cb_timer_clear_t clear)
{
	bool res=false;
	res=mtmo->timer_expire>0;
	if(mtmo->timer_expire>0){
		switch(clear){
		case CB_TIMER_NO_CLEAR:
			break;
		case CB_TIMER_DECREMENT:
			mtmo->timer_expire--;
			break;
		case CB_TIMER_ALL_CLEAR:
			mtmo->timer_expire=0;
			break;
		}
	}
	return res;
}
