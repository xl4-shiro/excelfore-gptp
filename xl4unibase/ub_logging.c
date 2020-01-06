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
 * ub_logging.c
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Shiro Ninomiya (shiro@excelfore.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "unibase_private.h"

static inline int check_cat_index(int cat_index, const char *func)
{
	if(cat_index >= MAX_LOGMSG_CATEGORIES || cat_index < 0){
		ub_console_print("%s:cat_index=%d is out of the range\n", func, cat_index);
		return -1;
	}
	return 0;
}

static int console_debug_print_va(bool console, bool debug, const char * format, va_list ap)
{
	char astr[UB_CHARS_IN_LINE+1];

	astr[UB_CHARS_IN_LINE]=0;
	vsnprintf(astr, UB_CHARS_IN_LINE, format, ap);
	if(console && ubcd.cbset.console_out) ubcd.cbset.console_out(false, astr);
	if(debug && ubcd.cbset.debug_out) ubcd.cbset.debug_out(false, astr);
	return strlen(astr);
}

static int console_debug_print(bool console, bool debug, const char * format, ...)
{
	va_list ap;
	int res;
	va_start(ap, format);
	res=console_debug_print_va(console, debug, format, ap);
	va_end(ap);
	return res;
}

int ub_console_print(const char * format, ...)
{
	va_list ap;
	int res;
	va_start(ap, format);
	res=console_debug_print_va(true, false, format, ap);
	va_end(ap);
	return res;
}

int ub_debug_print(const char * format, ...)
{
	va_list ap;
	int res;
	va_start(ap, format);
	res=console_debug_print_va(false, true, format, ap);
	va_end(ap);
	return res;
}

int ub_console_debug_print(const char * format, ...)
{
	va_list ap;
	int res;
	va_start(ap, format);
	res=console_debug_print_va(true, true, format, ap);
	va_end(ap);
	return res;
}

static int log_one_category(const char *ns, ub_logmsg_data_t *logmsgd, char *category_name)
{
	int i,v;
	const char *nss=ns;
	if(!category_name){
		v=strlen(ns);
		for(i=0;i<v;i++){
			if(ns[i]!=':') continue;
			break;
		}
		if(i>CATEGORY_NAME_CHAR_MAX){
			ub_console_print("%s:wrong init string:%s\n",__func__, ns);
			return -1;
		}
		memcpy(logmsgd->category_name, ns, i);
		logmsgd->category_name[i]=0;
		ns+=i+1;
	}else{
		strncpy(logmsgd->category_name, category_name, CATEGORY_NAME_CHAR_MAX);
	}
	v=ns[0]-'0';
	if(v<0 || v>UBL_DEBUGV){
		ub_console_print("%s:wrong console log level:%s\n",__func__, ns);
		return-1;
	}
	logmsgd->clevel=(ub_dbgmsg_level_t)v;
	ns++;
	v=ns[0]-'0';
	if(v<0 || v>UBL_DEBUGV){
		// it seems to be only 1 digit, use 'clevel' for 'dlevel'
		logmsgd->dlevel=logmsgd->clevel;
	}else{
		logmsgd->dlevel=(ub_dbgmsg_level_t)v;
		ns++;
	}
	if(*ns=='m'){
		ns++;
		logmsgd->flags=UB_CLOCK_MONOTONIC;
	}else if (*ns=='r'){
		ns++;
		logmsgd->flags=UB_CLOCK_REALTIME;
	}else if (*ns=='g'){
		ns++;
		logmsgd->flags=UB_CLOCK_GPTP;
	}else{
		logmsgd->flags=UB_CLOCK_DEFAULT;
	}
	return ns-nss;
}

void ub_log_init(const char *ns)
{
	int i,v;
	int cat_index;
	int defv=UBL_INFO; // default is the info level only on the console
	char defcname[8];

	if(ubcd.threadding) ubcd.cbset.mutex_lock(ubcd.gmutex);
	v=-1;
	if(ns){
		v=log_one_category(ns, &ubcd.logmsgd[0], "def00");
	}
	if(v<0){
		memset(&ubcd.logmsgd[0], 0, sizeof(ub_logmsg_data_t));
		strcpy(ubcd.logmsgd[0].category_name, "def00");
		ubcd.logmsgd[0].clevel=defv;
		ubcd.logmsgd[0].dlevel=defv;
		v=0;
	}
	for(i=1;i<MAX_LOGMSG_CATEGORIES;i++){
		memcpy(&ubcd.logmsgd[i], &ubcd.logmsgd[0], sizeof(ub_logmsg_data_t));
		sprintf(defcname, "def%02d", i);
		strcpy(ubcd.logmsgd[i].category_name, defcname);
	}
	if(!ns) goto erexit;
	ns+=v;
	cat_index=0;
	ubcd.log_categories=0;
	while(*ns){
		if(*ns==',') ns++;
		if(cat_index>=MAX_LOGMSG_CATEGORIES){
			ub_console_print("%s:no more room to add a category\n",__func__);
			break;
		}
		v=log_one_category(ns, &ubcd.logmsgd[cat_index], NULL);
		if(v<0) continue;
		ns+=v;
		cat_index++;
		ubcd.log_categories++;
	}
erexit:
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(ubcd.gmutex);
}

int ub_log_add_category(const char *catstr)
{
	int v;
	if(ubcd.log_categories>=MAX_LOGMSG_CATEGORIES){
		ub_console_print("%s:no more room to add a category\n",__func__);
		return -1;
	}
	if(ubcd.threadding) ubcd.cbset.mutex_lock(ubcd.gmutex);
	v=log_one_category(catstr, &ubcd.logmsgd[ubcd.log_categories], NULL);
	if(v>=0) ubcd.log_categories++;
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(ubcd.gmutex);
	if(v<0) return -1;
	return 0;
}

int ub_log_print(int cat_index, int flags, ub_dbgmsg_level_t level,
		 const char * format, ...)
{
	va_list ap;
	ub_clocktype_t addts;
	uint64_t ts64=0;
	uint32_t tsec, tnsec;
	char *level_mark[]=DBGMSG_LEVEL_MARK;

	if(level<0 || level>UBL_DEBUGV) return -1;
	if(check_cat_index(cat_index, __func__)) return -1;
	if((level > ubcd.logmsgd[cat_index].clevel &&
	    level > ubcd.logmsgd[cat_index].dlevel) || (level==0)) return 0;
	addts = ubcd.logmsgd[cat_index].flags & UBL_TS_BIT_FIELDS;
	if(!addts) addts = flags & UBL_TS_BIT_FIELDS;
	if(ubcd.threadding) ubcd.cbset.mutex_lock(ubcd.gmutex);
	if(addts!=UB_CLOCK_DEFAULT){
		if(ubcd.cbset.gettime64) ts64=ubcd.cbset.gettime64(addts);
		tsec=ts64/UB_SEC_NS;
		tnsec=ts64%UB_SEC_NS;
		console_debug_print(level <= ubcd.logmsgd[cat_index].clevel,
				    level <= ubcd.logmsgd[cat_index].dlevel,
				    "%s:%s:%06u-%06u:", level_mark[level],
				    ubcd.logmsgd[cat_index].category_name,
				    tsec%1000000, tnsec/1000);
	}else{
		console_debug_print(level <= ubcd.logmsgd[cat_index].clevel,
				    level <= ubcd.logmsgd[cat_index].dlevel,
				    "%s:%s:", level_mark[level],
				    ubcd.logmsgd[cat_index].category_name);
	}
	va_start(ap, format);
	console_debug_print_va(level <= ubcd.logmsgd[cat_index].clevel,
			       level <= ubcd.logmsgd[cat_index].dlevel,
			       format, ap);
	va_end(ap);
	if(ubcd.threadding) ubcd.cbset.mutex_unlock(ubcd.gmutex);
	return 0;
}

bool ub_clog_on(int cat_index, ub_dbgmsg_level_t level)
{
	if(level<=0) return false;
	if(check_cat_index(cat_index, __func__)) return false;
	if(level > ubcd.logmsgd[cat_index].clevel) return false;
	return true;
}

bool ub_dlog_on(int cat_index, ub_dbgmsg_level_t level)
{
	if(level<=0) return false;
	if(check_cat_index(cat_index, __func__)) return false;
	if(level > ubcd.logmsgd[cat_index].dlevel) return false;
	return true;
}

int ub_log_change(int cat_index, ub_dbgmsg_level_t clevel, ub_dbgmsg_level_t dlevel)
{
	if(check_cat_index(cat_index, __func__)) return -1;
	ubcd.logmsgd[cat_index].clevel_saved=ubcd.logmsgd[cat_index].clevel;
	ubcd.logmsgd[cat_index].clevel=clevel;
	ubcd.logmsgd[cat_index].dlevel_saved=ubcd.logmsgd[cat_index].dlevel;
	ubcd.logmsgd[cat_index].dlevel=dlevel;
	return 0;
}

int ub_log_return(int cat_index)
{
	if(check_cat_index(cat_index, __func__)) return false;
	ubcd.logmsgd[cat_index].clevel=ubcd.logmsgd[cat_index].clevel_saved;
	ubcd.logmsgd[cat_index].dlevel=ubcd.logmsgd[cat_index].dlevel_saved;
	return 0;
}

void ub_log_flush(void)
{
	if(ubcd.cbset.console_out) ubcd.cbset.console_out(true, "");
	if(ubcd.cbset.debug_out) ubcd.cbset.debug_out(true, "");
}
