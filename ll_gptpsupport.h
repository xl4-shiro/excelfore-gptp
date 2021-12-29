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

#ifndef PTPFD_TYPE
#define PTPFD_TYPE int
#define PTPFD_VALID(ptpfd) (ptpfd>=0)
#define PTPFD_INVALID -1
#define PRiFD "%d"
#endif

/*******************************************************
 * functions supported in the platform dependent layer
 *******************************************************/
PTPFD_TYPE ptpdev_clock_open(char *ptpdev, int permission);
int ptpdev_clock_close(PTPFD_TYPE fd);
int ptpdev_clock_gettime(PTPFD_TYPE fd, int64_t *ts64);
int ptpdev_clock_settime(PTPFD_TYPE fd, int64_t *ts64);
int ptpdev_clock_adjtime(PTPFD_TYPE ptpfd, int adjppb);
/*******************************************************/

/*
 * Virtual clock is a clock that has prefix name as #CB_VIRTUAL_PTPDEV_PREFIX
 * and fd range from #GPTP_VIRTUAL_PTPDEV_FDBASE to #GPTP_VIRTUAL_PTPDEV_FDMAX.
 */
#ifdef PTP_VIRTUAL_CLOCK_SUPPORT

#define VIRTUAL_CLOCKFD(fd) \
	((fd>=(PTPFD_TYPE)GPTP_VIRTUAL_PTPDEV_FDBASE) && \
	 (fd<=(PTPFD_TYPE)GPTP_VIRTUAL_PTPDEV_FDMAX))

#define VIRTUAL_CLOCKNAME(name) \
	(strstr(name, CB_VIRTUAL_PTPDEV_PREFIX)==name)

/*----------------------------------------------------------*/
/* These macros are used by application that use libgptp2 */

static inline int _zero_return(void){return 0;}

#define PTPDEV_CLOCK_OPEN(name, perm) \
	VIRTUAL_CLOCKNAME(name)?(PTPFD_TYPE)GPTP_VIRTUAL_PTPDEV_FDBASE:ptpdev_clock_open(name,perm)

#define PTPDEV_CLOCK_CLOSE(fd) \
	(fd==(PTPFD_TYPE)(GPTP_VIRTUAL_PTPDEV_FDBASE))?_zero_return():ptpdev_clock_close(fd)

#define PTPDEV_CLOCK_GETTIME(fd,ts64) \
{\
	if(!VIRTUAL_CLOCKFD(fd)){\
		ptpdev_clock_gettime(fd, (int64_t *)&(ts64));\
	}else{\
		ts64=ub_rt_gettime64();\
	}\
}
/*----------------------------------------------------------*/

/*----------------------------------------------------------*/
/* These macros are used by gptp2d */
uint64_t gptp_vclock_gettime(PTPFD_TYPE ptpfd);
int gptp_vclock_settime(PTPFD_TYPE ptpfd, uint64_t ts64);

#define GPTP_CLOCK_GETTIME(fd,ts64) \
{\
	if(!VIRTUAL_CLOCKFD(fd)){\
		ptpdev_clock_gettime(fd, (int64_t *)&(ts64));\
	}else{\
		ts64=gptp_vclock_gettime(fd);\
	}\
}

#define GPTP_CLOCK_SETTIME(fd,ts64) \
{\
	if(!VIRTUAL_CLOCKFD(fd)){\
		ptpdev_clock_settime(fd, (int64_t *)&(ts64));\
	}else{\
		gptp_vclock_settime(fd, ts64);\
	}\
}
/*----------------------------------------------------------*/

#else //PTP_VIRTUAL_CLOCK_SUPPORT

/*----------------------------------------------------------*/
/* These macros are used by application that use libgptp2 */
#define PTPDEV_CLOCK_OPEN(name, perm) ptpdev_clock_open(name,perm)
#define PTPDEV_CLOCK_CLOSE(fd) ptpdev_clock_close(fd)
#define PTPDEV_CLOCK_GETTIME(fd,ts64) ptpdev_clock_gettime(fd, (int64_t *)&(ts64))
/*----------------------------------------------------------*/

/*----------------------------------------------------------*/
/* These macros are used by gptp2d */
#define GPTP_CLOCK_GETTIME(fd,ts64) ptpdev_clock_gettime(fd, (int64_t *)&(ts64))
#define GPTP_CLOCK_SETTIME(fd,ts64) ptpdev_clock_settime(fd, (int64_t *)&(ts64))
/*----------------------------------------------------------*/

#endif //PTP_VIRTUAL_CLOCK_SUPPORT

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
