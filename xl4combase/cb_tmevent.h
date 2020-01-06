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
 * @defgroup timer and events utility functions
 * @{
 * @file cb_tmevent.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief timer and events utility functions
 */

#ifndef __CB_EVENT_H_
#define __CB_EVENT_H_

#ifdef CB_EVENT_NON_POSIX_H
/* non-posix platforms need to support necessary POSIX compatible
 * functions and types which are defined as CB_* macros below.
 * And provide them in a header file defined as CB_EVENT_NON_POSIX_H */
#include CB_EVENT_NON_POSIX_H
#else
#include <time.h>

/**
 *@brief micro second unit sleep
 */
#define CB_USLEEP(x) usleep(x)

/**
 *@brief second unit sleep
 */
#define CB_SLEEP(x) sleep(x)

/**
 *@brief nano second unit sleep
 */
static inline int cb_nanosleep64(int64_t ts64, int64_t *rts64)
{
	struct timespec ts, rts;
	int res;
	UB_NSEC2TS(ts64, ts);
	res=nanosleep(&ts, &rts);
	if(rts64) *rts64=UB_TS2NSEC(rts);
	return res;
}
#endif // CB_EVENT_NON_POSIX_H

/**
 * @brief handler for cb_timer_object
 * @note cb_timer is signal event based timer,
 * 	no thread, no callback happens with this.
 *	users need to call 'cb_timer_expired' to check the configured timer
 */
typedef struct cb_timer_object cb_timer_object_t;

/**
 * @brief enumeration used for timer to clear, decrement or no clear.
 * This enumeration has following members:
 * 1. CB_TIMER_NO_CLEAR		-> no action on timer value
 * 2. CB_TIMER_DECREMENT	-> timer value decrement
 * 3. CB_TIMER_ALL_CLEAR	-> set timer value to 0.
 */
typedef enum {
	CB_TIMER_NO_CLEAR,
	CB_TIMER_DECREMENT,
	CB_TIMER_ALL_CLEAR,
} cb_timer_clear_t;

/**
 * @brief creates timer.
 * @param tname	timer name
 * @return hanler of cb_timer_object_t
 */
cb_timer_object_t *cb_timer_create(char *tname);

/**
 * @brief close the timer
 * @param mtmo reference to cb_timer_object_t, returned by cb_timer_create().
 * @return void
 */
void cb_timer_close(cb_timer_object_t *mtmo);

/**
 * @brief start timer interval.
 * @param mtmo reference to cb_timer_object_t, created and returned by cb_timer_create().
 * @param value_ms initial value in msec.
 * @param interval_ms time interval in msec.
 * @return 0 on success, -1 on error
 * @note both value_ms and interval_ms is used by timer_settime.
 */
int cb_timer_start_interval(cb_timer_object_t *mtmo,
			    uint32_t value_ms, uint32_t interval_ms);

/**
 * @brief timer stop.
 * @param mtmo reference to cb_timer_object_t, which is created
 * and returned by cb_timer_create().
 * @return 0 on success, -1 on error.
 */
int cb_timer_stop(cb_timer_object_t *mtmo);

/**
 * @brief timer expired
 * @param mtmo reference to cb_timer_object_t, which is created
 * and returned by cb_timer_create().
 * @param clear	action by the definition of cb_timer_clear_t
 * @return true if timer not expired, false if timer expired.
 */
bool cb_timer_expired(cb_timer_object_t *mtmo, cb_timer_clear_t clear);


#endif
/** @}*/
