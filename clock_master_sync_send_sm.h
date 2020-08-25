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
#ifndef __CLOCK_MASTER_SYNC_SEND_SM_H_
#define __CLOCK_MASTER_SYNC_SEND_SM_H_

typedef struct clock_master_sync_send_data clock_master_sync_send_data_t;

void *clock_master_sync_send_sm(clock_master_sync_send_data_t *sm, uint64_t cts64);

void clock_master_sync_send_sm_init(clock_master_sync_send_data_t **sm,
				    int domainIndex,
				    PerTimeAwareSystemGlobal *ptasg);

int clock_master_sync_send_sm_close(clock_master_sync_send_data_t **sm);

#endif
