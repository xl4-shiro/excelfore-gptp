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
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include "gptpbasetypes.h"
#include "gptp_defaults.h"
#include "gptpipc.h"
#include "mdeth.h"
#include "md_abnormal_hooks.h"
#include <xl4unibase/unibase_binding.h>

static int main_terminate;

static int print_usage(char *pname)
{
	char *s;
	if((s=strchr(pname,'/'))==NULL) s=pname;
	ub_console_print("%s [options]\n", s);
	ub_console_print("-h|--help: this help\n");
	ub_console_print("-d|--domain domainIndex\n");
	ub_console_print("-p|--port portIndex\n");
	ub_console_print("-n|--node: print IPC node and exit\n");
	ub_console_print("-u|--udpport port_number: use UDP mode connection with port_number\n");
	ub_console_print("-a|--udpaddr IP address: use UDP mode connection with target IP\n");
	ub_console_print("-i|--interval query interval time(msec unit):\n");
	ub_console_print("-o|--oneshot: print one shot of messages and terminate program\n");
	return -1;
}

typedef struct gptpipcmon {
	int domainIndex;
	int portIndex;
	uint16_t udpport;
	char *udpaddr;
	int query_interval; //in msec
}gptpipcmon_t;

static int set_options(gptpipcmon_t *gipcmd, int argc, char *argv[])
{
	int oc;
	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"domain", required_argument, 0, 'd'},
		{"port", required_argument, 0, 'p'},
		{"node", no_argument, 0, 'n'},
		{"udpport", required_argument, 0, 'u'},
		{"udpaddr", required_argument, 0, 'a'},
		{"interval", required_argument, 0, 'i'},
		{"oneshot", no_argument, 0, 'o'},
	};
	while((oc=getopt_long(argc, argv, "hd:p:nu:a:i:o", long_options, NULL))!=-1){
		switch(oc){
		case 'd':
			gipcmd->domainIndex=strtol(optarg, NULL, 0);
			break;
		case 'p':
			gipcmd->portIndex=strtol(optarg, NULL, 0);
			break;
		case 'n':
			ub_console_print("%s\n", GPTP2D_IPC_CB_SOCKET_NODE);
			return 1;
		case 'u':
			gipcmd->udpport=strtol(optarg, NULL, 0);
			break;
		case 'a':
			gipcmd->udpaddr=optarg;
			break;
		case 'i':
			gipcmd->query_interval=strtol(optarg, NULL, 0);
			break;
		case 'o':
			main_terminate=1;
			break;
		case 'h':
		default:
			return print_usage(argv[0]);
		}
	}
	return 0;
}

static void signal_handler(int sig)
{
	main_terminate=1;
}

static void print_key_commands(char *rbuf)
{
	ub_console_print("bad request command:%s\n",rbuf);
	ub_console_print("    G domain,port : GM port info\n");
	ub_console_print("    N [domain],port : Network port info, "
			 "domain is not used\n");
	ub_console_print("    C domain,port : Clock info\n");
	ub_console_print("    T [domain,port] : Statistics info, "
			 "domain=-1:all domains, port=0:all ports\n");
	ub_console_print("    R [domain,port] : Reset Statistics info, "
			 "domain=-1:all domains, port=0:all ports\n");
	ub_console_print("    S: start TSN Scheduling\n");
	ub_console_print("    s: stop TSN Scheduling\n");
	ub_console_print("    A: domain,port,msgtype,eventtype,eventrate,"
			 "repeat,interval,eventpara: register an abnormal event\n");
	ub_console_print("       msgtype='none|sync|fup|pdreq|pdres|pdrfup|ann|sign'\n");
	ub_console_print("       eventtype='none|skip|dup|badsqn|nots|sender'\n");
	ub_console_print("       eventrate='0.0 to 1.0'\n");
	ub_console_print("       repeat: repeat times, 0 is infinite times\n");
	ub_console_print("       interval: interval when repeat > 1\n");
	ub_console_print("       eventpara: parameter for the event\n");
	ub_console_print("    a: deregister abnormal events\n");
}

static int ipc_register_abnormal_event(int ipcfd, char *rbuf)
{
	gptpipc_client_req_data_t cd;
	char *msgtype=NULL;
	char *eventtype=NULL;
	int res;
	int rval=-1;
	int i;

	const char *msgtstr[]={"none","sync","fup","pdreq","pdres",
			       "pdrfup","ann","sign",NULL};
	int32_t msgtint[]={-1,(int32_t)SYNC,(int32_t)FOLLOW_UP,(int32_t)PDELAY_REQ,
			   (int32_t)PDELAY_RESP,(int32_t)PDELAY_RESP_FOLLOW_UP,
			   (int32_t)ANNOUNCE, (int32_t)SIGNALING};
	const char *eventtstr[]={"none","skip","dup","badsqn","nots","sender"};
	int32_t eventtint[]={(int32_t)MD_ABN_EVENT_NONE,(int32_t)MD_ABN_EVENT_SKIP,
			     (int32_t)MD_ABN_EVENT_DUP,(int32_t)MD_ABN_EVENT_BADSEQN,
			     (int32_t)MD_ABN_EVENT_NOTS,(int32_t)MD_ABN_EVENT_SENDER};

	memset(&cd, 0, sizeof(cd));
	cd.cmd=GPTPIPC_CMD_REG_ABNORMAL_EVENT;
	if(rbuf[0]=='a') cd.abnd.subcmd=1;
	res=sscanf(rbuf+1, "%d,%d,%m[a-z],%m[a-z],%f,%d,%d,%d",
		   &cd.domainNumber, &cd.portIndex,
		   &msgtype, &eventtype,
		   &cd.abnd.eventrate, &cd.abnd.repeat, &cd.abnd.interval,
		   &cd.abnd.eventpara);
	if(res<4) {
		if(!cd.abnd.subcmd){
			UB_LOG(UBL_ERROR, "%s:number of parameters=%d is wrong\n",__func__,res);
			goto erexit;
		}
		cd.abnd.msgtype=-1;
	}
	if(res>=3){
		for(i=0;;i++){
			if(!msgtstr[i]) goto erexit;
			if(strcmp(msgtype,msgtstr[i])==0){
				cd.abnd.msgtype=msgtint[i];
				break;
			}
		}
	}
	if(res>=4){
		for(i=0;;i++){
			if(!eventtstr[i]) goto erexit;
			if(strcmp(eventtype,eventtstr[i])==0){
				cd.abnd.eventtype=eventtint[i];
				break;
			}
		}
	}
	if(res<5) cd.abnd.eventrate=1.0;
	if(res<6) cd.abnd.repeat=0;
	if(res<7) cd.abnd.interval=0;
	if(res<8) cd.abnd.eventpara=0;
	rval=0;
	res=write(ipcfd, &cd, sizeof(cd));
	if(res!=sizeof(cd)){
		UB_LOG(UBL_ERROR, "%s:can't send to the IPC socket\n",__func__);
	}
erexit:
	if(rval) print_key_commands(rbuf);
	if(msgtype) free(msgtype);
	if(eventtype) free(eventtype);
	return 0;
}

static int read_stdin(gptpipc_thread_data_t *ipctd)
{
	char rbuf[64];
	char *pb;
	int rsize;
	int domain=0;
	int port=0;
	rsize=read(STDIN_FILENO, rbuf, sizeof(rbuf)-1);
	if(rsize<0){
		UB_LOG(UBL_ERROR,"%s:stdin read error %s\n", __func__,strerror(errno));
		return 1;
	}
	pb=strchr(rbuf,'\n');
	if(pb) *pb=0;
	if(!strcmp(rbuf,"q")) return 1;
	if(rbuf[0]=='A' || rbuf[0]=='a')
		return ipc_register_abnormal_event(ipctd->ipcfd, rbuf);
	if(strchr(rbuf,',')){
		if(sscanf(rbuf+1,"%d,%d", &domain, &port)!=2){
			UB_LOG(UBL_WARN,"%s:bad format in 'domain,port'\n",__func__);
			return 0;
		}
	}else{
		if(sscanf(rbuf+1,"%d", &port)!=1) port=-1;
	}
	switch(rbuf[0]){
	case 'G':
		return send_ipc_request(ipctd->ipcfd, domain, port,
					GPTPIPC_CMD_REQ_GPORT_INFO);
	case 'N':
		return send_ipc_request(ipctd->ipcfd, domain, port,
					GPTPIPC_CMD_REQ_NDPORT_INFO);
	case 'C':
		return send_ipc_request(ipctd->ipcfd, domain, port,
					GPTPIPC_CMD_REQ_CLOCK_INFO);
	case 'T':
		return send_ipc_request(ipctd->ipcfd, domain, port,
					GPTPIPC_CMD_REQ_STAT_INFO);
	case 'R':
		return send_ipc_request(ipctd->ipcfd, domain, port,
					GPTPIPC_CMD_REQ_STAT_INFO_RESET);
	case 'S':
		return send_ipc_request(ipctd->ipcfd, 1, 0,
					GPTPIPC_CMD_TSN_SCHEDULE_CONTROL);
	case 's':
		return send_ipc_request(ipctd->ipcfd, 0, 0,
					GPTPIPC_CMD_TSN_SCHEDULE_CONTROL);
	default:
		print_key_commands(rbuf);
	}
	return 0;
}

static int ipcmon_notice_cb(gptpipc_gptpd_data_t *ipcrd, void *ptr)
{
	//printf("%s:domainNumber=%d, portIndex=%d, flag=0x%08X\n", __func__,
	//       notice->domainNumber, notice->portIndex, notice->event_flags);
	return 0;
}

static int ipc_reconnect(gptpipc_thread_data_t *ipctd, bool *ipcrun)
{
	if(*ipcrun){
		gptpipc_close(ipctd);
		*ipcrun=false;
	}
	while(gptpipc_init(ipctd, 1)){
		UB_TLOG(UBL_ERROR, "gptpipc can't be connected in 1 seconds, try again\n");
		gptpipc_close(ipctd);
		if(main_terminate) return -1;
	}
	*ipcrun=true;
	return 0;
}

int main(int argc, char *argv[])
{
	gptpipcmon_t gipcmd;
	int res;
	struct sigaction sigact;
	char suffix[64];
	uint64_t ts64;
	gptpipc_thread_data_t ipctd;
	bool ipcrun=false;
	unibase_init_para_t init_para;

	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr=UBL_OVERRIDE_ISTR("4,ubase:45,cbase:45,gptp:40", "UBL_GPTP");
	unibase_init(&init_para);
	memset(&gipcmd, 0, sizeof(gipcmd));
	memset(&ipctd, 0, sizeof(ipctd));
	gipcmd.domainIndex=-1;
	gipcmd.portIndex=-1;
	gipcmd.query_interval=10000;
	sigact.sa_handler=signal_handler;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	res=set_options(&gipcmd, argc, argv);
	if(res<0) return -1;
	if(res) return 0;

	ts64=ub_rt_gettime64();
	sprintf(suffix, "%"PRIi64, ts64);
	ipctd.pname=suffix;
	ipctd.printdata=true;
	ipctd.query_interval=gipcmd.query_interval;
	ipctd.cb=ipcmon_notice_cb;
	ipctd.cbdata=&ipctd;
	ipctd.udpport=gipcmd.udpport;
	ipctd.udpaddr=gipcmd.udpaddr;
	if(ipc_reconnect(&ipctd, &ipcrun)) return -1;
	if(gipcmd.domainIndex>0){
		send_ipc_request(ipctd.ipcfd, gipcmd.domainIndex, 1, GPTPIPC_CMD_REQ_GPORT_INFO);
		send_ipc_request(ipctd.ipcfd, gipcmd.domainIndex, 0, GPTPIPC_CMD_REQ_CLOCK_INFO);
		send_ipc_request(ipctd.ipcfd, gipcmd.domainIndex, 1, GPTPIPC_CMD_REQ_CLOCK_INFO);
		usleep(100000);
	}
	while(!main_terminate){
		res=read_stdin(&ipctd);
		if(res==1) break;
		if(res==-1) {
			if(ipc_reconnect(&ipctd, &ipcrun)) return -1;
		}
	}
	if(ipcrun) {
		send_ipc_request(ipctd.ipcfd, 0, 0, GPTPIPC_CMD_DISCONNECT);
		gptpipc_close(&ipctd);
	}
	return 0;
}
