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
#include <setjmp.h>
#include <cmocka.h>
#include <xl4unibase/unibase_binding.h>
#include "gptpcommon.h"

static void test_eui48to64(void **state)
{
	const uint8_t d1[6]={0x11,0x22,0x33,0x44,0x55,0x66};
	const uint8_t r1[8]={0x11,0x22,0x33,0xff,0xfe,0x44,0x55,0x66};
	const uint8_t r2[8]={0x11,0x22,0x33,0xaa,0xbb,0x44,0x55,0x66};
	uint8_t r[8];
	uint8_t i[2]={0xaa,0xbb};
	eui48to64(d1, r, NULL);
	assert_memory_equal(r, r1, 8);
	eui48to64(d1, r, i);
	assert_memory_equal(r, r2, 8);
}

static void test_log_to_nsec(void **state)
{
	assert_int_equal(LOG_TO_NSEC(0), 1 * UB_SEC_NS);
	assert_int_equal(LOG_TO_NSEC(-1), 500000000);
	assert_int_equal(LOG_TO_NSEC(-2), 250000000);
	assert_int_equal(LOG_TO_NSEC(-3), 125000000);
	assert_int_equal(LOG_TO_NSEC(1), 2 * UB_SEC_NS);
	assert_int_equal(LOG_TO_NSEC(2), 4 * UB_SEC_NS);
	assert_int_equal(LOG_TO_NSEC(3), 8 * UB_SEC_NS);
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
		cmocka_unit_test(test_eui48to64),
		cmocka_unit_test(test_log_to_nsec),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
