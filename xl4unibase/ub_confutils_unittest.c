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
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>
#include "unibase.h"
#include "unibase_binding.h"
#include "ubconftest_conf.h"
extern int ubconftestconf_values_test(void);

static void create_conf_file(void)
{
	FILE *fp;
	char *astr;
	fp=fopen("ubconfutils_test.conf", "w");
	astr="CONF_TEST_INT1 12\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="CONF_TEST_INT2 12 # comment post value\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="\n"; // empty line
	fwrite(astr, 1, strlen(astr), fp);
	astr="# this is a comment line\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="CONF_TEST_HEXINT1 0x1000000b\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="CONF_TEST_LINT1 10000000002\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="# this is another comment line\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="CONF_TEST_BYTE6 1a:2b:3c:4d:5e:70\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="CONF_TEST_STR2 \"hello world\"\n";
	fwrite(astr, 1, strlen(astr), fp);
	astr="CONF_TEST_STR3 \"hello world\" #comment post value\n";
	fwrite(astr, 1, strlen(astr), fp);

	fclose(fp);
}

static void test_ubconfutils(void **state)
{
	const uint8_t d6[]={0x1a,0x2b,0x3c,0x4d,0x5e,0x70};
	create_conf_file();
	assert_false(ub_read_config_file("ubconfutils_test.conf", ubconftestconf_set_stritem));
	ubconftestconf_values_test();
	assert_int_equal(ubconftestconf_get_intitem(CONF_TEST_INT1), 12);
	assert_int_equal(ubconftestconf_get_intitem(CONF_TEST_INT2), 12);
	assert_int_equal(ubconftestconf_get_intitem(CONF_TEST_HEXINT1), 0x1000000b);
	assert_int_equal(ubconftestconf_get_lintitem(CONF_TEST_LINT1), 10000000002);
	assert_memory_equal(ubconftestconf_get_item(CONF_TEST_BYTE6), d6, 6);
	assert_string_equal(ubconftestconf_get_item(CONF_TEST_STR2), "hello world");
	assert_string_equal(ubconftestconf_get_item(CONF_TEST_STR3), "hello world");
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
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
		cmocka_unit_test(test_ubconfutils),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
