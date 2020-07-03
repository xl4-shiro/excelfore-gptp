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
#include <sys/stat.h>        /* For mode constants */
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include "combase_private.h"

int cb_fdread_timeout(int fd, void *data, int size, int tout_ms)
{
	fd_set rfds;
	struct timeval tv={0,0};
	int res;
	tv.tv_sec=tout_ms/1000;
	tv.tv_usec=(tout_ms%1000)*1000;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	res=select(fd+1, &rfds, NULL, NULL, &tv);
	if(!res) return 0;
	return read(fd, data, size);
}

int cb_ipcsocket_init(int *ipcfd, char *node, char *suffix, char *server_node)
{
	struct sockaddr_un sockaddress;
	struct sockaddr_un serveraddress;
	int len;

	if(!node){
		UB_LOG(UBL_ERROR,"'node' must be set\n");
		return -1;
	}
	UB_LOG(UBL_INFO, "%s:combase-"XL4PKGVERSION"\n", __func__);
	if((*ipcfd=CB_SOCKET(AF_UNIX, SOCK_DGRAM, 0))<0){
		UB_LOG(UBL_ERROR,"failed to open ipc socket: %s\n", strerror(errno));
		*ipcfd=0;
		return -1;
	}

	memset(&sockaddress, 0, sizeof(sockaddress));
	sockaddress.sun_family = AF_UNIX;
	if(!suffix) suffix="";
	snprintf(sockaddress.sun_path, sizeof(sockaddress.sun_path),
		 "%s%s", node, suffix);

	unlink(sockaddress.sun_path);
	if(CB_SOCK_BIND(*ipcfd, (const struct sockaddr *) &sockaddress,
		sizeof(sockaddress)) < 0) {
		UB_LOG(UBL_ERROR,"Error, bind to %s: %s\n", sockaddress.sun_path, strerror(errno));
		CB_SOCK_CLOSE(*ipcfd);
		*ipcfd=0;
		return -1;
	}
	chmod(sockaddress.sun_path, 0777);
	if(!server_node) return 0;

	serveraddress.sun_family = AF_UNIX;
	strncpy(serveraddress.sun_path, server_node, sizeof(serveraddress.sun_path));
	len=sizeof(serveraddress.sun_family)+sizeof(serveraddress.sun_path);
	if(connect(*ipcfd, (const struct sockaddr *)&serveraddress, len)<0){
		UB_LOG(UBL_ERROR,"Error, connect to %s : %s\n", server_node, strerror(errno));
		CB_SOCK_CLOSE(*ipcfd);
		*ipcfd=0;
		return -1;
	}
	return 0;
}

int cb_ipcsocket_udp_init(int *ipcfd, char *own_ip, char *server_ip, int server_port)
{
	struct sockaddr_in sockaddress;
	int len;

	UB_LOG(UBL_INFO, "%s:combase-"XL4PKGVERSION"\n", __func__);
	if((*ipcfd=CB_SOCKET(AF_INET, SOCK_DGRAM, IPPROTO_UDP))<0){
		UB_LOG(UBL_ERROR,"failed to open ipc socket: %s\n", strerror(errno));
		*ipcfd=0;
		return -1;
	}
	memset(&sockaddress, 0, sizeof(sockaddress));
	sockaddress.sin_family = AF_INET;
	if(server_ip==NULL)
		sockaddress.sin_port = htons(server_port);
	else
		sockaddress.sin_port = 0;
	len=sizeof(sockaddress);

	if(own_ip){
		sockaddress.sin_addr.s_addr = inet_addr(own_ip);
	}else{
		sockaddress.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	if(CB_SOCK_BIND(*ipcfd, (const struct sockaddr *) &sockaddress,
		       sizeof(sockaddress)) < 0) {
		UB_LOG(UBL_ERROR,"Error, bind:%s\n", strerror(errno));
		CB_SOCK_CLOSE(*ipcfd);
		*ipcfd=0;
		return -1;
	}
	if(!server_ip) return 0;

	// only for client connetion
	sockaddress.sin_addr.s_addr = inet_addr(server_ip);
	sockaddress.sin_port = htons(server_port);
	if(connect(*ipcfd, (const struct sockaddr *)&sockaddress, len)<0){
		UB_LOG(UBL_ERROR,"Error, connect to %s:%d, %s\n",
		       server_ip, server_port, strerror(errno));
		CB_SOCK_CLOSE(*ipcfd);
		*ipcfd=0;
		return -1;
	}
	return 0;
}

int cb_ipcsocket_close(int ipcfd, char *node, char *suffix)
{
	char nodename[128];
	int res=0;
	if(ipcfd) {
		if(CB_SOCK_CLOSE(ipcfd)){
			UB_LOG(UBL_ERROR, "%s:can't close ipcfd, %s\n",__func__, strerror(errno));
			res=-1;
		}
	}
	if(!node) return res;
	if(!suffix) suffix="";
	snprintf(nodename, sizeof(nodename), "%s%s", node, suffix);
	if(unlink(nodename)){
		UB_LOG(UBL_ERROR, "%s:can't unlink %s, %s\n",__func__, nodename, strerror(errno));
		res=-1;
	}
	return res;
}

struct cb_ipcserverd {
	uint16_t udpport;
	int fd;
	char *node;
	char *suffix;
	ub_esarray_cstd_t *ipc_address;
};

static int find_ipc_client_in(cb_ipcserverd_t *ipcsd, struct sockaddr_in *client_address)
{
	int i;
	int en=ub_esarray_ele_nums(ipcsd->ipc_address);
	for(i=0;i<en;i++){
		struct sockaddr_in *addr=
			(struct sockaddr_in *)ub_esarray_get_ele(ipcsd->ipc_address, i);
		if(addr->sin_addr.s_addr == client_address->sin_addr.s_addr &&
		   addr->sin_port == client_address->sin_port){
			// client_address is already registered
			return i;
		}
	}
	return -1;
}

static int find_ipc_client_un(cb_ipcserverd_t *ipcsd, struct sockaddr_un *client_address)
{
	int i;
	int en=ub_esarray_ele_nums(ipcsd->ipc_address);
	for(i=0;i<en;i++){
		struct sockaddr_un *addr=
			(struct sockaddr_un *)ub_esarray_get_ele(ipcsd->ipc_address, i);
		if(!strcmp(addr->sun_path, client_address->sun_path)){
			// client_address is already registered
			return i;
		}
	}
	return -1;
}

static int find_ipc_client(cb_ipcserverd_t *ipcsd, struct sockaddr *client_address)
{
	if(client_address->sa_family==AF_UNIX){
		return find_ipc_client_un(ipcsd, (struct sockaddr_un *)client_address);
	}else if(client_address->sa_family==AF_INET){
		return find_ipc_client_in(ipcsd, (struct sockaddr_in *)client_address);
	}
	UB_LOG(UBL_ERROR,"%s:wrong IPC protocol\n",__func__);
	return -1;
}

static int register_ipc_client(cb_ipcserverd_t *ipcsd, struct sockaddr *client_address)
{
	int i;
	struct sockaddr_un *addr;
	struct sockaddr_un paddr;
	i=find_ipc_client(ipcsd, client_address);
	if(i>=0) return i;
	// As sizeof(struct sockaddr_un) > sizeof(struct sockaddr_in),
	// we use 'sockaddr_un' even for 'sockaddr_in'
	addr=(struct sockaddr_un *)ub_esarray_get_newele(ipcsd->ipc_address);
	if(addr){
		memset(addr, 0, sizeof(struct sockaddr_un));
		memcpy(addr, client_address, sizeof(struct sockaddr_un));
		return ub_esarray_ele_nums(ipcsd->ipc_address)-1;
	}
	UB_LOG(UBL_WARN,"%s:ipc clients space is full, remove the oldest connection\n",__func__);
	ub_esarray_pop_ele(ipcsd->ipc_address, (ub_esarray_element_t *)&paddr);
	addr=(struct sockaddr_un *)ub_esarray_get_newele(ipcsd->ipc_address);
	if(!addr) {
		UB_LOG(UBL_ERROR,"%s:shouldn't happen\n",__func__);
		return -1;
	}
	memset(addr, 0, sizeof(struct sockaddr_un));
	memcpy(addr, client_address, sizeof(struct sockaddr_un));
	return ub_esarray_ele_nums(ipcsd->ipc_address)-1;
}

static int one_client_write(cb_ipcserverd_t *ipcsd, uint8_t *data, int size,
			    struct sockaddr *client_address)
{
	int ssize;
	int res;
	if(!client_address) return -1;
	if(ipcsd->udpport)
		ssize=sizeof(struct sockaddr_in);
	else
		ssize=sizeof(struct sockaddr_un);
	res=sendto(ipcsd->fd, data, size, 0, client_address, ssize);
	if(res==size) return 0;
	if(ipcsd->udpport){
		struct sockaddr_in *addr=(struct sockaddr_in *)client_address;
		UB_LOG(UBL_WARN, "%s:can't send IPC data to %s port=%d\n",
		       __func__, inet_ntoa(addr->sin_addr),
		       ntohs(addr->sin_port));
	}else{
		struct sockaddr_un *addr=(struct sockaddr_un *)client_address;
		UB_LOG(UBL_WARN, "%s:can't send IPC data to %s\n",
		       __func__, addr->sun_path);
	}
	return -1;
}

int cb_ipcsocket_remove_client(cb_ipcserverd_t *ipcsd, struct sockaddr *client_address)
{
	return ub_esarray_del_index(ipcsd->ipc_address, find_ipc_client(ipcsd, client_address));
}

int cb_ipcsocket_server_read(cb_ipcserverd_t *ipcsd, cb_ipcsocket_server_rdcb ipccb, void *cbdata)
{
	int32_t res;
	socklen_t address_length;
	struct sockaddr_un client_address;
	uint8_t rbuf[1500];
	int ri;

	memset(&client_address, 0, sizeof(struct sockaddr_un));
	address_length=sizeof(struct sockaddr_un);
	res=recvfrom(ipcsd->fd, rbuf, 1500, 0,
		     (struct sockaddr *)&(client_address), &address_length);
	if(res>0){
		ri=register_ipc_client(ipcsd, (struct sockaddr *)&client_address);
		if(ri<0) return -1;
		if(ipccb && ipccb(cbdata, rbuf, res, (struct sockaddr *)&client_address))
			cb_ipcsocket_remove_client(ipcsd, (struct sockaddr *)&client_address);
		return 0;
	}

	if(res==0){
		UB_LOG(UBL_WARN, "%s:read returns 0, must be disconnected\n", __func__);
		res=-1;
	}else{
		if(errno==ECONNREFUSED){
			UB_LOG(UBL_WARN, "%s:the other side may not be listening\n", __func__);
		}else{
			UB_LOG(UBL_INFO, "%s:res=%d,%s, close fd\n", __func__,
			       res, strerror(errno));
		}
	}
	cb_ipcsocket_remove_client(ipcsd, (struct sockaddr *)&client_address);
	return -1;
}

int cb_ipcsocket_server_write(cb_ipcserverd_t *ipcsd, uint8_t *data, int size,
			      struct sockaddr *client_address)
{
	int en, i;
	if(client_address)
		return one_client_write(ipcsd, data, size, client_address);

	en=ub_esarray_ele_nums(ipcsd->ipc_address);
	for(i=0;i<en;i++){
		client_address=
			(struct sockaddr *)ub_esarray_get_ele(ipcsd->ipc_address, i);
		if(!client_address) continue;
		if(one_client_write(ipcsd, data, size, client_address)){
			// decrement 'esarray index' when the item is removed
			if(!cb_ipcsocket_remove_client(ipcsd, client_address)) i--;
		}
	}
	return 0;
}

cb_ipcserverd_t *cb_ipcsocket_server_init(char *node_ip, char *suffix, uint16_t port)
{
	cb_ipcserverd_t *ipcsd;
	ipcsd=malloc(sizeof(cb_ipcserverd_t));
	ub_assert(ipcsd, __func__, "malloc");
	memset(ipcsd, 0, sizeof(cb_ipcserverd_t));
	if(port){
		if(cb_ipcsocket_udp_init(&ipcsd->fd, node_ip, NULL, port)) goto erexit;
	}else{
		if(cb_ipcsocket_init(&ipcsd->fd, node_ip, suffix, NULL)) goto erexit;
		ipcsd->node=strdup(node_ip);
		ipcsd->suffix=strdup(suffix);
	}
	ipcsd->udpport=port;
	ipcsd->ipc_address=ub_esarray_init(2, sizeof(struct sockaddr_un), MAX_IPC_CLIENTS);
	return ipcsd;
erexit:
	free(ipcsd);
	return NULL;
}

void cb_ipcsocket_server_close(cb_ipcserverd_t *ipcsd)
{
	if(!ipcsd){
		UB_LOG(UBL_WARN, "%s:ipcsd==NULL\n", __func__);
		return;
	}
	cb_ipcsocket_close(ipcsd->fd, ipcsd->node, ipcsd->suffix);
	if(ipcsd->node) free(ipcsd->node);
	if(ipcsd->suffix) free(ipcsd->suffix);
	ub_esarray_close(ipcsd->ipc_address);
	free(ipcsd);
}

int cb_ipcsocket_getfd(cb_ipcserverd_t *ipcsd)
{
	return ipcsd->fd;
}
