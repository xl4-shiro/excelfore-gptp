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
#include "unibase.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#define RTS_SEC 10
#define RTS_MSEC 10
#define MTS_SEC 20
#define MTS_MSEC 20
#define GTS_SEC 30
#define GTS_MSEC 30

#define LINE_BUF_LEN 255
static char console_data[LINE_BUF_LEN+1];
static int cd_point;
static char debug_data[LINE_BUF_LEN+1];
static int dd_point;
enum {
	TEST_MOD0=0,
	TEST_MOD1,
	TEST_MOD2,
	TEST_MOD3,
	TEST_MOD4,
};

static int string_buf_out(char *sbuf, int *dp, const char *str)
{
	int res;
	res=strlen(str);
	if(cd_point+res > LINE_BUF_LEN) {
		*dp=0;
		return 0;
	}
	strcpy(sbuf+*dp, str);
	if(str[res-1]=='\n'){
		*dp=0;
	}else{
		*dp+=res;
	}
	return res;
}

int test_console_out(bool flush, const char *str)
{
	return string_buf_out(console_data, &cd_point, str);
}
int test_debug_out(bool flush, const char *str)
{
	return string_buf_out(debug_data, &dd_point, str);
}

uint64_t test_gettime64(ub_clocktype_t ctype)
{
	struct timespec ts={0,0};
	switch(ctype){
	case UB_CLOCK_REALTIME:
		//clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec=RTS_SEC;
		ts.tv_nsec=RTS_MSEC*1000000;
		break;
	case UB_CLOCK_MONOTONIC:
		//clock_gettime(CLOCK_MONOTONIC, &ts);
		ts.tv_sec=MTS_SEC;
		ts.tv_nsec=MTS_MSEC*1000000;
		break;
	case UB_CLOCK_GPTP:
		ts.tv_sec=GTS_SEC;
		ts.tv_nsec=GTS_MSEC*1000000;
		break;
	default:
		break;
	}
	return ts.tv_sec*UB_SEC_NS+ts.tv_nsec;
}

static const char *format1="%s:%s:%06u-%06u:int=%d, hex=0x%x, float=%f\n";
static const char *format2="%s:%s:int=%d, hex=0x%x, float=%f\n";
static const char *level_mark[]=DBGMSG_LEVEL_MARK;
static void test_ub_log_print(void **state)
{
	int i;
	float a;
	char pstr[256];
	int res;
	i=20;
	a=3.1415;

	// mod0 print REALTIME
	sprintf(pstr, format1,level_mark[1],"mod0",
		RTS_SEC, RTS_MSEC*1000, i, i, a);
	ub_log_print(TEST_MOD0, 0, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod1 print MONOTONICTIME
	sprintf(pstr, format1,level_mark[1],"mod1",
		MTS_SEC, MTS_MSEC*1000, i, i, a);
	ub_log_print(TEST_MOD1, 0, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod2 print GPTPTIME
	sprintf(pstr, format1,level_mark[1],"mod2",
		GTS_SEC, GTS_MSEC*1000, i, i, a);
	ub_log_print(TEST_MOD2, 0, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod0 print console level=3, debug level=4
	sprintf(pstr, format1,level_mark[4],"mod0",
		RTS_SEC, RTS_MSEC*1000, i, i, a);
	console_data[0]=0;
	debug_data[0]=0;
	ub_log_print(TEST_MOD0, 0, 4, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal("", console_data);
	assert_string_equal(pstr, debug_data);
	console_data[0]=0;
	debug_data[0]=0;
	ub_log_print(TEST_MOD0, 0, 5, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal("", console_data);
	assert_string_equal("", debug_data);
	sprintf(pstr, format1,level_mark[3],"mod0",
		RTS_SEC, RTS_MSEC*1000, i, i, a);
	console_data[0]=0;
	debug_data[0]=0;
	ub_log_print(TEST_MOD0, 0, 3, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod3, no time stamp
	sprintf(pstr, format2,level_mark[1],"mod3",i,i,a);
	console_data[0]=0;
	debug_data[0]=0;
	ub_log_print(TEST_MOD3, 0, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod3, force to print time stamp
	sprintf(pstr, format1,level_mark[1],"mod3",
		GTS_SEC, GTS_MSEC*1000, i, i, a);
	console_data[0]=0;
	debug_data[0]=0;
	ub_log_print(TEST_MOD3, UB_CLOCK_GPTP, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod4 is not registered, print with default config
	sprintf(pstr, format1,level_mark[1],"def04",
		GTS_SEC, GTS_MSEC*1000, i, i, a);
	console_data[0]=0;
	debug_data[0]=0;
	res=ub_log_print(TEST_MOD4, UB_CLOCK_GPTP, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// add mod4 config
	assert_int_equal(0, ub_log_add_category("mod4:33m"));
	sprintf(pstr, format1,level_mark[1],"mod4",
		MTS_SEC, MTS_MSEC*1000, i, i, a);
	res=ub_log_print(TEST_MOD4, 0, 1, "int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_int_equal(0, res);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

	// mod3:23
	assert_false(ub_clog_on(TEST_MOD3, 0));
	assert_true(ub_clog_on(TEST_MOD3, 1));
	assert_true(ub_clog_on(TEST_MOD3, 2));
	assert_false(ub_clog_on(TEST_MOD3, 3));
	assert_false(ub_clog_on(TEST_MOD3, 4));

	assert_false(ub_dlog_on(TEST_MOD3, 0));
	assert_true(ub_dlog_on(TEST_MOD3, 1));
	assert_true(ub_dlog_on(TEST_MOD3, 2));
	assert_true(ub_dlog_on(TEST_MOD3, 3));
	assert_false(ub_dlog_on(TEST_MOD3, 4));

	// change to mod3:34
	ub_log_change(TEST_MOD3, 3, 4);
	assert_true(ub_clog_on(TEST_MOD3, 3));
	assert_false(ub_clog_on(TEST_MOD3, 4));
	assert_true(ub_dlog_on(TEST_MOD3, 4));
	assert_false(ub_dlog_on(TEST_MOD3, 5));

	// return to mod3:23
	ub_log_return(TEST_MOD3);
	assert_true(ub_clog_on(TEST_MOD3, 2));
	assert_false(ub_clog_on(TEST_MOD3, 3));
	assert_true(ub_dlog_on(TEST_MOD3, 3));
	assert_false(ub_dlog_on(TEST_MOD3, 4));

}

#define UB_LOGCAT TEST_MOD0
#define UB_LOGTSTYPE UB_CLOCK_MONOTONIC
static void test_ub_log_print_macro(void **state)
{
	int i;
	float a;
	char pstr[256];
	i=20;
	a=3.1415;

	// mod0 print no TS
	console_data[0]=0;
	debug_data[0]=0;
	sprintf(pstr, format2,level_mark[UBL_INFO],"mod0", i, i, a);
	UB_LOG(UBL_INFO,"int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal("", console_data);
	assert_string_equal(pstr, debug_data);

	// mod0 print add TS
	console_data[0]=0;
	debug_data[0]=0;
	sprintf(pstr, format1,level_mark[UBL_WARN],"mod0",
		MTS_SEC, MTS_MSEC*1000, i, i, a);
	UB_TLOG(UBL_WARN,"int=%d, hex=0x%x, float=%f\n",i,i,a);
	assert_string_equal(pstr, console_data);
	assert_string_equal(pstr, debug_data);

}

static int setup1(void **state)
{
	unibase_init_para_t init_para={
		.cbset.console_out=test_console_out,
		.cbset.debug_out=test_debug_out,
		.cbset.mutex_init=NULL,
		.cbset.gettime64=test_gettime64,
		.ub_log_initstr="2,mod0:34r,mod1:45m,mod2:56g,mod3:23",
	};
	unibase_init(&init_para);
	return 0;
}

static int setup2(void **state)
{
	unibase_init_para_t init_para={
		.cbset.console_out=test_console_out,
		.cbset.debug_out=test_debug_out,
		.cbset.mutex_init=NULL,
		.cbset.gettime64=test_gettime64,
		.ub_log_initstr="mod0:34,mod1:56m",//3:WARN, 4:INFO
	};
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
		cmocka_unit_test_setup_teardown(test_ub_log_print, setup1, teardown),
		cmocka_unit_test_setup_teardown(test_ub_log_print_macro, setup2, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
