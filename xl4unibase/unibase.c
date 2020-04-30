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
 * unibase.c
 * unibase initialization
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Shiro Ninomiya (shiro@excelfore.com)
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "unibase_private.h"

/*
 * this exceptionally uses a static global variable.
 * In the initialized process,
 * all the functions are universally available through this static variable.
 * In normal usage, to be safe, 'ubcd.cbset' is initialized only once
 * and never changed in the entire life of the process.
 * The other part of 'ubcd' can be re-configured, in threadding env. gmutex is locked.
 */
unibase_cstd_t ubcd = {false,};

void unibase_init(unibase_init_para_t *ub_init_para)
{
	if(!ub_init_para) {
		printf("need 'ub_init_para'\n");
		abort();
	}
	if(ubcd.locked) return;
	memcpy(&ubcd.cbset, &ub_init_para->cbset, sizeof(unibase_cb_set_t));
	if(ubcd.cbset.mutex_init && ubcd.cbset.mutex_close &&
	   ubcd.cbset.mutex_lock && ubcd.cbset.mutex_unlock) {
		ubcd.threadding=true;
		ubcd.gmutex=ubcd.cbset.mutex_init();
	}
	ub_log_init(ub_init_para->ub_log_initstr);
	ubcd.locked=true;
	if(ubcd.cbset.console_out){
		ubcd.cbset.console_out(true, "unibase-"XL4PKGVERSION"\n");
	}
}

void unibase_close(void)
{
	if(!ubcd.locked) return;
	if(ubcd.threadding)
		ubcd.cbset.mutex_close(ubcd.gmutex);
	memset(&ubcd, 0, sizeof(ubcd));
}

void ub_abort(const char *mes1, const char *mes2)
{
	char astr[UB_CHARS_IN_LINE+1];
	astr[UB_CHARS_IN_LINE]=0;
	if(!mes1) mes1="";
	if(!mes2) mes2="";
	snprintf(astr, UB_CHARS_IN_LINE, "aborting:%s - %s\n", mes1, mes2);
	if(ubcd.cbset.console_out) ubcd.cbset.console_out(true, astr);
	if(ubcd.cbset.debug_out) ubcd.cbset.debug_out(true, astr);
	abort();
}

uint64_t ub_rt_gettime64(void)
{
	return ubcd.cbset.gettime64(UB_CLOCK_REALTIME);
}

uint64_t ub_mt_gettime64(void)
{
	return ubcd.cbset.gettime64(UB_CLOCK_MONOTONIC);
}

uint64_t ub_gptp_gettime64(void)
{
	return ubcd.cbset.gettime64(UB_CLOCK_GPTP);
}
