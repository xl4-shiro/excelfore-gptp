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
#ifndef __LINK_DELAY_INTERVAL_SETTING_SM_H_
#define __LINK_DELAY_INTERVAL_SETTING_SM_H_

typedef struct link_delay_interval_setting_data link_delay_interval_setting_data_t;

void *link_delay_interval_setting_sm(link_delay_interval_setting_data_t *sm, uint64_t cts64);

void link_delay_interval_setting_sm_init(link_delay_interval_setting_data_t **sm,
					 int portIndex,
					 PerTimeAwareSystemGlobal *ptasg,
					 PerPortGlobal *ppg,
					 MDEntityGlobal *mdeg);

int link_delay_interval_setting_sm_close(link_delay_interval_setting_data_t **sm);

void *link_delay_interval_setting_SignalingMsg1(
	link_delay_interval_setting_data_t *sm,
	PTPMsgIntervalRequestTLV *rcvdSignalingPtr, uint64_t cts64);


#endif
