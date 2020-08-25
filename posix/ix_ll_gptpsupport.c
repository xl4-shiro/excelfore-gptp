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
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include "ll_gptpsupport.h"

int ll_set_hw_timestamping(CB_SOCKET_T cfd, const char *dev)
{
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig, hwconfig_requested;
	int so_timestamping_flags = 0;
	int res;

	memset(&hwtstamp, 0, sizeof(hwtstamp));
	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		strcpy(hwtstamp.ifr_name, "lo");
	}else{
		snprintf(hwtstamp.ifr_name, sizeof(hwtstamp.ifr_name), "%s", dev);
	}
	hwtstamp.ifr_data = (void *)&hwconfig;

	memset(&hwconfig, 0, sizeof(hwconfig));
	hwconfig.tx_type = HWTSTAMP_TX_ON;
	hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
	hwconfig_requested = hwconfig;

	res=ioctl(cfd, SIOCSHWTSTAMP, &hwtstamp);
	if(res<0){
		hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		hwconfig_requested = hwconfig;
		res=ioctl(cfd, SIOCSHWTSTAMP, &hwtstamp);
	}

	if (res >= 0) {
		UB_LOG(UBL_INFO,"%s:SIOCSHWTSTAMP: tx_type %d requested, got %d; "
		       "rx_filter %d requested, got %d\n",__func__,
		       hwconfig_requested.tx_type, hwconfig.tx_type,
		       hwconfig_requested.rx_filter, hwconfig.rx_filter);

		so_timestamping_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
		so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
		so_timestamping_flags |= SOF_TIMESTAMPING_SYS_HARDWARE;
		so_timestamping_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;
	} else {
		UB_LOG(UBL_INFO,"HW TIMESTAMPING is not available, use software TIMESTAMPING\n");
		so_timestamping_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;
		so_timestamping_flags |= SOF_TIMESTAMPING_RX_SOFTWARE;
		so_timestamping_flags |= SOF_TIMESTAMPING_SOFTWARE;
	}

	if (setsockopt(cfd, SOL_SOCKET, SO_TIMESTAMPING,
		       &so_timestamping_flags, sizeof(so_timestamping_flags)) < 0) {
		UB_LOG(UBL_ERROR,"%s:setsockopt SO_TIMESTAMPING, %s\n",__func__, strerror(errno));
		return -1;
	}

	return 0;
}

int ll_close_hw_timestamping(CB_SOCKET_T cfd, const char *dev)
{
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig;

	UB_LOG(UBL_DEBUG,"%s:\n",__func__);
	memset(&hwtstamp, 0, sizeof(hwtstamp));
	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		strcpy(hwtstamp.ifr_name, "lo");
	}else{
		snprintf(hwtstamp.ifr_name, sizeof(hwtstamp.ifr_name), "%s", dev);
	}
	hwtstamp.ifr_data = (void *)&hwconfig;

	memset(&hwconfig, 0, sizeof(hwconfig));
	hwconfig.tx_type = HWTSTAMP_TX_OFF;
	hwconfig.rx_filter = HWTSTAMP_FILTER_NONE;
	return ioctl(cfd, SIOCSHWTSTAMP, &hwtstamp);
}

int ll_txmsg_timestamp(void *p, int64_t *ts64)
{
	struct cmsghdr *cmsg;
	struct timespec *tmp;
	int level, type;
	struct timespec *ts = NULL;
	struct msghdr *msg=(struct msghdr *)p;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		level = cmsg->cmsg_level;
		type  = cmsg->cmsg_type;

		if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {

			if (cmsg->cmsg_len < sizeof(*tmp)*3) {
				UB_LOG(UBL_ERROR,"received short so_timestamping\n");
				return -1;
			}
			tmp = (struct timespec*)CMSG_DATA(cmsg);
			if (tmp[1].tv_sec) {
				ts = &tmp[1];
				UB_LOG(UBL_DEBUG,"HW SYS Tx TIMESTAMP %ldsec, %ldnsec\n",
				       ts->tv_sec, ts->tv_nsec);
			}else if (tmp[2].tv_sec) {
				ts = &tmp[2];
				UB_LOG(UBL_DEBUG,"HW RAW Tx TIMESTAMP %ldsec, %ldnsec\n",
				       ts->tv_sec, ts->tv_nsec);
			}else if (tmp[0].tv_sec) {
				ts = &tmp[0];
				UB_LOG(UBL_DEBUG,"SW Tx TIMESTAMP %ldsec, %ldnsec\n",
				       ts->tv_sec, ts->tv_nsec);
			}

		}
	}
	if(ts) {
		*ts64=UB_TS2NSEC(*ts);
		return 0;
	}
	return 1;
}

int ll_recv_timestamp(void *p, int64_t *ts64)
{
	struct timeval *tv=NULL;
	struct cmsghdr *cmsg;
	bool gotts=false;
	struct msghdr *msg=(struct msghdr *)p;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		struct timespec *stamp;
		if (cmsg->cmsg_level != SOL_SOCKET) continue;
		switch (cmsg->cmsg_type){
		case SCM_TIMESTAMP:
			tv = (struct timeval *)CMSG_DATA(cmsg);
			*ts64=UB_TV2NSEC(*tv);
			gotts=true;
			break;
		case SO_TIMESTAMPING:
			/* array of three time stamps: software, HW, raw HW */
			stamp = (struct timespec*)CMSG_DATA(cmsg);

			if (cmsg->cmsg_len < sizeof(*stamp)*3) {
				UB_LOG(UBL_ERROR,"received short SO_TIMESTAMPING (%d/%d)\n",
				       (int)cmsg->cmsg_len, (int)sizeof(*stamp)*3);
				return -1;
			}
			/* look at second element in array which is the HW tstamp */
			stamp++;
			if (stamp->tv_sec) {
				*ts64=UB_TS2NSEC(*stamp);
				UB_LOG(UBL_DEBUG,"HW SYS Rx TIMESTAMP %ldsec, %ldnsec\n",
				       stamp->tv_sec, stamp->tv_nsec);
				gotts=true;
				break;
			}
			/* No SYS HW time stamp, look for a RAW HW time stamp. */
			stamp++;
			if (stamp->tv_sec) {
				*ts64=UB_TS2NSEC(*stamp);
				UB_LOG(UBL_DEBUG,"HW RAW Rx TIMESTAMP %ldsec, %ldnsec\n",
				       stamp->tv_sec, stamp->tv_nsec);
				gotts=true;
				break;
			}
			/* No HW time stamp, look for a SW time stamp. */
			stamp-=2;
			if (stamp->tv_sec) {
				*ts64=UB_TS2NSEC(*stamp);
				UB_LOG(UBL_DEBUG,"SW Rx TIMESTAMP %ldsec, %ldnsec\n",
				       stamp->tv_sec, stamp->tv_nsec);
				gotts=true;
				break;
			}
			UB_LOG(UBL_ERROR,"Time stamping for Rx Not received!!!\n");
			break;
		}
	}
	if(gotts) return 0;
	return 1;
}
