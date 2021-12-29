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
#include "ll_gptpsupport.h"
#include <time.h>
#include <sys/timex.h>

#define PTPDEV_CLOCKFD 3
#define FD_TO_CLOCKID(ptpfd) ((~(clockid_t) (ptpfd) << 3) | PTPDEV_CLOCKFD)

PTPFD_TYPE ptpdev_clock_open(char *ptpdev, int permission)
{
	return open(ptpdev, permission);
}

int ptpdev_clock_close(PTPFD_TYPE fd)
{
	return close(fd);
}

int ptpdev_clock_gettime(PTPFD_TYPE fd, int64_t *ts)
{
	struct timespec tspec;

	clock_gettime(FD_TO_CLOCKID(fd), &tspec);
	*ts = UB_TS2NSEC(tspec);

	return 0;
}

int ptpdev_clock_settime(PTPFD_TYPE fd, int64_t *ts)
{
	struct timespec tspec;

	UB_NSEC2TS(*ts,tspec);
	clock_settime(FD_TO_CLOCKID(fd), &tspec);

	return 0;
}

int ptpdev_clock_adjtime(PTPFD_TYPE ptpfd, int adjppb)
{
	struct timex tmx;

	memset(&tmx, 0, sizeof(tmx));
	tmx.modes=ADJ_FREQUENCY;
	tmx.freq=(long)(adjppb * 65.536);

	return clock_adjtime(FD_TO_CLOCKID(ptpfd), &tmx);
}
