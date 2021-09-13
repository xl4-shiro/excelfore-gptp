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
#ifndef __GPTPCLOCK_H_
#define __GPTPCLOCK_H_
#include <stdlib.h>
#include "gptpbasetypes.h"
#include "gptp_defaults.h"
#include "ll_gptpsupport.h"
#include "gptpipc.h"

#define GPTP_VIRTUAL_PTPDEV_FDBASE 3018
#define GPTP_VIRTUAL_PTPDEV_FDMAX 3117

typedef struct gptpclock_data gptpclock_data_t;

typedef enum {
	PTPCLOCK_NOWORK = 0,
	PTPCLOCK_NOACCESS,
	PTPCLOCK_RDONLY,
	PTPCLOCK_RDWR,
} ptpclock_state_t;

typedef struct gptp_clock_ppara {
	char ptpdev[MAX_PTPDEV_NAME];
	uint8_t domainNumber; //when accessed by domainIndex, need this domainNumber
	int64_t offset64;
	bool gmsync;
	uint32_t gmchange_ind;
	int64_t last_setts64;
	double adjrate;
} gptp_clock_ppara_t;

typedef struct gptp_master_clock_shm_head {
	int max_domains;
	int active_domain;
	CB_THREAD_MUTEX_T mcmutex;
}gptp_master_clock_shm_head_t;

typedef struct gptp_master_clock_shm {
	gptp_master_clock_shm_head_t head;
	gptp_clock_ppara_t gcpp[];
} gptp_master_clock_shm_t;

#define GPTP_MAX_SIZE_SHARED_MEMNAME 32
#define GPTP_MASTER_CLOCK_SHARED_MEM "/gptp_mc_shm"
#define GPTP_MASTER_CLOCK_MUTEX_TIMEOUT (10*UB_MSEC_NS)
static inline int gptpclock_mutex_trylock(CB_THREAD_MUTEX_T *mutex)
{
	struct timespec mtots;
	uint64_t mtout;
	if(CB_THREAD_MUTEX_TRYLOCK(mutex)){
		mtout=ub_rt_gettime64()+GPTP_MASTER_CLOCK_MUTEX_TIMEOUT;
		UB_NSEC2TS(mtout, mtots);
		if(CB_THREAD_MUTEX_TIMEDLOCK(mutex, &mtots)){
			// the mutex is on hold such a long time; the holder must crash
			UB_LOG(UBL_WARN, "%s:the process is very slow,"
			       " or some gptp2d client may crash\n", __func__);
			return -1;
		}
	}
	return 0;
}

int gptpclock_init(int max_domains, int max_ports);
void gptpclock_close(void);
int gptpclock_add_clock(int clockIndex, char *ptpdev, int domainIndex,
			uint8_t domainNumber, ClockIdentity id);
int gptpclock_del_clock(int clockIndex, uint8_t domainNumber);
int gptpclock_apply_offset(int64_t *ts64, int clockIndex, uint8_t domainNumber);
int gptpclock_setts64(int64_t ts64, int clockIndex, uint8_t domainNumber);
int gptpclock_setadj(int adjvppb, int clockIndex, uint8_t domainNumber);
void gptpclock_print_clkpara(ub_dbgmsg_level_t level);
int gptpclock_mode_master(int clockIndex, uint8_t domainNumber);
int gptpclock_mode_slave_main(int clockIndex, uint8_t domainNumber);
int gptpclock_mode_slave_sub(int clockIndex, uint8_t domainNumber);

int64_t gptpclock_getts64(int clockIndex, uint8_t domainNumber);
int64_t gptpclock_getoffset64(int clockIndex, uint8_t domainNumber);
int64_t gptpclock_gethwts64(int clockIndex, uint8_t domainNumber);
int gptpclock_tsconv(int64_t *ts64, int clockIndex, uint8_t domainNumber,
		     int clockIndex1, uint8_t domainNumber1);
uint8_t *gptpclock_clockid(int clockIndex, uint8_t domainNumber);
int gptpclock_rate_same(int clockIndex, uint8_t domainNumber,
			int clockIndex1, uint8_t domainNumber1);
int gptpclock_get_clock_params(int clockIndex, uint8_t domainNumber,
			       uint16_t *gmTimeBaseIndicator,
			       ScaledNs *lastGmPhaseChange,
			       double *lastGmFreqChange);
int gptpclock_setoffset64(int64_t ts64, int clockIndex, uint8_t domainNumber);
int gptpclock_active_domain_switch(int domainIndex);
int gptpclock_active_domain_status(void);
int gptpclock_set_gmsync(int clockIndex, uint8_t domainNumber, bool becomeGM);
int gptpclock_get_gmsync(int clockIndex, uint8_t domainNumber);
int gptpclock_reset_gmsync(int clockIndex, uint8_t domainNumber);
int gptpclock_set_gmchange(int domainNumber, ClockIdentity clockIdentity);
int gptpclock_get_gmchange_ind(int domainNumber);
uint32_t gptpclock_get_event_flags(int clockIndex, uint8_t domainNumber);
int gptpclock_get_ipc_clock_data(int clockIndex, uint8_t domainNumber, gptpipc_clock_data_t *cd);
void gptpclock_set_gmstable(int domainIndex, bool stable);
bool gptpclock_get_gmstable(int domainIndex);
int gptpclock_active_domain(void);
int64_t gptpclock_d0ClockfromRT(int clockIndex);

/**
 * @brief set clockIndex to the thisClock, clockIndex=0 is set as the master clock
 * @result 0:success, -1:error
 * @param clockIndex
 * @param comainNumber
 * @param set_clock_para	if true, the phase offset in the master clock is moved
 * to thisClock. the parameters in SLAVE_SUB mode time are also moved.
 * @Note the offset in the master clock is moved into thisClock, which prevent of a big
 * jump from the old GM
 * clockIndex=0 is set to PTPCLOCK_MASTER mode
 * thisClock becomes PTPCLOCK_SLAVE_MAIN mode, but when the same clockIndex is already
 * used as PTPCLOCK_SLAVE_MAIN in a different domain, it becomes PTPCLOCK_SLAVE_SUB mode.
 *
 * when thisClock becomes PTPCLOCK_SLAVE_MAIN, offset and adjrate are set into HW config.
 * when thisClock becomes PTPCLOCK_SLAVE_SUB, offset and adjrate are set into SW config.
 *
 * When this device becomes a new GM, this function should be called, then GM time phase
 * continues from the previous GM.
 */
int gptpclock_set_thisClock(int clockIndex, uint8_t domainNumber, bool set_clock_para);

/***************************************
 * functions supported in lower layers
 ***************************************/
ptpclock_state_t gptp_get_ptpfd(char *ptpdev, PTPFD_TYPE *ptpfd);
int gptp_close_ptpfd(PTPFD_TYPE ptpfd);
int gptp_clock_adjtime(PTPFD_TYPE ptpfd, int adjppb);
uint64_t gptpclock_gettime_ptpvfd(PTPFD_TYPE ptpfd);
int gptpclock_settime_ptpvfd(PTPFD_TYPE ptpfd, uint64_t ts64);

/**
 * @brief settimeofday by "year:mon:day:hour:min:sec"
 * @result 0:success, -1:error
 * @param tstr	timeofday string in "year:mon:day:hour:min:sec" string format
 */
int gptpclock_settime_str(char *tstr, int clockIndex, uint8_t domainNumber);

#endif
