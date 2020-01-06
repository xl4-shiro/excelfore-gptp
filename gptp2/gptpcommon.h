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
#ifndef __GPTPCOMMON_H_
#define __GPTPCOMMON_H_
#include "gptpbasetypes.h"
#include "gptp_defaults.h"
#include "gptp_config.h"

/****************************************
 *  plaform specific definitions
 ****************************************/
#if defined(GHINTEGRITY)
#define GH_SET_GPTP_SHM     gptpClockShmUpdateFlag=1
#define GH_UNSET_GPTP_SHM   gptpClockShmUpdateFlag=0
#define IS_GPTP_SHM_SET     (gptpClockShmUpdateFlag==1)
#else
#define GH_SET_GPTP_SHM
#define GH_UNSET_GPTP_SHM
#define IS_GPTP_SHM_SET
#endif
/***** end of platform specific definitions *****/

void eui48to64(const uint8_t *eui48, uint8_t *eui64, const uint8_t *insert);

#define LOG_TO_NSEC(x) (((x)>=0)?UB_SEC_NS*(1<<(x)):UB_SEC_NS/(1<<-(x)))

#define GET_FLAG_BIT(x,b) ((x) & (1<<(b)))
#define SET_FLAG_BIT(x,b) x |= (1<<(b))
#define RESET_FLAG_BIT(x,b) x &= ~(1<<(b))
#define SET_RESET_FLAG_BIT(c,x,b) if(c) SET_FLAG_BIT(x,b); else RESET_FLAG_BIT(x,b)

#define SM_CLOSE(f,x) if(x) f(&x)

/* allocate typed in *sm, then allocate typesm in (*sm)->thisSM */
#define INIT_SM_DATA(typed, typesm, sm) \
	if(!*sm){ \
		*sm=malloc(sizeof(typed)); \
		ub_assert(*sm, __func__, "malloc1"); \
	} \
	memset(*sm, 0, sizeof(typed)); \
	(*sm)->thisSM=malloc(sizeof(typesm)); \
	ub_assert((*sm)->thisSM, __func__, "malloc2");	  \
	memset((*sm)->thisSM, 0, sizeof(typesm)); \

/* free the above allocations */
#define CLOSE_SM_DATA(sm) \
	if(!*sm) return 0; \
	free((*sm)->thisSM); \
	free(*sm); \
	*sm=NULL;

/* check for portPriority vector, any non-zero value is sufficient */
#define HAS_PORT_PRIORITY(pp) \
        (((pp).rootSystemIdentity.priority1 !=0) || \
         ((pp).rootSystemIdentity.clockClass != 0) || \
         ((pp).rootSystemIdentity.clockAccuracy != 0) || \
         ((pp).rootSystemIdentity.offsetScaledLogVariance != 0) || \
         ((pp).rootSystemIdentity.priority2 != 0) || \
         ((pp).stepsRemoved != 0))

typedef enum {
        SAME_PRIORITY,
        SUPERIOR_PRIORITY,
        INFERIOR_PRIORITY,
} bmcs_priority_comparison_result;

void print_priority_vector(ub_dbgmsg_level_t level, const char *identifier, UInteger224 *priorityVector);
uint8_t compare_priority_vectors(UInteger224 *priorityA, UInteger224 *priorityB);

#endif
