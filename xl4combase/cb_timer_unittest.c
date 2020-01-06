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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <cmocka.h>
#include "combase.h"
#include <xl4unibase/unibase_binding.h>

#define UB_LOGCAT 2

static int check_one_expire(cb_timer_object_t *mtmo, int tms, int64_t ts1)
{
	int res=0;
	int64_t ts2;
	res=select(1, NULL, NULL, NULL, NULL);
	if(!res) return -1;
	if(errno!=EINTR){
		UB_LOG(UBL_ERROR,"%s:wrong interrupt of select\n",__func__);
		return -1;
	}
	ts2=ub_rt_gettime64();
	if(!cb_timer_expired(mtmo, CB_TIMER_ALL_CLEAR)){
		UB_LOG(UBL_INFO,"%s:timer is not expired\n",__func__);
		return 1;
	}
	ts2=ts2-ts1;
	UB_LOG(UBL_INFO,"%dmsec timer, %"PRIi64"nsec\n", tms, ts2);
	if(ts2>(tms+1)*UB_MSEC_NS || ts2<(tms-1)*UB_MSEC_NS){
		UB_LOG(UBL_ERROR,"%s:wrong passed time, %"PRIi64"nsec\n",__func__, ts2);
		return -1;
	}
	return 0;
}

static void unit_test1(void **state)
{
	cb_timer_object_t *mtmo;
	int64_t ts1;
	mtmo=cb_timer_create(NULL);
	ts1=ub_rt_gettime64();
	cb_timer_start_interval(mtmo, 100, 0);
	assert_false(check_one_expire(mtmo, 100, ts1));
}

static void unit_test2(void **state)
{
	cb_timer_object_t *mtmo[2];
	int i;
	int tms;
	int64_t ts1;
	mtmo[0]=cb_timer_create("tm1");
	mtmo[1]=cb_timer_create("tm2");

	ts1=ub_rt_gettime64();
	cb_timer_start_interval(mtmo[0], 10, 0);
	cb_timer_start_interval(mtmo[1], 100, 0);
	for(i=0;i<2;i++){
		tms=i==0?10:100;
		assert_false(check_one_expire(mtmo[i], tms, ts1));
	}
}

static void unit_test3(void **state)
{
	cb_timer_object_t *mtmo[2];
	int i;
	int tms;
	int64_t ts1;
	mtmo[0]=cb_timer_create("tm1");
	mtmo[1]=cb_timer_create("tm2");

	ts1=ub_rt_gettime64();
	cb_timer_start_interval(mtmo[0], 10, 10);
	cb_timer_start_interval(mtmo[1], 100, 100);
	UB_LOG(UBL_INFO, "%s:the first 9 times of 10 msec timer\n", __func__);
	for(i=0;i<9;i++){
		tms=10+i*10;
		assert_false(check_one_expire(mtmo[0], tms, ts1));
	}

	UB_LOG(UBL_INFO, "%s:the 10th time of 10 msec timer\n", __func__);
	assert_false(check_one_expire(mtmo[0], 100, ts1));

	UB_LOG(UBL_INFO, "%s:the first time of 100 msec timer\n", __func__);
	assert_true(cb_timer_expired(mtmo[1],CB_TIMER_ALL_CLEAR));

	for(i=0;i<5;i++){
		tms=110+i*10;
		UB_LOG(UBL_INFO, "%s:5 times of 10 msec timer after 100msec\n", __func__);
		assert_false(check_one_expire(mtmo[0], tms, ts1));
	}
	for(i=0;i<6;i++){
		UB_LOG(UBL_INFO, "%s:the 2nd time of 100 msec timer\n", __func__);
		tms=check_one_expire(mtmo[1], 200, ts1);
		if(tms==0) break;
		if(tms==1) continue;
		assert_true(false);
	}

	for(i=0;i<5;i++){
		UB_LOG(UBL_INFO, "%s:should have expired 5 times\n", __func__);
		assert_true(cb_timer_expired(mtmo[0], CB_TIMER_DECREMENT));
	}
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr="4,ubase:45,combase:45";
	unibase_init(&init_para);
	return 0;
}

static int teardown(void **state)
{
	unibase_close();
	return 0;
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(unit_test1),
		cmocka_unit_test(unit_test2),
		cmocka_unit_test(unit_test3),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
