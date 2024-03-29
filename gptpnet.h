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
#ifndef __GPTPNET_H_
#define __GPTPNET_H_
#include "gptpcommon.h"
#include "gptpipc.h"

// 10.4.3 Addresses
#define GPTP_MULTICAST_DEST_ADDR {0x01,0x80,0xC2,0x00,0x00,0x0E}

/* Maximum buffer size for PTP packets
 * This size is refered to by receive buffer queue and send buffer which may
 * result to the following total size statically allocated for these buffers:
 *  = (receive queue allocations) + (send buffer allocation)
 *  = (number of ports x GPTP_MAX_PACKET_SIZE x 2) + (GPTP_MAX_PACKET_SIZEx1)
 *
 * For optimization purposes, value of 256 should be enough for usual 802.1AS
 * frames. This value can be changed to this optimized value when memory
 * constraints is an issue.
 *
 * However, for Announce message which can contain variable size TLV, the
 * specified maximum TLV size is 1428 thus technically, the maximum frame for
 * Announce should be 1428+4+68=1500 bytes. It is also a mandatory requirement
 * for AVNU to support TLV of maximum length (gPTP.br.c.24.01 and
 * gPTP.br.c.24.02).
 */
#define GPTP_MAX_PACKET_SIZE 1500

typedef struct gptpnet_data gptpnet_data_t;

typedef enum {
	GPTPNET_EVENT_NONE=0,
	GPTPNET_EVENT_TIMEOUT,
	GPTPNET_EVENT_DEVUP,
	GPTPNET_EVENT_DEVDOWN,
	GPTPNET_EVENT_RECV,
	GPTPNET_EVENT_TXTS,
} gptpnet_event_t;

/*
 * callback function uses portIndex which is ndevIndex+1
 * when portIndex=0, the event is not network device specific
 */
typedef int (*gptpnet_cb_t)(void *cb_data, int portIndex, gptpnet_event_t event,
			    int64_t *event_ts64, void *event_data);

typedef struct event_data_txts {
	uint8_t msgtype;
	uint8_t domain;
	uint16_t seqid;
	int64_t ts64;
} event_data_txts_t;

typedef struct event_data_recv {
	uint8_t msgtype;
	uint8_t domain;
	int64_t ts64;
	uint8_t *recbptr;
} event_data_recv_t;

typedef gptpipc_data_netlink_t event_data_netlink_t;

typedef struct event_data_ipc {
	int client_index;
	gptpipc_client_req_data_t reqdata;
} event_data_ipc_t;

gptpnet_data_t *gptpnet_init(gptpnet_cb_t cb_func, cb_ipcsocket_server_rdcb ipc_cb,
			     void *cb_data, char *netdev[], int *num_ports, char *master_ptpdev);
int gptpnet_activate(gptpnet_data_t *gpnet);
int gptpnet_close(gptpnet_data_t *gpnet);
int gptpnet_eventloop(gptpnet_data_t *gpnet, int *stoploop);
uint8_t *gptpnet_get_sendbuf(gptpnet_data_t *gpnet, int ndevIndex);
int gptpnet_send(gptpnet_data_t *gpnet, int ndevIndex, uint16_t length);
char *gptpnet_ptpdev(gptpnet_data_t *gpnet, int ndevIndex);
int gptpnet_num_netdevs(gptpnet_data_t *gpnet);
int gptpnet_tsn_schedule(gptpnet_data_t *gpnet, uint32_t aligntime, uint32_t cycletime);

/**
 * @brief return portid, which is extended from MAC address by inserting FF:FE
 * @param ndevIndex	index of a network device
 */
uint8_t *gptpnet_portid(gptpnet_data_t *gpnet, int ndevIndex);

/**
 * @brief copy portid, and then replace 4th,5th byte to domainNumber for domainNumber!=0
 *	  for domainNumber==0, return copy of portid
 * @param ndevIndex	index of a network device
 * @param domainNumber	domain Number
 */
void gptpnet_create_clockid(gptpnet_data_t *gpnet, uint8_t *id,
			    int ndevIndex, int8_t domainNumber);

uint64_t gptpnet_txtslost_time(gptpnet_data_t *gpnet, int ndevIndex);
int gptpnet_get_nlstatus(gptpnet_data_t *gpnet, int ndevIndex, event_data_netlink_t *nlstatus);
int gptpnet_ipc_notice(gptpnet_data_t *gpnet, gptpipc_gptpd_data_t *ipcdata, int size);
int gptpnet_ipc_respond(gptpnet_data_t *gpnet, struct sockaddr *addr,
			gptpipc_gptpd_data_t *ipcdata, int size);
int gptpnet_ipc_client_remove(gptpnet_data_t *gpnet, struct sockaddr *addr);

/**
 * @brief make the next timeout happen in toutns (nsec)
 * @param toutns	if 0, use the default(GPTPNET_EXTRA_TOUTNS)
 */
void gptpnet_extra_timeout(gptpnet_data_t *gpnet, int toutns);

#endif
