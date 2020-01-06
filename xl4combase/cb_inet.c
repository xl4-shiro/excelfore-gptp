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
#include <stdio.h>
#include <stdlib.h>
#include "combase_private.h"

#define MAX_SERVNAME_LEN 16

CB_ADDRINFO_T * cb_name_to_addrinfo(int family, int socktype, const char *name, int port, bool numeric_only){
        CB_ADDRINFO_T *res = NULL;
        CB_ADDRINFO_T hints;
        char serv[MAX_SERVNAME_LEN];
        int err;

        memset(&hints, 0, sizeof(hints));
        snprintf(serv, sizeof(serv), "%i", port);
        hints.ai_family = family;
        if(numeric_only){
                hints.ai_flags = AI_NUMERICSERV|AI_NUMERICHOST;
        }
        hints.ai_socktype = socktype;
        if(family==AF_INET6){
                hints.ai_flags |= AI_V4MAPPED;
                hints.ai_flags |= AI_ALL;
        }

        err = CB_GETADDRINFO(name,serv,&hints,&res);
        if(err!=0){
                if(!numeric_only || err!=EAI_NONAME){
                        return NULL;
                }
        }
        return res;
}

int cb_addrinfo_to_ipaddr(const CB_ADDRINFO_T *ai, char *ip, size_t ip_size, int *port){
        char serv[MAX_SERVNAME_LEN];
        int err = CB_GETNAMEINFO(CB_ADDRINFO_AI_ADDR(ai), (CB_SOCKLEN_T)(CB_ADDRINFO_AI_ADDRLEN(ai)),
                                 ip, (CB_SOCKLEN_T)ip_size, serv, (CB_SOCKLEN_T)sizeof(serv),
                                 NI_NUMERICHOST|NI_NUMERICSERV);
        if(err!=0) return -1;
        if(port) *port=atoi(serv);
        return 0;
}

int cb_sockaddr_to_ipaddr(const CB_SOCKADDR_T *sa, CB_SOCKLEN_T salen, char *ip, size_t ip_size, int *port){
        CB_ADDRINFO_T ai;
        memset(&ai, 0, sizeof(ai));
        CB_ADDRINFO_AI_ADDR(&ai) = (CB_SOCKADDR_T*)sa;
        CB_ADDRINFO_AI_ADDRLEN(&ai) = salen;
        CB_ADDRINFO_AI_FAMILY(&ai) = CB_SOCKADDR_SA_FAMILY(sa);
        return cb_addrinfo_to_ipaddr(&ai, ip, ip_size, port);
};

bool cb_is_multicast_addr(const CB_SOCKADDR_T *addr){
        switch(CB_SOCKADDR_SA_FAMILY(addr)){
        case AF_INET:
                return IN_MULTICAST(ntohl(CB_SOCKADDR_IN_ADDR(addr)));
        case AF_INET6:
                return IN6_IS_ADDR_MULTICAST(&(CB_SOCKADDR_IN6_ADDR(addr)));
        default:
                return false;
        }
};
