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
#ifndef __ONE_STEP_TX_OPER_SETTING_SM_H_
#define __ONE_STEP_TX_OPER_SETTING_SM_H_

typedef struct one_step_tx_oper_setting_data one_step_tx_oper_setting_data_t;

void *one_step_tx_oper_setting_sm(one_step_tx_oper_setting_data_t *sm, uint64_t cts64);

void one_step_tx_oper_setting_sm_init(one_step_tx_oper_setting_data_t **sm,
				      int domainIndex, int portIndex,
				      PerTimeAwareSystemGlobal *ptasg,
				      PerPortGlobal *ppg,
				      MDEntityGlobal *mdeg);

int one_step_tx_oper_setting_sm_close(one_step_tx_oper_setting_data_t **sm);

void *one_step_tx_oper_setting_SignalingMsg4(one_step_tx_oper_setting_data_t *sm,
					     PTPMsgIntervalRequestTLV *rcvdSignalingPtr,
					     uint64_t cts64);
#endif
