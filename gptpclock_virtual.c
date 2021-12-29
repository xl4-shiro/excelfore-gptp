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
#include "gptpclock_virtual.h"
#include "gptpclock.h"
#include "gptp_config.h"

#define PTPVDEV_MAX_NAME 16
#define MAX_VPTPD (GPTP_VIRTUAL_PTPDEV_FDMAX-GPTP_VIRTUAL_PTPDEV_FDBASE+1)

typedef struct ptpfd_virtual {
	char ptpdev[PTPVDEV_MAX_NAME];
	PTPFD_TYPE fd;
	int freq_adj;
	uint64_t lastts;
	uint64_t lastpts;
	bool rdwr_mode;
	int users;
} ptpfd_virtual_t;

static ub_esarray_cstd_t *esvptpd = NULL;

static ptpfd_virtual_t *find_ptpfd_virtual(PTPFD_TYPE ptpfd)
{
	int i;
	ptpfd_virtual_t *pv;
	int en;
	if (!esvptpd) {
		UB_LOG(UBL_ERROR,"%s:esvptpd is NULL\n", __func__);
		return NULL;
	}
	en=ub_esarray_ele_nums(esvptpd);
	for(i=0;i<en;i++){
		pv=(ptpfd_virtual_t *)ub_esarray_get_ele(esvptpd, i);
		if(ptpfd==pv->fd) return pv;
	}
	UB_LOG(UBL_ERROR,"%s:ptpfd="PRiFD" is not opened\n", __func__, ptpfd);
	return NULL;
}

PTPFD_TYPE gptp_vclock_alloc_fd(char *ptpdev)
{
	int i,en;
	ptpfd_virtual_t *pv, *rpv;
	PTPFD_TYPE fdmax=(PTPFD_TYPE)(GPTP_VIRTUAL_PTPDEV_FDBASE-1);
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
	if(rpv->fd>(PTPFD_TYPE)GPTP_VIRTUAL_PTPDEV_FDMAX){
		ub_esarray_del_pointer(esvptpd, (ub_esarray_element_t *)rpv);
		return PTPFD_INVALID;
	}
	return rpv->fd;
}

int gptp_vclock_free_fd(PTPFD_TYPE ptpfd)
{
	ptpfd_virtual_t *pv=find_ptpfd_virtual(ptpfd);
	if(!pv){
		UB_LOG(UBL_ERROR,"%s:ptpfd="PRiFD" is not opened\n", __func__, ptpfd);
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

int gptp_vclock_settime(PTPFD_TYPE ptpfd, uint64_t ts64)
{
	ptpfd_virtual_t *pv=find_ptpfd_virtual(ptpfd);
	if(!pv) return -1;
	if(!pv->rdwr_mode) return -1;
	pv->lastpts=ts64;
	pv->lastts=ub_rt_gettime64();
	return 0;
}

uint64_t gptp_vclock_gettime(PTPFD_TYPE ptpfd)
{
	uint64_t ts64;
	int64_t dts64, dpts64;
	ptpfd_virtual_t *pv=find_ptpfd_virtual(ptpfd);
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

int gptp_vclock_adjtime(PTPFD_TYPE ptpfd, int adjppb)
{
	ptpfd_virtual_t *pv=find_ptpfd_virtual(ptpfd);
	if(!pv) return -1;
	if(!pv->rdwr_mode) return -1;
	pv->freq_adj=adjppb;
	return 0;
}
