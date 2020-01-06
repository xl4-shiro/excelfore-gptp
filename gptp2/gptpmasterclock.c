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
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "gptpclock.h"
#include "gptpmasterclock.h"

typedef struct gptp_master_clock_data{
	int max_domains;
	int shmfd;
	int shmsize;
	gptp_master_clock_shm_t *shm;
	PTPFD_TYPE *ptpfds;
	int suppress_msg;
	char shmem_name[GPTP_MAX_SIZE_SHARED_MEMNAME];
} gptp_master_clock_data_t;

static gptp_master_clock_data_t gmcd;

#define GMCD_INIT_CHECK gmcd.max_domains?0:gptpmasterclock_init(NULL)

static int ptpdev_open(void)
{
	int i;
	int res=-1;
	for(i=0;i<gmcd.max_domains;i++){
		if(gmcd.ptpfds[i]) continue; // already opened
		if(!gmcd.shm->gcpp[i].ptpdev[0]) continue; // no ptpdev, it may come later
		gmcd.ptpfds[i]=PTPDEV_CLOCK_OPEN(gmcd.shm->gcpp[i].ptpdev, O_RDONLY);
		if(!PTPFD_VALID(gmcd.ptpfds[i])){
			UB_LOG(UBL_ERROR, "ptpdev=%s, can't open, %s\n",
			       gmcd.shm->gcpp[i].ptpdev, strerror(errno));
			return -1;
		}
		UB_LOG(UBL_DEBUG,"domainIndex=%d, ptpdev=%s\n",i,gmcd.shm->gcpp[i].ptpdev);
		res=0;
	}
	return res;
}

static int gptpmasterclock_health_check(int domainIndex)
{
	if(GMCD_INIT_CHECK) return -1;
	if(!gmcd.shm->head.max_domains){
		UB_LOG(UBL_ERROR, "%s:gptp2d might be closed, re-initialize now\n",__func__);
		gptpmasterclock_close();
		if(gptpmasterclock_init(gmcd.shmem_name)) return -1;
	}
	if(!PTPFD_VALID(gmcd.ptpfds[domainIndex])){
		// this must be rare case. when there are multiple ptp devices,
		// there might be latency to add ptp devices.
		if(!gmcd.suppress_msg){
			UB_LOG(UBL_INFO, "%s:domainIndex=%d, ptpdev is not yet opened\n",
			       __func__, domainIndex);
		}
		// when ptpdev is not yet opened, it may be opened this time
		if(ptpdev_open() || !PTPFD_VALID(gmcd.ptpfds[domainIndex])){
			gmcd.suppress_msg=1;
			return -1;
		}
		UB_LOG(UBL_INFO, "%s:domainIndex=%d, ptpdev=%s is opened\n",
		       __func__, domainIndex, gmcd.shm->gcpp[domainIndex].ptpdev);
		gmcd.suppress_msg=0;
	}
	return 0;
}

int gptpmasterclock_init(const char *shmem_name)
{
	int *dnum;

	UB_LOG(UBL_INFO, "%s: gptp2-"XL4PKGVERSION"\n", __func__);
	if(gmcd.max_domains){
		UB_LOG(UBL_DEBUG, "%s: already initialized\n", __func__);
		return 0;
	}
	if(gmcd.suppress_msg) ub_log_change(CB_COMBASE_LOGCAT, UBL_NONE, UBL_NONE);
	if(shmem_name && shmem_name[0]) {
		strncpy(gmcd.shmem_name, shmem_name, GPTP_MAX_SIZE_SHARED_MEMNAME);
	}else{
		strcpy(gmcd.shmem_name, GPTP_MASTER_CLOCK_SHARED_MEM);
	}
	dnum=(int*)cb_get_shared_mem(&gmcd.shmfd, gmcd.shmem_name, sizeof(int), O_RDONLY);
	if(gmcd.suppress_msg) ub_log_return(CB_COMBASE_LOGCAT);
	if(!dnum){
		if(!gmcd.suppress_msg){
			UB_LOG(UBL_ERROR, "%s:master clock is not yet register by gptp\n",
			       __func__);
		}
		goto erexit;
	}
	if(*dnum==0){
		if(!gmcd.suppress_msg){
			UB_LOG(UBL_ERROR, "%s:master clock is not yet added\n", __func__);
		}
		goto erexit;
	}
	UB_LOG(UBL_DEBUG, "%s:*dnum=%d\n", __func__, *dnum);
	gmcd.max_domains=*dnum;
	cb_close_shared_mem(dnum, &gmcd.shmfd, gmcd.shmem_name, sizeof(int), false);
	gmcd.shmsize=sizeof(gptp_clock_ppara_t)*gmcd.max_domains +
		sizeof(gptp_master_clock_shm_head_t);
	gmcd.shm=(gptp_master_clock_shm_t *)cb_get_shared_mem(
		&gmcd.shmfd, gmcd.shmem_name, gmcd.shmsize, O_RDWR);
	if(!gmcd.shm){
		UB_LOG(UBL_ERROR, "%s:can't get the shared memory\n", __func__);
		goto erexit;
	}
	gmcd.ptpfds=malloc(gmcd.max_domains*sizeof(PTPFD_TYPE));
	memset(gmcd.ptpfds,0,gmcd.max_domains*sizeof(PTPFD_TYPE));
	if(ptpdev_open()) goto erexit;
	if(gmcd.suppress_msg){
		UB_LOG(UBL_INFO, "%s:recovered\n",__func__);
		gmcd.suppress_msg=0;
	}
	UB_LOG(UBL_DEBUG, "%s:done\n",__func__);
	return 0;
erexit:
	if(!gmcd.suppress_msg){
		UB_LOG(UBL_INFO, "%s:failed\n",__func__);
		gptpmasterclock_close();
		gmcd.suppress_msg=1;
	}else{
		gptpmasterclock_close();
	}
	return -1;
}

int gptpmasterclock_close(void)
{
	int i;
	if(!gmcd.max_domains) return -1;
	if(!gmcd.shm) return -1;
	for(i=0;i<gmcd.max_domains;i++){
		if(!gmcd.shm->gcpp[i].ptpdev[0]) continue;
		PTPDEV_CLOCK_CLOSE(gmcd.ptpfds[i]);
	}
	free(gmcd.ptpfds);
	cb_close_shared_mem(gmcd.shm, &gmcd.shmfd,
			    gmcd.shmem_name, gmcd.shmsize, false);
	memset(&gmcd, 0, sizeof(gmcd));
	return 0;
}

void gptpmasterclock_dump_offset(void)
{
	int i;
	if(GMCD_INIT_CHECK) return;
	printf("max_domains=%d, active_domain=%d\n", gmcd.shm->head.max_domains,
	       gmcd.shm->head.active_domain);
	for(i=0;i<gmcd.max_domains;i++){
		printf("index=%d, domainNumber=%d, gmsync=%d, gmchange_ind=%"PRIu32"\n",
		       i, gmcd.shm->gcpp[i].domainNumber,gmcd.shm->gcpp[i].gmsync,
		       gmcd.shm->gcpp[i].gmchange_ind);
		printf("offset %"PRIi64"nsec\n", gmcd.shm->gcpp[i].offset64);
	}
	printf("\n");
}

int gptpmasterclock_gm_domainIndex(void)
{
	if(GMCD_INIT_CHECK) return -1;
	return gmcd.shm->head.active_domain;
}

int gptpmasterclock_gm_domainNumber(void)
{
	int i;
	i= gptpmasterclock_gm_domainIndex();
	if(i<0) return -1;
	return gmcd.shm->gcpp[i].domainNumber;
}

int gptpmasterclock_gmchange_ind(void)
{
	int i;
	i= gptpmasterclock_gm_domainIndex();
	if(i<0) return -1;
	return gmcd.shm->gcpp[i].gmchange_ind;
}

int gptpmasterclock_get_max_domains(void)
{
	return gmcd.max_domains;
}

int gptpmasterclock_get_domain_ts64(int64_t *ts64, int domainIndex)
{
	int64_t dts;
	double adjrate;
	int rval=-1;
	if(gptpmasterclock_health_check(domainIndex)) return -1;
	if(domainIndex<0 || domainIndex>=gmcd.max_domains) return -1;
	gptpclock_mutex_trylock(&gmcd.shm->head.mcmutex);
	GPTP_CLOCK_GETTIMEMS(gmcd.ptpfds[domainIndex], *ts64);

	adjrate=gmcd.shm->gcpp[domainIndex].adjrate;
	if(adjrate != 0.0){
		// get dts, which is diff between now and last setts time
		dts=*ts64-gmcd.shm->gcpp[domainIndex].last_setts64;
	}

	// add offset
	*ts64+=gmcd.shm->gcpp[domainIndex].offset64;
	if(adjrate != 0.0) *ts64+=dts*adjrate;
	rval=0;

	CB_THREAD_MUTEX_UNLOCK(&gmcd.shm->head.mcmutex);
	return rval;
}

int64_t gptpmasterclock_getts64(void)
{
	int64_t ts64;
	if(GMCD_INIT_CHECK) return -1;
	if(gptpmasterclock_get_domain_ts64(&ts64, gmcd.shm->head.active_domain)) return -1;
	return ts64;
}

/*
 * We need an accurate timer here to sleep
 * nanosleep relies on 'Linux kernel hrtimers'; check hrtimers.txt
 * in the kernel Documents.
 */
int gptpmasterclock_wait_until_ts64(int64_t tts, int64_t vclose, int64_t toofar)
{
	int64_t cts,dts;
	int64_t rem;

	cts=gptpmasterclock_getts64();
	dts=tts-cts;
	if(dts<0) return 1;
	if (dts <= vclose) return 2;
	if(toofar && dts >= toofar){
		UB_LOG(UBL_INFO,"%s: %"PRIi64"nsec is farther than %"PRIi64"\n",
		       __func__, dts, toofar);
		return 3;
	}
	UB_LOG(UBL_DEBUG,"%s: wait for %"PRIi64"nsec\n", __func__, dts);
	if(cb_nanosleep64(dts,&rem)){
		if(errno==EINTR){
			cb_nanosleep64(rem, NULL);
		}else{
			UB_LOG(UBL_ERROR,"%s: error in nanosleep: %s:\n",
			       __func__, strerror(errno));
		}
		return -1;
	}
	return 0;
}

uint64_t gptpmasterclock_expand_timestamp(uint32_t timestamp)
{
	uint32_t ctimes;
	int32_t dtimes;
	int64_t ts64;

	ts64=gptpmasterclock_getts64();
	ctimes = (uint32_t)ts64;
	// dtimes becomes a range of  -2.147 to 2.147 secconds
	dtimes = (int32_t)(timestamp - ctimes);
	return ts64+dtimes;
}
