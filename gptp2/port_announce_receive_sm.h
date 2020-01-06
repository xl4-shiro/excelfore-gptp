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
#ifndef __PORT_ANNOUNCE_RECEIVE_SM_H_
#define __PORT_ANNOUNCE_RECEIVE_SM_H_

typedef struct port_announce_receive_data port_announce_receive_data_t;

void *port_announce_receive_sm(port_announce_receive_data_t *sm, uint64_t cts64);

void port_announce_receive_sm_init(port_announce_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal *bppg);

int port_announce_receive_sm_close(port_announce_receive_data_t **sm);

void port_announce_receive_sm_recv_announce(port_announce_receive_data_t *sm,
					    PTPMsgAnnounce *rcvdAnnounce,
					    uint64_t cts64);
#endif
