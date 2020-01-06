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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define UNIT_TESTING
#include <cmocka.h>
#include "combase.h"
#include <xl4unibase/unibase_binding.h>
#define UB_LOGCAT 2

static char *test_string1="cfo9(eb@jbf!t-+=@wi+x3pew7#e^xai*q6k_igi55t$%&1xwt";
static char *test_string2=")zj_(_brpo)kcb=&6r0@zpdrv42pe4#-754^s=-)jr#xm72u3_";
static void *test_noipcshmem_thread1(void *ptr)
{
	void *shmem;
	int memfd;
	int i;

	shmem=cb_get_shared_mem(&memfd, "testshmem1", 1024, O_RDWR);
	assert_non_null(shmem);
	strcpy((char*)shmem, test_string1);
	for(i=0;i<10;i++){
		if(strcmp((char*)shmem, test_string1)==0) break;
		usleep(1000);
	}
	assert_true(i<10);
	return NULL;
}

static void *test_noipcshmem_thread2(void *ptr)
{
	void *shmem;
	int memfd;
	int i;
	shmem=cb_get_shared_mem(&memfd, "testshmem1", 1024, O_RDWR);
	assert_non_null(shmem);
	for(i=0;i<10;i++){
		if(strcmp((char*)shmem, test_string1)==0) break;
		usleep(1000);
	}
	assert_true(i<10);
	strcpy((char*)shmem, test_string2);
	return NULL;
}

static void test_noipcshmem1(void **state)
{
	int memfd;
	void *shmem;
	shmem=cb_get_shared_mem(&memfd, "testshmem1", 1024, O_RDWR);
	assert_null(shmem);
	shmem=cb_get_shared_mem(&memfd, "testshmem1", 1024, O_RDWR|O_CREAT);
	assert_non_null(shmem);
	shmem=cb_get_shared_mem(&memfd, "testshmem2", 1024, O_RDWR|O_CREAT);
	assert_non_null(shmem);
	strcpy((char*)shmem, test_string1);
	shmem=cb_get_shared_mem(&memfd, "testshmem1", 1024, O_RDONLY);
	assert_non_null(shmem);
	assert_string_not_equal((char*)shmem, test_string1);
	shmem=cb_get_shared_mem(&memfd, "testshmem2", 1024, O_RDONLY);
	assert_non_null(shmem);
	assert_string_equal((char*)shmem, test_string1);
}

static void test_noipcshmem2(void **state)
{
	CB_THREAD_T th1, th2;
	int memfd;
	void *shmem;

	CB_THREAD_CREATE(&th1, NULL, test_noipcshmem_thread1, NULL);
	CB_THREAD_CREATE(&th2, NULL, test_noipcshmem_thread2, NULL);

	CB_THREAD_JOIN(th1, NULL);
	CB_THREAD_JOIN(th2, NULL);

	shmem=cb_get_shared_mem(&memfd, "testshmem1", 1024, O_RDONLY);
	assert_int_not_equal(memfd,0);
	cb_close_shared_mem(shmem, &memfd, "testshmem1", 1024, true);
	assert_int_equal(memfd,0);

	shmem=cb_get_shared_mem(&memfd, "testshmem2", 1024, O_RDONLY);
	assert_int_not_equal(memfd,0);
	cb_close_shared_mem(shmem, &memfd, "testshmem2", 1024, true);
	assert_int_equal(memfd,0);
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr="4,ubase:45,combase:45,noipcsm:55";
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
		cmocka_unit_test(test_noipcshmem1),
		cmocka_unit_test(test_noipcshmem2),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
