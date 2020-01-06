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
 * @defgroup unibase_binding bind unibase to a specific platform
 * @{
 * @file unibase_binding.h
 * @author Shiro Ninomiya <shiro@excelfore.com>
 * @copyright Copyright (C) 2019 Excelfore Corporation
 *
 * @brief functions to bind unibase to a specific platform
 *
 * - This layer support binding to a specific platform.
 * - The implementation is platform specific.
 * - Using this binding is optional, and for the unibase library,
 * a diferent binding layer can be used.
 */

#ifndef __UNIBASE_BINDING_H_
#define __UNIBASE_BINDING_H_

/**
 * @brief ubb_memory_out_init uses this value to allocate internal buffer when
 *	'mem' parameter is NULL.
 */
#define UBB_DEFAULT_DEBUG_LOG_MEMORY (64*1024)

/**
 * @brief ubb_memory_out always add this end mark at the end of printing.
 */
#define UBB_MEMOUT_ENDMARK "---###---"

/**
 * @brief ubb_default_initpara uses this string as 'ub_log_initstr'
 */
#define UBB_DEFAULT_LOG_INITSTR "4,ubase:45"

/**
 * @brief get a string from an environment variable
 */
#ifndef UBB_GETENV
#define UBB_GETENV getenv
#endif

/**
 * @brief initialize the internal memory_out function
 * @param mem	use this memory as a buffer for memory_out.
 *	if mem is NULL, allocate memory internally for 'size' bytes
 * @param size	size of memory to be used for memory_out
 * @return 0 on success, -1 on error
 * @note if this is not explicitly called, it is called internally with mem=NULL
 *	and size=UBB_DEFAULT_DEBUG_LOG_MEMORY
 */
int ubb_memory_out_init(char *mem, int size);

/**
 * @brief de-initialize memory_out function
 * @return 0 on success, -1 on error
 * @note this must be called whichever case of the explicit call or
 * 	the internal call of 'ubb_memory_out_init'
 */
int ubb_memory_out_close(void);

/**
 * @brief return the memory pointer of memory_out
 * @return memory pointer
 */
char *ubb_memory_out_buffer(void);

/**
 * @brief return the most recent output line in memory_out
 * @param str	pointer of string pointer to return result string
 * @param size	pointer of integer pointer to return result size
 * @return 0 on success, -1 on error
 */
int ubb_memory_out_lastline(char **str, int *size);

/**
 * @brief return the all written line data in the buffer
 * @param str	allocated pointer of the returned data
 * @param size	size of the returned data
 * @return 0 on success, -1 on error
 * @note returned pointer in *str must be freed by the caller.
 * 	the allocation size is the same as the size in ubb_memory_out_init
 */
int ubb_memory_out_alldata(char **rstr, int *size);

/**
 * @brief return the default initialization parameters supported in the binding layer
 * @param init_para	pointer of unibase initialization parameter
 */
void ubb_default_initpara(unibase_init_para_t *init_para);

#endif
/** @}*/
