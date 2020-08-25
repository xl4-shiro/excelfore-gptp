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
#include <signal.h>
#include <sys/stat.h>
#include <xl4unibase/unibase_binding.h>
#include <fcntl.h>
#include <stdio.h>
#include "gptpclock.h"
#include "gptpnet.h"
#include "mdeth.h"
#include "gptpman.h"
#include <getopt.h>
#include "gptp_config.h"

extern int gptpconf_values_test(void);

typedef struct gptpdpd {
	char **netdevs;
} gptpdpd_t;

typedef struct gptpoptd {
	char *devlist;
	char *conf_file;
	int netdnum;
	int domain_num;
	char *inittm;
} gptpoptd_t;

static int print_usage(char *pname, gptpoptd_t *gpoptd)
{
	char *s;
	if((s=strchr(pname,'/'))==NULL) s=pname;
	ub_console_print("%s [options]\n", s);
	ub_console_print("-h|--help: this help\n");
	ub_console_print("-d|--devs \"eth0,eth1,...\": comma separated network devices\n");
	ub_console_print("-n|--dnum number: max number of network devices\n");
	ub_console_print("-m|--domain number: max number of domain\n");
	ub_console_print("-c|--conf: config file\n");
	ub_console_print("-t|--inittm year:month:date:hour:minute:second: set initial time\n");
	return -1;
}

static int set_options(gptpoptd_t *gpoptd, int argc, char *argv[])
{
	int oc;
	int res;
	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"devs", required_argument, 0, 'd'},
		{"dnum", required_argument, 0, 'n'},
		{"domain", required_argument, 0, 'm'},
		{"conf", required_argument, 0, 'c'},
		{"inittm", required_argument, 0, 't'},
		{NULL, 0, 0, 0},
	};
	while((oc=getopt_long(argc, argv, "hd:n:c:m:t:", long_options, NULL))!=-1){
		switch(oc){
		case 'd':
			gpoptd->devlist=optarg;
			break;
		case 'c':
			gpoptd->conf_file=optarg;
			break;
		case 'n':
			gpoptd->netdnum=strtol(optarg, NULL, 0);
			break;
		case 'm':
			gpoptd->domain_num=strtol(optarg, NULL, 0);
			break;
		case 't':
			gpoptd->inittm=optarg;
			break;
		case 'h':
		default:
			return print_usage(argv[0], gpoptd);
		}
	}
	res=optind;
	optind=0;
	return res;
}

static void debug_memory_fileout()
{
	char *buf;
	int size;
	char *fname;
	int fd;
	if(ubb_memory_out_alldata(&buf, &size)) return;
	fname=gptpconf_get_item(CONF_DEBUGLOG_MEMORY_FILE);
	if(!fname || !fname[0]) goto erexit;
	fd=open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if(fd<0) {
		UB_LOG(UBL_ERROR, "%s:can't open file:%s\n", __func__, fname);
		goto erexit;
	}
	if(write(fd, buf, size)!=size){
		UB_LOG(UBL_ERROR, "%s:can't write into the file\n", __func__);
	}
	close(fd);
erexit:
	free(buf);
	return;
}

#ifndef GPTP2_IN_LIBRARY
#define GPTP2D_MAIN main
#endif

int GPTP2D_MAIN(int argc, char *argv[])
{
	gptpdpd_t gpdpd;
	gptpoptd_t gpoptd;
	int i, j, k;
	netdevname_t *netdevs;
	ptpdevname_t *ptpdevs;
	int res=-1;
	mode_t oumask;
	unibase_init_para_t init_para;
	ubb_default_initpara(&init_para);
	init_para.ub_log_initstr=UBL_OVERRIDE_ISTR("4,ubase:45,cbase:45,gptp:46", "UBL_GPTP");
	unibase_init(&init_para);
	ubb_memory_out_init(NULL, 0);// start with zero, so that the memory is not allocated

	if(gptpconf_values_test()) return -1;
	oumask=umask(011);
	memset(&gpdpd, 0, sizeof(gpdpd));
	memset(&gpoptd, 0, sizeof(gpoptd));
	if(set_options(&gpoptd, argc, argv)<0) MAIN_RETURN(-1);
	if(gpoptd.conf_file) ub_read_config_file(gpoptd.conf_file, gptpconf_set_stritem);
	ubb_memory_out_init(NULL, gptpconf_get_intitem(CONF_DEBUGLOG_MEMORY_SIZE)*1024);
	UB_LOG(UBL_INFO, "gptp2d: gptp2-"XL4PKGVERSION"\n");
	if(!gpoptd.netdnum) gpoptd.netdnum=gptpconf_get_intitem(CONF_MAX_PORT_NUMBER);
	if(!gpoptd.domain_num) gpoptd.domain_num=gptpconf_get_intitem(CONF_MAX_DOMAIN_NUMBER);
	netdevs=malloc(gpoptd.netdnum * sizeof(netdevname_t));
	ptpdevs=malloc(gpoptd.netdnum * sizeof(ptpdevname_t));
	gpdpd.netdevs=malloc((gpoptd.netdnum+1) * sizeof(char *));

	if(!gpoptd.devlist){
		gpoptd.netdnum = cb_get_all_netdevs(gpoptd.netdnum, netdevs);
		for(i=0,j=0;i<gpoptd.netdnum;i++){
			if(!cb_get_ptpdev_from_netdev(netdevs[i], ptpdevs[i]))
				gpdpd.netdevs[j++]=netdevs[i];
		}
		gpdpd.netdevs[j]=0;
	}else{
		j=0;
		gpdpd.netdevs[j++]=gpoptd.devlist;
		k=strlen(gpoptd.devlist);
		for(i=0;i<k;i++){
			if(gpoptd.devlist[i]!=',') continue;
			gpoptd.devlist[i++]=0;
			if(j>=gpoptd.netdnum) break;
			gpdpd.netdevs[j++]=gpoptd.devlist+i;
			continue;
		}
		gpdpd.netdevs[j]=0;
		gpoptd.netdnum=j;
	}
	for(i=0;i<gpoptd.netdnum;i++){
		if(!gpdpd.netdevs[i]) break;
		UB_LOG(UBL_DEBUG, "use network device:%s\n", gpdpd.netdevs[i]);
	}
	gptpconf_values_test();
	gptpman_run(gpdpd.netdevs, gpoptd.netdnum, gpoptd.domain_num, gpoptd.inittm);
	UB_LOG(UBL_INFO,"gptp2d going to close\n");
	res=0;
	free(gpdpd.netdevs);
	free(ptpdevs);
	free(netdevs);
	debug_memory_fileout();
	umask(oumask);
	ubb_memory_out_close();
	unibase_close();
	return res;
}
