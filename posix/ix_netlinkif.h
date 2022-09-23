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
#ifndef __IX_NETLINKIF_H_
#define __IX_NETLINKIF_H_

#include "gptpnet.h"

typedef struct ix_netlinkif ix_netlinkif_t;

/**
 * @brief open netlink fd to read network device status
 * @param cb_func	call back functoin to notice events
 * @param cb_data	data for the call back functoin
 */
ix_netlinkif_t *ix_netlinkif_init(gptpnet_cb_t cb_func, void *cb_data);

/**
 * @brief close
 */
void ix_netlinkif_close(ix_netlinkif_t *nlkd);

/**
 * @brief return netlink fd
 */
int ix_netlinkif_getfd(ix_netlinkif_t *nlkd);

/**
 * @brief when events on the netlink fd, read and process the nelink data
 */
int ix_netlinkif_read_event(ix_netlinkif_t *nlkd, gptpnet_data_t *gpnet,
			    int64_t *event_ts64);

#endif
