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
/**
 * @defgroup unibase general types and functions
 * @{
 * @file unibase.h
 * @author Shiro Ninomiya<shiro@excelfore.com>
 * @copyright Copyright (C) 2019 Excelfore Corporation
 *
 * @brief unibase general global header
 *
 * This header is intended to be included by application programs which
 * use 'unibase' library.  This is the only one header which needs to be
 * included.
 *
 * The categorized part like 'ub_logging' has separated header, and it
 * is included in this header.
 */
#ifndef __UNIBASE_H_
#define __UNIBASE_H_

#ifdef UB_OVERRIDE_H
#include UB_OVERRIDE_H
#endif

#include "unibase_macros.h"
#include "ub_logging.h"
#include "ub_esarray.h"
#include "ub_strutils.h"
#include "ub_llist.h"
#include "ub_confutils.h"

#define UB_UNIBASE_LOGCAT 0
/************************************************************
 * type declarations
 ************************************************************/
/**
 * @brief clock type which provides timestamp
 */
typedef enum {
	UB_CLOCK_DEFAULT=0,
	UB_CLOCK_REALTIME,
	UB_CLOCK_MONOTONIC,
	UB_CLOCK_GPTP,
}ub_clocktype_t;

typedef int (*ub_console_out)(bool flush, const char *str); //!< console output function
typedef int (*ub_debug_out)(bool flush, const char *str); //!< debug output function
typedef void* (*ub_mutex_init)(void); //!< mutex initialization function
typedef int (*ub_mutex_close)(void *mutex); //!< mutex de-initialization function
typedef int (*ub_mutex_lock)(void *mutex); //!< mutex lock function
typedef int (*ub_mutex_unlock)(void *mutex); //!< mutex unlock function
typedef uint64_t (*ub_gettime64)(ub_clocktype_t ctype); //!< 64-bit timestamp function

/**
 * @brief a set of callback functions to process platform specific tasks
 */
typedef struct unibase_cb_set {
	ub_console_out console_out; //!< console_out callback
	ub_debug_out debug_out; //!< debug_out callback
	ub_mutex_init mutex_init; //!< mutex_init callback
	ub_mutex_close mutex_close; //!< mutex_close callback
	ub_mutex_lock mutex_lock; //!< mutex_lock callback
	ub_mutex_unlock mutex_unlock; //!< mutex_unlock callback
	ub_gettime64 gettime64; //!< gettime64 callback
} unibase_cb_set_t;

/**
 * @brief initialization parameters
 */
typedef struct unibase_init_para {
	unibase_cb_set_t cbset; //!< a set of callback functions
	const char *ub_log_initstr; //!< look at 'ub_log_init' in 'ub_logging.h'
}unibase_init_para_t;

/************************************************************
 * functions
 ************************************************************/
/**
 * @brief initialize unibase
 * @param ub_init_para	a set of callback functions to support platfrom
 *	specific functions, and a string to initialize logging functions
 * @note unibase_init MUST be called at the beginning.
 * 	Calling unibase_init again after the first call are all ignored.
 *	If mutex_init=Null, it runs in a single thread mode.
 */

void unibase_init(unibase_init_para_t *ub_init_para);

/**
 * @brief claose unibase
 */
void unibase_close(void);

/**
 * @brief the abnormal termination with printing messages
 * @param mes1	printing message 1
 * @param mes2	printing message 2
 * @note unibase_init MUST be called at the beginning.
 * 	Calling unibase_init again after the first call are all ignored.
 *	If mutex_init=Null, it runs in a single thread mode.
 */
void ub_abort(const char *mes1, const char *mes2);

/**
 * @brief conditinal abort
 */
static inline void ub_assert(bool cond, const char *mes1, const char *mes2)
{
	if(cond) return;
	ub_abort(mes1, mes2);
}

/**
 * @brief get 64-bit REALTIME clock value
 * @return clock value
 */
uint64_t ub_rt_gettime64(void);

/**
 * @brief get 64-bit MONOTONIC clock value
 * @return clock value
 */
uint64_t ub_mt_gettime64(void);

/**
 * @brief get 64-bit PTP clock value
 * @return clock value
 */
uint64_t ub_gptp_gettime64(void);

#endif
/** @}*/
