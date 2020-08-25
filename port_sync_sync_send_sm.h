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
#ifndef __PORT_SYNC_SYNC_SEND_SM_H_
#define __PORT_SYNC_SYNC_SEND_SM_H_

typedef struct port_sync_sync_send_data port_sync_sync_send_data_t;

void *port_sync_sync_send_sm(port_sync_sync_send_data_t *sm, uint64_t cts64);

void port_sync_sync_send_sm_init(port_sync_sync_send_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg);

int port_sync_sync_send_sm_close(port_sync_sync_send_data_t **sm);

void *port_sync_sync_send_sm_portSyncSync(port_sync_sync_send_data_t *sm,
					  PortSyncSync *portSyncSync, uint64_t cts64);

#endif
