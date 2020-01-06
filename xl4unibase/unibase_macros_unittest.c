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
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include "unibase.h"
#include "ub_unittest_helper.c"

UB_BIT8_FIELD(bt1, 5, 3);
UB_BIT16_FIELD(bt2, 13, 3);
UB_BIT32_FIELD(bt3, 21, 3);

UB_BIT8_TOGGLE_FIELD(btt1, 7, 1);
UB_BIT16_TOGGLE_FIELD(btt2, 15, 1);
UB_BIT32_TOGGLE_FIELD(btt3, 31, 1);

static void test_macros1(void **state)
{
	ub_streamid_t sid={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};
	ub_macaddr_t mac={0x11,0x12,0x13,0x14,0x15,0x16};
	ub_streamid_t sid2;
	ub_macaddr_t mac2;
	char astr[128];
	uint8_t p;
	uint32_t x;
	uint16_t y;
	struct timeval tv1,tv2;
	struct timespec ts1,ts2;
	uint64_t u,v;

	/* operations on Stream ID string, MAC string */
	sprintf(astr, UB_PRIhexB8, UB_ARRAY_B8(sid));
	assert_string_equal(astr, "01:02:03:04:05:06:07:08");
	sprintf(astr, UB_PRIhexB6, UB_ARRAY_B6(mac));
	assert_string_equal(astr, "11:12:13:14:15:16");
	assert_true(UB_NON_ZERO_B8(sid));
	assert_true(UB_NON_ZERO_B6(mac));
	memset(&sid2, 0, sizeof(sid2));
	memset(&mac2, 0, sizeof(mac2));
	assert_false(UB_NON_ZERO_B8(sid2));
	assert_false(UB_NON_ZERO_B6(mac2));
	assert_false(UB_ALLFF_B8(sid2));
	assert_false(UB_ALLFF_B6(mac2));
	memset(&sid2, 0xff, sizeof(sid2));
	memset(&mac2, 0xff, sizeof(mac2));
	assert_true(UB_ALLFF_B8(sid2));
	assert_true(UB_ALLFF_B6(mac2));

	/* operations on bit fields */
	p=0xff;
	bt1_set_bit_field(&p, 0);
	assert_int_equal(p, 0x9f);
	bt1_set_bit_field(&p, 1);
	assert_int_equal(p, 0xbf);
	y=0xffff;
	bt2_set_bit_field(&y, 0);
	assert_int_equal(y, ntohs(0x9fff));
	bt2_set_bit_field(&y, 1);
	assert_int_equal(y, ntohs(0xbfff));
	x=0xffffffff;
	bt3_set_bit_field(&x, 0);
	assert_int_equal(x, ntohl(0xff9fffff));
	bt3_set_bit_field(&x, 1);
	assert_int_equal(x, ntohl(0xffbfffff));

	p=0xff;
	btt1_toggle_bit_field(&p);
	assert_int_equal(p, 0x7f);
	btt1_toggle_bit_field(&p);
	assert_int_equal(p, 0xff);
	y=0xffff;
	btt2_toggle_bit_field(&y);
	assert_int_equal(y, ntohs(0x7fff));
	btt2_toggle_bit_field(&y);
	assert_int_equal(y, ntohs(0xffff));
	x=0xffffffff;
	btt3_toggle_bit_field(&x);
	assert_int_equal(x, ntohl(0x7fffffff));
	btt3_toggle_bit_field(&x);
	assert_int_equal(x, ntohl(0xffffffff));

	/* operations on timestamp */
	tv1.tv_sec=1;
	tv1.tv_usec=1000;
	assert_int_equal(UB_TV2NSEC(tv1), 1001000000);
	assert_int_equal(UB_TV2USEC(tv1), 1001000);
	assert_int_equal(UB_TV2MSEC(tv1), 1001);
	ts1.tv_sec=1;
	ts1.tv_nsec=1000000;
	assert_int_equal(UB_TS2NSEC(ts1), 1001000000);
	assert_int_equal(UB_TS2USEC(ts1), 1001000);
	assert_int_equal(UB_TS2MSEC(ts1), 1001);

	UB_NSEC2TV(2002000000,tv1);
	assert_int_equal(tv1.tv_sec,2);
	assert_int_equal(tv1.tv_usec,2000);
	UB_USEC2TV(3003000,tv1);
	assert_int_equal(tv1.tv_sec,3);
	assert_int_equal(tv1.tv_usec,3000);
	UB_MSEC2TV(4004,tv1);
	assert_int_equal(tv1.tv_sec,4);
	assert_int_equal(tv1.tv_usec,4000);
	UB_NSEC2TS(2002000000,ts1);
	assert_int_equal(ts1.tv_sec,2);
	assert_int_equal(ts1.tv_nsec,2000000);
	UB_USEC2TS(3003000,ts1);
	assert_int_equal(ts1.tv_sec,3);
	assert_int_equal(ts1.tv_nsec,3000000);
	UB_MSEC2TS(4004,ts1);
	assert_int_equal(ts1.tv_sec,4);
	assert_int_equal(ts1.tv_nsec,4000000);

	gettimeofday(&tv1, NULL);
	tv2=tv1;
	assert_int_equal(UB_TV_DIFF64NS(tv1,tv2), 0);
	tv1.tv_usec=40;
	tv2.tv_usec=0;
	assert_int_equal(UB_TV_DIFF64NS(tv1,tv2), 40*UB_USEC_NS);
	assert_int_equal(UB_TV_DIFF64NS(tv2,tv1), -40*UB_USEC_NS);
	tv1.tv_sec+=1;
	assert_int_equal(UB_TV_DIFF64NS(tv1,tv2), UB_SEC_NS+40*UB_USEC_NS);
	assert_int_equal(UB_TV_DIFF64NS(tv2,tv1), -(UB_SEC_NS+40*UB_USEC_NS));
	tv2.tv_sec=10;
	tv2.tv_usec=100;
	assert_int_equal(UB_TV2NSEC(tv1)+10*UB_SEC_NS+100*UB_USEC_NS, UB_TV_ADD64NS(tv1,tv2));

	clock_gettime(CLOCK_REALTIME, &ts1);
	ts2=ts1;
	assert_int_equal(UB_TS_DIFF64NS(ts1,ts2), 0);
	ts1.tv_nsec=40;
	ts2.tv_nsec=0;
	assert_int_equal(UB_TS_DIFF64NS(ts1,ts2), 40);
	assert_int_equal(UB_TS_DIFF64NS(ts2,ts1), -40);
	ts1.tv_sec+=1;
	assert_int_equal(UB_TS_DIFF64NS(ts1,ts2), UB_SEC_NS+40);
	assert_int_equal(UB_TS_DIFF64NS(ts2,ts1), -(UB_SEC_NS+40));
	ts2.tv_sec=10;
	ts2.tv_nsec=100;
	assert_int_equal(UB_TS2NSEC(ts1)+10*UB_SEC_NS+100, UB_TS_ADD64NS(ts1,ts2));

	gettimeofday(&tv1, NULL);
	tv2=tv1;
	tv1.tv_sec=3;
	tv1.tv_usec=0;
	UB_TV_ADD_TV(tv1,tv1,tv2);
	assert_int_equal(tv2.tv_sec+3, tv1.tv_sec);
	assert_int_equal(tv2.tv_usec, tv1.tv_usec);
	tv1.tv_sec=4;
	tv1.tv_usec=999999;
	UB_TV_ADD_TV(tv1,tv1,tv2);
	UB_TV_DIFF_TV(tv1,tv1,tv2);
	assert_int_equal(tv1.tv_sec, 4);
	assert_int_equal(tv1.tv_usec, 999999);
	tv1.tv_sec=4;
	tv1.tv_usec=999999;
	UB_TV_ADD_TV(tv1,tv1,tv2);
	UB_TV_DIFF_TV(tv1,tv2,tv1);
	assert_int_equal(tv1.tv_sec, -4);
	assert_int_equal(tv1.tv_usec, -999999);

	clock_gettime(CLOCK_REALTIME, &ts1);
	ts2=ts1;
	ts1.tv_sec=3;
	ts1.tv_nsec=0;
	UB_TS_ADD_TS(ts1,ts1,ts2);
	assert_int_equal(ts2.tv_sec+3, ts1.tv_sec);
	assert_int_equal(ts2.tv_nsec, ts1.tv_nsec);
	ts1.tv_sec=4;
	ts1.tv_nsec=999999;
	UB_TS_ADD_TS(ts1,ts1,ts2);
	UB_TS_DIFF_TS(ts1,ts1,ts2);
	assert_int_equal(ts1.tv_sec, 4);
	assert_int_equal(ts1.tv_nsec, 999999);
	ts1.tv_sec=4;
	ts1.tv_nsec=999999;
	UB_TS_ADD_TS(ts1,ts1,ts2);
	UB_TS_DIFF_TS(ts1,ts2,ts1);
	assert_int_equal(ts1.tv_sec, -4);
	assert_int_equal(ts1.tv_nsec, -999999);

	/* htonll ntohll */
	u=0x89abcdef01234567;
	v=0x67452301efcdab89;
	assert_int_equal(UB_HTONLL(u), v);
	assert_int_equal(UB_NTOHLL(v), u);
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_macros1),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
