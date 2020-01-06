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
 * @file cb_inet.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Norman Salunga (norman.salunga@excelfore.com)
 *
 * @brief bindings to posix network IP layer functions
 */

#ifndef __CB_INET_H_
#define __CB_INET_H_

#ifdef CB_INET_NON_POSIX_H
/* non-posix platforms need to support necessary POSIX compatible
 * functions and types which are defined as CB_* macros below.
 * And provide them in a header file defined as CB_SOCKET_NON_POSIX_H */
#include CB_INET_NON_POSIX_H
#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#define CB_ADDRINFO_T struct addrinfo
#define CB_GETADDRINFO getaddrinfo
#define CB_FREEADDRINFO freeaddrinfo

#define CB_ADDRINFO_AI_FAMILY(x) ((x)->ai_family)
#define CB_ADDRINFO_AI_SOCKTYPE(x) ((x)->ai_socktype)
#define CB_ADDRINFO_AI_ADDR(x) ((x)->ai_addr)
#define CB_ADDRINFO_AI_ADDRLEN(x) ((x)->ai_addrlen)
#define CB_ADDRINFO_SA_FAMILY(x) ((x)->ai_addr->sa_family)

#define CB_SOCKADDR_SA_FAMILY(x) ((x)->sa_family)

#define CB_SOCKADDR_IN_ADDR(x)(((CB_SOCKADDR_IN_T *)(x))->sin_addr.s_addr)
#define CB_SOCKADDR_IN6_ADDR(x)(((CB_SOCKADDR_IN6_T *)(x))->sin6_addr)

#define CB_SOCK_BIND bind
#define CB_GETSOCKNAME getsockname
#define CB_GETNAMEINFO getnameinfo

/**
 * @brief Traverse CB_ADDRINFO_T
 */
#define CB_ADDRINFO_FOREACH(list, node) \
        for((node) = (list); (node); (node) = (node)->ai_next)

#endif // CB_INET_NON_POSIX_H

/**
 * @brief Define in6_pktinfo to prevent dereferencing of incomplete type
 * This structure is an addendum usually contained in <linux/in6.h> header file,
 * but should be placed in <netinet/in.h> instead.
 * Combase defines this explicitly to remove necessity to linux header file in posix.
 */
struct in6_pktinfo{
        struct in6_addr ipi6_addr;  /**< src/dst IPv6 address */
        unsigned int ipi6_ifindex;  /**< send/recv interface index */
};

/**
 * @brief Network address encapsulation structure for IPv4 or IPv6 address
 * This structure is used to encapsulation of address type such that the implementation does not need
 * to have separate containers for IPv4 and IPv6 address. It now supports either AF_INET or AF_INET6
 * network addresses.
 * This structure may not hold both address type at a given time.
 * Dual-stack implementation must assume that IPv4 can be effective in parallel with IPv6 address, thus
 * it is expected that the the implementation uses two instances of this structure/
 */
typedef struct cb_inetaddr {
        int family; /**< address family type */
        union {
                CB_IN_ADDR_T ip_inaddr;
                CB_IN6_ADDR_T ip6_inaddr;
        } addr; /** network address, hold either IPv4 or IPv6 address */
        unsigned short port; /**< port number */
} cb_inetaddr_t;

/**
 * @brief Network address to socket address mapping
 * This structure is used to store mapping between network address and socket address.
 */
typedef struct cb_inetaddr_map {
        CB_SOCKADDR_STORAGE_T ss; /**< socket storage */
        cb_inetaddr_t addr; /* network address */
        uint64_t ts; /* timestamp */
} cb_inetaddr_map_t;

/**
 * @brief Converts a name or ip address and port into CB_ADDRINFO_T
 * @param family    address protocol family type
 * @param socktype  preferred socket type
 * @param name      name of the service
 * @param port      port number of the service
 * @param numeric_only filter result to include numeric hosts only
 *
 * Pasing CB_AF_INET6 ensures that the list of CB_ADDRINFO_T returned are IPv6 address.
 * Otherwise passing CB_AF_UNSPEC leads to unspecified results, caller need to confirm the results.
 *
 * Note that this function allocates CB_ADDRINFO_T, the caller needs to perform CB_FREEADDRIINFO afterwards.
 */
CB_ADDRINFO_T * cb_name_to_addrinfo(int family, int socktype, const char *name, int port, bool numeric_only);

/**
 * @brief Translates socket address structure to IP address
 * @param sa        socket address structure
 * @param salen     socket address structure length
 * @param ip        buffer for the resulting IP address
 * @param ip_size   size of the buffer
 * @param port      resulting port number
 * @return 0 for success, otherwise error
 */
int cb_sockaddr_to_ipaddr(const CB_SOCKADDR_T *sa, CB_SOCKLEN_T salen, char *ip, size_t ip_size, int *port);

/**
 * @brief Translate socket information to IP address
 * @param ip        buffer for the resulting IP address
 * @param ip_size   size of the buffer
 * @param port      resulting port number
 * @return 0 for success, otherwise error
 */
int cb_addrinfo_to_ipaddr(const CB_ADDRINFO_T *ai, char *ip, size_t ip_size, int *port);


/**
 * @brief Checks if the proveded socket address is a multicast address
 * @param addr      socket address container
 * @return true if address is multicast, otherwise false
 */
bool cb_is_multicast_addr(const CB_SOCKADDR_T *addr);

#endif
/** @}*/
