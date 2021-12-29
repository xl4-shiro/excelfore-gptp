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
#include <stdio.h>
#include <xl4unibase/unibase_binding.h>
#include <setjmp.h>
#include <cmocka.h>
#include "gptpclock.h"

static void get_tsrp(int ptpfd, uint64_t *tsr, uint64_t *tsp)
{
	*tsr=ub_mt_gettime64();
	GPTP_CLOCK_GETTIME(ptpfd, *tsp)
}

static void test_one_adjust(int ptpfd, int adjppb)
{
	uint64_t tsr1, tsp1=0, tsr2, tsp2=0;
	int64_t dr,dp;
	int64_t rppm, dppb;
	gptp_clock_adjtime(ptpfd, adjppb);
	get_tsrp(ptpfd, &tsr1, &tsp1);
	sleep(1);
	get_tsrp(ptpfd, &tsr2, &tsp2);
	dr=tsr2-tsr1;
	dp=tsp2-tsp1;
	rppm=(dp-dr)*1000000/dr;
	printf("adj=%dppm, RC:%"PRIi64", PC:%"PRIi64", rate=%"PRIi64"ppm\n",
	       adjppb/1000, dr, dp, rppm);
	dppb=(adjppb==0)?2000:adjppb/25;
	assert_true(labs(rppm) <= labs((adjppb+dppb)/1000));
	if(adjppb) assert_true(labs(rppm) >= labs((adjppb-dppb)/1000));
}

static void test_two_adjust(int ptpfd1, int ptpfd2, int adjppba, int adjppbb)
{
	uint64_t tsr1, tspa1=0, tsr2, tspa2=0, tspb1, tspb2;
	int64_t dr, dpa, dpb;
	int64_t rppma, dppba, rppmb, dppbb;
	gptp_clock_adjtime(ptpfd1, adjppba);
	gptp_clock_adjtime(ptpfd2, adjppbb);
	get_tsrp(ptpfd1, &tsr1, &tspa1);
	GPTP_CLOCK_GETTIME(ptpfd2, tspb1);
	sleep(1);
	get_tsrp(ptpfd1, &tsr2, &tspa2);
	GPTP_CLOCK_GETTIME(ptpfd2, tspb2);
	dr=tsr2-tsr1;
	dpa=tspa2-tspa1;
	dpb=tspb2-tspb1;
	rppma=(dpa-dr)*1000000/dr;
	rppmb=(dpb-dr)*1000000/dr;
	printf("adja=%dppm, RC:%"PRIi64", PC:%"PRIi64", ratea=%"PRIi64"ppm\n",
	       adjppba/1000, dr, dpa, rppma);
	printf("adjb=%dppm, RC:%"PRIi64", PC:%"PRIi64", rateb=%"PRIi64"ppm\n",
	       adjppbb/1000, dr, dpb, rppmb);
	dppba=(adjppba==0)?2000:adjppba/25;
	dppbb=(adjppbb==0)?2000:adjppbb/25;
	assert_true(labs(rppma) <= labs((adjppba+dppba)/1000));
	if(adjppba) assert_true(labs(rppma) >= labs((adjppba-dppba)/1000));
	assert_true(labs(rppmb) <= labs((adjppbb+dppbb)/1000));
	if(adjppbb) assert_true(labs(rppmb) >= labs((adjppbb-dppbb)/1000));
}

static void test_adjustment(void **state)
{
	int ptpfd;
	char *ptpdev=CB_VIRTUAL_PTPDEV_PREFIX"w0";
	ptpclock_state_t pds;

	pds=gptp_get_ptpfd(ptpdev, &ptpfd);
	assert_int_equal(pds, PTPCLOCK_RDWR);
	test_one_adjust(ptpfd, 0);
	test_one_adjust(ptpfd, 100000);
	test_one_adjust(ptpfd, -100000);
	test_one_adjust(ptpfd, 1000000);
	test_one_adjust(ptpfd, -1000000);
	test_one_adjust(ptpfd, 10000000);
	test_one_adjust(ptpfd, -10000000);
}

static void test_multi(void **state)
{
	int ptpfd1, ptpfd2;
	char *ptpdev1=CB_VIRTUAL_PTPDEV_PREFIX"w0";
	char *ptpdev2=CB_VIRTUAL_PTPDEV_PREFIX"1";
	ptpclock_state_t pds;

	pds=gptp_get_ptpfd(ptpdev1, &ptpfd1);
	assert_int_equal(pds, PTPCLOCK_RDWR);
	pds=gptp_get_ptpfd(ptpdev2, &ptpfd2);
	assert_int_equal(pds, PTPCLOCK_RDONLY);
	test_two_adjust(ptpfd1, ptpfd2, 100000, 0);
	test_two_adjust(ptpfd1, ptpfd2, 100000, 0);
	test_two_adjust(ptpfd1, ptpfd2, -100000, 0);
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr=UBL_OVERRIDE_ISTR("4,ubase:45,cbase:45,gptp:46", "UBL_GPTP");
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
		cmocka_unit_test(test_adjustment),
		cmocka_unit_test(test_multi),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
