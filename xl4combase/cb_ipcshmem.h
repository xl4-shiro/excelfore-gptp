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
 * @defgroup IPC utility functions
 * @{
 * @file cb_ipcshmem.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief IPC utility functions
 */

#ifndef __CB_IPCSHMEM_H_
#define __CB_IPCCSHMEM_H_

/**
 * @brief get new shared memory
 * @param memfd	shared memory file descriptor
 * @param shmname	shared memory name to be used for shm_open
 * @param size	size of shared memroy
 * @param flag flag to be used for shm_open i.e O_RDONLY, O_RDWR,
 * O_WRONLY, O_CREAT, O_EXCL,O_TRUNC.
 * @see @c man @c shm_open().
 * @return a reference to the new shared memory, NULL if error
 *
 */
void *cb_get_shared_mem(int *memfd, const char *shmname, size_t size, int flag);

/**
 * @brief close shared memory
 * @param mem	mapped shared memory
 * @param memfd	shared memory file descriptor
 * @param shmname	shared memory name
 * @param size	size of shared memory
 * @param unlink flag indicating if shared memory will be unlinked
 * @return 0 on success, -1 on error
 */
int cb_close_shared_mem(void *mem, int *memfd, const char *shmname, size_t size, bool unlink);

#endif
/** @}*/
