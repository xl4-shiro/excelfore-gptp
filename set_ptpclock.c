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
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include "ll_gptpsupport.h"

#define PTPDEV_CLOCKFD 3
#define FD_TO_CLOCKID(ptpfd) ((~(clockid_t) (ptpfd) << 3) | PTPDEV_CLOCKFD)

static int print_usage(char *pname)
{
	char *s;
	if((s=strrchr(pname,'/'))==NULL)
		s=pname;
	else
		s++;
	printf("%s [options] [+/-]yyyy:mm:dd-HH:MM:SS\n", s);
	printf("-h|--help: this help\n");
	printf("-d|--ptpdev ptp_device (default:/dev/ptp0)\n");
	printf("\ne.g.\n");
	printf("# set to 2021 Jan. 1st 00:00:00\n");
	printf("  %s -d /dev/ptp1 2021:01:01-00:00:00\n", s);
	printf("# set 5 seconds ahead from the current, clock=/dev/ptp0 by default\n");
	printf("  %s +5\n", s);
	printf("# set 1 min 5 seconds behind from the current, clock=/dev/ptp0 by default\n");
	printf("  %s -- -01:10\n", s);
	return -1;
}

static int set_options(int argc, char *argv[], char **ptpdev)
{
	int oc;
	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"ptpdev", required_argument, 0, 'd'},
		{NULL, 0, 0, 0},
	};
	while((oc=getopt_long(argc, argv, "hd:", long_options, NULL))!=-1){
		switch(oc){
		case 'd':
			*ptpdev=optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			return 1;
		default:
			return print_usage(argv[0]);
		}
	}
	return 0;
}

static void print_tm(int ptpfd, struct timespec *nts)
{
	struct timespec ts;
	struct tm *ltm;
	char stime[64];

	if(!nts){
		clock_gettime(FD_TO_CLOCKID(ptpfd), &ts);
		nts=&ts;
	}
	ltm=localtime(&nts->tv_sec);
	strftime(stime, sizeof(stime), "%Y:%m:%d-%H:%M:%S", ltm);
	printf("%s\n", stime);
	return;
}

static int get_newts(int ptpfd, char *stv, struct timespec *nts)
{
	//struct timespec ts;
	int rv=0;
	int vv[6];
	int i,v,dn;
	char *nstv;
	int rt;
	struct tm *ltm;

	if(*stv=='+') rv=1;
	if(*stv=='-') rv=-1;
	if(rv) stv++;
	for(i=0;i<6;i++){
		v=strtol(stv, &nstv, 10);
		if(stv==nstv) break;
		vv[i]=v;
		stv=nstv+1;
	}
	dn=i;
	if(dn==0){
		printf("invalid string: %s\n", stv);
		return -1;
	}
	if(!rv){
		// set absolute time
		clock_gettime(FD_TO_CLOCKID(ptpfd), nts);
		ltm=localtime(&nts->tv_sec);
		if(dn>5) ltm->tm_year=vv[dn-6]-1900;
		if(dn>4) ltm->tm_mon=vv[dn-5]-1;
		if(dn>3) ltm->tm_mday=vv[dn-4];
		if(dn>2) ltm->tm_hour=vv[dn-3];
		if(dn>1) ltm->tm_min=vv[dn-2];
		if(dn>0) ltm->tm_sec=vv[dn-1];
		nts->tv_sec=mktime(ltm);
	}else{
		if(dn>3){
			printf("only HH:MM:SS is valide to set diff: %s\n", stv);
			return -1;
		}
		clock_gettime(FD_TO_CLOCKID(ptpfd), nts);
		rt=1;
		for(i=dn-1;i>=0;i--){
			nts->tv_sec+=rv*vv[i]*rt;
			rt*=60;
		}
	}
	return 0;
}


int main(int argc, char* argv[])
{
	char *ptpdev="/dev/ptp0";
	int ptpfd;

	if(set_options(argc, argv, &ptpdev)) return -1;
	ptpfd = open(ptpdev, O_RDWR);
	if(ptpfd<0){
		printf("can't open ptpdev=%s\n", ptpdev);
		return -1;
	}
	print_tm(ptpfd, NULL);
	if(argc>optind){
		struct timespec nts;
		get_newts(ptpfd, argv[optind], &nts);
		print_tm(ptpfd, &nts);
		clock_settime(FD_TO_CLOCKID(ptpfd), &nts);
	}

	close(ptpfd);
	return 0;
}
