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
/**
 * @defgroup IPC utility functions
 * @{
 * @file cb_ipcsock.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief IPC utility functions
 */

#ifndef __CB_IPCSOCK_H_
#define __CB_IPCSOCK_H_

/**
 * @brief read from file descriptor with timeout
 * @return 0 on timeout, -1 on error, positive number on read size
 * @param fd	a file descriptor
 * @param data	buffer to read data, which must have more than size bytes
 * @param size	read size
 * @param tout_ms	timeout time in milliseconds
 */
int cb_fdread_timeout(int fd, void *data, int size, int tout_ms);

/**
 * @brief creats and initializes Unix Domain Socket for IPC
 * @return 0 on success, -1 on error
 * @param ipcfd	return opened file descriptor
 * @param node	main part of file node i.e /temp/node
 * @param suffix	suffix part of file node, set "" for non suffix
 * @param server_node	if NULL, create a socket without connection. \n
 * if not NULL, connect to this server_node(must be existing node).
 */
int cb_ipcsocket_init(int *ipcfd, char *node, char *suffix, char *server_node);

/**
 * @brief open Unix Domain Socket in UDP mode for IPC
 * @return 0 on success, -1 on error.
 * @param ipcfd	return opened file descriptor for socket.
 * @param own_ip	NULL for any IF, '127.0.0.1' for local only
 * @param server_ip	NULL in the server mode, set IP in the client mode
 * @param server_port	port number on which the server mode listens
 */
int cb_ipcsocket_udp_init(int *ipcfd, char *own_ip, char *server_ip, int server_port);

/**
 * @brief close Unix Domain Socket for IPC
 * @return 0 on success, -1 on error
 * @param ipcfd	ipc file descriptor
 * @param node	main part of file node
 * @param suffix	suffix part of file node, set "" for non suffix
 * @note this call unlink the node name of node+suffix
 */
int cb_ipcsocket_close(int ipcfd, char *node, char *suffix);

/*
 * this number of IPC clients can be allowed to connect.
 * 2-connection memory data is expanded by increasing of connections,
 * and when the disconnection is detected the data is removed and freed.
 */
#define MAX_IPC_CLIENTS 16
typedef struct cb_ipcserverd cb_ipcserverd_t;

/**
 * @brief initialize the server mode ipc socket
 * @return the data handle
 * @param node_ip	unix domain socket file node name OR udp socket port IP address
 * @param suffix	suffix part of file node, set "" for non suffix
 * @param port	the local port number for udp mode connection. Set 0 for unix domain socket.
 */
cb_ipcserverd_t *cb_ipcsocket_server_init(char *node_ip, char *suffix, uint16_t port);

/**
 * @brief close the server mode ipc socket
 * @param ipcsd	the data handle
 */
void cb_ipcsocket_server_close(cb_ipcserverd_t *ipcsd);

/**
 * @brief send ipc data to a specific client_address or internally managed IPC clients
 * @return 0 on success, -1 on error
 * @param ipcsd	the data handle
 * @param data	send data
 * @param size	send data size
 * @param client_address	if set, the data is sent to this client_address,
 * 	if NULL, the data is sent to all IPC cients
 * 	depends on 'ipcsd->udpport', client_address is 'sockaddr_in' or 'sockaddr_un'
 */
int cb_ipcsocket_server_write(cb_ipcserverd_t *ipcsd, uint8_t *data, int size,
			      struct sockaddr *client_address);

/**
 * @brief callback function to be called from 'cb_ipcsocket_server_read'
 * @note  if the callback returns non-zero, the connection is closed.
 */
typedef int(* cb_ipcsocket_server_rdcb)(void *cbdata, uint8_t *rdata,
					int size, struct sockaddr *addr);

/**
 * @brief receive data on the IPC socket.
 * @return 0 on success, -1 on error
 * @param ipcsd	the data handle
 * @param ipccb	a callback function to be called with the read data
 * @param cbdata	data to be passed with the callback
 * @note this may block the process.  the caller functin must check events not to be blocked
 */
int cb_ipcsocket_server_read(cb_ipcserverd_t *ipcsd,
			     cb_ipcsocket_server_rdcb ipccb, void *cbdata);

/**
 * @brief return ipc socket fd
 */
int cb_ipcsocket_getfd(cb_ipcserverd_t *ipcsd);

/**
 * @brief remove IPC client from the managed list
 */
int cb_ipcsocket_remove_client(cb_ipcserverd_t *ipcsd, struct sockaddr *client_address);

#endif
/** @}*/
