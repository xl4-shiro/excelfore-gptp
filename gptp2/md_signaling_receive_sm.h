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
#ifndef __MD_SIGNALING_RECEIVE_SM_H_
#define __MD_SIGNALING_RECEIVE_SM_H_

typedef struct md_signaling_receive_data md_signaling_receive_data_t;

typedef struct md_signaling_receive_stat_data{
	uint32_t signal_rec;
	uint32_t signal_msg_interval_rec;
	uint32_t signal_gptp_capable_rec;
}md_signaling_receive_stat_data_t;

void *md_signaling_receive_sm(md_signaling_receive_data_t *sm, uint64_t cts64);

void md_signaling_receive_sm_init(md_signaling_receive_data_t **sm,
				  int domainIndex, int portIndex,
				  PerTimeAwareSystemGlobal *ptasg,
				  PerPortGlobal *ppg);

int md_signaling_receive_sm_close(md_signaling_receive_data_t **sm);

void *md_signaling_receive_sm_mdSignalingRec(md_signaling_receive_data_t *sm,
					     event_data_recv_t *edrecv, uint64_t cts64);

void md_signaling_receive_stat_reset(md_signaling_receive_data_t *sm);

md_signaling_receive_stat_data_t *md_signaling_receive_get_stat(md_signaling_receive_data_t *sm);

#endif
