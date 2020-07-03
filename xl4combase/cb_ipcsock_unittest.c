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
#include <xl4combase/combase.h>
#include <xl4unibase/unibase_binding.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#define UNIT_TESTING
#include <cmocka.h>
#include "cb_ipcsock.h"
#define UB_LOGCAT 2
#define IPC_TESTNODE "/tmp/cbipcsok_test"
#define IPC_TESTSVNODE "/tmp/cbipcsok_testsv"
#define IPC_TESTPORT_SERVER 7000

typedef struct testdata{
	bool udpmode;
	int clientnum;
	int stop;
	struct sockaddr_un adrun[MAX_IPC_CLIENTS+1];
	uint8_t csdata[MAX_IPC_CLIENTS+1][1024];
	int cssize[MAX_IPC_CLIENTS+1];
	int fd[MAX_IPC_CLIENTS+1];
	uint16_t clports[MAX_IPC_CLIENTS+1];
} testd_t;

static int ipcsrcb(void *cbdata, uint8_t *rdata, int size, struct sockaddr *adr)
{
	struct sockaddr_un *adrun=(struct sockaddr_un *)adr;
	struct sockaddr_in *adrin=(struct sockaddr_in *)adr;
	testd_t *testd=(testd_t *)cbdata;
	int suf;
	if(adr->sa_family==AF_UNIX){
		UB_LOG(UBL_INFO, "%s:size=%d, from client_node=%s\n", __func__, size, adrun->sun_path);
		assert_int_equal(sscanf(adrun->sun_path, IPC_TESTNODE"%d", &suf), 1);
	}else{
		UB_LOG(UBL_INFO, "%s:size=%d, from client_port=%d\n", __func__, size,
		       ntohs(adrin->sin_port));
		for(suf=0;suf<=MAX_IPC_CLIENTS;suf++)
			if(testd->clports[suf]==ntohs(adrin->sin_port)) break;
	}
	assert_true(suf>=0 && suf<=MAX_IPC_CLIENTS);
	assert_int_equal(size, testd->cssize[suf]);
	assert_memory_equal(rdata, testd->csdata[suf], size);
	memcpy(&testd->adrun[suf], adr, sizeof(struct sockaddr_un));
	if(rdata[0]==0xff) {
		UB_LOG(UBL_INFO, "%s: received 0xff\n", __func__);
		return -1;
	}
	return 0;
}

static void *client_thread1(void *arg)
{
	testd_t *testd=(testd_t *)arg;
	int res;
	int i;
	char suf[4];
	uint8_t sdata[1024];
	int cln=testd->clientnum;
	int stn=0;
	struct sockaddr_in addr;
	socklen_t addrlen=sizeof(addr);

	if(cln>MAX_IPC_CLIENTS) stn=cln-MAX_IPC_CLIENTS;
	for(i=stn;i<cln;i++){
		sprintf(suf, "%d", i);
		if(testd->udpmode){
			res=cb_ipcsocket_udp_init(&testd->fd[i], "127.0.0.1",
						  "127.0.0.1", IPC_TESTPORT_SERVER);
			assert_int_equal(res, 0);
			getsockname(testd->fd[i], (struct sockaddr *)&addr, &addrlen);
			UB_LOG(UBL_INFO, "open udp port, i=%d, PORT=%d\n", i, ntohs(addr.sin_port));
			testd->clports[i]=ntohs(addr.sin_port);
		}else{
			res=cb_ipcsocket_init(&testd->fd[i], IPC_TESTNODE, suf, IPC_TESTSVNODE);
			assert_int_equal(res, 0);
		}

		testd->cssize[i]=sizeof(testd->csdata[i]);
		memset(testd->csdata[i], i, testd->cssize[i]);
		res=write(testd->fd[i], testd->csdata[i], testd->cssize[i]);
		assert_int_equal(res, testd->cssize[i]);
	}

	// receive notice from the server
	memset(sdata, 0, sizeof(sdata));
	for(i=stn;i<cln;i++){
		res=read(testd->fd[i], testd->csdata[i], testd->cssize[i]);
		if(res<0) UB_LOG(UBL_ERROR, "%s:read i=%d, %s\n", __func__, i, strerror(errno));
		assert_int_equal(res, testd->cssize[i]);
		assert_memory_equal(testd->csdata[i], sdata, res);
	}

	// receive replying to each client from the server
	for(i=stn;i<cln;i++){
		res=read(testd->fd[i], testd->csdata[i], testd->cssize[i]);
		assert_int_equal(res, testd->cssize[i]);
	}
	for(i=stn;i<cln;i++){
		memset(sdata, i+1, sizeof(sdata));
		assert_int_equal(res, testd->cssize[i]);
	}

	while(!testd->stop) usleep(100000);
	for(i=stn;i<cln;i++){
		sprintf(suf, "%d", i);
		if(testd->udpmode){
			cb_ipcsocket_close(testd->fd[i], NULL, NULL);
		}else{
			cb_ipcsocket_close(testd->fd[i], IPC_TESTNODE, suf);
		}
	}
	return NULL;
}

static void test_2clients(int udpport)
{
	cb_ipcserverd_t *ipcsd;
	testd_t testd;
	CB_THREAD_T clthread;
	uint8_t sdata[1024];
	int ssize=1024;

	memset(&testd, 0, sizeof(testd));
	testd.clientnum=2;
	if(udpport){
		testd.udpmode=true;
		ipcsd=cb_ipcsocket_server_init("127.0.0.1", NULL, udpport);
	}else{
		ipcsd=cb_ipcsocket_server_init(IPC_TESTSVNODE, "", udpport);
	}
	assert_non_null(ipcsd);
        CB_THREAD_CREATE(&clthread, NULL, client_thread1, &testd);
	cb_ipcsocket_server_read(ipcsd, ipcsrcb, &testd);
	cb_ipcsocket_server_read(ipcsd, ipcsrcb, &testd);

	memset(sdata, 0, sizeof(sdata));
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, NULL); // send to all
	memset(sdata, 1, sizeof(sdata));
	// send to 1st client
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, (struct sockaddr *)&testd.adrun[0]);
	memset(sdata, 2, sizeof(sdata));
	// send to 2nd client
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, (struct sockaddr *)&testd.adrun[1]);

	testd.stop=1;
        CB_THREAD_JOIN(clthread, NULL);
	cb_ipcsocket_server_close(ipcsd);
}

static void test_ipcsock1(void **state)
{
	// connection test with 2 IPC clients
	test_2clients(0); // unix domain socket mode
	test_2clients(IPC_TESTPORT_SERVER); // udp socket mode
}

static void test_too_many_clients(void)
{
	cb_ipcserverd_t *ipcsd;
	testd_t testd;
	CB_THREAD_T clthread;
	uint8_t sdata[1024];
	int ssize=1024;
	int i;

	memset(&testd, 0, sizeof(testd));
	testd.clientnum=MAX_IPC_CLIENTS+1;
	ipcsd=cb_ipcsocket_server_init(IPC_TESTSVNODE, "", 0);
	assert_non_null(ipcsd);
        CB_THREAD_CREATE(&clthread, NULL, client_thread1, &testd);
	// the first connection must be removed
	for(i=1;i<testd.clientnum;i++){
		cb_ipcsocket_server_read(ipcsd, ipcsrcb, &testd);
		memset(sdata, i, sizeof(sdata));
		assert_memory_equal(sdata, testd.csdata[i], testd.cssize[i]);
	}

	memset(sdata, 0, sizeof(sdata));
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, NULL); // send to all
	for(i=1;i<testd.clientnum;i++){
		memset(sdata, i+1, sizeof(sdata));
		cb_ipcsocket_server_write(ipcsd, sdata, ssize, (struct sockaddr *)&testd.adrun[i]);
	}

	testd.stop=1;
        CB_THREAD_JOIN(clthread, NULL);
	cb_ipcsocket_server_close(ipcsd);
}

static void test_ipcsock2(void **state)
{
	test_too_many_clients();
}

static void *client_thread2(void *arg)
{
	testd_t *testd=(testd_t *)arg;
	int res;
	int fd;
	uint8_t rdata[1024];

	res=cb_ipcsocket_init(&fd, IPC_TESTNODE, "0", IPC_TESTSVNODE);
	assert_int_equal(res, 0);

	testd->cssize[0]=1024;
	memset(&testd->csdata[0], 0, sizeof(testd->csdata[0]));
	res=write(fd, testd->csdata[0], testd->cssize[0]);
	assert_int_equal(res, testd->cssize[0]);

	// receive notice from the server
	res=read(fd, rdata, 1024);
	assert_int_equal(res, 1024);

	// let the server disconnect this connection
	testd->csdata[0][0]=0xff;
	res=write(fd, testd->csdata[0], testd->cssize[0]);
	assert_int_equal(res, testd->cssize[0]);

	// 2nd notice shouldn't come, because it is disconnected
	res=cb_fdread_timeout(fd, rdata, 1024, 100);
	assert_int_equal(res, 0);

	// connect again
	testd->csdata[0][0]=0;
	res=write(fd, testd->csdata[0], testd->cssize[0]);
	assert_int_equal(res, testd->cssize[0]);

	// 3rd notice should come
	res=cb_fdread_timeout(fd, rdata, 1024, 100);
	assert_int_equal(res, 1024);

	while(!testd->stop) usleep(100000);
	cb_ipcsocket_close(fd, IPC_TESTNODE, "0");
	return NULL;
}

static void test_client_discon(void)
{
	cb_ipcserverd_t *ipcsd;
	testd_t testd;
	CB_THREAD_T clthread;
	uint8_t sdata[1024];
	int ssize=1024;

	memset(&testd, 0, sizeof(testd));
	ipcsd=cb_ipcsocket_server_init(IPC_TESTSVNODE, "", 0);
	assert_non_null(ipcsd);
        CB_THREAD_CREATE(&clthread, NULL, client_thread2, &testd);
	cb_ipcsocket_server_read(ipcsd, ipcsrcb, &testd);

	memset(sdata, 0, sizeof(sdata));
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, NULL); // send to all

	// this will receive 0xff and disconnect the client
	cb_ipcsocket_server_read(ipcsd, ipcsrcb, &testd);

	// 2nd notice
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, NULL); // send to all

	// re-connected by the data
	cb_ipcsocket_server_read(ipcsd, ipcsrcb, &testd);

	// 3rd notice
	cb_ipcsocket_server_write(ipcsd, sdata, ssize, NULL); // send to all

	testd.stop=1;
        CB_THREAD_JOIN(clthread, NULL);
	cb_ipcsocket_server_close(ipcsd);
}

static void test_ipcsock3(void **state)
{
	test_client_discon();
}

static int setup(void **state)
{
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr="4,ubase:45,combase:45,ipcsock:55";
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
		cmocka_unit_test(test_ipcsock1),
		cmocka_unit_test(test_ipcsock2),
		cmocka_unit_test(test_ipcsock3),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
