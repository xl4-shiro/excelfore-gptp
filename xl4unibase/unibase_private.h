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
 * @file unibase_private.h
 * @author Shiro Ninomiya<shiro@excelfore.com>
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @brief private data types in unibase layer
 */
#ifndef __UNIBASE_PRIVATE_H_
#define __UNIBASE_PRIVATE_H_

#include "unibase.h"

#define UB_LOGCAT UB_UNIBASE_LOGCAT
#define UB_LOGTSTYPE UB_CLOCK_REALTIME
#define UB_CHARS_IN_LINE 255
#define CATEGORY_NAME_CHAR_MAX 7

typedef struct ub_logmsg_data{
	ub_dbgmsg_level_t clevel;
	ub_dbgmsg_level_t clevel_saved;
	ub_dbgmsg_level_t dlevel;
	ub_dbgmsg_level_t dlevel_saved;
	uint32_t flags;
	char category_name[CATEGORY_NAME_CHAR_MAX+1];
} ub_logmsg_data_t;

typedef struct unibase_cstd {
	int locked;
	bool threadding;
	unibase_cb_set_t cbset;
	ub_logmsg_data_t logmsgd[MAX_LOGMSG_CATEGORIES];
	ub_logmsg_data_t logmsgd_ovrd;
	int log_categories;
	void *gmutex;
} unibase_cstd_t;

extern unibase_cstd_t ubcd;

#endif
