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
#ifndef __MD_PDELAY_REQ_SM_H_
#define __MD_PDELAY_REQ_SM_H_

typedef struct md_pdelay_req_data md_pdelay_req_data_t;

typedef struct md_pdelay_req_stat_data {
	uint32_t pdelay_req_send;
	uint32_t pdelay_resp_rec;
	uint32_t pdelay_resp_rec_valid;
	uint32_t pdelay_resp_fup_rec;
	uint32_t pdelay_resp_fup_rec_valid;
} md_pdelay_req_stat_data_t;

int md_pdelay_req_sm(md_pdelay_req_data_t *sm, uint64_t cts64);
void md_pdelay_req_sm_init(md_pdelay_req_data_t **sm,
			   int portIndex,
			   gptpnet_data_t *gpnetd,
			   PerTimeAwareSystemGlobal *ptasg,
			   PerPortGlobal *ppg,
			   MDEntityGlobal *mdeg);
int md_pdelay_req_sm_close(md_pdelay_req_data_t **mdpdrd);
void md_pdelay_req_sm_txts(md_pdelay_req_data_t *sm, event_data_txts_t *edtxts,
			   uint64_t cts64);
void md_pdelay_req_sm_recv_resp(md_pdelay_req_data_t *sm, event_data_recv_t *edrecv,
				uint64_t cts64);
void md_pdelay_req_sm_recv_respfup(md_pdelay_req_data_t *sm, event_data_recv_t *edrecv,
				   uint64_t cts64);
md_pdelay_req_stat_data_t *md_pdelay_req_get_stat(md_pdelay_req_data_t *sm);
void md_pdelay_req_stat_reset(md_pdelay_req_data_t *sm);

#endif
