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
#include <stdio.h>
#include <fcntl.h>
#include "gptpcommon.h"

void eui48to64(const uint8_t *eui48, uint8_t *eui64, const uint8_t *insert)
{
	eui64[0]=eui48[0];
	eui64[1]=eui48[1];
	eui64[2]=eui48[2];
	if(!insert){
		eui64[3]=0xff;
		eui64[4]=0xfe;
	}else{
		eui64[3]=insert[0];
		eui64[4]=insert[1];
	}
	eui64[5]=eui48[3];
	eui64[6]=eui48[4];
	eui64[7]=eui48[5];
}

void print_priority_vector(ub_dbgmsg_level_t level, const char *identifier,
			   UInteger224 *priorityVector)
{
	UB_LOG(level, "%s:%s P1=%d CC=%d CA=%d LV=%d P2=%d CI=%02X%02X%02X%02X%02X%02X%02X%02X"
	       " SR=%d SI=%02X%02X%02X%02X%02X%02X%02X%02X\n",
               __func__, identifier,
               priorityVector->rootSystemIdentity.priority1,
               priorityVector->rootSystemIdentity.clockClass,
               priorityVector->rootSystemIdentity.clockAccuracy,
               priorityVector->rootSystemIdentity.offsetScaledLogVariance,
               priorityVector->rootSystemIdentity.priority2,
               priorityVector->rootSystemIdentity.clockIdentity[0],
               priorityVector->rootSystemIdentity.clockIdentity[1],
               priorityVector->rootSystemIdentity.clockIdentity[2],
               priorityVector->rootSystemIdentity.clockIdentity[3],
               priorityVector->rootSystemIdentity.clockIdentity[4],
               priorityVector->rootSystemIdentity.clockIdentity[5],
               priorityVector->rootSystemIdentity.clockIdentity[6],
               priorityVector->rootSystemIdentity.clockIdentity[7],
               priorityVector->stepsRemoved,
               priorityVector->sourcePortIdentity.clockIdentity[0],
               priorityVector->sourcePortIdentity.clockIdentity[1],
               priorityVector->sourcePortIdentity.clockIdentity[2],
               priorityVector->sourcePortIdentity.clockIdentity[3],
               priorityVector->sourcePortIdentity.clockIdentity[4],
               priorityVector->sourcePortIdentity.clockIdentity[5],
               priorityVector->sourcePortIdentity.clockIdentity[6],
               priorityVector->sourcePortIdentity.clockIdentity[7]
        );
}

uint8_t compare_priority_vectors(UInteger224 *priorityA, UInteger224 *priorityB)
{
        /* 10.3.5 priority vector comparison */
        uint8_t result = SAME_PRIORITY;

        /* 10.3.4 For all components, a lsser numerical value is better,
           and earlier components in the list are more significant */
        int compare = memcmp(priorityA, priorityB,sizeof(UInteger224));
        if (compare < 0){
                result = SUPERIOR_PRIORITY;
        }
        else if (compare > 0){
                result = INFERIOR_PRIORITY;
        }
        return result;
}
