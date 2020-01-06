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
#ifndef __COMBASE_H_
#define __COMBASE_H_

#include <xl4unibase/unibase.h>

#define CB_COMBASE_LOGCAT 1

#ifndef COMBASE_NO_ETH
#include "cb_ethernet.h"
#endif

#ifndef COMBASE_NO_INET
#include "cb_inet.h"
#endif

#ifndef COMBASE_NO_THREAD
#include "cb_thread.h"
#endif

#include "cb_ipcshmem.h"

#ifndef COMBASE_NO_IPCSOCK
#include "cb_ipcsock.h"
#endif

#ifndef COMBASE_NO_EVENT
#include "cb_tmevent.h"
#endif

#endif
