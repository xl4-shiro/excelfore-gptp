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
#ifndef __PORT_STATE_SELECTION_SM_H_
#define __PORT_STATE_SELECTION_SM_H_

typedef struct port_state_selection_data port_state_selection_data_t;

void *port_state_selection_sm(port_state_selection_data_t *sm, uint64_t cts64);

void port_state_selection_sm_init(port_state_selection_data_t **sm,
	int domainIndex,
	PerTimeAwareSystemGlobal *ptasg,
	PerPortGlobal **ppgl,
	BmcsPerTimeAwareSystemGlobal *bptasg,
	BmcsPerPortGlobal **bppgl,
	int max_ports,
	port_state_selection_data_t **allDomainSM);

int port_state_selection_sm_close(port_state_selection_data_t **sm);

#endif
