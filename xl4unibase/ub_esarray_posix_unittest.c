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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <setjmp.h>
#include <cmocka.h>
#include "unibase.h"
#include "unibase_binding.h"

#define MAX_ES_ELEMENTS 1000

#define TD_DATA_SIZE 10
typedef struct test_data1 {
	int index;
	int actime;
	uint8_t data[TD_DATA_SIZE];
} test_data1_t;

typedef struct th_data1 {
	ub_esarray_cstd_t *eah;
	int thindex;
	int elenum;
	int thnum;
}th_data1_t;

static void *test_thread1(void *ptr)
{
	th_data1_t *thd=(th_data1_t *)ptr;
	test_data1_t td;
	int i;
	for(i=0;i<thd->elenum;i++){
		assert_int_equal(0, ub_esarray_pop_ele(thd->eah, (ub_esarray_element_t *)&td));
		td.actime++;
		assert_int_equal(0,ub_esarray_add_ele(thd->eah, (ub_esarray_element_t *)&td));
	}
	return NULL;
}

static void test_esarray1(void **state)
{
	ub_esarray_cstd_t *eah;
	int i,j;
	const int thnum=10;
	const int elenum=500;
	pthread_t th[thnum];
	th_data1_t thd[thnum];
	test_data1_t td;
	int resd[elenum];

	eah = ub_esarray_init(8, sizeof(test_data1_t), MAX_ES_ELEMENTS);
	for(i=0;i<elenum;i++){
		resd[i]=0;
		td.index=i;
		td.actime=0;
		for(j=0;j<TD_DATA_SIZE;j++) td.data[j]=i+j;
		ub_esarray_add_ele(eah, (ub_esarray_element_t *)&td);
	}
	for(i=0;i<thnum;i++){
		thd[i].thindex=i;
		thd[i].eah=eah;
		thd[i].thnum=thnum;
		thd[i].elenum=elenum;
		pthread_create(&th[i], NULL, test_thread1, &thd[i]);
	}
	for(i=0;i<thnum;i++){
		pthread_join(th[i], NULL);
	}
	for(i=0;i<elenum;i++){
		assert_int_equal(0, ub_esarray_pop_ele(thd->eah, (ub_esarray_element_t *)&td));
		resd[td.index]=td.actime;
		for(j=0;j<TD_DATA_SIZE;j++){
			assert_int_equal((td.index+j)&0xff, td.data[j]);
		}
	}
	for(i=0;i<elenum;i++){
		/* possibly some data get more accesses than the others.
		 * when a data is checked out, the other thread check out and check in
		 * the other data.  the first checked out data has less access than
		 * the others.  How often it happens depends on context switching.
		 * (1, 2*thnum-1), this range is used by that reason. the most of times,
		 * it must be in (thnum-1,thnum+1) range.
		 */
		assert_in_range(resd[i], 1, 2*thnum-1);
	}
	ub_esarray_close(eah);
}

static void *test_thread2(void *ptr)
{
	th_data1_t *thd=(th_data1_t *)ptr;
	test_data1_t *td;
	int i;
	int *ip;
	ip=(int *)malloc(sizeof(int)*thd->elenum);

	while(ub_esarray_data_lock(thd->eah)) usleep(1000);
	for(i=0;i<ub_esarray_ele_nums(thd->eah);i++){
		td=(test_data1_t *)ub_esarray_get_ele(thd->eah, i);
		if(td->index%thd->thnum != thd->thindex)
			ip[i]=td->actime;
		else
			td->actime+=1;
	}
	for(i=0;i<ub_esarray_ele_nums(thd->eah);i++){
		td=(test_data1_t *)ub_esarray_get_ele(thd->eah, i);
		if(td->index%thd->thnum != thd->thindex) td->actime=ip[i];
	}
	ub_esarray_data_unlock(thd->eah);
	free(ip);
	return NULL;
}

static void test_esarray2(void **state)
{
	ub_esarray_cstd_t *eah;
	const int thnum=50;
	const int elenum=500;
	pthread_t th[thnum];
	th_data1_t thd[thnum];
	test_data1_t td;
	int resd[elenum];
	int i,j;

	eah = ub_esarray_init(15, sizeof(test_data1_t), MAX_ES_ELEMENTS);
	for(i=0;i<elenum;i++){
		resd[i]=0;
		td.index=i;
		td.actime=0;
		for(j=0;j<TD_DATA_SIZE;j++) td.data[j]=i+j;
		ub_esarray_add_ele(eah, (ub_esarray_element_t *)&td);
	}
	for(i=0;i<thnum;i++){
		thd[i].thindex=i;
		thd[i].eah=eah;
		thd[i].thnum=thnum;
		thd[i].elenum=elenum;
		pthread_create(&th[i], NULL, test_thread2, &thd[i]);
	}
	for(i=0;i<thnum;i++){
		pthread_join(th[i], NULL);
	}
	for(i=0;i<elenum;i++){
		assert_int_equal(0, ub_esarray_pop_ele(thd->eah, (ub_esarray_element_t *)&td));
		resd[td.index]=td.actime;
		for(j=0;j<TD_DATA_SIZE;j++){
			assert_int_equal((td.index+j)&0xff, td.data[j]);
		}
	}
	for(i=0;i<elenum;i++){
		assert_int_equal(resd[i], 1);
	}
	ub_esarray_close(eah);
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
		cmocka_unit_test(test_esarray1),
		cmocka_unit_test(test_esarray2),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
