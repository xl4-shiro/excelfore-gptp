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
#ifndef __ANNOUNCE_INTERVAL_SETTING_SM_H_
#define __ANNOUNCE_INTERVAL_SETTING_SM_H_

typedef struct announce_interval_setting_data announce_interval_setting_data_t;

void *announce_interval_setting_sm(announce_interval_setting_data_t *sm, uint64_t cts64);

void announce_interval_setting_sm_init(announce_interval_setting_data_t **sm,
	int domainIndex, int portIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal *ppg,
	BmcsPerPortGlobal *bppg);

int announce_interval_setting_sm_close(announce_interval_setting_data_t **sm);

void *announce_interval_setting_sm_SignalingMsg2(announce_interval_setting_data_t *sm,
                                                PTPMsgIntervalRequestTLV *rcvdSignalingPtr,
                                                 uint64_t cts64);
#endif
