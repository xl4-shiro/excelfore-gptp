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
 * @defgroup thread functions binding
 * @{
 * @file cb_thread.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief bindings to posix thread, mutex and semaphore functions
 */

#ifndef __CB_THREAD_H_
#define __CB_THREAD_H_

#ifdef CB_THREAD_NON_POSIX_H
/* non-posix platforms need to support necessary POSIX compatible
 * functions and types which are defined as CB_* macros below.
 * And provide them in a header file defined as CB_THREAD_NON_POSIX_H */
#include CB_THREAD_NON_POSIX_H
#else
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

#define CB_THREAD_T pthread_t
#define CB_THREAD_CREATE(th, attr, func, arg) pthread_create(th, NULL, func, arg)
#define CB_THREAD_JOIN pthread_join
#define CB_THREAD_EXIT pthread_exit
#define CB_THREAD_MUTEX_T pthread_mutex_t
#define CB_THREAD_MUTEX_LOCK pthread_mutex_lock
#define CB_THREAD_MUTEX_TRYLOCK pthread_mutex_trylock
#define CB_THREAD_MUTEX_TIMEDLOCK pthread_mutex_timedlock
#define CB_THREAD_MUTEX_UNLOCK pthread_mutex_unlock
#define CB_THREAD_MUTEX_INIT pthread_mutex_init
#define CB_THREAD_MUTEX_DESTROY pthread_mutex_destroy
#define CB_THREAD_MUTEXATTR_T pthread_mutexattr_t
#define CB_THREAD_MUTEXATTR_INIT pthread_mutexattr_init
#define CB_THREAD_MUTEXATTR_SETPSHARED pthread_mutexattr_setpshared
#define CB_THREAD_PROCESS_SHARED PTHREAD_PROCESS_SHARED
#define CB_THREAD_PROCESS_PRIVATE PTHREAD_PROCESS_PRIVATE
#define CB_THREAD_COND_T pthread_cond_t
#define CB_THREAD_COND_INIT pthread_cond_init
#define CB_THREAD_COND_DESTROY pthread_cond_destroy
#define CB_THREAD_COND_WAIT pthread_cond_wait
#define CB_THREAD_COND_SIGNAL pthread_cond_signal
#define CB_THREAD_COND_BROADCAST pthread_cond_broadcast

#define CB_SEM_T sem_t
#define CB_SEM_INIT sem_init
#define CB_SEM_GETVALUE sem_getvalue
#define CB_SEM_WAIT sem_wait
#define CB_SEM_TRYWAIT sem_trywait
#define CB_SEM_TIMEDWAIT sem_timedwait
#define CB_SEM_POST sem_post
#define CB_SEM_DESTROY sem_destroy
#endif

/**
 * @brief data structure for thread attributes
 */
#define CB_XL4_THREAD_NAME_SIZE 128

/**
 * @brief parameters to initialize thread
 */
typedef struct cb_xl4_thread_attr{
	int pri;
	int stack;
	char name[CB_XL4_THREAD_NAME_SIZE];
} cb_xl4_thread_attr_t;

static inline int cb_xl4_thread_attr_init(cb_xl4_thread_attr_t *attr, int pri,
					  int stack, const char* name)
{
	if(!attr) return -1;
	memset(attr, 0, sizeof(cb_xl4_thread_attr_t));
	if(pri>0)
		attr->pri=pri;
	if(stack>0)
		attr->stack=stack;
	if(name)
		strncpy(attr->name, name, CB_XL4_THREAD_NAME_SIZE);
	return 0;
}

/**
 * @brief Wait points that blocks the execution of a thread at a specific code point until an event occurs
 *
 * The cb_waitpoint mechanism bundles the pthread_cond with pthread_mutex to ensure thread safety and re-entrant
 * blocking mechanism. Ideally, when pthread_cond is used, it is always required to guard its access with
 * pthread_mutex. This waitpoint abstracts the use of pthread_cond and pthread_mutex into single waitpoint structure.
 */
typedef struct cb_waitpoint {
        CB_THREAD_MUTEX_T lock;
        CB_THREAD_COND_T condition;
        uint64_t time;
        bool wakeup;
} cb_waitpoint_t;

/**
 * @brief Wait point initialization
 * @param wp object reference to a waitpoint
 */
static inline void cb_waitpoint_init(cb_waitpoint_t *wp){
        CB_THREAD_MUTEX_INIT(&wp->lock, NULL);
        CB_THREAD_COND_INIT(&wp->condition, NULL);
        wp->time=0;
        wp->wakeup=false;
}

/**
 * @brief Wait point de-initialization
 * @param wp object reference to a waitpoint
 */
static inline void cb_waitpoint_deinit(cb_waitpoint_t *wp){
        CB_THREAD_COND_DESTROY(&wp->condition);
        CB_THREAD_MUTEX_DESTROY(&wp->lock);
}

/**
 * @brief Wait point check
 * @param wp object reference to a waitpoint
 * @param time reference time, usually the current time
 * @return true if wait time has elapsed and not yet awoke, otherwise false
 */
static inline bool cb_waitpoint_check(cb_waitpoint_t *wp, uint64_t time){
        if(wp->wakeup){
                // check time diff, consider wrap around to uint64_t max value
                // unsigned substract typecasted to unsigned stores the 2s complement
                if(((uint64_t)(time-wp->time)<((uint64_t)1<<63))){
                        wp->wakeup=false;
                        return true;
                }
        }
        return false;
}

/**
 * @brief Wait point blocks until wake point has been awoken and after specified time
 * @param wp object reference to a waitpoint
 * @param time reference time, usually the current time
 * @return true if wait time has elapsed and not yet awoke, otherwise false
 *
 * This interface should be called within and cb_waitpoint_lock, which is then automatically
 * unlocked before entering blocking state and then automatically locked again when exiting the blocked state.
 */
static inline void cb_waitpoint_wakeup_at(cb_waitpoint_t *wp, uint64_t time, bool dosleep){
        wp->time=time;
        wp->wakeup = true;
        if(dosleep){
                CB_THREAD_COND_WAIT(&wp->condition, &wp->lock);
        }
}

/**
 * @brief Wait point wakeup to signal any one thread currently at this wait point
 * @param wp object reference to a waitpoint
 *
 * This interface should be called within and cb_waitpoint_lock/cb_waitpoint_unlock.
 */
#define cb_waitpoint_wakeup(wp) CB_THREAD_COND_SIGNAL(&(wp)->condition)

/**
 * @brief Wait point wakeup to signal all threads currently at this wait point
 * @param wp object reference to a waitpoint
 *
 * This interface should be called within and cb_waitpoint_lock/cb_waitpoint_unlock.
 */
#define cb_waitpoint_broadcast(wp) CB_THREAD_COND_BROADCAST(&(wp)->condition)

/**
 * @brief Wait point lock
 * @param wp object reference to a waitpoint
 */
#define cb_waitpoint_lock(wp) CB_THREAD_MUTEX_LOCK(&(wp)->lock)

/**
 * @brief Wait point unlock
 * @param wp object reference to a waitpoint
 */
#define cb_waitpoint_unlock(wp) CB_THREAD_MUTEX_UNLOCK(&(wp)->lock)

#endif
/** @}*/
