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
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "combase_private.h"

void *cb_get_shared_mem(int *memfd, const char *shmname, size_t size, int flag)
{
	void *mem;
	int prot;
	*memfd=shm_open(shmname, flag, 0666);
	if(*memfd<0) {
		UB_LOG(UBL_ERROR,"%s:can't %s shared memory, name=%s, error=%d:%s\n", __func__,
		       (flag & O_CREAT)?"create":"open", shmname, errno, strerror(errno));
		goto erexit;
	}
	UB_LOG(UBL_DEBUG,"%s:%s shared memory, name=%s, size=%zd\n",__func__,
	       (flag & O_CREAT)?"create":"open", shmname, size);

	if(((flag && O_WRONLY)||(flag && O_RDWR)) && ftruncate(*memfd, size)){
		UB_LOG(UBL_ERROR,"%s:ftruncate,%s\n", __func__, strerror(errno));
		goto erexit;
	}

	if(flag && O_RDWR) prot=PROT_READ|PROT_WRITE;
	else if(flag && O_WRONLY) prot=PROT_WRITE;
	else prot=PROT_READ;

	mem=mmap(NULL, size, prot, MAP_SHARED, *memfd, 0);
	if(mem==MAP_FAILED){
		UB_LOG(UBL_ERROR,"%s:mmap, %s\n", __func__, strerror(errno));
		close(*memfd);
		shm_unlink(shmname);
		goto erexit;
	}
	return mem;
erexit:
	*memfd=0;
	return NULL;
}

int cb_close_shared_mem(void *mem, int *memfd, const char *shmname, size_t size, bool unlink)
{
	if(mem && size){
		if(munmap(mem, size)){
			UB_LOG(UBL_ERROR,"%s:munmap, %s\n", __func__,strerror(errno));
			return -1;
		}
	}
	if(memfd && *memfd){
		close(*memfd);
		*memfd=0;
	}
	if(shmname && *shmname && (unlink)){
		if(shm_unlink(shmname)){
			UB_LOG(UBL_ERROR,"%s:shm_unlink, shmname=%s, %s\n",
			       __func__, shmname, strerror(errno));
			return -1;
		}
	}
	return 0;
}
