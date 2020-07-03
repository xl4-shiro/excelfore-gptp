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
 * @addtogroup gptp
 * @{
 * @file gptpmasterclock.h
 * @author Shiro Ninomiya
 * @copyright Copyright (C) 2017-2018 Excelfore Corporation
 * @brief file contains gptp master clock related functions.
 *
 */

#ifndef __GPTPMASTERCLOCK_H_
#define __GPTPMASTERCLOCK_H_

/**
 * @brief Pre-initialize to get the gptp clock.
 * @param object platform specific object, refer to the platforms supported below.
 * @return -1 on error, 0 on Successful initialization.
 *
 * @note This API needs to called only on platforms that recommends against using
 * shared memory.
 *
 * The following are known target platforms requiring this API:
 *
 * GHS INTEGRITY:
 *
 *     usage: gptpmasterclock_preinit(Semaphore object)
 *           The semaphore object used to synchronize communication between
 *           gptp2d VAS and VASes the uses libgptp2If library. User can get
 *           this shared semaphore object by calling the INTEGRITY API
 *           SemaphoreObjectNumber(TheObjectIndex). Where TheObjectIndex
 *           is a LINK object number defined in the application's ".int"
 *           file which is linked to the binary semaphore defined in the
 *           gptp2d '.int' file.
 *           e.g: gptpmasterclock_preinit(SemaphoreObjectNumber(10));
 *
 * This API doesn't need to be called in platforms other than the above mentioned.
 * Calling this API in other platforms has no effect.
 */
int gptpmasterclock_preinit(void *object);

/**
 * @brief initialize to get gptp clock from gptp2 daemon.
 * if previously initialized, it will simply return 0.
 * @param shemem_name	shared memory node name. set NULL to use the default
 * @return -1 on error, 0 on Successful initialization.
 * @note   argument 'shmem_name' will not be used in platforms that recommends against
 * using shared memory (e.g GHS INTEGRITY). Pass NULL is such case.
 */
int gptpmasterclock_init(const char *shmem_name);

/**
 * @brief close gptpmasterclcock
 * @return -1: on error, 0:on successfull
 */
int gptpmasterclock_close(void);

/**
 * @brief return the domainIndex which is currently used as systeme wide gptp clock.
 * @return domainIndex, -1: error
 */
int gptpmasterclock_gm_domainIndex(void);

/**
 * @brief return the domainNumber which is currently used as system wide gptp clock.
 * @return domainIndex, -1: error, domain number on success.
 */
int gptpmasterclock_gm_domainNumber(void);

/**
 * @brief get 64-bit nsec unit ts of system wide gptp clock
 * @return 0 on success, -1 on error
 *
 */
int64_t gptpmasterclock_getts64(void);

/**
 * @brief Wait until tts comes.
 * @return -1: error, 1:already passed, 2:in vclose, 3:farther than toofar seconds,
 * 0:returns from nanosleep(has been waited to very close timing)
 * @param tts	target time in nano second unit
 * @param vclose        nano second unit; treated as very close, and stop waiting
 * even ttv is still in future
 * @param toofar        nano second unit; treated as too far, and stop waiting
 */
int gptpmasterclock_wait_until_ts64(int64_t tts, int64_t vclose, int64_t toofar);

/**
 * @brief expand 32-bit nsec time to 64 bit with aligning to gptp clock.
 * @param timestamp timestamp which we are going to convert into 32bit to 64 bit.
 * @return expanded time
 * @note        a range of  -2.147 to 2.147 secconds can be correctly aligned
 *
 */
uint64_t gptpmasterclock_expand_timestamp(uint32_t timestamp);

/**
 * @brief get GM change indicator, the number is incremented whenever GM is changed
 * @return GM change indicator value, -1: on error
 */
int gptpmasterclock_gmchange_ind(void);

/**
 * @brief get maximum number of domains
 * @param void
 * @return returns availabe number of domains.
 */
int gptpmasterclock_get_max_domains(void);

/**
 * @brief get a synchronized clock value on specific domain
 * @param ts64	pointer to return clock value
 * @param domainIndex domain index number
 * @return 0 on success, -1 on error.
 */
int gptpmasterclock_get_domain_ts64(int64_t *ts64, int domainIndex);

/**
 * @brief print phase offset for all domains
 */
void gptpmasterclock_dump_offset(void);

#endif
/** @}*/
