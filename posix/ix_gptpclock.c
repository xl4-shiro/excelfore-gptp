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
#include <time.h>
#include <unistd.h>
#include "gptpclock.h"
#include "gptp_config.h"
#include "ll_gptpsupport.h"
#include "gptpclock_virtual.h"

#define PTPCLOCK_OPEN_TOUT 100 // msec
ptpclock_state_t gptp_get_ptpfd(char *ptpdev, PTPFD_TYPE *ptpfd)
{
	int toutmsec = PTPCLOCK_OPEN_TOUT;
	ptpclock_state_t state=PTPCLOCK_NOWORK;

#ifdef PTP_VIRTUAL_CLOCK_SUPPORT
	if (VIRTUAL_CLOCKNAME(ptpdev)) {
		*ptpfd = gptp_vclock_alloc_fd(ptpdev);
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
#endif //PTP_VIRTUAL_CLOCK_SUPPORT

	while(true){
		*ptpfd = ptpdev_clock_open(ptpdev, O_RDWR);
		if (PTPFD_VALID(*ptpfd)){
			state = PTPCLOCK_RDWR;
			break;
		}else if (errno==EACCES) {
			*ptpfd = ptpdev_clock_open(ptpdev, O_RDONLY);
			if (PTPFD_VALID(*ptpfd)){
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
			CB_USLEEP(10000);
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

int gptp_close_ptpfd(PTPFD_TYPE ptpfd)
{
#ifdef PTP_VIRTUAL_CLOCK_SUPPORT
	if (VIRTUAL_CLOCKFD(ptpfd)) {
		return gptp_vclock_free_fd(ptpfd);
	}
#endif //PTP_VIRTUAL_CLOCK_SUPPORT
	return ptpdev_clock_close(ptpfd);
}

int gptp_clock_adjtime(PTPFD_TYPE ptpfd, int adjppb)
{
#ifdef PTP_VIRTUAL_CLOCK_SUPPORT
	if (VIRTUAL_CLOCKFD(ptpfd)) {
		return gptp_vclock_adjtime(ptpfd, adjppb);
	}
#endif //PTP_VIRTUAL_CLOCK_SUPPORT
	return ptpdev_clock_adjtime(ptpfd, adjppb);
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
		ts64=CB_MKTIME(&tmv)*UB_SEC_NS;
	}
	gptpclock_setts64(ts64, clockIndex, domainNumber);
	UB_LOG(UBL_INFO,"set up time to %s\n",tstr);
	return 0;
}
