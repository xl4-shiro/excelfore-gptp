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
#include <linux/rtnetlink.h>
#include <errno.h>
#include "xl4combase/cb_ethernet.h"
#include "ix_netlinkif.h"

struct ix_netlinkif {
	gptpnet_cb_t cb_func;
	void *cb_data;
	int netlinkfd;
};

// to update private data in ix_gptpnet.c, need this function
extern int gptpnet_getfd_nlstatus(gptpnet_data_t *gpnet, char *ifname,
				  event_data_netlink_t **nlstatus, int *fd);

static int netlink_init(int *fd)
{
	struct sockaddr_nl sa;
	*fd = CB_SOCKET(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (!CB_SOCKET_VALID(*fd)) {
		UB_LOG(UBL_ERROR,"%s:can't open, %s\n", __func__, strerror(errno));
		*fd=CB_SOCKET_INVALID_VALUE;
		return -1;
	}
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = RTMGRP_LINK;
	sa.nl_pid = getpid();
	if (bind(*fd, (struct sockaddr *)&sa, sizeof(sa))<0) {
		UB_LOG(UBL_ERROR,"%s:can't bind, %s\n", __func__, strerror(errno));
		close(*fd);
		*fd=CB_SOCKET_INVALID_VALUE;
		return -1;
	}
	return 0;
}

static int netlink_msg_handler(ix_netlinkif_t *nlkd, struct nlmsghdr *msg,
			       gptpnet_data_t *gpnet, int64_t *event_ts64)
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
	char *ststr;
	event_data_netlink_t *nlstatus;
	int fd;

	if(!nlkd->cb_func) return -1;
	if(msg->nlmsg_type != RTM_NEWLINK) return 0;
	if_indextoname(ifa->ifa_index, ifname);
        ifi = NLMSG_DATA (msg);

        UB_TLOG(UBL_DEBUG, "%s:netlink msg_type=%d on %s, ifi_flags=0x%x\n",__func__,
		msg->nlmsg_type, ifname, ifi->ifi_flags);

	ndevIndex=gptpnet_getfd_nlstatus(gpnet, ifname, &nlstatus, &fd);
	if(ndevIndex<0) return 0; // maybe an event on not monitoring ports
	memcpy(&edtnl, nlstatus, sizeof(event_data_netlink_t));
	if(!(ifi->ifi_flags & IFF_RUNNING)){
		event=GPTPNET_EVENT_DEVDOWN;
		edtnl.up=false;
	}else{
		cb_get_ethtool_linkstate(fd, ifname, &linkstate);
		edtnl.up=linkstate?true:false;
		if(edtnl.up){
			cb_get_ethtool_info(fd, ifname, &speed, &duplex);
			edtnl.speed=speed;
			edtnl.duplex=duplex;
			event=GPTPNET_EVENT_DEVUP;
		}else{
			event=GPTPNET_EVENT_DEVDOWN;
		}
	}
	if(!memcmp(&edtnl, nlstatus, sizeof(event_data_netlink_t)))
		return 0; // status no change
	ststr=(event==GPTPNET_EVENT_DEVUP)?"UP":"DOWN";
        UB_TLOG(UBL_INFO, "%s:netlink msg_type=%d on %s, status change to %s\n",__func__,
		msg->nlmsg_type, ifname, ststr);
	memcpy(nlstatus, &edtnl, sizeof(event_data_netlink_t));
	return nlkd->cb_func(nlkd->cb_data, ndevIndex+1, event, event_ts64, &edtnl);
}

ix_netlinkif_t *ix_netlinkif_init(gptpnet_cb_t cb_func, void *cb_data)
{
	ix_netlinkif_t *nlkd;
	nlkd=ub_malloc_or_die(__func__, sizeof(ix_netlinkif_t));
	nlkd->cb_func=cb_func;
	nlkd->cb_data=cb_data;
	if(netlink_init(&nlkd->netlinkfd)){
		ix_netlinkif_close(nlkd);
		return NULL;
	}
	return nlkd;
}

void ix_netlinkif_close(ix_netlinkif_t *nlkd)
{
	if(!nlkd) return;
	if(CB_SOCKET_VALID(nlkd->netlinkfd)) close(nlkd->netlinkfd);
	free(nlkd);
}

int ix_netlinkif_getfd(ix_netlinkif_t *nlkd)
{
	if(!nlkd) return CB_SOCKET_INVALID_VALUE;
	return nlkd->netlinkfd;
}

int ix_netlinkif_read_event(ix_netlinkif_t *nlkd, gptpnet_data_t *gpnet,
			    int64_t *event_ts64)
{
	int res;
	int ret = 0;
	char buf[4096];
	struct iovec iov = { buf, sizeof buf };
	struct sockaddr_nl snl;
	struct msghdr msg = { (void*)&snl, sizeof snl, &iov, 1, NULL, 0, 0};
	struct nlmsghdr *h;

	if(!nlkd) return -1;
	res = recvmsg(nlkd->netlinkfd, &msg, 0);
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

		ret = netlink_msg_handler(nlkd, h, gpnet, event_ts64);
		if(ret < 0) return ret;
	}
	return ret;
}
