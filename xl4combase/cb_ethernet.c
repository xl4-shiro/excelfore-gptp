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
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "combase_private.h"

static int ovip_socket_open(CB_SOCKET_T *sfd, CB_SOCKADDR_IN_T *saddr,
			    cb_rawsock_ovip_para_t *ovipp)
{
	int optval;

	*sfd = CB_SOCKET(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*sfd < 0){
		UB_LOG(UBL_ERROR,"%s: %s\n",__func__, strerror(errno));
		return -1;
	}
	memset(saddr, 0, sizeof(CB_SOCKADDR_IN_T));
	saddr->sin_addr.s_addr = htonl(ovipp->laddr);
	saddr->sin_family = AF_INET;
	saddr->sin_port = htons(ovipp->lport);
	optval=1;
	CB_SETSOCKOPT(*sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if (CB_SOCK_BIND(*sfd, (struct sockaddr *)saddr, sizeof(CB_SOCKADDR_IN_T)) < 0) {
		UB_LOG(UBL_ERROR,"%s: %s, port=%d\n",__func__, strerror(errno), ovipp->lport);
		CB_SOCK_CLOSE(*sfd);
		return -1;
        }

	if(ovipp->daddr){
		memset(saddr, 0, sizeof(CB_SOCKADDR_IN_T));
		saddr->sin_addr.s_addr = htonl(ovipp->daddr);
		saddr->sin_family = AF_INET;
		saddr->sin_port = htons(ovipp->dport);
		connect(*sfd, (struct sockaddr *)saddr, sizeof(CB_SOCKADDR_IN_T));
	}
	return 0;
}

int cb_rawsock_open(cb_rawsock_paras_t *llrawp, CB_SOCKET_T *fd, CB_SOCKADDR_LL_T *addr,
		    int *mtusize, ub_macaddr_t bmac)
{
	int ifindex;
	char mstr[20];
	CB_IFREQ_T ifr;

	UB_LOG(UBL_INFO, "%s:combase-"XL4PKGVERSION"\n", __func__);
	if(llrawp->sock_mode==CB_SOCK_MODE_OVIP){
		if(!llrawp->ovipp) {
			UB_LOG(UBL_ERROR,"%s:ovipp parameters don't exist\n",__func__);
			return -1;
		}
		// sizeof(CB_SOCKADDR_LL_T) >= sizeof(CB_SOCKADDR_IN_T) must be true
		if(ovip_socket_open(fd, (CB_SOCKADDR_IN_T *)addr,  llrawp->ovipp)) {
			*fd=0;
			return -1;
		}
	}else{
		*fd = CB_SOCKET(AF_PACKET, SOCK_RAW, htons(llrawp->proto));
		if (*fd < 0){
			UB_LOG(UBL_ERROR,"%s:socket error, %s\n",__func__, strerror(errno));
			*fd=0;
			return -1;
		}
	}

	if(cb_get_mac_bydev(*fd, llrawp->dev, bmac)) goto erexit;
	sprintf(mstr, "%02X:%02X:%02X:%02X:%02X:%02X",
		bmac[0],bmac[1],bmac[2],bmac[3],bmac[4],bmac[5]);
	UB_LOG(UBL_INFO,"set new source address: %s\n",mstr);

	if(llrawp->sock_mode==CB_SOCK_MODE_OVIP){
		if(mtusize)
			*mtusize=1500-(sizeof(CB_ETHHDR_T) + 4 + 20); //IP+UDP heaer=20bytes
		return 0;
	}else{
		if((ifindex = CB_IF_NAMETOINDEX(llrawp->dev))<0) goto erexit;
		addr->sll_family = PF_PACKET;
		addr->sll_protocol = htons(llrawp->proto);
		addr->sll_ifindex = ifindex;
		addr->sll_hatype = ARPHRD_ETHER;
		addr->sll_pkttype = PACKET_OTHERHOST;
		addr->sll_halen = ETH_ALEN;

		if(CB_SOCK_BIND(*fd, (CB_SOCKADDR_T*)addr, sizeof(*addr)) < 0) goto erexit;
	}

	if(!mtusize) return 0;
	/* expand the size of MTU if needed, normally MTU size = 1500.
	   if it is not possible, keep the same MTU size */
	strncpy(ifr.ifr_name, llrawp->dev, IFNAMSIZ);
	if(CB_SOCK_IOCTL(*fd, SIOCGIFMTU, &ifr)<0) goto erexit;
	*mtusize -= sizeof(CB_ETHHDR_T) + 4;
	if(ifr.ifr_mtu<*mtusize){
		ifr.ifr_mtu=*mtusize;
		if (CB_SOCK_IOCTL(*fd, SIOCSIFMTU, &ifr) < 0)
			UB_LOG(UBL_INFO,"%s: MTU size can't be expandable\n",__func__);
		if(CB_SOCK_IOCTL(*fd, SIOCGIFMTU, &ifr) < 0) goto erexit;
	}
	/* ifr.ifr_mtu doesn't include eth header size.
	   sizeof(CB_ETHHDR_T) + VLAN_AF_SIZE = 18 can be normally added */
	*mtusize = ifr.ifr_mtu + sizeof(CB_ETHHDR_T) + 4;
	return 0;
erexit:
	UB_LOG(UBL_ERROR,"%s:dev=%s %s\n",__func__, llrawp->dev, strerror(errno));
	if(*fd) CB_SOCK_CLOSE(*fd);
	*fd=0;
	return -1;
}

int cb_rawsock_close(CB_SOCKET_T fd)
{
	return CB_SOCK_CLOSE(fd);
}

int cb_set_promiscuous_mode(CB_SOCKET_T sfd, const char *dev, bool enable)
{
	CB_IFREQ_T ifr;
	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		// ignore it
		return 0;
	}
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if(CB_SOCK_IOCTL(sfd, SIOCGIFFLAGS, &ifr)<0){
		UB_LOG(UBL_ERROR,"%s:error CB_SOCK_IOCTL SIOCGIFFLAGS, %s\n",
		       __func__, strerror(errno));
		return -1;
	}
	if(enable)
		ifr.ifr_flags |= IFF_PROMISC;
	else
		ifr.ifr_flags &= ~IFF_PROMISC;
	if(CB_SOCK_IOCTL(sfd, SIOCSIFFLAGS, &ifr)<0){
		UB_LOG(UBL_ERROR,"%s:error CB_SOCK_IOCTL SIOCSIFFLAGS, %s\n",
		       __func__, strerror(errno));
		return -1;
	}
	if(enable){
		UB_LOG(UBL_INFO,"%s:%s set promiscuouse mode\n",__func__, dev);
	}else{
		UB_LOG(UBL_INFO,"%s:%s reset promiscuouse mode\n",__func__, dev);
	}
	return 0;
}
static int ifrqd_ioctr(CB_SOCKET_T sfd, const char *dev, CB_IFREQ_T *ifrqd, uint32_t iocreq)
{
	int fd=sfd;
	if (fd==-1){
		/* open a udp socket to get it */
		fd = CB_SOCKET(AF_INET,SOCK_DGRAM,0);
		if (fd < 0){
			UB_LOG(UBL_ERROR,"%s: %s\n",__func__, strerror(errno));
			return -1;
		}
	}
	strncpy(ifrqd->ifr_name, dev, IF_NAMESIZE);
	if(CB_SOCK_IOCTL(fd, iocreq, ifrqd) == -1){
		UB_LOG(UBL_ERROR,"%s: %s, dev=%s\n",__func__, strerror(errno),dev);
		return -1;
	}
	if(sfd==-1) CB_SOCK_CLOSE(fd); /* close if we opened here */
	return 0;
}

int cb_get_mac_bydev(CB_SOCKET_T sfd, const char *dev, ub_macaddr_t bmac)
{
	CB_IFREQ_T ifrqd;

	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		int di=strlen(CB_VIRTUAL_ETHDEV_PREFIX);
		int s=0;
		uint8_t bm[4]=CB_VIRTUAL_ETHDEV_MACU32;
		while(dev[di]) s=s+dev[di++];
		memcpy(bmac, bm, 4);
		bmac[4]=s>>8;
		bmac[5]=s&0xff;
		return 0;
	}
	memset(&ifrqd, 0, sizeof(CB_IFREQ_T));
	if(ifrqd_ioctr(sfd, dev, &ifrqd, SIOCGIFHWADDR)) return -1;
	memcpy(bmac,ifrqd.ifr_hwaddr.sa_data,6);
	return 0;
}

int cb_get_ip_bydev(CB_SOCKET_T sfd, const char *dev, CB_IN_ADDR_T *inp)
{
	CB_IFREQ_T ifrqd;

	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		inp->s_addr=htonl(0x7f000001);
		return 0;
	}
	memset(&ifrqd, 0, sizeof(CB_IFREQ_T));
	if(ifrqd_ioctr(sfd, dev, &ifrqd, SIOCGIFADDR)) return -1;
	memcpy(inp, &(((CB_SOCKADDR_IN_T *)&ifrqd.ifr_addr)->sin_addr),
	       sizeof(CB_IN_ADDR_T));
	return 0;
}

int cb_get_brdip_bydev(CB_SOCKET_T sfd, const char *dev, CB_IN_ADDR_T *inp)
{
	CB_IFREQ_T ifrqd;

	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		inp->s_addr=htonl(0x7fffffff);
		return 0;
	}
	memset(&ifrqd, 0, sizeof(CB_IFREQ_T));
	if(ifrqd_ioctr(sfd, dev, &ifrqd, SIOCGIFBRDADDR)) return -1;
	memcpy(inp, &(((CB_SOCKADDR_IN_T *)&ifrqd.ifr_broadaddr)->sin_addr),
	       sizeof(CB_IN_ADDR_T));
	return 0;
}

int cb_reg_multicast_address(CB_SOCKET_T fd, const char *dev,
			     const unsigned char *mcastmac, int del)
{
	CB_IFREQ_T ifr;
	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		// ignore it
		return 0;
	}
	/* add multicast address */
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	memcpy(ifr.ifr_hwaddr.sa_data, mcastmac, ETH_ALEN);
	ifr.ifr_hwaddr.sa_family = AF_UNSPEC;
	if(del){
		if (CB_SOCK_IOCTL(fd, SIOCDELMULTI, &ifr) == -1) return -1;
	}else{
		if (CB_SOCK_IOCTL(fd, SIOCADDMULTI, &ifr) == -1) return -1;
	}
	return 0;
}

int cb_get_all_netdevs(int maxdevnum, netdevname_t *netdevs)
{
	struct ifaddrs *ifa;
	struct ifaddrs *ifad;
	int i=0;

	if(getifaddrs(&ifa)){
		UB_LOG(UBL_ERROR,"%s:failed in getifaddrs: %s\n", __func__, strerror(errno));
		return 0;
	}
	for(ifad=ifa;ifad && i<maxdevnum;ifad=ifad->ifa_next){
		if(ifad->ifa_addr->sa_family != AF_INET) continue;
		// In QNX, local name is lo0
		if(!strncmp(ifad->ifa_name, "lo", 2)) continue;
		strcpy(netdevs[i], ifad->ifa_name);
		UB_LOG(UBL_DEBUG, "%s:found netdev=%s\n",__func__, netdevs[i]);
		i++;
	}
	if(ifa) freeifaddrs(ifa);
	return i;
}

#ifdef LINUX_ETHTOOL
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <dirent.h>
int cb_get_ethtool_info(CB_SOCKET_T cfd, const char *dev, uint32_t *speed, uint32_t *duplex)
{
	struct ifreq ifr;
	struct ethtool_cmd ecmd;

	if(strstr(dev, CB_VIRTUAL_ETHDEV_PREFIX)==dev){
		*duplex=1;
		*speed=1000;
		return 0;
	}
	memset(&ifr, 0, sizeof(ifr));
	memset(&ecmd, 0, sizeof(ecmd));
	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (void *)&ecmd;
	if(ifrqd_ioctr(cfd, dev, &ifr, SIOCETHTOOL)) return -1;
	*speed = (ecmd.speed_hi << 16) | ecmd.speed;
	if (*speed == 0 || *speed == (uint16_t)(-1) || *speed == (uint32_t)(-1)) {
		*speed=0; // unknown speed
	}
	switch(ecmd.duplex){
	case DUPLEX_FULL:
		*duplex=1;
		break;
	case DUPLEX_HALF:
		*duplex=2;
		break;
	default: //duplex unknown
		*duplex=0;
		break;
	}
	return 0;
}

static int get_first_dir_name(char *pname, char *dirname)
{
	DIR *ndir;
	struct dirent *dir;
	int res=-1;

	ndir=opendir(pname);
	if(!ndir) return -1;
	while ((dir = readdir(ndir)) != NULL) {
		if(dir->d_type != DT_DIR) continue;
		if(!strcmp(dir->d_name,".") || !strcmp(dir->d_name,"..")) continue;
		strncpy(dirname, dir->d_name, MAX_PTPDEV_NAME-6);
		res=0;
		break;
	}
	closedir(ndir);
	return res;
}

int cb_get_netdev_from_ptpdev(char *ptpdev, char *netdev)
{
	char *ptpname;
	char pname[128];

	if(strstr(ptpdev, CB_VIRTUAL_PTPDEV_PREFIX)==ptpdev){
		int di=strlen(CB_VIRTUAL_PTPDEV_PREFIX);
		strcpy(netdev, CB_VIRTUAL_ETHDEV_PREFIX);
		strcpy(netdev+strlen(CB_VIRTUAL_ETHDEV_PREFIX), ptpdev+di);
		return 0;
	}
	ptpname=strrchr(ptpdev,'/');
	if(!ptpname)
		ptpname=ptpdev;
	else
		ptpname++;
	snprintf(pname, sizeof(pname), "/sys/class/ptp/%s/device/net",ptpname);
	return get_first_dir_name(pname, netdev);
}

int cb_get_ptpdev_from_netdev(char *netdev, char *ptpdev)
{
	char pname[128];
	if(strstr(netdev, CB_VIRTUAL_ETHDEV_PREFIX)==netdev){
		int di=strlen(CB_VIRTUAL_ETHDEV_PREFIX);
		strcpy(ptpdev, CB_VIRTUAL_PTPDEV_PREFIX);
		strcpy(ptpdev+strlen(CB_VIRTUAL_PTPDEV_PREFIX), netdev+di);
		return 0;
	}
	snprintf(pname, sizeof(pname), "/sys/class/net/%s/device/ptp", netdev);
	strcpy(ptpdev,"/dev/");
	return get_first_dir_name(pname, ptpdev+5);
}

#endif
