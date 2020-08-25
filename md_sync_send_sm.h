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
#ifndef __MD_SYNC_SEND_SM_H_
#define __MD_SYNC_SEND_SM_H_

typedef struct md_sync_send_data md_sync_send_data_t;

typedef struct md_sync_send_stat_data{
	uint32_t sync_send;
	uint32_t sync_fup_send;
}md_sync_send_stat_data_t;

int md_sync_send_sm(md_sync_send_data_t *sm, uint64_t cts64);

void md_sync_send_sm_init(md_sync_send_data_t **sm,
	int domainIndex, int portIndex,
	gptpnet_data_t *gptpnet,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	MDEntityGlobal *mdeg);

int md_sync_send_sm_close(md_sync_send_data_t **sm);

int md_sync_send_sm_mdSyncSend(md_sync_send_data_t *sm,
			       MDSyncSend *mdSyncSend, uint64_t cts64);

void md_sync_send_sm_txts(md_sync_send_data_t *sm, event_data_txts_t *edtxts,
			  uint64_t cts64);

void md_sync_send_stat_reset(md_sync_send_data_t *sm);

md_sync_send_stat_data_t *md_sync_send_get_stat(md_sync_send_data_t *sm);

#endif
