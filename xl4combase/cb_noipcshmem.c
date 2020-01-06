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
#include "cb_ipcshmem.h"

static ub_esarray_cstd_t *shmem_table=NULL;
typedef struct shared_mem_table{
	char name[32];
	void *mem;
}shared_mem_table_t;

static shared_mem_table_t *find_shared_mem(const char *shmname)
{
	int i;
	shared_mem_table_t *memt;
        if(!shmname) return NULL;
	if(!shmem_table) return NULL;
	for(i=0;i<ub_esarray_ele_nums(shmem_table);i++){
		memt=(shared_mem_table_t *)ub_esarray_get_ele(shmem_table, i);
		if(!strcmp(shmname, memt->name)) return memt;
	}
	return NULL;
}

void *cb_get_shared_mem(int *memfd, const char *shmname, size_t size, int flag)
{
	shared_mem_table_t *memt;
	*memfd=0;

	if(!shmname) {
		UB_LOG(UBL_ERROR,"%s:shmname is NULL\n", __func__);
		return NULL;
	}

	if(!shmem_table)
		shmem_table=ub_esarray_init(8, sizeof(shared_mem_table_t), 32);
	memt=find_shared_mem(shmname);
	if(flag & O_CREAT){
		if(memt){
			UB_LOG(UBL_ERROR,"%s:shared memory, name=%s is already opened\n", __func__,
			       shmname);
			return NULL;
		}
		memt=(shared_mem_table_t *)ub_esarray_get_newele(shmem_table);
		strncpy(memt->name, shmname, 32);
		memt->mem=malloc(size);
		if(!memt->mem) {
			UB_LOG(UBL_ERROR,"%s:malloc error for size=%zu, %s\n",__func__,
			       size, strerror(errno));
			cb_close_shared_mem(NULL, 0, shmname, 0, true);
			return NULL;
		}
	}else{
		if(!memt){
			UB_LOG(UBL_ERROR,"%s:shared memory, name=%s doesn't exist\n", __func__,
			       shmname);
			return NULL;
		}
	}
	*memfd=(int)((uintptr_t)memt & 0x7fffffff);
	return memt->mem;
}

int cb_close_shared_mem(void *mem, int *memfd, const char *shmname, size_t size, bool unlink)
{
	shared_mem_table_t *memt;

	if(!shmname) {
		UB_LOG(UBL_ERROR,"%s:shmname is NULL\n", __func__);
		return -1;
	}

	memt=find_shared_mem(shmname);
	if(!memt){
                UB_LOG(UBL_ERROR,"%s:shared memory, name=%s doesn't exist\n",__func__, shmname);
		return -1;
	}
	if(mem && unlink){
		if(mem!=memt->mem){
			UB_LOG(UBL_ERROR,"%s:pointer doesn't match\n",__func__);
			return -1;
		}
		free(mem);
	}
        if(unlink){
                ub_esarray_del_pointer(shmem_table, (ub_esarray_element_t *)memt);
		if(!ub_esarray_ele_nums(shmem_table)){
			ub_esarray_close(shmem_table);
			shmem_table=NULL;
		}
        }
        if(memfd){
                *memfd=0;
        }
	return 0;
}
