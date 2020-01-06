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

static int find_ipc_client_in(gptpnet_data_t *gpnet, struct sockaddr_in *client_address)
{
	int i;
	for(i=0;i<GPTPD_IPC_MAX_CONNECTIONS;i++){
		struct sockaddr_in *addr=(struct sockaddr_in *)&gpnet->ipc_address[i];
		if(addr->sin_addr.s_addr == client_address->sin_addr.s_addr &&
		   addr->sin_port == client_address->sin_port){
			// client_address is already registered
			return i;
		}
	}
	return -1;
}

static int find_ipc_client_un(gptpnet_data_t *gpnet, struct sockaddr_un *client_address)
{
	int i;
	for(i=0;i<GPTPD_IPC_MAX_CONNECTIONS;i++){
		if(!strcmp(gpnet->ipc_address[i].sun_path, client_address->sun_path)){
			// client_address is already registered
			return i;
		}
	}
	return -1;
}

static int register_ipc_client(gptpnet_data_t *gpnet, struct sockaddr *client_address)
{
	int i;
	const uint8_t zero4[4]={0,0,0,0};
	if(client_address->sa_family==AF_UNIX){
		i=find_ipc_client_un(gpnet, (struct sockaddr_un *)client_address);
	}else if(client_address->sa_family==AF_INET){
		i=find_ipc_client_in(gpnet, (struct sockaddr_in *)client_address);
	}else {
		UB_LOG(UBL_ERROR,"%s:wrong IPC protocol\n",__func__);
		return -1;
	}
	if(i>=0) return i;
	// As sizeof(struct sockaddr_un) > sizeof(struct sockaddr_in),
	// we use 'sockaddr_un' even for 'sockaddr_in'
	for(i=0;i<GPTPD_IPC_MAX_CONNECTIONS;i++){
		if(memcmp(gpnet->ipc_address[i].sun_path, zero4, 4)) continue;
		memset(&gpnet->ipc_address[i], 0, sizeof(struct sockaddr_un));
		memcpy(&gpnet->ipc_address[i], client_address, sizeof(struct sockaddr_un));
		return i;
	}
	UB_LOG(UBL_WARN,"%s:ipc clients space is full, remove the oldest connection\n",__func__);
	for(i=0;i<GPTPD_IPC_MAX_CONNECTIONS-1;i++){
		memcpy(&gpnet->ipc_address[i], &gpnet->ipc_address[i+1],
		       sizeof(struct sockaddr_un));
	}
	i=GPTPD_IPC_MAX_CONNECTIONS-1;
	memset(&gpnet->ipc_address[i], 0, sizeof(struct sockaddr_un));
	memcpy(&gpnet->ipc_address[i], client_address, sizeof(struct sockaddr_un));
	return i;
}

static int read_ipc_event(gptpnet_data_t *gpnet)
{
	int32_t res;
	socklen_t address_length;
	struct sockaddr_un client_address;
	event_data_ipc_t edipc;

	address_length=sizeof(struct sockaddr_un);
	res=recvfrom(gpnet->ipcfd, &edipc.reqdata, sizeof(gptpipc_client_req_data_t), 0,
		     (struct sockaddr *) &(client_address),  &address_length);
	if(res != sizeof(gptpipc_client_req_data_t)) {
		UB_LOG(UBL_INFO,"%s:wrong received size:%d\n",__func__, res);
		return -1;
	}
	if((edipc.client_index=register_ipc_client(
		    gpnet, (struct sockaddr *)&client_address))<0) return -1;
	if(!gpnet->cb_func) return -1;
	return gpnet->cb_func(gpnet->cb_data, 0, GPTPNET_EVENT_IPC,
			      &gpnet->event_ts64, &edipc);
}

static int gptpnet_ipc_init(gptpnet_data_t *gpnet)
{
	uint16_t ipc_udpport=0;
	int res=0;

	ipc_udpport=gptpconf_get_intitem(CONF_IPC_UDP_PORT);
	if(ipc_udpport){
		res=cb_ipcsocket_udp_init(&gpnet->ipcfd, NULL, NULL, ipc_udpport);
	}else{
		res=cb_ipcsocket_init(&gpnet->ipcfd, GPTP2D_IPC_CB_SOCKET_NODE, "", NULL);
	}
	if(res){
		UB_LOG(UBL_ERROR, "%s:can't open ipc socket\n", __func__);
	}
	return res;
}

static int gptpnet_ipc_close(gptpnet_data_t *gpnet)
{
	uint16_t ipc_udpport=0;
	int res=0;

	ipc_udpport=gptpconf_get_intitem(CONF_IPC_UDP_PORT);
	if(ipc_udpport){
		res=cb_ipcsocket_close(gpnet->ipcfd, NULL, NULL);
	}else{
		res=cb_ipcsocket_close(gpnet->ipcfd, GPTP2D_IPC_CB_SOCKET_NODE, NULL);
	}
	return res;
}

int gptpnet_ipc_notice(gptpnet_data_t *gpnet, gptpipc_gptpd_data_t *ipcdata, int size)
{
	int i, res;
	for(i=0;i<GPTPD_IPC_MAX_CONNECTIONS;i++){
		if(!gpnet->ipc_address[i].sun_path[0]) continue;
		res=sendto(gpnet->ipcfd, ipcdata, size, 0,
			   (struct sockaddr *)&gpnet->ipc_address[i], sizeof(struct sockaddr_un));
		if(res!=size){
			UB_LOG(UBL_INFO, "%s:can't send IPC data to %s, deregister it\n",
			       __func__, gpnet->ipc_address[i].sun_path);
			memset(&gpnet->ipc_address[i], 0, sizeof(struct sockaddr_un));
		}
	}
	return 0;
}

int gptpnet_ipc_respond(gptpnet_data_t *gpnet, int client_index,
			gptpipc_gptpd_data_t *ipcdata, int size)
{
	int res;
	if(client_index < 0 || client_index > GPTPD_IPC_MAX_CONNECTIONS){
		UB_LOG(UBL_ERROR, "%s:client_index=%d is out of range\n",__func__, client_index);
		return -1;
	}
	if(!gpnet->ipc_address[client_index].sun_path[0]) {
		UB_LOG(UBL_ERROR, "%s:client_index=%d is not registered\n",__func__, client_index);
		return -1;
	}
	res=sendto(gpnet->ipcfd, ipcdata, size, 0,
		   (struct sockaddr *)&gpnet->ipc_address[client_index],
		   sizeof(struct sockaddr_un));
	if(res!=size){
		UB_LOG(UBL_ERROR, "%s:can't send IPC data to %s\n",
		       __func__, gpnet->ipc_address[client_index].sun_path);
		return -1;
	}
	return 0;
}
