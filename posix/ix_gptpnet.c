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
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <linux/rtnetlink.h>
#include <sys/un.h>
#include "gptpnet.h"
#include "gptpclock.h"
#include "xl4combase/cb_ethernet.h"


#define GPTPNET_FRAME_SIZE (GPTP_MAX_PACKET_SIZE+sizeof(CB_ETHHDR_T))

extern char *PTPMsgType_debug[];

typedef struct sendbuf {
	CB_ETHHDR_T ehd;
	uint8_t pdata[GPTP_MAX_PACKET_SIZE];
} __attribute__((packed)) sendbuf_t;

typedef struct netdevice {
	int fd;
	int mtusize;
	struct sockaddr_ll addr;
	sendbuf_t sbuf;
	event_data_netlink_t nlstatus;
	uint64_t txtslost_time;
	uint64_t guard_time;
	bool waiting_txts;
	uint64_t waiting_txts_tout;
	int waiting_txts_msgtype;
	uint16_t ovip_port;
} netdevice_t;

struct gptpnet_data {
	gptpnet_cb_t cb_func;
	cb_ipcsocket_server_rdcb ipc_cb;
	void *cb_data;
	int num_netdevs;
	netdevice_t *netdevices;
	int netlinkfd;
	int64_t event_ts64;
	cb_ipcserverd_t *ipcsd;
	int64_t next_tout64;
};

static int netlink_init(int *fd)
{
	struct sockaddr_nl sa;
	*fd = CB_SOCKET(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (*fd<0) {
		UB_LOG(UBL_ERROR,"%s:can't open, %s\n", __func__, strerror(errno));
		*fd=0;
		return -1;
	}
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = RTMGRP_LINK;
	sa.nl_pid = getpid();
	if (bind(*fd, (struct sockaddr *)&sa, sizeof(sa))<0) {
		UB_LOG(UBL_ERROR,"%s:can't bind, %s\n", __func__, strerror(errno));
		close(*fd);
		*fd=0;
		return -1;
	}
	return 0;
}

static int onenet_init(netdevice_t *ndev, char *netdev)
{
	cb_rawsock_paras_t llrawp;
	cb_rawsock_ovip_para_t ovipp;
	const ub_macaddr_t destmac = GPTP_MULTICAST_DEST_ADDR;
	int res=0;

	snprintf(ndev->nlstatus.devname, IFNAMSIZ, "%s", netdev);
	if(!cb_get_ptpdev_from_netdev(ndev->nlstatus.devname,
				      ndev->nlstatus.ptpdev)) {
		res=1;
	}else{
		ndev->nlstatus.ptpdev[0]=0;
	}

	ndev->txtslost_time = gptpconf_get_intitem(CONF_TXTS_LOST_TIME);
	memset(&llrawp, 0, sizeof(llrawp));
	llrawp.dev=ndev->nlstatus.devname;
	llrawp.proto=ETH_P_1588;
	llrawp.vlan_proto=0;
	llrawp.priority=0;
	llrawp.rw_type=CB_RAWSOCK_RDWR;
	if(ndev->ovip_port){
		memset(&ovipp, 0, sizeof(ovipp));
		ovipp.laddr=127<<24|0<<16|0<<8|1;
		ovipp.daddr=127<<24|0<<16|0<<8|1;
		ovipp.lport=ndev->ovip_port;
		ovipp.dport=(ndev->ovip_port%2)?ndev->ovip_port-1:ndev->ovip_port+1;
		llrawp.ovipp=&ovipp;
		llrawp.sock_mode=CB_SOCK_MODE_OVIP;
	}
	if(cb_rawsock_open(&llrawp, &ndev->fd, &ndev->addr, &ndev->mtusize,
			   ndev->sbuf.ehd.H_SOURCE)) return -1;
	memcpy(ndev->sbuf.ehd.H_DEST, destmac, ETH_ALEN);
	ndev->sbuf.ehd.H_PROTO = htons(ETH_P_1588);
	if(cb_reg_multicast_address(ndev->fd, ndev->nlstatus.devname, ndev->sbuf.ehd.H_DEST, 0)) {
		UB_LOG(UBL_ERROR,"failed to add multicast address");
		goto erexit;
	}
	if(ll_set_hw_timestamping(ndev->fd, ndev->nlstatus.devname)) goto erexit;
	eui48to64(ndev->sbuf.ehd.H_SOURCE, ndev->nlstatus.portid,NULL);
	return res;
erexit:
	close(ndev->fd);
	ndev->fd = 0;
	return -1;
}

static int onenet_activate(gptpnet_data_t *gpnet, int ndevIndex)
{
	netdevice_t *ndev=&gpnet->netdevices[ndevIndex];
	uint32_t linkstate=0;
	uint32_t speed;
	uint32_t duplex;
	cb_get_ethtool_linkstate(ndev->fd, ndev->nlstatus.devname, &linkstate);
	cb_get_ethtool_info(ndev->fd, ndev->nlstatus.devname,
			    &speed, &duplex);

	ndev->nlstatus.up = linkstate?true:false;
	ndev->nlstatus.speed=speed;
	ndev->nlstatus.duplex=duplex;
	if(ndev->nlstatus.speed == 0)
		ndev->nlstatus.up = false;
	if(!gpnet->cb_func || !ndev->nlstatus.up) return 0;
	return gpnet->cb_func(gpnet->cb_data, ndevIndex+1, GPTPNET_EVENT_DEVUP,
			      &gpnet->event_ts64, &ndev->nlstatus);
}

static int find_netdev(netdevice_t *devices, int dnum, char *netdev)
{
	int i;
	for(i=0;i<dnum;i++){
		if(!strcmp(netdev, devices[i].nlstatus.devname)) return i;
	}
	return -1;
}

static int netlink_msg_handler(gptpnet_data_t *gpnet, struct nlmsghdr *msg)
{
	struct ifaddrmsg *ifa=NLMSG_DATA(msg);
	char ifname[IFNAMSIZ];
	gptpnet_event_t event=0;
	int ndevIndex;
	struct ifinfomsg *ifi;
	event_data_netlink_t edtnl;
	uint32_t linkstate;
	uint32_t speed;
	uint32_t duplex;

	if_indextoname(ifa->ifa_index,ifname);
	UB_LOG(UBL_INFO, "%s:netlink msg_type=%d on %s\n",__func__, msg->nlmsg_type, ifname);

	if(!gpnet->cb_func) return -1;
	if(msg->nlmsg_type != RTM_NEWLINK) return 0;
        ifi = NLMSG_DATA (msg);
	ndevIndex=find_netdev(gpnet->netdevices, gpnet->num_netdevs, ifname);
	if(ndevIndex<0) return 0;
	memcpy(&edtnl, &gpnet->netdevices[ndevIndex].nlstatus, sizeof(event_data_netlink_t));
	if(!(ifi->ifi_flags & IFF_RUNNING)){
		event=GPTPNET_EVENT_DEVDOWN;
		edtnl.up=false;
	}else{
		cb_get_ethtool_linkstate(gpnet->netdevices[ndevIndex].fd,
				gpnet->netdevices[ndevIndex].nlstatus.devname,
				&linkstate);
		edtnl.up=linkstate?true:false;
		if(edtnl.up){
			cb_get_ethtool_info(gpnet->netdevices[ndevIndex].fd,
					gpnet->netdevices[ndevIndex].nlstatus.devname,
					&speed, &duplex);
			edtnl.speed=speed;
			edtnl.duplex=duplex;
			event=GPTPNET_EVENT_DEVUP;
		}else{
			event=GPTPNET_EVENT_DEVDOWN;
		}
	}
	if(!memcmp(&edtnl, &gpnet->netdevices[ndevIndex].nlstatus, sizeof(event_data_netlink_t)))
		return 0; // status no change
	memcpy(&gpnet->netdevices[ndevIndex].nlstatus, &edtnl, sizeof(event_data_netlink_t));
	return gpnet->cb_func(gpnet->cb_data, ndevIndex+1, event, &gpnet->event_ts64, &edtnl);
}

static int read_netlink_event(gptpnet_data_t *gpnet)
{
	int res;
	int ret = 0;
	char buf[4096];
	struct iovec iov = { buf, sizeof buf };
	struct sockaddr_nl snl;
	struct msghdr msg = { (void*)&snl, sizeof snl, &iov, 1, NULL, 0, 0};
	struct nlmsghdr *h;

	res = recvmsg(gpnet->netlinkfd, &msg, 0);
	if(res < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			UB_LOG(UBL_INFO, "%s:non blocking?\n",__func__);
			return ret;
		}
		UB_LOG(UBL_ERROR,"%s:error in recvmsg, %s\n", __func__, strerror(errno));
		return -1;
	}

	for(h = (struct nlmsghdr *) buf; NLMSG_OK (h, (unsigned int)res);
	    h = NLMSG_NEXT (h, res)) {
		if (h->nlmsg_type == NLMSG_DONE)
			return ret;

		if (h->nlmsg_type == NLMSG_ERROR) {
			UB_LOG(UBL_ERROR,"%s: Message is an error - decode TBD\n",__func__);
			return -1;
		}

		ret = netlink_msg_handler(gpnet, h);
		if(ret < 0) return ret;
	}
	return ret;
}

static int read_txts(gptpnet_data_t *gpnet, int dvi, struct msghdr *msg, int len)
{
	netdevice_t *ndev;
	uint8_t *buf=msg->msg_iov[0].iov_base;
	event_data_txts_t edtxts;

	// once the TxTS is captured, the guard time is not needed
	gpnet->netdevices[dvi].guard_time=0;

	memset(&edtxts, 0, sizeof(edtxts));
	if(len < 48) {
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg returned only %d bytes\n",
		       __func__, dvi, len);
		return -1;
	}
	if(gpnet->netdevices[dvi].ovip_port) buf+=ETH_HLEN+20+8;
	if(ntohs(*(uint16_t *)(buf + 12))!=ETH_P_1588){
		UB_LOG(UBL_DEBUG,
		       "%s:deviceIndex=%d, not ETH_P_1588 packet 0x%02X%02X\n",
		       __func__, dvi, buf[12], buf[13]);
		return -1;
	}

	edtxts.msgtype=PTP_HEAD_MSGTYPE(buf+ETH_HLEN);
	edtxts.seqid=PTP_HEAD_SEQID(buf+ETH_HLEN);
	edtxts.domain=PTP_HEAD_DOMAIN_NUMBER(buf+ETH_HLEN);
	if(edtxts.msgtype >= 8){
		UB_LOG(UBL_DEBUG,"deviceIndex=%d, msgtype:%d is not Event, ignore this\n",
		       dvi, edtxts.msgtype);
		return -1;
	}

	if(ll_txmsg_timestamp(msg, &edtxts.ts64)) return -1;
	if(gpnet->netdevices[dvi].ovip_port && edtxts.msgtype==0)
		edtxts.ts64+=gptpclock_d0ClockfromRT(dvi+1);
	if(!gpnet->cb_func) return -1;
	ndev=&gpnet->netdevices[dvi];
	ndev->waiting_txts=false;
	gpnet->cb_func(gpnet->cb_data, dvi+1, GPTPNET_EVENT_TXTS,
		       &gpnet->event_ts64, &edtxts);
	return 0;
}

static int read_recdata(gptpnet_data_t *gpnet, int dvi, struct msghdr *msg, int len)
{
	uint8_t *buf=msg->msg_iov[0].iov_base;
	event_data_recv_t edtrecv;
	memset(&edtrecv, 0, sizeof(edtrecv));
	if (msg->msg_flags & MSG_TRUNC) {
		UB_LOG(UBL_ERROR,"deviceIndex=%d, received truncated message\n", dvi);
		return -1;
	}
	if (msg->msg_flags & MSG_CTRUNC) {
		UB_LOG(UBL_ERROR,"deviceIndex=%d, received truncated ancillary data\n", dvi);
		return -1;
	}
	edtrecv.recbptr=buf+ETH_HLEN;
	edtrecv.domain=PTP_HEAD_DOMAIN_NUMBER(buf+ETH_HLEN);
	edtrecv.msgtype=PTP_HEAD_MSGTYPE(buf+ETH_HLEN);
	if(edtrecv.msgtype<8){
		if(ll_recv_timestamp(msg, &edtrecv.ts64)) {
			UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, no Rx timestamp for msgtype=%s,"
			       " domain=%d\n",
			       __func__, dvi, PTPMsgType_debug[edtrecv.msgtype], edtrecv.domain);
			return -1;
		}
		if(gpnet->netdevices[dvi].ovip_port && edtrecv.msgtype==0)
			edtrecv.ts64+=gptpclock_d0ClockfromRT(dvi+1);
	}
	if(!gpnet->cb_func) return -1;
	return gpnet->cb_func(gpnet->cb_data, dvi+1, GPTPNET_EVENT_RECV,
				      &gpnet->event_ts64, &edtrecv);
}

static int read_netdev_event(gptpnet_data_t *gpnet, int dvi)
{
	struct iovec vec[1];
	struct msghdr msg;
	char control[512];
	unsigned char buf[GPTPNET_FRAME_SIZE];
	int res;
	netdevice_t *ndev=&gpnet->netdevices[dvi];

	vec[0].iov_base = buf;
	vec[0].iov_len = sizeof(buf);
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	res = recvmsg(ndev->fd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
	if(res > 0){
		return read_txts(gpnet, dvi, &msg, res);
	}else if(res == 0){
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg for EQ returned 0\n",
		       __func__, dvi);
		return -1;
	}else if(errno!=EAGAIN ) {
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg for EQ failed: %s\n",
		       __func__, dvi, strerror(errno));
		return -1;
	}

	res = recvmsg(ndev->fd, &msg, MSG_DONTWAIT);
	if (res > 0){
		if(!ndev->nlstatus.up &&
		   strstr(ndev->nlstatus.devname,
			  CB_VIRTUAL_ETHDEV_PREFIX)==ndev->nlstatus.devname){
			UB_LOG(UBL_DEBUG,"%s:deviceIndex=%d, device up\n", __func__, dvi);
			ndev->nlstatus.up=true;
			gpnet->cb_func(gpnet->cb_data, dvi+1, GPTPNET_EVENT_DEVUP,
				       &gpnet->event_ts64, &ndev->nlstatus);
		}
		return read_recdata(gpnet, dvi, &msg, res);
	}else if(res == 0){
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg returned 0\n", __func__, dvi);
		return -1;
	}else if(errno!=EAGAIN ) {
		if(ndev->ovip_port && errno==ECONNREFUSED){
			if(ndev->nlstatus.up &&
			   strstr(ndev->nlstatus.devname,
				  CB_VIRTUAL_ETHDEV_PREFIX)==ndev->nlstatus.devname){
				UB_LOG(UBL_DEBUG,"%s:deviceIndex=%d, device down\n", __func__, dvi);
				ndev->nlstatus.up=false;
				gpnet->cb_func(gpnet->cb_data, dvi+1, GPTPNET_EVENT_DEVDOWN,
					       &gpnet->event_ts64, &ndev->nlstatus);
			}
			// need to wait a connection
			return 1;
		}
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg failed: %s\n",
		       __func__, dvi, strerror(errno));
		return -1;
	}

	return 1;
}

#define GPTPNET_INTERVAL_TIMEOUT 125000000
static int gptpnet_catch_event(gptpnet_data_t *gpnet)
{
	fd_set rfds;
	int maxfd=0;
	int64_t ts64, tstout64;
	struct timeval tvtout;
	int res=0;
	int i;
	static int64_t last_ts64=0;
	int ipcfd=cb_ipcsocket_getfd(gpnet->ipcsd);

	FD_ZERO(&rfds);
	for(i=0;i<gpnet->num_netdevs;i++){
		if(gpnet->netdevices[i].fd) FD_SET(gpnet->netdevices[i].fd, &rfds);
		maxfd=UB_MAX(maxfd, gpnet->netdevices[i].fd);
	}
	if(gpnet->netlinkfd){
		FD_SET(gpnet->netlinkfd, &rfds);
		maxfd=UB_MAX(maxfd, gpnet->netlinkfd);
	}
	if(ipcfd){
		FD_SET(ipcfd, &rfds);
		maxfd=UB_MAX(maxfd, ipcfd);
	}

	ts64=ub_mt_gettime64();
	tstout64=ts64-last_ts64;
	// every 10 seconds, print clock parameters for debug
	if(tstout64>10*UB_SEC_NS){
		gptpclock_print_clkpara(UBL_INFO);
		last_ts64=ts64;
	}

	if(gpnet->next_tout64){
		tstout64=gpnet->next_tout64-ts64;
		if(tstout64<0){
			gpnet->next_tout64=0;
			UB_LOG(UBL_DEBUG,"%s:call missed or extra TIMEOUT CB\n", __func__);
			res = gpnet->cb_func(gpnet->cb_data, 0, GPTPNET_EVENT_TIMEOUT,
					     &ts64, NULL);
			return 0;
		}
	} else {
		gpnet->next_tout64=((ts64 / GPTPNET_INTERVAL_TIMEOUT) + 1) *
			GPTPNET_INTERVAL_TIMEOUT;
	}
	UB_NSEC2TV(gpnet->next_tout64-ts64, tvtout);
	res=select(maxfd+1, &rfds, NULL, NULL, &tvtout);
	if(res == -1){
		UB_LOG(UBL_ERROR,"%s:select error %s\n", __func__, strerror(errno));
		return -1;
	}
	gpnet->event_ts64=ub_mt_gettime64();

	if(res == 0){
		if(!gpnet->cb_func) return -1;
		gpnet->next_tout64=0;
		res = gpnet->cb_func(gpnet->cb_data, 0, GPTPNET_EVENT_TIMEOUT,
				     &gpnet->event_ts64, NULL);
		return res;
	}
	if(FD_ISSET(gpnet->netlinkfd, &rfds)){
		res|=read_netlink_event(gpnet);
	}
	for(i=0;i<gpnet->num_netdevs;i++){
		if(FD_ISSET(gpnet->netdevices[i].fd, &rfds)){
			while(!read_netdev_event(gpnet, i)) ;
		}
	}
	if(FD_ISSET(ipcfd, &rfds)){
		res|=cb_ipcsocket_server_read(gpnet->ipcsd, gpnet->ipc_cb, gpnet->cb_data);
	}
	return res;
}

int gptpnet_ipc_notice(gptpnet_data_t *gpnet, gptpipc_gptpd_data_t *ipcdata, int size)
{
	return cb_ipcsocket_server_write(gpnet->ipcsd, (uint8_t*)ipcdata, size, NULL);
}

int gptpnet_ipc_respond(gptpnet_data_t *gpnet, struct sockaddr *addr,
			gptpipc_gptpd_data_t *ipcdata, int size)
{
	return cb_ipcsocket_server_write(gpnet->ipcsd, (uint8_t*)ipcdata, size, addr);
}

int gptpnet_ipc_client_remove(gptpnet_data_t *gpnet, struct sockaddr *addr)
{
	return cb_ipcsocket_remove_client(gpnet->ipcsd, addr);
}

gptpnet_data_t *gptpnet_init(gptpnet_cb_t cb_func, cb_ipcsocket_server_rdcb ipc_cb,
			     void *cb_data, char *netdev[], int *num_ports, char *master_ptpdev)
{
	gptpnet_data_t *gpnet;
	int i;
	int first_devwptp=-1;
	int res;
	uint16_t ipc_udpport;

	for(i=0;netdev && netdev[i] && netdev[i][0];i++) ;
	if(i==0){
		UB_LOG(UBL_ERROR,"%s:at least one netdev need\n",__func__);
		return NULL;
	}else if(i>MAX_PORT_NUMBER_LIMIT){
		UB_LOG(UBL_ERROR,"%s:too many netework devices\n",__func__);
		return NULL;
	}
	gpnet=malloc(sizeof(gptpnet_data_t));
	ub_assert(gpnet!=NULL, __func__, "malloc");
	memset(gpnet, 0, sizeof(gptpnet_data_t));
	gpnet->num_netdevs=i;
	*num_ports=i;
	gpnet->netdevices=malloc(i * sizeof(netdevice_t));
	ub_assert(gpnet->netdevices, __func__, "malloc");
	memset(gpnet->netdevices, 0, i * sizeof(netdevice_t));
	for(i=0;i<gpnet->num_netdevs;i++){
		if(strstr(netdev[i], CB_VIRTUAL_ETHDEV_PREFIX)==netdev[i]){
			gpnet->netdevices[i].ovip_port =
				gptpconf_get_intitem(CONF_OVIP_MODE_STRT_PORTNO)+i*2;
		}
		if(!(res=onenet_init(&gpnet->netdevices[i], netdev[i]))) continue;
		if(res<0) {
			UB_LOG(UBL_ERROR, "networkd device:%s can't be opened\n",netdev[i]);
			continue;
		}
		if(first_devwptp<0) first_devwptp=i;
		// if master_ptpdev option is set, use it as the first device
		if(master_ptpdev && !strcmp(master_ptpdev,
					    gpnet->netdevices[i].nlstatus.ptpdev))
			first_devwptp=i;
	}
	if(first_devwptp<0){
		UB_LOG(UBL_ERROR,"%s:ptp device is needed for at least one network device\n",
		       __func__);
		gptpnet_close(gpnet);
		return NULL;
	}
	if(first_devwptp>0){
		netdevice_t swapdev;
		// swap to make the first device have ptpdev
		memcpy(&swapdev, &gpnet->netdevices[0], sizeof(netdevice_t));
		memcpy(&gpnet->netdevices[0], &gpnet->netdevices[first_devwptp],
		       sizeof(netdevice_t));
		memcpy(&gpnet->netdevices[first_devwptp], &swapdev, sizeof(netdevice_t));
	}
	for(i=1;i<gpnet->num_netdevs;i++){
		if(gpnet->netdevices[i].nlstatus.ptpdev[0]) continue;
		UB_LOG(UBL_INFO, "%s:network device %s doesn't have a ptp device, "
		       "use the first ptp device\n", __func__,
		       gpnet->netdevices[i].nlstatus.devname);
		strcpy(gpnet->netdevices[i].nlstatus.ptpdev,
		       gpnet->netdevices[0].nlstatus.ptpdev);
	}
	gpnet->cb_func=cb_func;
	gpnet->ipc_cb=ipc_cb;
	gpnet->cb_data=cb_data;
	gpnet->event_ts64=ub_mt_gettime64();
	ipc_udpport=gptpconf_get_intitem(CONF_IPC_UDP_PORT);
	if(ipc_udpport)
		gpnet->ipcsd=cb_ipcsocket_server_init(NULL, NULL, ipc_udpport);
	else
		gpnet->ipcsd=cb_ipcsocket_server_init(GPTP2D_IPC_CB_SOCKET_NODE, "", 0);
	return gpnet;
}

int gptpnet_activate(gptpnet_data_t *gpnet)
{
	int i;
	for(i=0;i<gpnet->num_netdevs;i++){
		onenet_activate(gpnet, i);
	}
	return netlink_init(&gpnet->netlinkfd);
}

int gptpnet_close(gptpnet_data_t *gpnet)
{
	int i;
	UB_LOG(UBL_DEBUGV, "%s:\n",__func__);
	if(!gpnet) return -1;
	if(gpnet->netdevices){
		for(i=0;i<gpnet->num_netdevs;i++){
			if(!gpnet->netdevices[i].fd) continue;
			close(gpnet->netdevices[i].fd);
		}
	}
	if(gpnet->netlinkfd) close(gpnet->netlinkfd);
	cb_ipcsocket_server_close(gpnet->ipcsd);
	free(gpnet->netdevices);
	free(gpnet);
	return 0;
}

int gptpnet_eventloop(gptpnet_data_t *gpnet, int *stoploop)
{
	while(!*stoploop){
		gptpnet_catch_event(gpnet);
	}
	return 0;
}

uint8_t *gptpnet_get_sendbuf(gptpnet_data_t *gpnet, int ndevIndex)
{
	return gpnet->netdevices[ndevIndex].sbuf.pdata;
}

int gptpnet_send(gptpnet_data_t *gpnet, int ndevIndex, uint16_t length)
{
	char *msg;
	int msgtype;
	uint64_t cts64;
	netdevice_t *ndev;

	if(length>GPTP_MAX_PACKET_SIZE){
		UB_LOG(UBL_ERROR, "%s:deviceIndex=%d, length=%d is too big\n",
		       __func__, ndevIndex, length);
		return -1;
	}
	ndev=&gpnet->netdevices[ndevIndex];
	msgtype=PTP_HEAD_MSGTYPE(ndev->sbuf.pdata);
	if(msgtype<=15)
		msg=PTPMsgType_debug[msgtype];
	else
		msg="unknow";

	cts64=ub_mt_gettime64();
	if(ndev->waiting_txts && msgtype<8){
		uint16_t seqid=PTP_HEAD_SEQID(ndev->sbuf.pdata);
		uint8_t domain=PTP_HEAD_DOMAIN_NUMBER(ndev->sbuf.pdata);
		if(ndev->waiting_txts_tout < cts64){
			UB_TLOG(UBL_INFO, "%s:timed out, waiting_txts, send msg=%s, "
				 "dom=%d, sqid=%d, waiting=%s\n",
				 __func__, msg, domain, seqid,
				 PTPMsgType_debug[ndev->waiting_txts_msgtype]);
			ndev->waiting_txts=false;
		}else{
			UB_TLOG(UBL_DEBUG, "%s:waiting_txts, defer this msg=%s, dom=%d, "
				 "sqid=%d, waiting=%s\n", __func__, msg, domain, seqid,
				 PTPMsgType_debug[ndev->waiting_txts_msgtype]);
			// make sure it will be sent in a short time
			gptpnet_extra_timeout(gpnet, 0);
			return -1;
		}
	}
	UB_LOG(UBL_DEBUGV, "SEND:deviceIndex=%d, msgtype=%s\n", ndevIndex, msg);
	if(ndev->guard_time > cts64){
		UB_TLOG(UBL_DEBUG, "%s:defered by gurad_time:deviceIndex=%d, msgtype=%s\n",
			 __func__, ndevIndex, msg);
		gptpnet_extra_timeout(gpnet, 0); // make sure it will be sent in a short time
		return -1;
	}
	ndev->guard_time= cts64 + gptpconf_get_intitem(CONF_AFTERSEND_GUARDTIME);
	if(msgtype<8) {
		ndev->waiting_txts=true;
		// to let this timeout happen before the other point of TXTS_LOST_TIME,
		// subtract 1msec
		ndev->waiting_txts_tout=cts64 + gptpconf_get_intitem(CONF_TXTS_LOST_TIME) -
			1000000;
		ndev->waiting_txts_msgtype=msgtype;
	}
	return write(ndev->fd, &ndev->sbuf, length+sizeof(CB_ETHHDR_T));
}

char *gptpnet_ptpdev(gptpnet_data_t *gpnet, int ndevIndex)
{
	return gpnet->netdevices[ndevIndex].nlstatus.ptpdev;
}

int gptpnet_num_netdevs(gptpnet_data_t *gpnet)
{
	return gpnet->num_netdevs;
}

uint8_t *gptpnet_portid(gptpnet_data_t *gpnet, int ndevIndex)
{
	return gpnet->netdevices[ndevIndex].nlstatus.portid;
}

void gptpnet_create_clockid(gptpnet_data_t *gpnet, uint8_t *id,
			    int ndevIndex, int8_t domainNumber)
{
	memcpy(id, gpnet->netdevices[ndevIndex].nlstatus.portid, sizeof(ClockIdentity));
	if(domainNumber==0) return;
	id[3]=0;
	id[4]=domainNumber;
}

int gptpnet_get_nlstatus(gptpnet_data_t *gpnet, int ndevIndex, event_data_netlink_t *nlstatus)
{
	if(ndevIndex < 0 || ndevIndex >= gpnet->num_netdevs){
		UB_LOG(UBL_ERROR, "%s:ndevIndex=%d doesn't exist\n",__func__, ndevIndex);
		return -1;
	}
	memcpy(nlstatus, &gpnet->netdevices[ndevIndex].nlstatus, sizeof(event_data_netlink_t));
	return 0;
}

uint64_t gptpnet_txtslost_time(gptpnet_data_t *gpnet, int ndevIndex)
{
	/* give up to read TxTS, if it can't be captured in this time */
	return gpnet->netdevices[ndevIndex].txtslost_time;
}

void gptpnet_extra_timeout(gptpnet_data_t *gpnet, int toutns)
{
	if(toutns==0) toutns=gptpconf_get_intitem(CONF_GPTPNET_EXTRA_TOUTNS);
	gpnet->next_tout64=ub_mt_gettime64();
	gpnet->next_tout64+=toutns;
}

int gptpnet_tsn_schedule(gptpnet_data_t *gpnet, uint32_t aligntime, uint32_t cycletime)
{
	/* IEEE 802.1qbv (time-aware traffic shaping) not yet supported */
	return 0;
}
