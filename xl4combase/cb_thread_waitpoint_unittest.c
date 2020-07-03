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
#include <sched.h>
#define UNIT_TESTING
#include <cmocka.h>
#include "combase.h"
#include <xl4unibase/unibase_binding.h>

#define UB_LOGCAT 2

#define WPNUM 2
cb_waitpoint_t wp[WPNUM];

#define THREAD_COUNT 12
#define ITERATIONS 64
#define DATALEN 100
int shared_data[DATALEN];
int errors=0;

static int setup(void **state){
	int i;
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr="4,ubase:45,combase:45,cbwp:55m";
	unibase_init(&init_para);

	for(i=0;i<WPNUM;i++){
		cb_waitpoint_init(&wp[i]);
	}
	return 0;
}

static int teardown(void **state){
	int i;
	for(i=0;i<WPNUM;i++){
		cb_waitpoint_deinit(&wp[i]);
	}
	unibase_close();
	return 0;
}

void nonatomic_operation(){
	int i;
	memset(&shared_data, 0, sizeof(shared_data));
	for(i=0; i<DATALEN; i++){
		// allow context switch
		nanosleep((const struct timespec[]){{0, 10}}, NULL);
		// set data value to 1 for summation to 100 in check_shared()
		shared_data[i]=1;
	}
}

bool check_shared(){
	int total, i;
	for(i=0,total=0; i<DATALEN; i++){
		total+= shared_data[i];
	}
	return total==DATALEN?true:false;
}

static void *thread_op1(void *arg){
	int i;
	for(i=0; i<ITERATIONS; i++){
		cb_waitpoint_lock(&wp[0]);
		nonatomic_operation();
		if(!check_shared()){
			// increment error count when check_shared returns false
			errors++;
		}
		cb_waitpoint_unlock(&wp[0]);
	}
	return NULL;
}

static void *thread_corrupt1(void *arg){
	int i;
	for(i=0; i<ITERATIONS; i++){
		cb_waitpoint_lock(&wp[0]);
		memset(&shared_data, 0xFF, sizeof(shared_data));
		cb_waitpoint_unlock(&wp[0]);
	}
	return NULL;
}

static void *thread_wait_pingpong(void *arg){
	int i, id=*(int *)arg;
	int dest = 1-id;

	for(i=0; i<ITERATIONS; i++){
		// act as the sender of signal
		cb_waitpoint_lock(&wp[dest]);
		shared_data[dest]=id;
		UB_LOG(UBL_INFO, "%s: [%d](%d/%d) sending wake-up signal...\n", __func__, id, i, ITERATIONS);
		cb_waitpoint_wakeup(&wp[dest]);
		cb_waitpoint_unlock(&wp[dest]);
		if(i<ITERATIONS-1){
			// act as the receiver of signal
			cb_waitpoint_lock(&wp[id]);
			while(shared_data[id]!=dest){
				UB_LOG(UBL_INFO, "%s: [%d](%d/%d) waiting for wake-up signal...\n", __func__, id, i, ITERATIONS);
				cb_waitpoint_wakeup_at(&wp[id], 0 /* can wake up anytime */, true);
				// blocks until cb_waitpoint_wakeup is called
				UB_LOG(UBL_INFO, "%s: [%d](%d/%d) received wake-up signal...\n", __func__, id, i, ITERATIONS);
			}
			shared_data[id]=id;
			cb_waitpoint_unlock(&wp[id]);
		}
	}
	return NULL;
}

static void *thread_timedwait_pingpong(void *arg){
	int i, id=*(int *)arg;
	int dest = 1-id;
	uint64_t t1, t2;

	for(i=0; i<ITERATIONS; i++){
		// act as the sender of signal
		cb_waitpoint_lock(&wp[dest]);
		shared_data[dest]=id;
		UB_LOG(UBL_INFO, "%s: [%d](%d/%d) sending wake-up signal...\n", __func__, id, i, ITERATIONS);
		cb_waitpoint_wakeup(&wp[dest]);

		cb_waitpoint_unlock(&wp[dest]);
		if(i<ITERATIONS-1){
			// act as the receiver of signal
			cb_waitpoint_lock(&wp[id]);
			while(shared_data[id]!=dest){
				UB_LOG(UBL_INFO, "%s: [%d](%d/%d) waiting for wake-up signal...\n", __func__, id, i, ITERATIONS);
				t1 = ub_rt_gettime64();
				cb_waitpoint_wakeup_at(&wp[id], t1+2*UB_MSEC_NS, true);
				// blocks until cb_waitpoint_wakeup is called
				while(!cb_waitpoint_check(&wp[id], ub_rt_gettime64())){
					nanosleep((const struct timespec[]){{0, 10}}, NULL);
				}
				UB_LOG(UBL_INFO, "%s: [%d](%d/%d) received wake-up signal...\n", __func__, id, i, ITERATIONS);
				t2 = ub_rt_gettime64();
				if(t1>=t2){
					UB_LOG(UBL_ERROR, "%s: [%d](%d/%d) t1 is greater...\n", __func__, id, i, ITERATIONS);
					errors++;
				}
				if(t2-t1<2*UB_MSEC_NS){
					errors++;
				}
			}
			shared_data[id]=id;
			cb_waitpoint_unlock(&wp[id]);
		}
	}
	return NULL;
}

static void *thread_wait_signal(void *arg){
	int id = *((int *)arg);
	cb_waitpoint_lock(&wp[0]);
	UB_LOG(UBL_INFO, "%s: [%d] waiting for wake-up signal...\n", __func__, id);
	errors++;
	cb_waitpoint_wakeup_at(&wp[0], ub_gptp_gettime64(), true);
	errors--;
	UB_LOG(UBL_INFO, "%s: [%d] received wake-up signal...\n", __func__, id);
	cb_waitpoint_unlock(&wp[0]);
	return NULL;
}

static void test_race_condition(void **state){
	CB_THREAD_T th[THREAD_COUNT];
	int i;

	pthread_attr_t tattr;
	int newprio = THREAD_COUNT;
	struct sched_param param;

	for(i=0; i<THREAD_COUNT; i++){
		pthread_attr_init (&tattr);
		pthread_attr_getschedparam (&tattr, &param);
		// priority of the next thread should be higher to increase
		// chance of race condition
		param.sched_priority = newprio--;
		pthread_attr_setschedparam (&tattr, &param);

		if(i%2==0)
			CB_THREAD_CREATE(&th[i], &tattr, thread_op1, NULL);
		else
			CB_THREAD_CREATE(&th[i], &tattr, thread_corrupt1, NULL);
	}

	for(i=0; i<THREAD_COUNT; i++){
		CB_THREAD_JOIN(th[i], NULL);
	}
	assert_int_equal(errors, 0);
}

static void test_ping_pong(void **state){
	CB_THREAD_T ping;
	CB_THREAD_T pong;
	int id1=0, id2=1;
	CB_THREAD_CREATE(&ping, NULL, thread_wait_pingpong, &id1);
	CB_THREAD_CREATE(&pong, NULL, thread_wait_pingpong, &id2);

	CB_THREAD_JOIN(ping, NULL);
	CB_THREAD_JOIN(pong, NULL);
	assert_int_equal(errors, 0);
}

static void test_timed_ping_pong(void **state){
	CB_THREAD_T ping;
	CB_THREAD_T pong;
	int id1=0, id2=1;

	CB_THREAD_CREATE(&ping, NULL, thread_timedwait_pingpong, &id1);
	CB_THREAD_CREATE(&pong, NULL, thread_timedwait_pingpong, &id2);

	CB_THREAD_JOIN(ping, NULL);
	CB_THREAD_JOIN(pong, NULL);
	assert_int_equal(errors, 0);
}

static void test_waitpoint_broadcast(void **state){
	CB_THREAD_T th[THREAD_COUNT];
	int i;
	for(i=0; i<THREAD_COUNT; i++){
		CB_THREAD_CREATE(&th[i], NULL, thread_wait_signal, &errors);
	}
	// ensure sequence
	while(errors!=THREAD_COUNT) nanosleep((const struct timespec[]){{0, 10000}}, NULL);
	// broadcast to all threads to continue
	cb_waitpoint_broadcast(&wp[0]);
	for(i=0; i<THREAD_COUNT; i++){
		CB_THREAD_JOIN(th[i], NULL);
	}
	assert_int_equal(errors, 0);
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_race_condition),
		cmocka_unit_test(test_ping_pong),
		cmocka_unit_test(test_timed_ping_pong),
		cmocka_unit_test(test_waitpoint_broadcast),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
