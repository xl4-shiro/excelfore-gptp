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
#ifndef __GPTPCLOCK_VIRTUAL_H_
#define __GPTPCLOCK_VIRTUAL_H_

#include "ll_gptpsupport.h"

PTPFD_TYPE gptp_vclock_alloc_fd(char *ptpdev);

int gptp_vclock_free_fd(PTPFD_TYPE ptpfd);

uint64_t gptp_vclock_gettime(PTPFD_TYPE ptpfd);

int gptp_vclock_settime(PTPFD_TYPE ptpfd, uint64_t ts64);

int gptp_vclock_adjtime(PTPFD_TYPE ptpfd, int adjppb);

#endif //__GPTPCLOCK_VIRTUAL_H_
