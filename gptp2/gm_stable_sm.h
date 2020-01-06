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
#ifndef __GM_STABLE_SM_H_
#define __GM_STABLE_SM_H_

typedef struct gm_stable_data gm_stable_data_t;

void *gm_stable_sm(gm_stable_data_t *sm, uint64_t cts64);

void gm_stable_sm_init(gm_stable_data_t **sm,
	int domainIndex,
	PerTimeAwareSystemGlobal *ptasg);

int gm_stable_sm_close(gm_stable_data_t **sm);

void gm_stable_gm_change(gm_stable_data_t *sm, ClockIdentity clockIdentity, uint64_t cts64);

#endif
