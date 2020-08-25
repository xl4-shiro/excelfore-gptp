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
#ifndef __LL_GPTPSUPPORT_H_
#define __LL_GPTPSUPPORT_H_

#ifdef LINUX_PTPCLOCK
#include <unistd.h>
#define PTPDEV_CLOCKFD 3
#define FD_TO_CLOCKID(ptpfd) ((~(clockid_t) (ptpfd) << 3) | PTPDEV_CLOCKFD)
#define PTPDEV_CLOCK_OPEN(x,y) (strstr(x, CB_VIRTUAL_PTPDEV_PREFIX)==x)?\
	GPTP_VIRTUAL_PTPDEV_FDBASE:open(x,y)
static inline int _zero_return(void){return 0;}
#define PTPDEV_CLOCK_CLOSE(x) (x==GPTP_VIRTUAL_PTPDEV_FDBASE)?_zero_return():close(x)
#define PTPFD_TYPE int
#define PTPFD_VALID(ptpfd) (ptpfd>0)

#elif defined(GHINTEGRITY)
/* Green Hills INTEGRITY definitions */
#include <unistd.h>
#include "xl4combase/combase.h"
#include "ghintg/gh_ptpclock.h"
#define PTPFD_TYPE void*
#define PTPFD_VALID(ptpfd) (ptpfd!=NULL)
#define PTPDEV_CLOCK_OPEN(x,y) gh_ptpclock_open(x,y)
#define PTPDEV_CLOCK_CLOSE(x)  gh_ptpclock_close(x)

#endif

#ifndef MAIN_RETURN
#define MAIN_RETURN(x) return x
#endif

#if (defined(LINUX_PTPCLOCK) && !defined(SJA1105))
#define GPTP_CLOCK_GETTIME(x,y) {					\
	if(x<GPTP_VIRTUAL_PTPDEV_FDBASE || x>GPTP_VIRTUAL_PTPDEV_FDMAX){ \
		struct timespec ts;					\
		clock_gettime(FD_TO_CLOCKID(x), &ts);			\
		y=UB_TS2NSEC(ts);					\
	}else{								\
		y=gptpclock_gettime_ptpvfd(x);				\
	}								\
	}
#define GPTP_CLOCK_SETTIME(x,y) {					\
	if(x<GPTP_VIRTUAL_PTPDEV_FDBASE || x>GPTP_VIRTUAL_PTPDEV_FDMAX){ \
		struct timespec ts;					\
		UB_NSEC2TS(y,ts);					\
		clock_settime(FD_TO_CLOCKID(x), &ts);			\
	}else{								\
		gptpclock_settime_ptpvfd(x, y);				\
	}								\
	}
// virtual ptpdev clock has to use PTPCLOCK_RDONLY mode.
#define GPTP_CLOCK_GETTIMEMS(x,y) {					\
	if(x<GPTP_VIRTUAL_PTPDEV_FDBASE || x>GPTP_VIRTUAL_PTPDEV_FDMAX){ \
		struct timespec ts;					\
		clock_gettime(FD_TO_CLOCKID(x), &ts);			\
		y=UB_TS2NSEC(ts);					\
	}else{								\
		y=ub_rt_gettime64();					\
	}								\
	}
#elif defined(SJA1105)
extern int gptp_clock_gettime(int fd, int64_t *ts);
extern int gptp_clock_settime(int fd, const int64_t *ts);
#define GPTP_CLOCK_GETTIME(x,y) gptp_clock_gettime(x,&y)
#define GPTP_CLOCK_SETTIME(x,y) gptp_clock_settime(x,&y)

#elif defined(GHINTEGRITY)
/* Green Hills INTEGRITY definitions */
#define REGULAR_PTPDEV(x) (x<(PTPFD_TYPE)GPTP_VIRTUAL_PTPDEV_FDBASE || x>(PTPFD_TYPE)GPTP_VIRTUAL_PTPDEV_FDMAX)
#define GPTP_CLOCK_GETTIME(x,y) { \
	if(REGULAR_PTPDEV(x)){		\
		struct timespec ts;			\
		gh_ptpclock_gettime(x,&ts); \
		y=UB_TS2NSEC(ts);			\
	}else{							\
		y=gptpclock_gettime_ptpvfd(x);	\
	}								\
	}

#define GPTP_CLOCK_SETTIME(x,y) { \
	if(REGULAR_PTPDEV(x)){		\
		struct timespec ts;		\
		UB_NSEC2TS(y,ts);		\
		gh_ptpclock_settime(x,&ts);					\
	}else{											\
		gptpclock_settime_ptpvfd(x, y);	\
	}								\
	}

#define GPTP_CLOCK_GETTIMEMS(x,y) { \
	if(REGULAR_PTPDEV(x)){		\
		struct timespec ts;			\
		gh_ptpclock_gettime(x,&ts); \
		y=UB_TS2NSEC(ts);			\
	}else{							\
		y=ub_rt_gettime64();		\
	}								\
	}

#else
extern int gptp_clock_gettime(int fd, struct timespec *ts);
extern int gptp_clock_settime(int fd, const struct timespec *ts);
static inline int GPTP_CLOCK_GETTIME(int fd, int64_t *ts64)
{
	struct timespec ts;
	int res;
	res=gptp_clock_gettime(fd,&ts);
	*ts64=UB_TS2NSEC(ts);
	return res;
}
static inline int GPTP_CLOCK_SETTIME(int fd, int64_t ts64)
{
	struct timespec ts;
	UB_NSEC2TS(ts64,ts);
	return gptp_clock_settime(fd,&ts);
}
#endif

/**
 * @brief enables H/W Timestamping for socket
 * @param dev	ethernet device name like 'eth0', 'eno1', 'enp2s0'.
 * @return 0 on success, -1 on error
 */
int ll_set_hw_timestamping(CB_SOCKET_T cfd, const char *dev);

/**
 * @brief disables hardware timestamping for socket
 * @param dev	ethernet device name like 'eth0'
 * @return 0 on success, -1 on error
 */
int ll_close_hw_timestamping(CB_SOCKET_T cfd, const char *dev);

/**
 * @brief get Tx timestamp from msg
 * @param tts	return timestamp
 * @return 0 on success, -1 on error
 */
int ll_txmsg_timestamp(void *p, int64_t *ts64);

/**
 * @brief get Rx timestamp from msg
 * @param ts	return timestamp
 * @return 0 on success, -1 on error
 */
int ll_recv_timestamp(void *p, int64_t *ts64);

#endif
