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
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>
#include "gptpclock.h"

typedef struct ptpfd_index {
	const char *ptpdev;
	int fd;
	int clockid;
	int64_t phase_adj;
	int freq_adj;
	uint64_t lastts;
	uint64_t lastpts;
} ptpfd_index_t;

#define MAX_PTPFD_NUM 4
static ptpfd_index_t ptpd[5]={
	{GPTP_VIRTUAL_PTPDEV_PREFIX"0", 10, FD_TO_CLOCKID(10), 0, 0},
	{GPTP_VIRTUAL_PTPDEV_PREFIX"1", 11, FD_TO_CLOCKID(11), 0, 0},
	{GPTP_VIRTUAL_PTPDEV_PREFIX"2", 12, FD_TO_CLOCKID(12), 0, 0},
	{GPTP_VIRTUAL_PTPDEV_PREFIX"3", 13, FD_TO_CLOCKID(13), 0, 0},
	{NULL, -1, 0, 0},
};

static int ptpd_index_from_ptpfd(int ptpfd)
{
	int i;
	for(i=0;i<MAX_PTPFD_NUM;i++){
		if(ptpd[i].fd==ptpfd) return i;
	}
	return -1;
}

static int ptpd_index_from_ptpdev(char *ptpdev)
{
	int i;
	for(i=0;i<MAX_PTPFD_NUM;i++){
		if(!strcmp(ptpd[i].ptpdev, ptpdev)) return i;
	}
	return -1;
}

int __wrap_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	int i;
	uint64_t ts64;
	int64_t dts64, dpts64;
	switch(clk_id){
	case CLOCK_REALTIME:
		ts64=ub_rt_gettime64();
		UB_NSEC2TS(ts64, *tp);
		return 0;
	case CLOCK_MONOTONIC:
		ts64=ub_mt_gettime64();
		UB_NSEC2TS(ts64, *tp);
		return 0;
	default:
		for(i=0;i<MAX_PTPFD_NUM;i++){
			if(ptpd[i].clockid!=clk_id) continue;
			ts64=ub_mt_gettime64();
			if(!ptpd[i].lastts){
				ptpd[i].lastpts=ts64;
				ptpd[i].lastts=ts64;
				UB_NSEC2TS(ts64, *tp);
				return 0;
			}
			dts64=ts64-ptpd[i].lastts;
			dpts64=dts64*ptpd[i].freq_adj/UB_SEC_NS;
			ptpd[i].lastts=ts64;
			ts64=ptpd[i].lastpts+dts64+dpts64;
			ptpd[i].lastpts=ts64;
			UB_NSEC2TS(ts64, *tp);
			return 0;
		}
	}
	return -1;
}

int __wrap_clock_settime(clockid_t clk_id, const struct timespec *tp)
{
	int i;
	uint64_t ts64;
	for(i=0;i<MAX_PTPFD_NUM;i++){
		if(ptpd[i].clockid!=clk_id) continue;
		ts64=UB_TS2NSEC(*tp);
		ptpd[i].lastpts=ts64;
		ptpd[i].lastts=ub_mt_gettime64();
		return 0;
	}
	return -1;
}

int __wrap_gptp_clock_adjtime(int ptpfd, int adjppb)
{
	int i;
	if((i=ptpd_index_from_ptpfd(ptpfd))<0) return -1;
	ptpd[i].freq_adj=adjppb;
	return 0;
}

#define PTPCLOCK_OPEN_TOUT 100 // msec
ptpclock_state_t __wrap_gptp_get_ptpfd(char *ptpdev, int *ptpfd)
{
	int i;
	if((i=ptpd_index_from_ptpdev(ptpdev))<0) return PTPCLOCK_NOWORK;
	*ptpfd=ptpd[i].fd;
	return (ptpclock_state_t)mock();
}

int __wrap_gptp_close_ptpfd(int ptpfd)
{
	if(ptpd_index_from_ptpfd(ptpfd)<0) return -1;
	return 0;
}
