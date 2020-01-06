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
#ifndef __MD_PDELAY_RESP_SM_H_
#define __MD_PDELAY_RESP_SM_H_

typedef struct md_pdelay_resp_data md_pdelay_resp_data_t;

typedef struct  md_pdelay_resp_stat_data{
	uint32_t pdelay_req_rec;
	uint32_t pdelay_req_rec_valid;
	uint32_t pdelay_resp_send;
	uint32_t pdelay_resp_fup_send;
} md_pdelay_resp_stat_data_t;

int md_pdelay_resp_sm(md_pdelay_resp_data_t *sm, uint64_t cts64);
int md_pdelay_resp_sm_recv_req(md_pdelay_resp_data_t *sm, event_data_recv_t *edrecv,
				uint64_t cts64);
void md_pdelay_resp_sm_txts(md_pdelay_resp_data_t *sm, event_data_txts_t *edtxts,
			    uint64_t cts64);
void md_pdelay_resp_sm_init(md_pdelay_resp_data_t **mdpdrespd, int portIndex,
			    gptpnet_data_t *gpnetd,
			    PerTimeAwareSystemGlobal *tasglb,
			    PerPortGlobal *ppglb);
int md_pdelay_resp_sm_close(md_pdelay_resp_data_t **mdpdrespd);
void md_pdelay_resp_stat_reset(md_pdelay_resp_data_t *sm);
md_pdelay_resp_stat_data_t *md_pdelay_resp_get_stat(md_pdelay_resp_data_t *sm);

#endif
