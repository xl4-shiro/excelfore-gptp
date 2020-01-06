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
/*
 * unibase_posix.c
 * unibase posix binding
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Shiro Ninomiya (shiro@excelfore.com)
 */

#include "unibase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "unibase_binding.h"

typedef struct memory_out_data {
	char *buffer;
	int size;
	bool own_buffer;
	uint32_t wp;
	pthread_mutex_t mutex;
} memory_out_data_t;
static memory_out_data_t memoutd;
static const char *endmark=UBB_MEMOUT_ENDMARK;
static const int endmlen=sizeof(UBB_MEMOUT_ENDMARK)-1;
int ubb_memory_out_init(char *mem, int size)
{
	ubb_memory_out_close();
	memoutd.size=size;
	if(!size) {
		memoutd.size=-1;
		return 0;
	}
	if(mem){
		memoutd.buffer=mem;
		memoutd.own_buffer=false;
	}else{
		memoutd.buffer=malloc(size);
		if(!memoutd.buffer) abort();
		memoutd.own_buffer=true;
	}
	memset(memoutd.buffer, 0, memoutd.size);
	pthread_mutex_init(&memoutd.mutex, NULL);
	return 0;
}

int ubb_memory_out_close(void)
{
	if(memoutd.size<=0) return 0;
	if(!memoutd.buffer) return -1;
	pthread_mutex_destroy(&memoutd.mutex);
	if(memoutd.own_buffer) free(memoutd.buffer);
	memset(&memoutd, 0, sizeof(memory_out_data_t));
	return 0;
}

char *ubb_memory_out_buffer(void)
{
	return memoutd.buffer;
}

static int memory_out_find_line(int *point, int *size)
{
	int lwp;
	int i, c, p1;
	// skip the bottom null characters
	for(lwp=*point;lwp>=0;lwp--) {
		if(*(memoutd.buffer+lwp)!=0) break;
	}
	if(lwp<0) return -1;
	p1=-1;
	for(c=0,i=lwp;i>=0;i--){
		if(*(memoutd.buffer+i)!='\n') continue;
		if(c++==1){
			*point=i+1;
			*size=p1-i;
			return 0;
		}
		p1=i;
	}
	if(p1>=0){
		*point=0;
		*size=p1+1;
		return 0;
	}
	return -1;
}

int ubb_memory_out_lastline(char **str, int *size)
{
	int lwp;
	int res=-1;
	*str=NULL;
	*size=0;
	if(memoutd.size<=0) return -1;
	pthread_mutex_lock(&memoutd.mutex);
	if(memcmp(memoutd.buffer+memoutd.wp, endmark, endmlen)) {
		goto erexit;
	}
	lwp=memoutd.wp;
	if(lwp==0) lwp=memoutd.size-1;
	if(!memory_out_find_line(&lwp, size)){
		*str=memoutd.buffer+lwp;
		res=0;
	}else{
		if(memoutd.wp==0) {
			goto erexit;
		}
		lwp=memoutd.size-1;
		if(!memory_out_find_line(&lwp, size)){
			*str=memoutd.buffer+lwp;
			res=0;
		}
	}
erexit:
	pthread_mutex_unlock(&memoutd.mutex);
	return res;
}

int ubb_memory_out_alldata(char **rstr, int *size)
{
	int lwp;
	int i;
	int res=-1;
	int size1=0;
	char *str1=NULL;

	if(memoutd.size<=0) return res;
	*rstr=malloc(memoutd.size);
	ub_assert(*rstr, __func__, "malloc error");
	memset(*rstr, 0, memoutd.size);

	pthread_mutex_lock(&memoutd.mutex);
	if(memcmp(memoutd.buffer+memoutd.wp, endmark, endmlen)) {
		free(*rstr);
		*rstr=NULL;
		*size=0;
		goto erexit;
	}
	lwp=memoutd.wp;
	str1=memoutd.buffer+lwp+endmlen+1;
	for(i=lwp+endmlen+1;i<memoutd.size;i++){
		if(*(memoutd.buffer+i)==0){
			break;
		}
	}
	size1=i-(lwp+endmlen+1);
	if(size1) memcpy(*rstr, str1, size1);
	if(lwp) memcpy(*rstr+size1, memoutd.buffer, lwp);
	*size=size1+lwp;
	res=0;
erexit:
	pthread_mutex_unlock(&memoutd.mutex);
	return res;
}

static int ubb_memory_out(bool flush, const char *str)
{
	int v;
	if(memoutd.size<0) return 0;
	if(!memoutd.size){
		if(ubb_memory_out_init(NULL, UBB_DEFAULT_DEBUG_LOG_MEMORY)) return -1;
	}
	pthread_mutex_lock(&memoutd.mutex);
	v=strlen(str);
	if(memoutd.wp+v >= memoutd.size-(endmlen+1)){
		// if it is at the bottom, fill 0 in the remaining buffer
		memset(memoutd.buffer+memoutd.wp, 0, memoutd.size-memoutd.wp);
		memoutd.wp=0;
	}
	strcpy(memoutd.buffer+memoutd.wp, str);
	strcpy(memoutd.buffer+memoutd.wp+v, endmark);
	memoutd.wp+=v;
	pthread_mutex_unlock(&memoutd.mutex);
	return v;
}

static int ubb_stdout(bool flush, const char *str)
{
	int res;
	res=fwrite(str, 1, strlen(str), stdout);
	if(flush) fflush(stdout);
	return res;
}

static void *ubb_mutex_init(void)
{
	pthread_mutex_t *mt;
	mt=(pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	ub_assert(mt, __func__, "malloc error");
	ub_assert(pthread_mutex_init(mt, NULL)==0, __func__, "pthread_mutex_init error");
	return mt;
}

static int ubb_mutex_close(void *mt)
{
	int res;
	if(!mt) return -1;
	res=pthread_mutex_destroy((pthread_mutex_t *)mt);
	free(mt);
	return res;
}

static int ubb_mutex_lock(void *mt)
{
	return pthread_mutex_lock((pthread_mutex_t *)mt);
}

static int ubb_mutex_unlock(void *mt)
{
	return pthread_mutex_unlock((pthread_mutex_t *)mt);
}

static uint64_t ubb_gettime64(ub_clocktype_t ctype)
{
	struct timespec ts={0,0};
	switch(ctype)
	{
	case UB_CLOCK_DEFAULT:
		break;
	case UB_CLOCK_REALTIME:
		clock_gettime(CLOCK_REALTIME, &ts);
		break;
	case UB_CLOCK_MONOTONIC:
		clock_gettime(CLOCK_MONOTONIC, &ts);
		break;
	case UB_CLOCK_GPTP:
		break;
	}
	return ts.tv_sec*UB_SEC_NS+ts.tv_nsec;
}

void ubb_default_initpara(unibase_init_para_t *init_para)
{
	static const char *default_log_initstr=UBB_DEFAULT_LOG_INITSTR;
	memset(init_para, 0, sizeof(unibase_init_para_t));
	init_para->cbset.console_out=ubb_stdout;
	init_para->cbset.debug_out=ubb_memory_out;
	init_para->cbset.mutex_init=ubb_mutex_init;
	init_para->cbset.mutex_close=ubb_mutex_close;
	init_para->cbset.mutex_lock=ubb_mutex_lock;
	init_para->cbset.mutex_unlock=ubb_mutex_unlock;
	init_para->cbset.gettime64=ubb_gettime64;
	init_para->ub_log_initstr=default_log_initstr;
}
