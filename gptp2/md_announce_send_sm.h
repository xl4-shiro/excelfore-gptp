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
#ifndef __MD_ANNOUNCE_SEND_SM_H_
#define __MD_ANNOUNCE_SEND_SM_H_

typedef struct md_announce_send_data md_announce_send_data_t;

typedef struct md_announce_send_stat_data{
	uint32_t announce_send;
}md_announce_send_stat_data_t;

void *md_announce_send_sm(md_announce_send_data_t *sm, uint64_t cts64);

void md_announce_send_sm_init(md_announce_send_data_t **sm,
	int domainIndex, int portIndex,
	gptpnet_data_t *gpnetd,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerPortGlobal *bppg);

int md_announce_send_sm_close(md_announce_send_data_t **sm);

void *md_announce_send_sm_mdAnnouncSend(md_announce_send_data_t *sm,
					PTPMsgAnnounce *msgAnnounce, uint64_t cts64);

void md_announce_send_stat_reset(md_announce_send_data_t *sm);

md_announce_send_stat_data_t *md_announce_send_get_stat(md_announce_send_data_t *sm);

#endif
