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
#include <xl4unibase/unibase_binding.h>
#include <setjmp.h>
#include <cmocka.h>
#include "mdeth.h"
#include "md_abnormal_hooks.h"
#include "gptpnet.h"

static gptpnet_data_t *gpnet;

static int gptpnet_cb(void *cb_data, int portIndex, gptpnet_event_t event,
		      int64_t *event_ts, void *event_data)
{
	return 0;
}

static void test_abnormal_events1(void **state)
{
	md_abn_event_t event1={domainNumber:0, ndevIndex:0, msgtype:SYNC,
			       eventtype:MD_ABN_EVENTP_SKIP,
			       eventrate:1.0, repeat:0, interval:0, eventpara:0};
	int length=sizeof(MDPTPMsgSync);
	ClockIdentity clockid={0,};

	md_header_compose(gpnet, 1, SYNC, length, clockid, 1, 100, -3);
	// without md_abnormal_init, no event happens
	assert_int_equal(md_abnormal_register_event(&event1),-1);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);

	md_abnormal_init();
	assert_int_equal(md_abnormal_register_event(&event1),0);

	// repeat every time forever
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// repeat=1, interval=0 : repeat one time
	event1.repeat=1;
	event1.interval=0;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// repeat=1, interval=1 : repeat one time
	event1.repeat=1;
	event1.interval=1;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// repeat=2, interval=0 : repeat 2 times
	event1.repeat=2;
	event1.interval=0;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// repeat=2, interval=1 : repeat 2 times with interval 1
	event1.repeat=2;
	event1.interval=1;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// repeat=2, interval=2 : repeat 2 times with interval 2
	event1.repeat=2;
	event1.interval=2;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// repeat=3, interval=2 : repeat 3 times with interval 2
	event1.repeat=3;
	event1.interval=2;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_SKIP);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	md_abnormal_close();
}

static void test_abnormal_events2(void **state)
{
	md_abn_event_t event1={domainNumber:0, ndevIndex:0, msgtype:PDELAY_REQ,
			       eventtype:MD_ABN_EVENTP_DUPLICATE,
			       eventrate:1.0, repeat:0, interval:0, eventpara:0};
	int length=sizeof(MDPTPMsgPdelayReq);
	ClockIdentity clockid={0,};
	int i, count;

	md_header_compose(gpnet, 1, SYNC, length, clockid, 1, 100, -3);
	md_abnormal_init();

	// PDELAY_REQ != SYNC, the event doesn't happen
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	md_header_compose(gpnet, 1, PDELAY_REQ, length, clockid, 1, 100, -3);
	// repeat every time forever
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_DUPLICATE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_DUPLICATE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_DUPLICATE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// possibility=0.0, the event never happens
	event1.eventrate=0.0;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_gptpnet_send_hook(gpnet, 0, length), MD_ABN_EVENTP_NONE);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	// possibility=0.5, the event happens 50%
	event1.eventrate=0.5;
	assert_int_equal(md_abnormal_register_event(&event1),0);
	for(i=0,count=0;i<1000;i++){
		if(md_abnormal_gptpnet_send_hook(gpnet, 0, length)==MD_ABN_EVENTP_DUPLICATE)
			count++;
	}
	assert_in_range(count, 400, 600);
	assert_int_equal(md_abnormal_deregister_all_events(),0);

	md_abnormal_close();
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	char *netdevs[2]={"cbeth0",NULL};
	int np;

	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr=UBL_OVERRIDE_ISTR("4,ubase:45,cbase:45,gptp:46", "UBL_GPTP");
	unibase_init(&init_para);
	gpnet=gptpnet_init(gptpnet_cb, NULL, netdevs, &np, NULL);

	return 0;
}

static int teardown(void **state)
{
	gptpnet_close(gpnet);
	unibase_close();
	return 0;
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_abnormal_events1),
		cmocka_unit_test(test_abnormal_events2),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
