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
#include <xl4unibase/unibase_binding.h>
#include "gptpnet.h"
#include "gptpclock.h"
#include "mdeth.h"
#define MAX_PORTS_NUM 10

extern char *PTPMsgType_debug[];
extern char *gptpnet_event_debug[];
static const uint8_t port_id[]={0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7};

static void create_sample_packet(PTPMsgHeader *head)
{
	head->majorSdoId=1;
	head->messageType=SYNC;
	head->minorVersionPTP=1;
	head->versionPTP=2;
	head->messageLength=44;
	head->domainNumber=0;
	head->minorSdoId=0;
	head->flags[0]=0x2;
	head->flags[1]=0x0;
	head->correctionField=0;
	memset(head->messageTypeSpecific,0,4);
	memcpy(head->sourcePortIdentity.clockIdentity, port_id, 8);
	head->sourcePortIdentity.portNumber=0;
	head->sequenceId=1;
	head->control=0x5;
	head->logMessageInterval=0;
}

static int gptpnet_cb(void *cb_data, int portIndex, gptpnet_event_t event,
		      int64_t *event_ts, void *event_data)
{
	gptpnet_data_t *gpnet=*((gptpnet_data_t **)(cb_data));
	uint8_t *pdata;
	PTPMsgHeader head;
	static int64_t lastsend_ts;
	int ndevIndex=portIndex-1;

	switch(event){
	case GPTPNET_EVENT_NONE:
		break;
	case GPTPNET_EVENT_TIMEOUT:
		for(ndevIndex=0;ndevIndex<gptpnet_num_netdevs(gpnet);ndevIndex++){
			if(gptpnet_ptpdev(gpnet,ndevIndex)[0]) break;
		}
		if(ndevIndex>=gptpnet_num_netdevs(gpnet)) return 0;
		UB_LOG(UBL_DEBUG, "index=%d event=%s\n", ndevIndex, gptpnet_event_debug[event]);
		pdata=gptpnet_get_sendbuf(gpnet, ndevIndex);
		create_sample_packet(&head);
		memset(pdata, 0, 44);
		md_compose_head(&head, (MDPTPMsgHeader*)pdata);
		lastsend_ts=gptpclock_getts64(ndevIndex, 0);
		gptpnet_send(gpnet, ndevIndex, 44);
		return 0;
	case GPTPNET_EVENT_DEVUP:
	{
		event_data_netlink_t *ed=(event_data_netlink_t *)event_data;
		char *dup;
		switch(ed->duplex){
		case 1: dup="full"; break;
		case 2: dup="half"; break;
		default: dup="unknown"; break;
		}
		UB_LOG(UBL_INFO, "index=%d speed=%d, duplex=%s, ptpdev=%s\n",
		       ndevIndex, ed->speed, dup, ed->ptpdev);
		gptpclock_add_clock(ndevIndex, ed->ptpdev, 0, 0, ed->portid);
		return 0;
	}
	case GPTPNET_EVENT_DEVDOWN:
		break;
	case GPTPNET_EVENT_RECV:
	{
		event_data_recv_t *ed=(event_data_recv_t *)event_data;
		UB_LOG(UBL_INFO, "RECV: index=%d msgtype=%s, TS=%lldsec, %lldnsec\n",
		       ndevIndex, PTPMsgType_debug[ed->msgtype], ed->ts64/UB_SEC_NS,
		       ed->ts64%UB_SEC_NS);
		return 0;
	}
	case GPTPNET_EVENT_TXTS:
	{
		event_data_txts_t *ed=(event_data_txts_t *)event_data;
		int64_t ts64;
		ts64=ed->ts64-lastsend_ts;
		UB_LOG(UBL_INFO, "TXTS: index=%d msgtype=%s, TS=%lldsec, %lld"
		       "nsec, seqid=%d\n",
		       ndevIndex, PTPMsgType_debug[ed->msgtype], ts64/UB_SEC_NS,
		       ts64%UB_SEC_NS, ed->seqid);
		return 0;
	}
	}
	UB_LOG(UBL_INFO, "index=%d event=%s\n", ndevIndex, gptpnet_event_debug[event]);
	return 0;
}

static char ifname[5][IFNAMSIZ];
static int stopgptp;
static void signal_handler(int sig)
{
	stopgptp=1;
}

int main(int argc, char *argv[])
{
	struct ifaddrs *ifa;
	struct ifaddrs *ifad;
	int i=0, np;
	char *netdevs[MAX_PORTS_NUM];
	gptpnet_data_t *gpnet;
	struct sigaction sigact;
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr=UBL_OVERRIDE_ISTR("4,ubase:45,cbase:45,gptp:46", "UBL_GPTP");
	unibase_init(&init_para);

	sigact.sa_handler=signal_handler;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	getifaddrs(&ifa);

	for(ifad=ifa;ifad;ifad=ifad->ifa_next){
		if(ifad->ifa_addr->sa_family != AF_INET) continue;
		if(!strcmp(ifad->ifa_name, "lo")) continue;
		strcpy(&ifname[i++][0], ifad->ifa_name);
	}
	for(i=0;ifname[i][0];i++){
		printf("ifname=%s\n",&ifname[i][0]);
		netdevs[i]=&ifname[i][0];
	}
	netdevs[i]=NULL;

	gptpclock_init(1, MAX_PORTS_NUM);
	gpnet=gptpnet_init(gptpnet_cb, NULL, &gpnet, netdevs, &np, NULL);
	gptpnet_activate(gpnet);
	gptpnet_eventloop(gpnet, &stopgptp);

	freeifaddrs(ifa);
	UB_LOG(UBL_INFO, "exit program\n");
	unibase_close();
	return 0;
}
