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
#ifndef __CLOCK_SLAVE_SYNC_SM_H_
#define __CLOCK_SLAVE_SYNC_SM_H_

typedef struct clock_slave_sync_data clock_slave_sync_data_t;

void *clock_slave_sync_sm(clock_slave_sync_data_t *sm, uint64_t cts64);

void clock_slave_sync_sm_init(clock_slave_sync_data_t **sm,
			      int domainIndex,
			      PerTimeAwareSystemGlobal *ptasg);

int clock_slave_sync_sm_close(clock_slave_sync_data_t **sm);

void *clock_slave_sync_sm_loclClockUpdate(clock_slave_sync_data_t *sm, uint64_t cts64);

void *clock_slave_sync_sm_portSyncSync(clock_slave_sync_data_t *sm,
				       PortSyncSync *portSyncSync, uint64_t cts64);

#endif
