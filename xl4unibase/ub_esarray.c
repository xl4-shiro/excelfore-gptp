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
 * ub_esarray.c
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Shiro Ninomiya (shiro@excelfore.com)
 */
#include "unibase_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/*
 * the array is expanded by 'expnd_unit'
 * 'expnd_unit' is also hysteresis number,
 * when it needs one more 'expnd_unit' it expands.
 * when it has unused area as 2 of 'expnd_unit', it shrinks one 'expand_unit'
 */
struct ub_esarray_cstd {
	int expnd_unit;
	int expand_hysteresis;
	int element_size;
	int allocate_nums;
	int element_nums;
	int max_element_nums;
	uint8_t *data;
	void *fmutex;
	bool data_lock;
};

static void *get_newele_nomutex(ub_esarray_cstd_t *eah)
{
	uint8_t *np;
	if(eah->element_nums >= eah->max_element_nums){
		UB_LOG(UBL_ERROR, "%s:can't expand more than max number of elements: %d\n",
		       __func__, eah->max_element_nums);
		return NULL;
	}
	if(eah->allocate_nums<=eah->element_nums){
		if(eah->data_lock){
			UB_LOG(UBL_ERROR, "%s:data is locked\n", __func__);
			return NULL;
		}
		/* expand allocated memory */
		np=realloc(eah->data, eah->element_size * (eah->allocate_nums + eah->expnd_unit));
		if(np == NULL){
			UB_LOG(UBL_ERROR, "%s:realloc error, %s\n", __func__, strerror(errno));
			return NULL;
		}
		eah->allocate_nums+= eah->expnd_unit;
		eah->data = np;
	}
	return (void*)(eah->data + eah->element_size * eah->element_nums++);
}

static int del_index_nomutex(ub_esarray_cstd_t *eah, int index)
{
	if(index+1 > eah->element_nums || index < 0){
		UB_LOG(UBL_ERROR, "%s: index=%d doesn't exist\n", __func__, index);
		return -1;
	}

	/* move data after *ed */
	if(eah->element_nums - (index + 1) > 0){
		memmove(eah->data +  eah->element_size * index,
			eah->data +  eah->element_size * (index + 1),
			eah->element_size * (eah->element_nums - (index + 1)));
	}
	if(--eah->element_nums <= eah->allocate_nums-2*eah->expnd_unit){
		if(eah->data_lock){
			UB_LOG(UBL_WARN, "%s:data is locked\n", __func__);
			return 0;
		}
		/* shrink allocate */
		eah->data = realloc(eah->data, eah->element_size *
				    (eah->allocate_nums-eah->expnd_unit));
		if(eah->element_nums > 0 && eah->data == NULL){
			UB_LOG(UBL_ERROR, "%s:failed to shrink allocated memory, %s\n",
			       __func__, strerror(errno));
			return -1;
		}
		eah->allocate_nums-=eah->expnd_unit;
	}
	return 0;
}

ub_esarray_cstd_t *ub_esarray_init(int expnd_unit, int element_size, int max_element_nums)
{
	ub_esarray_cstd_t *eah;
	eah = malloc(sizeof(ub_esarray_cstd_t));
	ub_assert(eah!=NULL, __func__, "malloc error");
	memset(eah, 0, sizeof(ub_esarray_cstd_t));
	eah->expnd_unit = expnd_unit;
	eah->element_size = element_size;
	eah->max_element_nums = max_element_nums;
	if(ubcd.threadding) {
		eah->fmutex=ubcd.cbset.mutex_init();
		ubcd.cbset.mutex_lock(eah->fmutex);
	}
	get_newele_nomutex(eah); // start with allocting the first block
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	eah->element_nums=0;
	return eah;
}

void ub_esarray_close(ub_esarray_cstd_t *eah)
{
	if(ubcd.threadding) ubcd.cbset.mutex_close(eah->fmutex);
	if(eah->data) free(eah->data);
	free(eah);
}

int ub_esarray_add_ele(ub_esarray_cstd_t *eah, ub_esarray_element_t *ed)
{
	uint8_t *pt;
	int res=-1;
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	pt = get_newele_nomutex(eah);
	if(pt == NULL) goto erexit;
	memcpy(pt, ed, eah->element_size);
	res=0;
erexit:
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return res;
}

int ub_esarray_pop_ele(ub_esarray_cstd_t *eah, ub_esarray_element_t *ed)
{
	ub_esarray_element_t *resd;
	int res=-1;
	if(eah->data_lock){
		UB_LOG(UBL_ERROR, "%s: data is locked\n", __func__);
		return -1;
	}
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	resd = ub_esarray_get_ele(eah, 0);
	if(resd==NULL) goto erexit;
	memcpy(ed, resd, eah->element_size);
	del_index_nomutex(eah, 0);
	res = 0;
erexit:
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return res;
}

int ub_esarray_pop_last_ele(ub_esarray_cstd_t *eah, ub_esarray_element_t *ed)
{
	ub_esarray_element_t *resd;
	int res=-1;
	if(eah->data_lock){
		UB_LOG(UBL_ERROR, "%s: data is locked\n", __func__);
		return -1;
	}
	if(!eah->element_nums) return res;
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	resd = ub_esarray_get_ele(eah, eah->element_nums-1);
	if(resd==NULL) goto erexit;
	memcpy(ed, resd, eah->element_size);
	del_index_nomutex(eah, eah->element_nums-1);
	res = 0;
erexit:
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return res;
}

int ub_esarray_del_ele(ub_esarray_cstd_t *eah, ub_esarray_element_t *ed)
{
	int i;
	int res;
	if(eah->data_lock){
		UB_LOG(UBL_ERROR, "%s: data is locked\n", __func__);
		return -1;
	}
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	for(i=0;i<eah->element_nums;i++){
		if(!memcmp(eah->data +  eah->element_size * i, ed, eah->element_size))
			break;
	}
	if(i == eah->element_nums){
		UB_LOG(UBL_ERROR, "%s: this data is not in the array\n",__func__);
		res=-1;
	}else{
		res=del_index_nomutex(eah, i);
	}
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return res;
}

/*
 * Once ub_esarray_data_lock is called, eah->data_lock is locked.
 * In the locked status, any elements of data can't be deleted,
 * also, expanding the allocated area can't happen.
 *
 * eah->data_lock is unlocked by the parameter on es_del_index
 * or es_del_pointer.
 *
 * In single thread env., data_lock doesn't have to be cared.
 */
int ub_esarray_data_lock(ub_esarray_cstd_t *eah)
{
	bool res;
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	res=!eah->data_lock;
	eah->data_lock=true;
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	if(!res){
		UB_LOG(UBL_WARN, "%s:already locked\n", __func__);
		return -1;
	}
	return 0;
}

int ub_esarray_data_unlock(ub_esarray_cstd_t *eah)
{
	bool res;
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	res=eah->data_lock; // if already unlocked, return false;
	eah->data_lock=false;
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	if(!res){
		UB_LOG(UBL_WARN, "%s:already unlocked\n", __func__);
		return -1;
	}
	return 0;
}

int ub_esarray_ele_nums(ub_esarray_cstd_t *eah)
{
	return eah->element_nums;
}

ub_esarray_element_t *ub_esarray_get_newele(ub_esarray_cstd_t *eah)
{
	ub_esarray_element_t *ele;
	if(!eah) return NULL;
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	ele=get_newele_nomutex(eah);
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return ele;
}

ub_esarray_element_t *ub_esarray_get_ele(ub_esarray_cstd_t *eah, int index)
{
	if(!eah) return NULL;
	if(index >= eah->element_nums || index < 0) return NULL;
	return (ub_esarray_element_t *)(eah->data + eah->element_size * index);
}

int ub_esarray_del_index(ub_esarray_cstd_t *eah, int index)
{
	int res;
	if(eah->data_lock){
		UB_LOG(UBL_ERROR, "%s: data is locked\n", __func__);
		return -1;
	}
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	res=del_index_nomutex(eah, index);
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return res;
}


int ub_esarray_del_pointer(ub_esarray_cstd_t *eah, ub_esarray_element_t *ed)
{
	int i;
	int res;
	if(eah->data_lock){
		UB_LOG(UBL_ERROR, "%s: data is locked\n", __func__);
		return -1;
	}
	if(ubcd.threadding) ubcd.cbset.mutex_lock(eah->fmutex);
	for(i=0;i<eah->element_nums;i++){
		if(eah->data +  eah->element_size * i == ed) break;
	}
	if(i == eah->element_nums){
		UB_LOG(UBL_ERROR, "%s: this data is not in the array\n",__func__);
		res=-1;
	}else{
		res=del_index_nomutex(eah, i);
	}
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(eah->fmutex);
	return res;
}
