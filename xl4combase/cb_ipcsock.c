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
