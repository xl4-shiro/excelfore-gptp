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
 * @defgroup network functions binding
 * @{
 * @file cb_ethernet.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief bindings to posix network functions
 */

#ifndef __CB_ETHERNET_H_
#define __CB_ETHERNET_H_

#ifdef CB_ETHERNET_NON_POSIX_H
/* non-posix platforms need to support necessary POSIX compatible
 * functions and types which are defined as CB_* macros below.
 * And provide them in a header file defined as CB_SOCKET_NON_POSIX_H */
#include CB_ETHERNET_NON_POSIX_H
#else
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <fcntl.h>

#define CB_SOCKET_T int
#define CB_ETHHDR_T struct ethhdr
#define CB_SOCKLEN_T socklen_t
#define CB_SOCKADDR_T struct sockaddr
#define CB_SOCKADDR_LL_T struct sockaddr_ll
#define CB_SOCKADDR_IN_T struct sockaddr_in
#define CB_SOCKADDR_IN6_T struct sockaddr_in6
#define CB_SOCKADDR_STORAGE_T struct sockaddr_storage
#define CB_IN_ADDR_T struct in_addr
#define CB_IN6_ADDR_T struct in6_addr
#define CB_IFREQ_T struct ifreq

#define CB_SOCKET socket
#define CB_IF_NAMETOINDEX if_nametoindex
#define CB_SOCK_BIND bind
#define CB_SOCK_IOCTL ioctl
#define CB_SOCK_CLOSE close
#define CB_SETSOCKOPT setsockopt
#define CB_SOCK_SENDTO sendto
#define CB_SOCK_RECVFROM recvfrom
#define CB_SOCK_RECVMSG recvmsg
#define CB_SOCK_WRITE write
#define CB_SOCK_CONNECT connect

#define CB_FCNTL fcntl
#define CB_FD_SET_T fd_set
#define CB_FDSET FD_SET
#define CB_FDCLR FD_CLR
#define CB_FDISSET FD_ISSET
#define CB_FDZERO FD_ZERO

#endif // CB_ETHERNET_NON_POSIX_H

/* virtual eth ports and ptp devices are used in the ovip mode.
 * common suffix should be added after these prefixes */
/**
 *@brief prefix of virtual network device, which supports
 * raw ethernet packet over udp
 */
#define CB_VIRTUAL_ETHDEV_PREFIX "cbeth"

/**
 *@brief prefix of virtual ptp device.
 * the suffix must be common with the virtual network device.
 */
#define CB_VIRTUAL_PTPDEV_PREFIX "cbptp"

/**
 *@brief virtual MAC address of the virtual network device.
 * the lower 2 bytes are calculated from the suffix
 */
#define CB_VIRTUAL_ETHDEV_MACU32 {0x02,0x01,0x45,0x10}

/* the following definitions have some variations for platforms */
#ifndef H_SOURCE
#define H_SOURCE h_source
#endif

#ifndef H_DEST
#define H_DEST h_dest
#endif

#ifndef H_PROTO
#define H_PROTO h_proto
#endif

/************************************************************
 * definitions to handle PTP messages
 ************************************************************/
/**
 *@brief forms ptp header msgtype.
 */
#define PTP_HEAD_MSGTYPE(x) ((*(uint8_t *) ((uint8_t *)(x) + 0)) & 0x0F)

/**
 * @brief macro which is used to form ptp header sequence id.
 */
#define PTP_HEAD_SEQID(x) (ntohs(*(uint16_t *) ((uint8_t *)(x) + 30)))

/**
 * @brief macro used to form ptp header domain number.
 */
#define PTP_HEAD_DOMAIN_NUMBER(x) (*(uint8_t *)((uint8_t *)(x) + 4))

/** @brief maximum character number of ptp device name */
#define MAX_PTPDEV_NAME 32
/**
 * @brief ptpdevice name.
 */
typedef char ptpdevname_t[MAX_PTPDEV_NAME];


/************************************************************
 * definitions to handle raw socket
 ************************************************************/
/**
 * @brief this enumeration defines permission for raw socket.
 * @verbatim for example @endverbatim
 * CB_RAWSOCK_RDWR for read and write.
 * CB_RAWSOCK_RDONLY for read only.
 * CB_RAWSOCK_WRONLY for write only.
 */
typedef enum {
	CB_RAWSOCK_RDWR = 0,
	CB_RAWSOCK_RDONLY,
	CB_RAWSOCK_WRONLY,
} cb_rawsock_rw_t;

/**
 * @brief parameters to open the over IP mode raw socket, the values are in host order
 */
typedef struct cb_rawsock_ovip_para {
	uint32_t laddr; //!< local IP address
	uint16_t lport; //!< local IP port
	uint32_t daddr; //!< destination IP address
	uint16_t dport; //!< destination IP port
} cb_rawsock_ovip_para_t;

typedef enum {
	CB_SOCK_MODE_OVIP = -1,
	CB_SOCK_MODE_NORMAL,
} cb_sock_mode_t;

/**
 * @brief raw socket parameters.to open or create raw socket this structure must be filled.
 */
typedef struct cb_rawsock_paras{
	const char *dev; //!< ethernet device name
	uint16_t proto; //!< protocol value like ETH_P_1588
	uint16_t vlan_proto; //!< protocol value in VLAN tag, not used in non-tagged
	int priority; //!< PCP priority value in VLAN tag
	cb_rawsock_rw_t rw_type; //!< one of RAWSOCK_WRONLY, RAWSOCK_RDONLY, RAWSOCK_RDWR
	cb_sock_mode_t sock_mode; //!< -1:raw socket over udp, 0:normal,
	cb_rawsock_ovip_para_t *ovipp; //!< over-udp mode parameter
} cb_rawsock_paras_t;

/**
 * @brief network device name.
 */
typedef char netdevname_t[IFNAMSIZ];

/************************************************************
 * functions
 ************************************************************/
/**
 * @brief get mac address from device name like 'eth0'
 * @param sfd	if sfd!=-1, pre-opened socket is used to get the mac.
 * if sfd==-1, a newly opened udp socket is used to get the mac Address.
 * @param dev	ethernet device name like 'eth0'
 * @param bmac refernce to buffer which is used to store mac address
 * of ethernet device.
 * @return 0 on success, -1 on error
 */
int cb_get_mac_bydev(CB_SOCKET_T sfd, const char *dev, ub_macaddr_t bmac);

/**
 * @brief get ip address from device name like 'eth0'
 * @param sfd	if sfd!=-1, pre-opened socket is used to get the mac.
 * if sfd==-1, a newly opened udp socket is used to get the MAC Address.
 * @param dev	ethenert device name like 'eth0'
 * @param inp reference to 'CB_IN_ADDR_T' where IP address is saved
 * @return 0 on success, -1 on error
 */
int cb_get_ip_bydev(CB_SOCKET_T sfd, const char *dev, CB_IN_ADDR_T *inp);

/**
 * @brief get broadcast ip address from device name like 'eth0'
 * @param sfd	if sfd!=-1, pre-opened socket is used to get the mac.
 * if sfd==-1, a newly opened udp socket is used.
 * @param dev	ethenert device name like 'eth0'
 * @param inp reference to 'CB_IN_ADDR_T' where IP address is saved
 * @return 0 on success, -1 on error
 */
int cb_get_brdip_bydev(CB_SOCKET_T sfd, const char *dev, CB_IN_ADDR_T *inp);

/**
 * @brief generic raw ethernet open
 * @param llrawp cb_rawsock_paras_t -> raw socket open parameters,
 * this parameter is passed as refernce to cb_rawsock_paras_t.
 * @param fd	return a descriptor of opened socket
 * @param addr	return sockaddr information which is used to open the socket
 * @param mtusize	MTU size including ETH header size.
 *	if *mtusize>default size, try to resize MTU size.
 * @param bmac	the mac address of 'dev' is returned in 'bmac'
 * @return 0 on success, -1 on error
 * @note for general this function support the both of 'avtp raw' and
 *	  'general raw', for 'nos'(no OS or primitive OS), this is for
 *	  'avtp raw' and nos_rawe_socket_open is for 'general raw'
 * @note before calling to this function, llrawp must be filled.
 * @see @c cb_rawsock_paras_t
 */
int cb_rawsock_open(cb_rawsock_paras_t *llrawp, CB_SOCKET_T *fd, CB_SOCKADDR_LL_T *addr,
		    int *mtusize, ub_macaddr_t bmac);

/**
 * @brief close the socket opened by cb_rawsock_open
 * @param fd	descriptor of the opened socket
 * @return 0 on success, -1 on error.
 */
int cb_rawsock_close(CB_SOCKET_T fd);

/**
 * @brief set the promiscuous mode on the socket
 * @param fd	descriptor of the socket
 * @param enable	true:enable, false:disable
 * @return 0 on success, -1 on error.
 */
int cb_set_promiscuous_mode(CB_SOCKET_T sfd, const char *dev, bool enable);

/**
 * @brief register/deregister multicast address to receive
 * @param fd	descriptor of the socket
 * @param dev	ethernet device name like eth0
 * @param mcastmac	multicast address
 * @param del	0:register 1:deregister
 * @return 0 on success, -1 on error.
 */
int cb_reg_multicast_address(CB_SOCKET_T fd, const char *dev,
			     const unsigned char *mcastmac, int del);

/**
 * @brief get all network devices name availble in systems.
 * @param maxdevnum number of devices presents in systems.
 * @param netdevs reference to netdevname_t which stores net devices name.
 * @return index number of network devices presents on system.
 */
int cb_get_all_netdevs(int maxdevnum, netdevname_t *netdevs);

/**
 * @brief get ethtool info(speed and duplex) from device name like 'eth0'
 * @param cfd	if cfd!=-1, pre-opened socket is used to get the ethtool info.
 * if cfd==-1, a newly opened udp socket is used.
 * @param dev	ethenert device name like 'eth0'
 * @param spped	to store speed value(0:unknow, 10:10Mbps, 100:100Mbps, 1000:1Gbps)
 * @param duplex to store duplex value(0:unknow, 1:Full, 2:Half)
 * @return 0 on success, -1 on error
 * @note Linux platform supports this function.  Other platform must suport
 * in the outside of this layer.
 */
int cb_get_ethtool_info(CB_SOCKET_T cfd, const char *dev, uint32_t *speed, uint32_t *duplex);

/**
 * @brief find network device name from ptp device name
 * @return 0 on success, -1 on error
 * @param ptpdev	ptpdevice name(either format of '/dev/ptp0' or 'ptp0')
 * @param netdev	the result of network device name, must have enough space
 * @note Linux platform supports this function.  Other platform must suport
 * in the outside of this layer.
 */
int cb_get_netdev_from_ptpdev(char *ptpdev, char *netdev);

/**
 * @brief find ptp device name from network device name
 * @return 0 on success, -1 on error
 * @param netdev	network device name (like 'eth0')
 * @param ptpdev	the result of ptpdevice name(like 'ptp0', no preceding '/dev/'), \n
 * must have enough space.
 * @note Linux platform supports this function.  Other platform must suport
 * in the outside of this layer.
 */
int cb_get_ptpdev_from_netdev(char *netdev, char *ptpdev);

#endif
/** @}*/
