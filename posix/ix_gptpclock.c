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
#include <errno.h>
#include <stdio.h>
#include "gptpclock.h"
#include "gptp_config.h"

/* When glibc offers the syscall, this will go away. */
#include <sys/timex.h>
#include <sys/syscall.h>

#define PTPVDEV_MAX_NAME 16
typedef struct ptpfd_virtual {
	char ptpdev[PTPVDEV_MAX_NAME];
	int fd;
	int freq_adj;
	uint64_t lastts;
	uint64_t lastpts;
	bool rdwr_mode;
	int users;
} ptpfd_virtual_t;
ub_esarray_cstd_t *esvptpd;

#define MAX_VPTPD (GPTP_VIRTUAL_PTPDEV_FDMAX-GPTP_VIRTUAL_PTPDEV_FDBASE+1)

static ptpfd_virtual_t *get_ptpfd_virtual(int ptpfd)
{
	int i;
	ptpfd_virtual_t *pv;
	int en=ub_esarray_ele_nums(esvptpd);
	for(i=0;i<en;i++){
		pv=(ptpfd_virtual_t *)ub_esarray_get_ele(esvptpd, i);
		if(ptpfd==pv->fd) return pv;
	}
	UB_LOG(UBL_ERROR,"%s:ptpfd=%d is not opened\n", __func__, ptpfd);
	return NULL;
}

static int clock_adjtime(clockid_t id, struct timex *tx)
{
	return syscall(__NR_clock_adjtime, id, tx);
}

int gptp_clock_adjtime(int ptpfd, int adjppb)
{
	ptpfd_virtual_t *pv;
	struct timex tmx;
	if(ptpfd<GPTP_VIRTUAL_PTPDEV_FDBASE || ptpfd>GPTP_VIRTUAL_PTPDEV_FDMAX){
		memset(&tmx, 0, sizeof(tmx));
		tmx.modes=ADJ_FREQUENCY;
		tmx.freq=(long)(adjppb * 65.536);
		return clock_adjtime(FD_TO_CLOCKID(ptpfd), &tmx);
	}
	pv=get_ptpfd_virtual(ptpfd);
	if(!pv) return -1;
	if(!pv->rdwr_mode) return -1;
	pv->freq_adj=adjppb;
	return 0;
}

int gptpclock_settime_ptpvfd(int ptpfd, uint64_t ts64)
{
	ptpfd_virtual_t *pv=get_ptpfd_virtual(ptpfd);
	if(!pv) return -1;
	if(!pv->rdwr_mode) return -1;
	pv->lastpts=ts64;
	pv->lastts=ub_rt_gettime64();
	return 0;
}

uint64_t gptpclock_gettime_ptpvfd(int ptpfd)
{
	uint64_t ts64;
	int64_t dts64, dpts64;
	ptpfd_virtual_t *pv=get_ptpfd_virtual(ptpfd);
	if(!pv) return 0;
	ts64=ub_rt_gettime64();
	if(!pv->lastts){
		pv->lastpts=ts64;
		pv->lastts=ts64;
		return ts64;
	}
	dts64=ts64-pv->lastts;
	if(pv->rdwr_mode)
		dpts64=dts64*(pv->freq_adj+gptpconf_get_intitem(CONF_PTPVFD_CLOCK_RATE))/UB_SEC_NS;
	else
		dpts64=dts64*(gptpconf_get_intitem(CONF_PTPVFD_CLOCK_RATE))/UB_SEC_NS;
	pv->lastts=ts64;
	ts64=pv->lastpts+dts64+dpts64;
	pv->lastpts=ts64;
	return ts64;
}

static int get_ptpvfd(char *ptpdev)
{
	int i,en;
	ptpfd_virtual_t *pv, *rpv;
	int fdmax=GPTP_VIRTUAL_PTPDEV_FDBASE-1;
	if(!esvptpd){
		esvptpd=ub_esarray_init(4, sizeof(ptpfd_virtual_t), MAX_VPTPD);
	}
	en=ub_esarray_ele_nums(esvptpd);
	for(i=0;i<en;i++){
		pv=(ptpfd_virtual_t *)ub_esarray_get_ele(esvptpd, i);
		if(!strcmp(pv->ptpdev, ptpdev)) {
			(pv->users)++;
			return pv->fd;
		}
		fdmax=UB_MAX(pv->fd, fdmax);
	}
	rpv=(ptpfd_virtual_t *)ub_esarray_get_newele(esvptpd);
	memset(rpv, 0, sizeof(ptpfd_virtual_t));
	strncpy(rpv->ptpdev, ptpdev, PTPVDEV_MAX_NAME-1);
	rpv->fd=fdmax+1;
	rpv->users=1;
	if(ptpdev[strlen(CB_VIRTUAL_PTPDEV_PREFIX)]=='w') rpv->rdwr_mode=true;
	if(rpv->fd>GPTP_VIRTUAL_PTPDEV_FDMAX){
		ub_esarray_del_pointer(esvptpd, (ub_esarray_element_t *)rpv);
		return -1;
	}
	return rpv->fd;
}

#define PTPCLOCK_OPEN_TOUT 100 // msec
ptpclock_state_t gptp_get_ptpfd(char *ptpdev, int *ptpfd)
{
	int toutmsec = PTPCLOCK_OPEN_TOUT;
	ptpclock_state_t state;
	if(strstr(ptpdev, CB_VIRTUAL_PTPDEV_PREFIX)==ptpdev){
		*ptpfd = get_ptpvfd(ptpdev);
		if(*ptpfd<0) {
			UB_LOG(UBL_ERROR,"%s:no access on %s\n", __func__, ptpdev);
			return PTPCLOCK_NOACCESS;
		}
		/* virtual ptp clock has to use PTPCLOCK_SLAVE_SUB to support
		 * the clock value for the other processes through gptpmasterclock.c
		 * So, regularly this returns PTPCLOCK_RDONLY
		 * For testing purpose, add 'w' as the first character of the suffix
		 * then this returns PTPCLOCK_RDWR, and the adjustment parameters are
		 * maintained in this layer. */
		if(ptpdev[strlen(CB_VIRTUAL_PTPDEV_PREFIX)]=='w') return PTPCLOCK_RDWR;
		return PTPCLOCK_RDONLY;
	}

	while(true){
		*ptpfd = open(ptpdev, O_RDWR);
		if (*ptpfd >= 0){
			state = PTPCLOCK_RDWR;
			break;
		}else if (errno==EACCES) {
			*ptpfd = open(ptpdev, O_RDONLY);
			if (*ptpfd >= 0){
				UB_LOG(UBL_INFO,"%s:opening %s in READONLY mode\n",
				       __func__, ptpdev);
				state = PTPCLOCK_RDONLY;
				break;
			}else if (errno==EACCES) {
				UB_LOG(UBL_ERROR,"%s:no access on %s\n", __func__, ptpdev);
				state = PTPCLOCK_NOACCESS;
				break;
			}
		}
		if(toutmsec>0){
			// sleep 10msec and try again
			usleep(10000);
			toutmsec-=10;
			continue;
		}
		state = PTPCLOCK_NOWORK;
		UB_LOG(UBL_ERROR,"%s:can't open ptpdev=\"%s\":%s\n",
		       __func__, ptpdev, strerror(errno));
		break;
	}
	return state;
}

int gptp_close_ptpfd(int ptpfd)
{
	ptpfd_virtual_t *pv;
	if(ptpfd>=GPTP_VIRTUAL_PTPDEV_FDBASE && ptpfd<=GPTP_VIRTUAL_PTPDEV_FDMAX){
		pv=get_ptpfd_virtual(ptpfd);
		if(!pv){
			UB_LOG(UBL_ERROR,"%s:ptpfd=%d is not opened\n", __func__, ptpfd);
			return -1;
		}
		if(--(pv->users)==0){
			ub_esarray_del_pointer(esvptpd, (ub_esarray_element_t *)pv);
			if(ub_esarray_ele_nums(esvptpd)==0){
				ub_esarray_close(esvptpd);
				esvptpd=NULL;
			}
		}
		return 0;
	}
	return close(ptpfd);
}

int gptpclock_settime_str(char *tstr, int clockIndex, uint8_t domainNumber)
{
	struct tm tmv;
	int64_t ts64;
	int i;
	if(!strcmp(tstr,"sysclk")){
		ts64=ub_rt_gettime64();
	}else{
		memset(&tmv,0,sizeof(tmv));
		i=sscanf(tstr,"%d:%d:%d:%d:%d:%d",
			 &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday,
			 &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec);
		if(i!=6){
			UB_LOG(UBL_ERROR,"time format is wrong in '%s'\n", tstr);
			return -1;
		}
		tmv.tm_year-=1900;
		tmv.tm_mon-=1;
		ts64=mktime(&tmv)*UB_SEC_NS;
	}
	gptpclock_setts64(ts64, clockIndex, domainNumber);
	UB_LOG(UBL_INFO,"set up time to %s\n",tstr);
	return 0;
}
