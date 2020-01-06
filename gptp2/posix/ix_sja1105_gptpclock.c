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
#include <stdio.h>
#include <fcntl.h>
#include "gptpclock.h"

// 'gptp_clock_adjtime' is provided in ix_sja1150_gptpnet.c

ptpclock_state_t gptp_get_ptpfd(char *ptpdev, int *ptpfd)
{
	if(sscanf(ptpdev, "ptpspi%d", ptpfd)!=1) return PTPCLOCK_NOWORK;
	return PTPCLOCK_RDWR;
}

int  gptp_close_ptpfd(int ptpfd)
{
	return 0;
}

int gptpclock_settime_str(char *tstr, int clockIndex, uint8_t domainNumber)
{
	UB_LOG(UBL_ERROR,"%s:not supported for SJA1105\n",__func__);
	return -1;
}
