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
#ifndef __MD_SYNC_RECEIVE_SM_H_
#define __MD_SYNC_RECEIVE_SM_H_

typedef struct md_sync_receive_data md_sync_receive_data_t;

typedef struct md_sync_receive_stat_data{
	uint32_t sync_rec;
	uint32_t sync_rec_valid;
	uint32_t sync_fup_rec;
	uint32_t sync_fup_rec_valid;
}md_sync_receive_stat_data_t;

void *md_sync_receive_sm(md_sync_receive_data_t *sm, uint64_t cts64);

void md_sync_receive_sm_init(md_sync_receive_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	MDEntityGlobal *mdeg);

int md_sync_receive_sm_close(md_sync_receive_data_t **sm);

void md_sync_receive_sm_recv_sync(md_sync_receive_data_t *sm, event_data_recv_t *edrecv,
				  uint64_t cts64);

void *md_sync_receive_sm_recv_fup(md_sync_receive_data_t *sm, event_data_recv_t *edrecv,
				  uint64_t cts64);

void md_sync_receive_stat_reset(md_sync_receive_data_t *sm);

md_sync_receive_stat_data_t *md_sync_receive_get_stat(md_sync_receive_data_t *sm);

#endif
