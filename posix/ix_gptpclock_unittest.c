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
#include <sys/types.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>
#include <xl4unibase/unibase_binding.h>
#include "gptpnet.h"
#include "gptpclock.h"
#include "mdeth.h"
#define MAX_PORTS_NUM 5
#define TEST_VALUE_RANGE 50000

//typedef char devnam_t[IFNAMSIZ];
static netdevname_t netdevs[MAX_PORTS_NUM]={CB_VIRTUAL_ETHDEV_PREFIX"w0",
					    CB_VIRTUAL_ETHDEV_PREFIX"w1",
					    CB_VIRTUAL_ETHDEV_PREFIX"w2",
					    CB_VIRTUAL_ETHDEV_PREFIX"w3"};
static ptpdevname_t ptpdevs[MAX_PORTS_NUM]={CB_VIRTUAL_PTPDEV_PREFIX"w0",
					    CB_VIRTUAL_PTPDEV_PREFIX"w1",
					    CB_VIRTUAL_PTPDEV_PREFIX"w2",
					    CB_VIRTUAL_PTPDEV_PREFIX"w3"};
static int num_ports=4;

static void test_add_and_del(void **state) __attribute__((unused));
static void test_add_and_del(void **state)
{
	int i;
	ub_macaddr_t macid;
	ClockIdentity clockId;
	uint8_t cidex[2]={0,0};

	for(i=0;i<num_ports;i++){
		assert_int_equal(cb_get_mac_bydev(0, netdevs[i], macid), 0);
		cidex[1]=i;
		eui48to64(macid, clockId, cidex);
		assert_int_equal(gptpclock_add_clock(i, ptpdevs[i], 0, 0, clockId), 0);
	}

	for(i=0;i<num_ports;i++){
		assert_int_equal(gptpclock_del_clock(i, 0), 0);
	}
}

static void test_master_slave_main_sub(void **state) __attribute__((unused));
static void test_master_slave_main_sub(void **state)
{
	int64_t ts0,ts1,ts2,tsv1,tsv2;
	ClockIdentity clockId;
	uint8_t cidex[2]={0,0};
	ub_macaddr_t macid;

	cidex[1]=0;
	cb_get_mac_bydev(0, netdevs[0], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 0, 0, clockId));
	cidex[1]=1;
	cb_get_mac_bydev(0, netdevs[1], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[1], 1, 1, clockId));
	cidex[1]=2;
	cb_get_mac_bydev(0, netdevs[2], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[2], 2, 2, clockId));
	gptpclock_mode_master(0, 0);
	gptpclock_mode_slave_main(0, 1);
	gptpclock_mode_slave_sub(0, 2);

	ts0=gptpclock_getts64(0, 0);
	ts1=gptpclock_getts64(0, 1);
	ts2=gptpclock_getts64(0, 2);
	tsv1=ts1-ts0;
	tsv2=ts2-ts1;
	printf("ts1-ts0 = %"PRIi64" nsec, should be near 0\n", tsv1);
	assert_true(tsv1 > -TEST_VALUE_RANGE);
	assert_true(tsv1 < TEST_VALUE_RANGE);
	printf("ts2-ts1 = %"PRIi64" nsec, should be near 0\n", tsv2);
	assert_true(tsv2 > -TEST_VALUE_RANGE);
	assert_true(tsv2 < TEST_VALUE_RANGE);

	tsv1=gptpclock_getts64(0, 2);
	tsv1+=UB_SEC_NS;
	gptpclock_setts64(tsv1, 0, 2); // set clock 2 +1 sec
	ts0=gptpclock_getts64(0, 0);
	ts2=gptpclock_getts64(0, 2);
	tsv1=ts2-ts0;
	printf("ts2-ts0 = %"PRIi64" nsec, should be near %lld\n", tsv1, UB_SEC_NS);
	assert_true(tsv1 > UB_SEC_NS-TEST_VALUE_RANGE);
	assert_true(tsv1 < UB_SEC_NS+TEST_VALUE_RANGE);

	printf("## from here, need write access to the clock device ##\n");

	tsv1=ub_mt_gettime64();
	gptpclock_setts64(tsv1, 0, 1);
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	tsv1=ts1-ts0;
	printf("ts1-ts0 = %"PRIi64" nsec, should be near 0\n", tsv1);
	assert_true(tsv1 > -TEST_VALUE_RANGE);
	assert_true(tsv1 < TEST_VALUE_RANGE);
	tsv1=gptpclock_getts64(0, 1);
	tsv1+=UB_SEC_NS;
	gptpclock_setts64(tsv1, 0, 1); // set clock 1 +1 sec
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	assert_true((tsv1=ts1-ts0)>=0);
	printf("ts1-ts0 = %"PRIi64" nsec, should be near %lld\n", tsv1, UB_SEC_NS);
	assert_true(tsv1 > UB_SEC_NS-TEST_VALUE_RANGE);
	assert_true(tsv1 < UB_SEC_NS+TEST_VALUE_RANGE);

	tsv1=ub_mt_gettime64();
	tsv1+=UB_SEC_NS;
	gptpclock_setts64(tsv1, 0, 1); // set clock 1 +1 sec
	tsv1+=UB_SEC_NS;
	gptpclock_setts64(tsv1, 0, 2); // set clock 2 +2 sec
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	ts2=gptpclock_getts64(0, 2);
	tsv1=ts1-ts0;
	tsv2=ts2-ts0;
	printf("ts1-ts0 = %"PRIi64" nsec, should be near %lld\n", tsv1, UB_SEC_NS);
	assert_true(tsv1 > UB_SEC_NS-TEST_VALUE_RANGE);
	assert_true(tsv1 < UB_SEC_NS+TEST_VALUE_RANGE);
	printf("ts2-ts0 = %"PRIi64" nsec, should be near %lld\n", tsv2, 2*UB_SEC_NS);
	assert_true(tsv2 > 2*UB_SEC_NS-TEST_VALUE_RANGE);
	assert_true(tsv2 < 2*UB_SEC_NS+TEST_VALUE_RANGE);

	assert_false(gptpclock_del_clock(0, 0));
	assert_false(gptpclock_del_clock(0, 1));
	assert_false(gptpclock_del_clock(0, 2));
}

static void test_adj_freq(void **state) __attribute__((unused));
static void test_adj_freq(void **state)
{
	int64_t ts0,ts1,ts2,tsv,tsd;
	ClockIdentity clockId;
	uint8_t cidex[2]={0,0};
	int64_t gmdiff;
	ub_macaddr_t macid;

	cidex[1]=0;
	cb_get_mac_bydev(0, netdevs[0], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 0, 0, clockId));
	cidex[1]=1;
	cb_get_mac_bydev(0, netdevs[1], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 1, 1, clockId));
	cidex[1]=2;
	cb_get_mac_bydev(0, netdevs[2], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 2, 2, clockId));
	gptpclock_mode_master(0, 0);     // domain 0, clock0: Master
	gptpclock_mode_slave_main(0, 1); // domain 1, clock0: Slave Main
	gptpclock_mode_slave_sub(0, 2);  // domain 2, clock0: Slave Sub

	// get diff with adj=0
	gptpclock_setadj(0, 0, 1); // domain 1 +0ppm, HW Freq. adjust
	tsv=ub_mt_gettime64();
	gptpclock_setts64(tsv, 0, 1); // set domain 1 = tsv, HW Phase adjust
	sleep(1);
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	tsd=ts0-tsv;
	tsv=ts1-tsv;
	gmdiff=tsv-tsd;
	printf("diff btw gptpclock and monotonic = %"PRIi64"\n",gmdiff);

	gptpclock_setadj(100000, 0, 1); // domain 1 +100ppm, HW Freq. adjust
	tsv=ub_mt_gettime64();
	gptpclock_setts64(tsv, 0, 1); // set domain 1 = tsv, HW Phase adjust
	ts1=gptpclock_getts64(0, 1);
	sleep(1);
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	assert_true((tsv=ts1-ts0)>=0);
	printf("ts1-ts0 = %"PRIi64" nsec, should be near %"PRIi64" nsec\n", tsv, 100000L+gmdiff);
	assert_true(tsv > 100000L+gmdiff-TEST_VALUE_RANGE);
	assert_true(tsv < 100000L+gmdiff+TEST_VALUE_RANGE);

	gptpclock_setadj(-100000, 0, 1); // domain  -100ppm
	tsv=ub_mt_gettime64();
	gptpclock_setts64(tsv, 0, 1); // set domain 1 = tsv
	sleep(1);
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	assert_true((tsv=ts1-ts0)<0);
	printf("ts1-ts0 = %"PRIi64" nsec, should be near %"PRIi64" nsec\n", tsv, -100000L+gmdiff);
	assert_true(tsv > -100000L+gmdiff-TEST_VALUE_RANGE);
	assert_true(tsv < -100000L+gmdiff+TEST_VALUE_RANGE);

	gptpclock_setadj(100000, 0, 1); // domain 1 +100ppm, HW Freq. adjust
	gptpclock_setadj(100000, 0, 2); // domain 2 +100ppm, SW Freq. adjust to domain 1
	tsv=ub_mt_gettime64();
	gptpclock_setts64(tsv, 0, 1); // set domain 1 = tsv
	gptpclock_setts64(tsv, 0, 2); // set domain 2 = tsv
	sleep(1);
	ts0=ub_mt_gettime64();
	ts1=gptpclock_getts64(0, 1);
	ts2=gptpclock_getts64(0, 2);
	tsv=ts1-ts0;
	tsd=ts2-ts0;
	printf("ts0-tsv = %"PRIi64" nsec\n", ts0-tsv);
	printf("ts1-ts0 = %"PRIi64" nsec, should be near %"PRIi64" nsec\n", tsv, 100000L+gmdiff);
	assert_true(tsv > 100000L+gmdiff-TEST_VALUE_RANGE);
	assert_true(tsv < 100000L+gmdiff+TEST_VALUE_RANGE);
	printf("ts2-ts0 = %"PRIi64" nsec, should be near %"PRIi64" nsec\n", tsd, 200000L+2*gmdiff);
	assert_true(tsd > 200000L+2*gmdiff-TEST_VALUE_RANGE);
	assert_true(tsd < 200000L+2*gmdiff+TEST_VALUE_RANGE);

	assert_false(gptpclock_del_clock(0, 0));
	assert_false(gptpclock_del_clock(0, 1));
	assert_false(gptpclock_del_clock(0, 2));
}

#define C0_C2_OFFSET (5*UB_SEC_NS)
#define C1_C2_OFFSET (10*UB_SEC_NS)
static void test_domain_clock(char rdwr) __attribute__((unused));
static void test_domain_clock(char rdwr)
{
	int64_t ts0,ts1,ts2,ts3,ts4;
	int64_t tss0,tss1,tss2,tss3,tss4;
	int64_t tsd, tsv;
	int64_t tsd1,tsd2,tsd3,tsd4;
	ClockIdentity clockId;
	uint8_t cidex[2]={0,0};
	ub_macaddr_t macid;
	int64_t range=TEST_VALUE_RANGE;
	int64_t tdiff=0;

	netdevs[0][strlen(netdevs[0])-2]=rdwr;
	ptpdevs[0][strlen(ptpdevs[0])-2]=rdwr;
	cidex[1]=1;
	cb_get_mac_bydev(0, netdevs[0], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(1, ptpdevs[0], 0, 0, clockId));
	cidex[1]=2;
	cb_get_mac_bydev(0, netdevs[1], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(2, ptpdevs[0], 0, 0, clockId));
	cidex[1]=0;
	cb_get_mac_bydev(0, netdevs[0], macid);
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 0, 0, clockId));

	// clockid=2:SLAVE_MAIN/SUB(freq,phase), clockid=0,1:MASTER(phase) as default
	assert_false(gptpclock_set_thisClock(2, 0, true));
	ts0=gptpclock_getts64(0,0);
	ts1=gptpclock_getts64(1,0);
	ts2=gptpclock_getts64(2,0);
	tsd=ts1-ts0;
	assert_true(tsd > -range);
	assert_true(tsd < range);
	tsd=ts2-ts0;
	assert_true(tsd > -range);
	assert_true(tsd < range);

	gptpclock_setts64(ts2+C0_C2_OFFSET, 0, 0);
	gptpclock_setts64(ts2+C1_C2_OFFSET, 1, 0);
	ts0=gptpclock_getts64(0,0);
	ts1=gptpclock_getts64(1,0);
	ts2=gptpclock_getts64(2,0);
	tsd=ts0-ts2;
	assert_true(tsd > C0_C2_OFFSET-range);
	assert_true(tsd < C0_C2_OFFSET+range);
	tsd=ts1-ts2;
	assert_true(tsd > C1_C2_OFFSET-range);
	assert_true(tsd < C1_C2_OFFSET+range);

	gptpclock_setadj(100000, 2, 0);
	if(rdwr=='w') tdiff=100000;
	ts0=gptpclock_getts64(0,0);
	ts1=gptpclock_getts64(1,0);
	ts2=gptpclock_getts64(2,0);
	ts3=gptpclock_gethwts64(2,0);
	ts4=ub_mt_gettime64();
	sleep(1);
	tss0=gptpclock_getts64(0,0);
	tss1=gptpclock_getts64(1,0);
	tss2=gptpclock_getts64(2,0);
	tss3=gptpclock_gethwts64(2,0);
	tss4=ub_mt_gettime64();

	tsv=tss4-ts4;
	tsd1=tss3-ts3;
	tsd2=tss2-ts2;
	tsd3=tss1-ts1;
	tsd4=tss0-ts0;

	printf("tsv=%"PRIi64"\n", tsv);
	printf("clock HW %"PRIi64", %"PRIi64", %"PRIi64"\n",
	       tsd1-tsv, tdiff-range, tdiff+range);
	assert_true(tsd1-tsv > tdiff-range);
	assert_true(tsd1-tsv < tdiff+range);
	printf("clockID=2 %"PRIi64", %"PRIi64", %"PRIi64"\n",
	       tsd2-tsv, 100000-range, 100000+range);
	assert_true(tsd2-tsv > 100000-range);
	assert_true(tsd2-tsv < 100000+range);
	printf("clockID=1 %"PRIi64", %"PRIi64", %"PRIi64"\n",
	       tsd3-tsv, tdiff-range, tdiff+range);
	assert_true(tsd3-tsv > tdiff-range);
	assert_true(tsd3-tsv < tdiff+range);
	printf("clockID=0 %"PRIi64", %"PRIi64", %"PRIi64"\n",
	       tsd4-tsv, tdiff-range, tdiff+range);
	assert_true(tsd4-tsv > tdiff-range);
	assert_true(tsd4-tsv < tdiff+range);

	assert_false(gptpclock_del_clock(0, 0));
	assert_false(gptpclock_del_clock(1, 0));
	assert_false(gptpclock_del_clock(2, 0));
}
static void test_thisClock(void **state) __attribute__((unused));
static void test_thisClock(void **state)
{
	test_domain_clock('r');
	test_domain_clock('w');
}

static void test_tsconv(void **state) __attribute__((unused));
static void test_tsconv(void **state)
{
	int64_t ts0,ts1,ts2,tsv;
	ClockIdentity clockId;
	uint8_t cidex[2]={0,0};
	ub_macaddr_t macid;

	cb_get_mac_bydev(0, netdevs[0], macid);
	cidex[1]=0;
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 0, 0, clockId));
	cidex[1]=1;
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(1, ptpdevs[0], 0, 0, clockId));
	cidex[1]=2;
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(1, ptpdevs[0], 1, 1, clockId));
	cidex[1]=3;
	eui48to64(macid, clockId, cidex);
	assert_false(gptpclock_add_clock(0, ptpdevs[0], 1, 1, clockId));

	assert_false(gptpclock_mode_master(1, 0));
	assert_false(gptpclock_mode_slave_sub(1, 1));

	tsv=ub_mt_gettime64();
	gptpclock_setts64(tsv, 1, 0); // set clock1 = mono
	gptpclock_setts64(tsv, 1, 1); // set clock1 = mono
	ts0=tsv;
	ts1=tsv;
	gptpclock_tsconv(&ts1,1,0,1,1);
	ts2=ts1-ts0;
	printf("gptpclock_tsconv (domain 0 -> 1) diff = %"PRIi64" nsec, should be near 0\n", ts2);
	assert_true(ts2 > -TEST_VALUE_RANGE);
	assert_true(ts2 < TEST_VALUE_RANGE);

	ts2=330000000;
	tsv=ub_mt_gettime64();
	ts0=tsv;
	ts1=tsv;
	tsv=tsv+ts2;
	gptpclock_setts64(tsv, 1, 1);
	gptpclock_tsconv(&ts1,1,0,1,1);
	ts2=ts1-ts0;
	printf("gptpclock_tsconv (domain 0 -> 1) diff = %"PRIi64" nsec, should be near "
	       "3sec 30000000nsec\n", ts2);
	assert_true(ts2 > 330000000-TEST_VALUE_RANGE);
	assert_true(ts2 < 330000000+TEST_VALUE_RANGE);

	ts2=330000000;
	tsv=ub_mt_gettime64();
	ts0=tsv;
	ts1=tsv;
	tsv=tsv-ts2;
	gptpclock_setts64(tsv, 1, 1);
	gptpclock_tsconv(&ts1,1,0,1,1);
	ts2=ts1-ts0;
	printf("gptpclock_tsconv (domain 0 -> 1) diff = %"PRIi64" nsec, should be near "
	       "-3sec 30000000nsec\n", ts2);
	assert_true(ts2 > -330000000-TEST_VALUE_RANGE);
	assert_true(ts2 < -330000000+TEST_VALUE_RANGE);

	assert_false(gptpclock_mode_master(1, 1));
	assert_false(gptpclock_mode_slave_sub(1, 0));
	tsv=ub_mt_gettime64();
	gptpclock_setts64(tsv, 1, 0); // set clock1 = mono
	gptpclock_setts64(tsv, 1, 1); // set clock1 = mono
	ts0=tsv;
	ts1=tsv;
	gptpclock_tsconv(&ts1,1,1,1,0);
	ts2=ts1-ts0;
	printf("gptpclock_tsconv (domain 1 -> 0) diff = %"PRIi64" nsec, should be near 0\n", ts2);
	assert_true(ts2 > -TEST_VALUE_RANGE);
	assert_true(ts2 < TEST_VALUE_RANGE);

	ts2=330000000;
	tsv=ub_mt_gettime64();
	ts0=tsv;
	ts1=tsv;
	tsv=tsv+ts2;
	gptpclock_setts64(tsv, 1, 0);
	gptpclock_tsconv(&ts1,1,1,1,0);
	ts2=ts1-ts0;
	printf("gptpclock_tsconv (domain 1 -> 0) diff = %"PRIi64" nsec, should be near "
	       "3sec 30000000nsec\n", ts2);
	assert_true(ts2 > 330000000-TEST_VALUE_RANGE);
	assert_true(ts2 < 330000000+TEST_VALUE_RANGE);

	ts2=330000000;
	tsv=ub_mt_gettime64();
	ts0=tsv;
	ts1=tsv;
	tsv=tsv-ts2;
	gptpclock_setts64(tsv, 1, 0);
	gptpclock_tsconv(&ts1,1,1,1,0);
	ts2=ts1-ts0;
	printf("gptpclock_tsconv (domain 1 -> 0) diff = %"PRIi64" nsec, should be near "
	       "-3sec 30000000nsec\n", ts2);
	assert_true(ts2 > -330000000-TEST_VALUE_RANGE);
	assert_true(ts2 < -330000000+TEST_VALUE_RANGE);

	assert_false(gptpclock_del_clock(0, 0));
	assert_false(gptpclock_del_clock(0, 1));
	assert_false(gptpclock_del_clock(1, 0));
	assert_false(gptpclock_del_clock(1, 1));
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr=UBL_OVERRIDE_ISTR("4,ubase:45,cbase:45,gptp:46", "UBL_GPTP");
	unibase_init(&init_para);

	gptpconf_set_item(CONF_MASTER_CLOCK_SHARED_MEM, "/gptp_mc_shm0");
	gptpclock_init(3, MAX_PORTS_NUM);
	return 0;
}

static int teardown(void **state)
{
	gptpclock_close();
	unibase_close();
	return 0;
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_add_and_del),
		cmocka_unit_test(test_master_slave_main_sub),
		cmocka_unit_test(test_adj_freq),
		cmocka_unit_test(test_tsconv),
		cmocka_unit_test(test_thisClock),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
