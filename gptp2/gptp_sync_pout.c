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
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <xl4unibase/unibase_binding.h>
#include "gptpmasterclock.h"

#define DEFAULT_SETUPTIME 1000 /* 1uSec */
#define DEFAULT_WAKEUPTIME 200000 /* 200uSec */
#define DEFAULT_INTERVAL 100000000 /* 100msec */
#define GOOD_TIMING_RANGE 100 /* nsec */
#define BAD_TIMING_THRESH 10000 /* 10 usec */

static int pout_prepare(int portn)
{
	int fd;
	char *dev="/sys/class/gpio/export";
	char gpstr[16];

	sprintf(gpstr, "%d", portn);
	if((fd=open(dev, O_WRONLY))<0){
		UB_LOG(UBL_ERROR, "%s:can't open %s:%s\n", __func__, dev, strerror(errno));
		return -1;
	}
	if(write(fd, gpstr, strlen(gpstr))<0){
		UB_LOG(UBL_ERROR, "%s:can't write:%s\n", __func__, strerror(errno));
	}
	close(fd);
	return 0;
}

static int pout_set_direction(int out, int portn)
{
	int fd=0;
	char gpstr[64];
	int res;

	sprintf(gpstr, "/sys/class/gpio/gpio%d/direction", portn);
	if((fd=open(gpstr, O_RDWR))<0){
		UB_LOG(UBL_ERROR, "%s:can't open %s:%s\n", __func__, gpstr, strerror(errno));
		return -1;
	}
	if(out)
		res=write(fd,"out",3);
	else
		res=write(fd,"in",2);
	if(res<0){
		UB_LOG(UBL_ERROR, "%s:can't write:%s\n", __func__, strerror(errno));
		return -1;
	}
	close(fd);
	return 0;
}

static int gpiofd;
static int sysfs_pout_init(int portn)
{
	char gpstr[64];
	if(pout_prepare(portn)) return -1;
	pout_set_direction(1, portn);
	sprintf(gpstr, "/sys/class/gpio/gpio%d/value", portn);
	if((gpiofd=open(gpstr, O_RDWR))<0){
		UB_LOG(UBL_ERROR, "%s:can't open %s:%s\n", __func__, gpstr, strerror(errno));
		return -1;
	}
	return 0;
}

static int sysfs_pout_write(int d)
{
	if(d)
		return write(gpiofd,"1",1);
	else
		return write(gpiofd,"0",1);
}

static int sysfs_pout_close(int portn)
{
	if(gpiofd>0) close(gpiofd);
	gpiofd=0;
	pout_set_direction(0, portn);
	return 0;
}

#define BASEPORT 0x378 /* lp1, 0x3bc, 0x278 */
static int pcprt_pout_init()
{
	//set permissions to access port
	if (ioperm(BASEPORT, 3, 1)){
		UB_LOG(UBL_ERROR, "%s:ioperm:%s\n", __func__, strerror(errno));
		return -1;
	}
	return 0;
}

static int pcprt_pout_close()
{
	if (ioperm(BASEPORT, 3, 0)) {
		UB_LOG(UBL_ERROR, "%s:ioperm:%s\n", __func__, strerror(errno));
		return -1;
	}
	return 0;
}

static void pcprt_pout_write(int d)
{
	outb(d, BASEPORT);
}


static int stoplpout=0;
static void getout(int sig)
{
	UB_LOG(UBL_INFO, "going to terminate the process\n");
	stoplpout=1;
}


static int lpout(int setup, int wakeup, int interval, int portn)
{
	struct timespec ts1, ts2;
	int64_t cts64, ts64; // current time
	int64_t tgts64; // target time
	int64_t tsiv64=interval;
	int64_t last_tgts64=0;
	bool over_run=false;

	if(portn>=0){
		if(sysfs_pout_init(portn)) return -1;
	}else{
		if(pcprt_pout_init()) return -1;
	}
	while (stoplpout==0) {
		cts64=gptpmasterclock_getts64();
		tgts64=cts64+tsiv64;
		tgts64=(tgts64/interval)*interval; // align to the interval time
		ts64=tgts64-cts64;

		while(ts64>wakeup){
			ts64-=wakeup;
			memset(&ts2,0,sizeof(ts2));
			UB_LOG(UBL_DEBUGV, "sleep for %"PRIi64"nsec\n",ts64);
			UB_NSEC2TS(ts64, ts1);
			if(clock_nanosleep(CLOCK_REALTIME,0,&ts1,&ts2)){
				UB_LOG(UBL_INFO, "sleep failed %s\n", strerror(errno));
				cts64=gptpmasterclock_getts64();
				ts64=tgts64-cts64;
				if(ts64<=0) break;
			}else{
				break;
			}
		}

		cts64=gptpmasterclock_getts64();
		ts64=tgts64-cts64;
		if(ts64<0){
			over_run=true;
			UB_LOG(UBL_DEBUGV, "over run by sleep, %"PRIi64" nsec\n", ts64);
		}else{
			UB_LOG(UBL_DEBUGV, "spin for %"PRIi64"nsec\n",ts64);
			while(ts64>setup){
				cts64=gptpmasterclock_getts64();
				ts64=tgts64-cts64;
			}
		}
		cts64=gptpmasterclock_getts64();
		// rising edge
		if(portn>=0){
			sysfs_pout_write(1);
		}else{
			pcprt_pout_write(0xff);
		}
		ts64=gptpmasterclock_getts64();
		ts64=ts64-cts64;
		ts64 /= 2;
		ts64 += cts64; // ts64 = the center btw ts64 and cts
		ts64 -= tgts64; // ts64 = diff to target time
		if(over_run){
			UB_LOG(UBL_INFO, "sleep over run happened, %s %"PRIi64"nsec away\n",
			       ts64<0?"-":"+", ts64);
			goto fedge;
		}else if(ts64 > BAD_TIMING_THRESH){
			UB_LOG(UBL_INFO, "the result time was too bad, %s %"PRIi64"nsec away\n",
			       ts64<0?"-":"+", ts64);
			goto fedge;
		}
		if(ts64 < GOOD_TIMING_RANGE){
			UB_LOG(UBL_DEBUG, "good result time, %s %"PRIi64"nsec away\n",
			       ts64<0?"-":"+", ts64);
			goto fedge;
		}

		UB_LOG(UBL_DEBUG, "the result time, %s %"PRIi64"nsec away\n",
		       ts64<0?"-":"+", ts64);
		if(ts64<0){
			// move to later timing, shorten 'setup'
			setup -= ts64/2;
			if(setup<0) setup=0;
		}else{
			// move to earlier timing, lengthen 'setup'
			setup += ts64/2;
		}
		if(tgts64-last_tgts64>UB_SEC_NS){
			UB_LOG(UBL_INFO, "new setup = %d\n",setup);
		}
		last_tgts64=tgts64;

	fedge:
		usleep(tsiv64/10000); // duty 10% pulse
		// falling edge
		if(portn>=0){
			sysfs_pout_write(0);
		}else{
			pcprt_pout_write(0);
		}
	}

	if(portn>=0){
		sysfs_pout_write(0);
		sysfs_pout_close(portn);
	}else{
		pcprt_pout_write(0);
		pcprt_pout_close();
	}
	return 0;
}

static int print_usage(const char *argv0)
{
	UB_LOG(UBL_INFO, "%s [-s setup_time][-w wakeup_time][-i interval_time(in msec)]\n", argv0);
	UB_LOG(UBL_INFO, "      [-n sysfs gpio port number, if not defined use Parallel port]\n");
	UB_LOG(UBL_INFO, "BBB: -n 60 -w 600000\n");
	return -1;
}

int main(int argc, char* argv[])
{

	int setup=DEFAULT_SETUPTIME; /* hit the port in advance of this time */
	int wakeup=DEFAULT_WAKEUPTIME; /* wake up the process this time before the target time */
	int interval=DEFAULT_INTERVAL; /* pulse interval time in nsec */
	int opt;
	int portn=-1;
	unibase_init_para_t init_para;

	ubb_default_initpara(&init_para);
	unibase_init(&init_para);
	gptpmasterclock_init(NULL);
	while ((opt = getopt(argc, (char **)argv, "hs:w:i:n:")) != -1){
		switch (opt){
		case 's':
			setup=atoi(optarg);
			break;
		case 'n':
			portn=atoi(optarg);
			break;
		case 'w':
			wakeup=atoi(optarg);
			break;
		case 'i':
			interval=atoi(optarg)*1000000;
			break;
		case 'h':
			return print_usage(argv[0]);
		}
	}

	signal(SIGINT, getout);
	lpout(setup, wakeup, interval, portn);
	gptpmasterclock_close();

	return 0;
}
