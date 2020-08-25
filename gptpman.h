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
#ifndef __GPTPMAN_H_
#define __GPTPMAN_H_

#ifdef GHINTEGRITY
extern Semaphore g_gptpd_ready_semaphore;
#define GPTP_READY_NOTICE CB_SEM_POST(&g_gptpd_ready_semaphore);
#else
#define GPTP_READY_NOTICE
#endif

typedef struct gptpman_data gptpman_data_t;

int gptpman_run(char *netdevs[], int max_ports, int max_domains, char *inittm);

#endif
