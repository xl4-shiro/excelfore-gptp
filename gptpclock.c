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
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include "gptp_defaults.h"
#include "gptpclock.h"
#include "gptpcommon.h"

/*

|------------------------+-------------+------------+-------------+------------|
|                        | HW PhaseAdj | HW FreqAdj | SW PhaseAdj | SW FreqAdj |
|------------------------+-------------+------------+-------------+------------|
| M:PTPCLOCK_MASTER      | No          | No         | Yes         | No         |
| SM:PTPCLOCK_SLAVE_MAIN | Yes         | Yes        | No          | No         |
| SS:PTPCLOCK_SLAVE_SUB  | No          | No         | Yes         | Yes        |
|------------------------+-------------+------------+-------------+------------|
HW PhaseAdj, FreqAdj: applied on ptp device(like /dev/ptp0) clock directly
SW PhaseAdj, FreqAdj: applied on software conversion

When there are 3 Network device and each one has independent PTP Clock device,
the clocks become as follows:
|-----+----------+--------------+---------------+--------------+--------------|
|     |          | masterClock  | thisClock     |              |              |
| TAS |          | Clk0(M,ptp0) | Clk1(SM,ptp0) | Clk2(M,ptp1) | Clk3(M,ptp3) |
|-----+----------+--------------+---------------+--------------+--------------|
| D0  | PhaseAdj | SW           |               |              |              |
|     | FreqAdj  | by Clk1      | HW            |              |              |
|-----+----------+--------------+---------------+--------------+--------------|
masterClock and thisClock work together to provide the ptp clock of 'D0 TAS'.
Because applying HW phase adjustment upset time-stamping, we never do it
other than the initialization time.

HW frequency adjustment is applied on 'thisClock',
the same adjustment is also applied on 'masterClock',
because they are based on the same 'ptp0' clock device.

'masterClock' is exported as 'ptp clock of D0', and by the SW PhaseAdj and
the HW FreqAdj, it shynchronize to 'GM clock'.

timestamps are always stamped by direct ptp device HW clock value.
In a single clock system, there is nothing complicated, and no conversions are needed.

For PdelayReq and PdelayResp, timestamps are used only for one port,
and timestamp value conversions are not needed.
(PdelayReq reception time and PdelayResp transmit time are always based on the same clock)

For Sync, which is received on one port and transmitted on the other port.
Two different clocks are possibly involved, and timestamp value conversions are required.

For the conversions, we use 'thisClock' as a common clock, and convert all timestamps to
'thisClcok' based value.

E.G. when a Sync message comes on port2, which is based on Clk2: SyncRecTS(clk2)
this Sync message propagates to the other ports.
When it is sent on port3, get a timestamp based on Clk3: SyncSendTS(clk3)
We convert those values:
  SyncRecTS(clk2) --> SyncRecTS(thisClock)
  SyncSendTS(clk3) --> SyncSendTS(thisClock)
Then, 'SyncSendTS(thisClock) - SyncRecTS(thisClock)' can be calculated for the correction field.


The things are more complicated for multiple domains.

case 1: 'thisClock of D0' and 'thisClock of D1' are based on different clocks.
|-----+----------+--------------+---------------+---------------+--------------|
|     |          | masterClock  | thisClock     |               |              |
| TAS |          | Clk0(M,ptp0) | Clk1(SM,ptp0) | Clk2(M,ptp1)  | Clk3(M,ptp3) |
|-----+----------+--------------+---------------+---------------+--------------|
| D0  | PhaseAdj | SW           |               |               |              |
|     | FreqAdj  | by Clk1      | HW            | HW(D1)        |              |
|-----+----------+--------------+---------------+---------------+--------------|
|     |          | masterClock  |               | thisClock     |              |
| TAS |          | Clk0(M,ptp1) | Clk1(M,ptp0)  | Clk2(SM,ptp1) | Clk3(M,ptp3) |
|-----+----------+--------------+---------------+---------------+--------------|
| D1  | PhaseAdj | SW           |               |               |              |
|     | FreqAdj  | by Clk2      | HW(D0)        | HW            |              |
|-----+----------+--------------+---------------+---------------+--------------|

case 2: 'thisClock of D0' and 'thisClock of D1' are based on the same clock.
|-----+----------+--------------+---------------+--------------+--------------|
|     |          | masterClock  | thisClock     |              |              |
| TAS |          | Clk0(M,ptp0) | Clk1(SM,ptp0) | Clk2(M,ptp1) | Clk3(M,ptp3) |
|-----+----------+--------------+---------------+--------------+--------------|
| D0  | PhaseAdj | SW           |               |              |              |
|     | FreqAdj  | by Clk1      | HW            |              |              |
|-----+----------+--------------+---------------+--------------+--------------|
|     |          | masterClock  | thisClock     |              |              |
| TAS |          | Clk0(M,ptp0) | Clk1(SS,ptp0) | Clk2(M,ptp1) | Clk3(M,ptp3) |
|-----+----------+--------------+---------------+--------------+--------------|
| D1  | PhaseAdj | SW           |               |              |              |
|     | FreqAdj  | by Clk1      | SW+HW(D0)     |              |              |
|-----+----------+--------------+---------------+--------------+--------------|

HW adjustment are always applied on the same clcok port for all domains.

For the same example,
  D0 Sync propagation from port2 to port3 becomes as follows:
    SyncRecTS(clk2D0) --> SyncRecTS(thisClockD0)
    SyncSendTS(clk3D0) --> SyncSendTS(thisClockD0)
  D1 Sync propagation from port2 to port3 becomes as follows:
    SyncRecTS(clk2D1) --> SyncRecTS(thisClockD1)
    SyncSendTS(clk3D1) --> SyncSendTS(thisClockD1)

domains other than D0, only 'masterClock' and 'thisClock' are needed,
and other clocks are not generated, and D0 clocks are used for them.
For the 'case 2' above, clk2D0 and clk3D0 are used for clk2D1 and clk3D1.
For the 'case 1' above, clk2D0 and clk2D1 are the same.
For D1, it becomes as follows:
  D1 Sync propagation from port2 to port3 becomes as follows:
    SyncRecTS(clk2D0) --> SyncRecTS(thisClockD1)
    SyncSendTS(clk3D0) --> SyncSendTS(thisClockD1)

For 'case 2', thisClock(Clk1) for D1 has additional SW frequency adjustment.
It is not applied for timestamps, and timestamps are based on D0 clock.
When conversions of 'TS(thisClockD0) --> TS(thisClockD1)' are done,
the SW frequency adjustment is applied.

After all, whatever Sync timestamps, use D0 clock as source, and convert to 'thisClock of TAS'
    TS(clkND0) --> TS(thisClock of TAS): 'thisClockD0 for D0', 'thisClockD1 for D1'
This works for all Sync TS conversions.

*/

/*
To sync between GM-D0 and GM-D1, use the case 2 above.
case 2: 'thisClock of D0' and 'thisClock of D1' are based on the same clock.

And when this device becomes GM in D1, set 0 for 'FreqAdj of thisClock of D1'.
Then Freq. of GM in D1 sync to Freq of GM in D0.
'phase of thisClock of D1' is synced to 'phase of thisClock of D0'

For this, 'adjrate' and 'last_setts64' must be shared by the shared memory,
and gptpmasterclock should be accesible for these values.
'adjrate' and 'last_setts64' for 'This Clock' is copied to shared memory.

Further more, when this device starts up and becomes GM of D0,
let this device become slave for D0 first and sync to existing GM,
after that this device takes over GM. With this way, there is no big jump in GM of D0.

When 'thisClock' is in PTPCLOCK_SLAVE_SUB mode, 'offset64' is used
on the clock. Even though it applies only 'adjrate' explicitly, SW frequency
adjustment updates the phase adjustment(offset64).
'offset64' of 'thisClock' and 'masterClock', the both of them must be maintained
separately, and the combined value must be copied into the shared memory.
*/

/*
  switching of the active domain
  Currently there is no rule for this in the standard.

  We introduce 'active domain' with which applications get the ptp clock.
  it is controled by GM status of each domain, and CONF_ACTIVE_DOMAIN_AUTO_SWITCH
 */

#define MAX_CONSEC_TS_DIFF 500000 //500usec
#define LASTGM_OFFSET64_INVALID LLONG_MAX
#define LASTGM_ADJVPPB_INVALID INT_MAX

typedef enum {
	PTPCLOCK_MASTER = 0, // no adjustment
	PTPCLOCK_SLAVE_MAIN, // hw adjustment(use clock_adjtime)
	PTPCLOCK_SLAVE_SUB, // sw adjustment(use internal conversion)
} ptpclock_mode_t;

typedef struct oneclock_data {
	gptp_clock_ppara_t *pp;
	gptp_clock_ppara_t ppe;
	int clockIndex;
	ClockIdentity clockId;
	PTPFD_TYPE ptpfd;
	ptpclock_mode_t mode;
	int domainIndex;
	ptpclock_state_t state;
	int64_t offset64; // this is not the same as pp->offset64
	int64_t last_setts64; // this is not the same as pp->last_setts64
	double adjrate; // this is not the same as pp->adjrate
	int adjvppb;
	uint16_t timeBaseIndicator;
	ScaledNs lastGmPhaseChange;
	int ts2diff;
	uint32_t flags;
	double lastGmFreqChange;
	int lastgm_adjvppb;
	int64_t lastgm_offset64;
} oneclock_data_t;

typedef struct per_domain_data {
	struct timespec last_gmch_ts;
	bool waiting_d0_sync;
	bool gm_stable;
	uint8_t domainNumber; // put this, to get domainNumber from domainIndex
	ClockIdentity gmClockId;
	int thisClockIndex;
} per_domain_data_t;

struct gptpclock_data {
	ub_esarray_cstd_t *clds;
	int shmfd;
	int shmsize;
	gptp_master_clock_shm_t *shm;
	per_domain_data_t *pdd;
	int active_domain_switch;
};

static gptpclock_data_t gcd;

#define GPTPCLOCK_FN_ENTRY(od,clockIndex,domainNumber)  {	\
	if(!gcd.clds) return -1; \
	if((od=get_clockod(clockIndex, domainNumber))==NULL) return -1; \
	if(!PTPFD_VALID(od->ptpfd)) return -1; \
	}

static oneclock_data_t *get_clockod(int clockIndex, uint8_t domainNumber)
{
	int i;
	oneclock_data_t *od;
	for(i=0;i<ub_esarray_ele_nums(gcd.clds);i++){
		od = (oneclock_data_t *)ub_esarray_get_ele(gcd.clds, i);
		if(od->clockIndex != clockIndex || od->pp->domainNumber != domainNumber) continue;
		return od;
	}
	return NULL;
}

static int gptpclock_getts_od(int64_t *ts64, oneclock_data_t *od)
{
	int64_t dts64=0;
	double adjrate;

	GPTP_CLOCK_GETTIME(od->ptpfd, *ts64);
	if(!od->offset64) return 0;
	adjrate=od->adjrate;
	if(adjrate != 0.0){
		// get dts, which is diff between now and last setts time
		dts64=*ts64-od->last_setts64;
		dts64=adjrate * (double)dts64;
		UB_LOG(UBL_DEBUGV, "%s:applied SW adjrate, dts=%"PRIi64"nsec\n",
		       __func__, dts64);
	}

	// add offset
	*ts64+=od->offset64;
	*ts64+=dts64;
	return 0;
}

static int gptpclock_setoffset_od(oneclock_data_t *od)
{
	oneclock_data_t *od0, *odt;

	if(od->mode==PTPCLOCK_MASTER){
		od0=get_clockod(0, od->pp->domainNumber);
		odt=get_clockod(gcd.pdd[od->domainIndex].thisClockIndex, od->pp->domainNumber);
		if(odt && od==od0) {
			// offset64 in the shm must be updated with the one of 'thisClock'
			od->pp->offset64=od->offset64+odt->offset64;
		}
	}else if(od->mode==PTPCLOCK_SLAVE_SUB){
		od0=get_clockod(0, od->pp->domainNumber);
		odt=get_clockod(gcd.pdd[od->domainIndex].thisClockIndex, od->pp->domainNumber);
		if(od0 && odt==od){
			od0->pp->last_setts64=od->last_setts64;
			od0->pp->offset64=od0->offset64+od->offset64;
		}
	}else{
		return -1;
	}
	return 0;
}

static int gptpclock_setts_od(int64_t ts64, oneclock_data_t *od)
{
	GPTP_CLOCK_GETTIME(od->ptpfd, od->last_setts64);

	if(!od->clockIndex || od->mode==PTPCLOCK_SLAVE_SUB)
		gptpclock_mutex_trylock(&gcd.shm->head.mcmutex);

	od->offset64=ts64-od->last_setts64;
	if(od->mode==PTPCLOCK_SLAVE_MAIN){
		od->offset64=0;
		GPTP_CLOCK_SETTIME(od->ptpfd, ts64);
	}else{
		gptpclock_setoffset_od(od);
	}

	if(!od->clockIndex || od->mode==PTPCLOCK_SLAVE_SUB)
		CB_THREAD_MUTEX_UNLOCK(&gcd.shm->head.mcmutex);

	GH_SET_GPTP_SHM;
	return 0;
}


/* returns latency time in this function, if it is too long this setting is not accurate */
static int64_t time_setoffset64(int64_t ts64, int clockIndex, uint8_t domainNumber)
{
	int64_t ats64;
	int64_t mt1,mt2;
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	mt1=ub_mt_gettime64();
	gptpclock_getts_od(&ats64, od);
	ats64 += ts64;
	gptpclock_setts_od(ats64, od);
	mt2=ub_mt_gettime64();
	return mt2-mt1;
}

static int avarage_time_setoffset(int clockIndex, uint8_t domainNumber)
{
	int64_t v;
	int64_t vmax=0;
	int av=0, avc;
	int count=0;
	int i;
	for(i=0;i<10;i++){
		v = time_setoffset64(0, clockIndex, domainNumber);
		if(v > MAX_CONSEC_TS_DIFF) continue;
		if(abs(vmax)<abs(v)) vmax=v;
		av += v;
		count ++;
	}
	// remove max value
	av-=vmax;
	count--;
	if(count<=0){
		UB_LOG(UBL_ERROR, "%s:clockIndex=%d, domainNumber=%d,"
		       "can't calculate setoffset time\n",
		       __func__, clockIndex, domainNumber);
		return 0;
	}
	av = av/count;
	// it was measured in a short loop, and likely shorter value than real use case value.
	avc = av*gptpconf_get_intitem(CONF_TS2DIFF_CACHE_FACTOR)/100;
	UB_LOG(UBL_DEBUG, "%s:clockIndex=%d, domainNumber=%d,"
	       "calculate setoffset time av=%d, avc=%d, vmax=%"PRIi64"\n",
	       __func__, clockIndex, domainNumber, av, avc, vmax);
	return avc;
}


#define PTPCLOCK_OPEN_TOUT 100 // msec
/* It is okay to use ptpdev which doesn't belong to portIndex.
   In succh case, the mode shouldn't be SLAVE_MAIN  */
int gptpclock_add_clock(int clockIndex, char *ptpdev, int domainIndex,
			uint8_t domainNumber, ClockIdentity id)
{
	int i;
	oneclock_data_t *od;
	if(!gcd.clds) return -1;
	for(i=0;i<ub_esarray_ele_nums(gcd.clds);i++){
		od = (oneclock_data_t *)ub_esarray_get_ele(gcd.clds, i);
		if(od->clockIndex == clockIndex && od->pp->domainNumber == domainNumber){
			UB_LOG(UBL_ERROR,"%s:already exists, clockIndex=%d, domainNumber=%d\n",
			       __func__, clockIndex, domainNumber);
			return -1;
		}
	}
	od = (oneclock_data_t *)ub_esarray_get_newele(gcd.clds);
	memset(od, 0, sizeof(oneclock_data_t));
	if(clockIndex!=0){
		od->pp=&od->ppe;
	}else{
		//pp for id=0 must be shared with other processes
		od->pp=&gcd.shm->gcpp[domainIndex];
		memset(od->pp, 0, sizeof(gptp_clock_ppara_t));
		od->pp->gmchange_ind=1; //start with 1
	}
	od->clockIndex=clockIndex;
	od->pp->domainNumber=domainNumber;
	od->domainIndex=domainIndex;
	gcd.pdd[domainIndex].domainNumber=domainNumber;
	memcpy(od->clockId, id, sizeof(ClockIdentity));
	od->state = gptp_get_ptpfd(ptpdev, &od->ptpfd);
	if(od->state == PTPCLOCK_RDWR || od->state == PTPCLOCK_RDONLY){
		snprintf(od->pp->ptpdev, MAX_PTPDEV_NAME, "%s", ptpdev);
	}else{
		UB_LOG(UBL_ERROR, "%s:clockIndex=%d, ptpdev=%s is not accessible\n",
		       __func__, clockIndex, ptpdev);
		od->ptpfd=0;
		gptpclock_del_clock(clockIndex, domainNumber);
		return -1;
	}
	od->ts2diff = avarage_time_setoffset(clockIndex, domainNumber);
	od->pp->offset64=0;
	od->offset64=0;
	UB_LOG(UBL_DEBUG, "%s:clockIndex=%d, ptpdev=%s, domainNumber=%d\n",
	       __func__, clockIndex, ptpdev, domainNumber);
	GH_SET_GPTP_SHM;
	return 0;
}

int gptpclock_del_clock(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	if(!gcd.clds) return 0;
	if((od=get_clockod(clockIndex, domainNumber))){
		if(od->ptpfd) gptp_close_ptpfd(od->ptpfd);
		ub_esarray_del_pointer(gcd.clds, (ub_esarray_element_t *)od);
		UB_LOG(UBL_DEBUG, "%s:clockIndex=%d, domainNumber=%d\n",
		       __func__, clockIndex, domainNumber);
		return 0;
	}
	UB_LOG(UBL_ERROR, "%s:doens't exist clockIndex=%d, domainNumber=%d\n",
	       __func__, clockIndex, domainNumber);
	return -1;
}

int gptpclock_init(int max_domains, int max_ports)
{
        int max_clocks = max_domains * max_ports;
	CB_THREAD_MUTEXATTR_T mattr;
	char *shmem_name;
	memset(&gcd, 0, sizeof(gptpclock_data_t));
	gcd.pdd=malloc(max_domains*sizeof(per_domain_data_t));
	ub_assert(gcd.pdd, __func__, "malloc error");
	memset(gcd.pdd, 0, max_domains*sizeof(per_domain_data_t));
	gcd.active_domain_switch=-1; //default is automatic switch to a stable domain

        // clock data has pointer element, thus disallow realloc of container
        // set max elements and expansion units with the same values
	gcd.clds = ub_esarray_init(max_clocks, sizeof(oneclock_data_t), max_clocks);
	gcd.shmsize = sizeof(gptp_clock_ppara_t)*max_domains +
		sizeof(gptp_master_clock_shm_head_t);
	shmem_name=gptpconf_get_item(CONF_MASTER_CLOCK_SHARED_MEM);
	if(!shmem_name[0]) shmem_name=GPTP_MASTER_CLOCK_SHARED_MEM;
	gcd.shm=(gptp_master_clock_shm_t *)cb_get_shared_mem(
		&gcd.shmfd, shmem_name, gcd.shmsize, O_CREAT | O_RDWR);
	if(!gcd.shm) return -1;
	memset(gcd.shm, 0, gcd.shmsize);
	gcd.shm->head.max_domains = max_domains;
	UB_LOG(UBL_DEBUG, "%s:done, max_domains=%d, shmsize=%d\n",
	       __func__, max_domains, gcd.shmsize);
	CB_THREAD_MUTEXATTR_INIT(&mattr);
	CB_THREAD_MUTEXATTR_SETPSHARED(&mattr, CB_THREAD_PROCESS_SHARED);
	CB_THREAD_MUTEX_INIT(&gcd.shm->head.mcmutex, &mattr);
	GH_SET_GPTP_SHM;
	return 0;
}

void gptpclock_close(void)
{
	oneclock_data_t od;
	char *shmem_name;
	if(!gcd.clds) return;
	gcd.shm->head.max_domains=0;
	while(!ub_esarray_pop_ele(gcd.clds, (ub_esarray_element_t *)&od)){
		if(od.mode==PTPCLOCK_SLAVE_MAIN){
			// return HW adjustment rate to 0
			gptp_clock_adjtime(od.ptpfd, 0);
		}
		if(PTPFD_VALID(od.ptpfd)) gptp_close_ptpfd(od.ptpfd);
	}
	ub_esarray_close(gcd.clds);
	CB_THREAD_MUTEX_DESTROY(&gcd.shm->head.mcmutex);
	shmem_name=gptpconf_get_item(CONF_MASTER_CLOCK_SHARED_MEM);
	if(!shmem_name[0]) shmem_name=GPTP_MASTER_CLOCK_SHARED_MEM;
	cb_close_shared_mem(gcd.shm, &gcd.shmfd, shmem_name, gcd.shmsize, true);
	free(gcd.pdd);
	GH_SET_GPTP_SHM;
	UB_LOG(UBL_DEBUGV, "%s:closed\n", __func__);
}

int gptpclock_apply_offset(int64_t *ts64, int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	*ts64+=od->offset64;
	return 0;
}

int64_t gptpclock_getts64(int clockIndex, uint8_t domainNumber)
{
	int64_t ts64;
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	gptpclock_getts_od(&ts64, od);
	return ts64;
}

int64_t gptpclock_getoffset64(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	return od->offset64;
}

int gptpclock_active_domain(void)
{
	return gcd.shm->head.active_domain;
}

int64_t gptpclock_gethwts64(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	int64_t ts64;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	GPTP_CLOCK_GETTIME(od->ptpfd, ts64);
	return ts64;
}

int gptpclock_setts64(int64_t ts64, int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	return gptpclock_setts_od(ts64, od);
}

int gptpclock_setoffset64(int64_t ts64, int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);

	if(od->mode != PTPCLOCK_SLAVE_MAIN){
		if(!clockIndex) gptpclock_mutex_trylock(&gcd.shm->head.mcmutex);
		od->offset64=ts64;
		if(od->lastgm_offset64!=LASTGM_OFFSET64_INVALID){
			od->flags |= GPTPIPC_EVENT_CLOCK_FLAG_PHASE_UPDATE;
			od->lastGmPhaseChange.nsec = od->offset64 - od->lastgm_offset64;
			od->lastgm_offset64 = LASTGM_OFFSET64_INVALID;
			UB_LOG(UBL_INFO, "%s:lastGmPhaseChange=%"PRIi64"\n",
			       __func__, od->lastGmPhaseChange.nsec);
		}
		gptpclock_setoffset_od(od);
		if(!clockIndex) CB_THREAD_MUTEX_UNLOCK(&gcd.shm->head.mcmutex);
		GH_SET_GPTP_SHM;
		return 0;
	}

	ts64 = od->ts2diff/2 + ts64;
	if(time_setoffset64(ts64, clockIndex, domainNumber) > od->ts2diff*10){
		UB_LOG(UBL_WARN, "%s:clockIndex=%d, domainNumber=%d, "
		       "can't set in the time. the result must be inaccurate\n",
		       __func__, clockIndex, domainNumber);
		return -1;
	}
	return 0;
}

int gptpclock_setadj(int adjvppb, int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	oneclock_data_t *od0;
	uint32_t save_flags;
	int64_t ts;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	switch(od->mode){
	case PTPCLOCK_SLAVE_MAIN:
		if(gptp_clock_adjtime(od->ptpfd, adjvppb)<0){
			UB_LOG(UBL_ERROR, "%s:can't adjust freq, clockIndex=%d, domainNumber=%d\n",
			       __func__, clockIndex, domainNumber);
			return -1;
		}
		break;
	case PTPCLOCK_MASTER:
		UB_LOG(UBL_ERROR,"%s:MASTER can't adjust freq.\n",__func__);
		return -1;
	case PTPCLOCK_SLAVE_SUB:
		// to apply new adjrate, update offset value. it updates 'last_setts64'.
		save_flags=od->flags;
		ts=time_setoffset64(od->ts2diff/2, clockIndex, domainNumber);
		if(ts > od->ts2diff*10){
			UB_LOG(UBL_WARN, "%s:clockIndex=%d, domainNumber=%d, time_setoffset64 "
			       "took too long, %"PRIi64"/%d\n",
			       __func__, clockIndex, domainNumber, ts, od->ts2diff);
		}
		od->flags=save_flags; // don't update the flag by the above procedure
		od->adjrate = (double)adjvppb/1.0E9;
		od0=get_clockod(0, domainNumber);
		// od0->pp->adjrate is in the shared memory
		// it is different from od0->adjrate,
		od0->pp->adjrate = od->adjrate;
		GH_SET_GPTP_SHM;
		break;
	}
	od->adjvppb=adjvppb;
	return 0;
}

// this function is for debug purpose
void gptpclock_print_clkpara(ub_dbgmsg_level_t level)
{
	int i;
	gptp_clock_ppara_t *pp;
	oneclock_data_t *odt;

	if(!ub_clog_on(UB_LOGCAT, level)) return;
	for(i=0;i<gcd.shm->head.max_domains;i++){
		pp=&gcd.shm->gcpp[i];
		if((odt=get_clockod(gcd.pdd[i].thisClockIndex, i))==NULL){
			UB_LOG(UBL_WARN, "domain=%d thisClockIndex=%d doesn't exits\n",
			       i,gcd.pdd[i].thisClockIndex);
			return;
		}
		ub_console_print("domain=%d, offset=%"PRIi64"nsec, ",
				 i, odt->offset64);
		switch(odt->mode){
		case PTPCLOCK_SLAVE_MAIN:
			ub_console_print("hw-adjrate=%dppb\n", odt->adjvppb);
			break;
		case PTPCLOCK_MASTER:
			ub_console_print("adjrate=0(master)\n");
			break;
		case PTPCLOCK_SLAVE_SUB:
			ub_console_print("sw-adjrate=%e\n", pp->adjrate);
			break;
		}
		ub_console_print("        gmsync=%s, last_setts64=%"PRIi64"nsec\n",
				 pp->gmsync?"true":"false", pp->last_setts64);
	}
}

int gptpclock_mode_master(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	od->mode=PTPCLOCK_MASTER;
	return 0;
}

int gptpclock_mode_slave_main(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od, *od1;
	int i;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);

	for(i=0;i<ub_esarray_ele_nums(gcd.clds);i++){
		od1 = (oneclock_data_t *)ub_esarray_get_ele(gcd.clds, i);
		if(od1->clockIndex != od->clockIndex ||
		   od1->pp->domainNumber == od->pp->domainNumber)
			continue;
		if(od1->mode==PTPCLOCK_SLAVE_MAIN){
			UB_LOG(UBL_INFO, "%s:domainNumer=%d already in SLAVE_MAIN\n",
			       __func__, od1->pp->domainNumber);
			return -1;
		}
	}
	od->mode=PTPCLOCK_SLAVE_MAIN;
	od->adjrate=0.0;
	// if SLAVE_MAIN is used, SLAVE_SUB is not used in the same domain.
	// When SLAVE_SUB is not used, adjrate in the shared mem. must be 0.0,
	// and offset64 is not needed to combine with the one of 'thisClock'
	GPTPCLOCK_FN_ENTRY(od, 0, domainNumber);
	od->pp->adjrate=0.0;
	od->pp->offset64=od->offset64;
	GH_SET_GPTP_SHM;
	return 0;
}

int gptpclock_mode_slave_sub(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	od->mode=PTPCLOCK_SLAVE_SUB;
	return 0;
}

static int diff_in_two_clocks(int64_t *tss64,
                              int clockIndex, uint8_t domainNumber,
			      int clockIndex1, uint8_t domainNumber1)
{
        oneclock_data_t *od, *od1;
	int64_t ts1, ts2, ts3;

        GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
        GPTPCLOCK_FN_ENTRY(od1, clockIndex1, domainNumber1);

	// get (ts2-ts1) - (ts3-ts1)/2
        if(gptpclock_getts_od(&ts1, od)){
		UB_LOG(UBL_ERROR, "%s:can't get ts1=TS(clk=%d,D=%d)\n",
                       __func__, od->clockIndex, od->pp->domainNumber);
                return -2;
        }
	if(gptpclock_getts_od(&ts2, od1)){
		UB_LOG(UBL_ERROR, "%s:can't get ts2=TS(clk=%d,D=%d)\n",
                       __func__, od1->clockIndex, od1->pp->domainNumber);
		return -2;
	}
        if(gptpclock_getts_od(&ts3, od)){
		UB_LOG(UBL_ERROR, "%s:can't get ts3=TS(clk=%d,D=%d)\n",
                       __func__, od->clockIndex, od->pp->domainNumber);
                return -2;
        }

	ts3=(ts3-ts1)/2;
	if(ts3 > od->ts2diff*10) return -1;
	*tss64=ts2-ts1-ts3;
	return 0;
}

int gptpclock_tsconv(int64_t *ts64, int clockIndex, uint8_t domainNumber,
		     int clockIndex1, uint8_t domainNumber1)
{
	int64_t dtss;
	oneclock_data_t *od;

	if(clockIndex==clockIndex1 && domainNumber==domainNumber1) return 0;
	if((od=get_clockod(clockIndex1, domainNumber1))==NULL) return -1;
	if(clockIndex==clockIndex1 && od->mode!=PTPCLOCK_SLAVE_SUB) return 0;
	if(diff_in_two_clocks(&dtss, clockIndex, domainNumber,
			      clockIndex1, domainNumber1)){
		// in case of a fail by context switching, we'll try twice
		if(diff_in_two_clocks(&dtss, clockIndex, domainNumber,
				      clockIndex1, domainNumber1)){
			UB_LOG(UBL_ERROR, "%s:can't convert ts, (ci=%d,di=%d)->(ci=%d,di=%d)\n",
			       __func__, clockIndex, domainNumber, clockIndex1, domainNumber1);
			return -1;
		}
	}
	//UB_LOG(UBL_DEBUGV, "%s:(cI/dN) %d/%d -> %d/%d dtss=%"PRIi64"\n",__func__,
	//       clockIndex, domainNumber, clockIndex1, domainNumber1, dtss);
	*ts64+=dtss;
	return 0;
}

uint8_t *gptpclock_clockid(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	if(!gcd.clds) return NULL;
	if((od=get_clockod(clockIndex, domainNumber))==NULL) return NULL;
	return od->clockId;
}

int gptpclock_rate_same(int clockIndex, uint8_t domainNumber,
			int clockIndex1, uint8_t domainNumber1)
{
	oneclock_data_t *od, *od1;
	if(!gcd.clds) return -1;
	if((od=get_clockod(clockIndex, domainNumber))==NULL) return -1;
	if((od1=get_clockod(clockIndex1, domainNumber1))==NULL) return -1;
	if(!strcmp(od->pp->ptpdev, od1->pp->ptpdev)){
		if(od->mode != PTPCLOCK_SLAVE_SUB && od1->mode != PTPCLOCK_SLAVE_SUB)
			return 0;
		if(od->adjrate == 0.0 && od1->adjrate == 0.0)
			return 0;
		if(od->adjrate == 0.0 && od1->mode != PTPCLOCK_SLAVE_SUB)
			return 0;
		if(od1->adjrate == 0.0 && od->mode != PTPCLOCK_SLAVE_SUB)
			return 0;
	}
	return 1;
}

int gptpclock_get_clock_params(int clockIndex, uint8_t domainNumber,
			       uint16_t *gmTimeBaseIndicator,
			       ScaledNs *lastGmPhaseChange,
			       double *lastGmFreqChange)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	*lastGmFreqChange = od->lastGmFreqChange;
	*gmTimeBaseIndicator = od->timeBaseIndicator;
	*lastGmPhaseChange = od->lastGmPhaseChange;
	return 0;
}

static int switch_active_domain(int di)
{
	oneclock_data_t *od;
	if(gcd.shm->head.active_domain==di) return 1;
	UB_TLOG(UBL_INFO, "active domain switched from %d to %d\n",
		 gcd.shm->head.active_domain, di);
	gcd.shm->head.active_domain=di;
	GPTPCLOCK_FN_ENTRY(od, 0, gcd.pdd[di].domainNumber);
	od->flags |= GPTPIPC_EVENT_CLOCK_FLAG_ACTIVE_DOMAIN;
	GH_SET_GPTP_SHM;
	return 0;
}

static int gptpclock_update_active_domain(void)
{
	int i;

	UB_LOG(UBL_DEBUGV, "%s:current active domain=%d\n",__func__,gcd.shm->head.active_domain);
	if(gcd.active_domain_switch>=0){
		return switch_active_domain(gcd.active_domain_switch);
	}

	if(gptpconf_get_intitem(CONF_ACTIVE_DOMAIN_AUTO_SWITCH)==0) return 0;

	if(gcd.pdd[gcd.shm->head.active_domain].gm_stable &&
	   gptpconf_get_intitem(CONF_ACTIVE_DOMAIN_AUTO_SWITCH)==1){
		UB_LOG(UBL_DEBUG, "%s:current active domain=%d is stable, don't switch\n",
		       __func__,gcd.shm->head.active_domain);
		return 0;
	}

	if(gcd.pdd[0].gm_stable){
		return switch_active_domain(0);
	}

	for(i=0;i<gcd.shm->head.max_domains;i++){
		if(gcd.pdd[i].gm_stable){
			return switch_active_domain(i);
		}
	}

	UB_LOG(UBL_DEBUG, "%s:no stable GM in all domains, current active domain=%d\n",
	       __func__,gcd.shm->head.active_domain);
	return -1;
}

static int adjust_GM_btw_domains(int domainNumber)
{
	UB_TLOG(UBL_INFO, "%s:domainNumber=%d\n",__func__, domainNumber);
	oneclock_data_t *od, *od1;
	// this is called after gm_stable of D0 becomes true
	GPTPCLOCK_FN_ENTRY(od, 0, 0);
	GPTPCLOCK_FN_ENTRY(od1, 0, domainNumber);

	if(gcd.pdd[0].thisClockIndex != gcd.pdd[od1->domainIndex].thisClockIndex){
		int64_t ts64;
		gptpclock_getts_od(&ts64, od);
		gptpclock_setts_od(ts64, od1);
		return 0;
	}
	// 'thisClock of D0' and 'thisClock of Di' is based on the same clock.
	od1->adjrate=0.0; // GM Freq. sync to Domain0
	od1->pp->adjrate=0.0;
	od1->offset64=od->offset64;
	od1->pp->offset64=od->pp->offset64;
	GH_SET_GPTP_SHM;
	return 0;
}

int gptpclock_active_domain_switch(int domainIndex)
{
	//domainIndex=-1:auto, domainIndex>=0:fix to the domain
	gcd.active_domain_switch=domainIndex;
	if(domainIndex<0 || domainIndex>=gcd.shm->head.max_domains) return 0;
	return switch_active_domain(gcd.active_domain_switch);
}

int gptpclock_active_domain_status(void)
{
	return gcd.shm->head.active_domain;
}

int gptpclock_set_gmsync(int clockIndex, uint8_t domainNumber, bool becomeGM)
{
	oneclock_data_t *od;
	UB_LOG(UBL_DEBUGV, "%s:clockIndex=%d, domainNumber=%d, becomeGM=%d\n",
	       __func__, clockIndex, domainNumber, becomeGM);
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	if(od->pp->gmsync) return 0;
	od->flags |= GPTPIPC_EVENT_CLOCK_FLAG_GM_SYNCED;
	od->pp->gmsync=true;
	if(clockIndex==0 && domainNumber!=0 && becomeGM)
		adjust_GM_btw_domains(domainNumber);
	if(clockIndex==0 && becomeGM && gptpconf_get_intitem(CONF_RESET_FREQADJ_BECOMEGM))
		gptpclock_setadj(0, 0, domainNumber);
	GH_SET_GPTP_SHM;
	return 0;
}

int gptpclock_reset_gmsync(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	UB_LOG(UBL_DEBUGV, "%s:clockIndex=%d, domainNumber=%d\n",
	       __func__, clockIndex, domainNumber);
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	if(!od->pp->gmsync) return 0;
	od->flags |= GPTPIPC_EVENT_CLOCK_FLAG_GM_UNSYNCED;
	od->pp->gmsync=false;
	return 0;
}

int gptpclock_get_gmsync(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	if(od->pp->gmsync) return 1;
	return 0;
}

void gptpclock_set_gmstable(int domainIndex, bool stable)
{
	oneclock_data_t *od;
	if(domainIndex<0 || domainIndex>=gcd.shm->head.max_domains) return;
	if(gcd.pdd[domainIndex].gm_stable==stable) return;
	gcd.pdd[domainIndex].gm_stable=stable;
	gptpclock_update_active_domain();
	if((od=get_clockod(gcd.pdd[domainIndex].thisClockIndex,
			   gcd.pdd[domainIndex].domainNumber))==NULL) return;
	if(od->lastgm_adjvppb!=LASTGM_ADJVPPB_INVALID){
		od->lastGmFreqChange = (double)(od->adjvppb - od->lastgm_adjvppb)/1.0E9;
		od->flags |= GPTPIPC_EVENT_CLOCK_FLAG_FREQ_UPDATE;
		UB_LOG(UBL_INFO, "%s:lastGmFreqChange=%fppb, last=%d, new=%d\n",
		       __func__, od->lastGmFreqChange*1.0E9,
		       od->lastgm_adjvppb, od->adjvppb);
		od->lastgm_adjvppb=LASTGM_ADJVPPB_INVALID;
	}
}

bool gptpclock_get_gmstable(int domainIndex)
{
	return gcd.pdd[domainIndex].gm_stable;
}

int gptpclock_set_gmchange(int domainNumber, ClockIdentity clockIdentity)
{
	oneclock_data_t *od;
	UB_LOG(UBL_DEBUGV, "%s:domainNumber=%d\n", __func__, domainNumber);
	GPTPCLOCK_FN_ENTRY(od, 0, domainNumber);
	od->flags |= GPTPIPC_EVENT_CLOCK_FLAG_GM_CHANGE;
	od->pp->gmchange_ind++;
	memcpy(gcd.pdd[od->domainIndex].gmClockId, clockIdentity, sizeof(ClockIdentity));
	od->timeBaseIndicator++;
	od->lastgm_offset64=od->offset64;
	od->lastGmPhaseChange.nsec=0;
	// FreqAdj is in thisClock
	if((od=get_clockod(gcd.pdd[od->domainIndex].thisClockIndex,
			   domainNumber))==NULL) return -1;
	od->lastgm_adjvppb=od->adjvppb;
	od->lastGmFreqChange=0.0;
	GH_SET_GPTP_SHM;
	return 0;
}

int gptpclock_get_gmchange_ind(int domainNumber)
{
	oneclock_data_t *od;
	GPTPCLOCK_FN_ENTRY(od, 0, domainNumber);
	return od->pp->gmchange_ind;
}

uint32_t gptpclock_get_event_flags(int clockIndex, uint8_t domainNumber)
{
	oneclock_data_t *od;
	uint32_t flags;
	if(!gcd.clds) return 0;
	if((od=get_clockod(clockIndex, domainNumber))==NULL) return 0;
	flags=od->flags;
	od->flags=0;
	return flags;
}

int gptpclock_get_ipc_clock_data(int clockIndex, uint8_t domainNumber, gptpipc_clock_data_t *cd)
{
	oneclock_data_t *od;
	double lastGmFreqChange;
	GPTPCLOCK_FN_ENTRY(od, clockIndex, domainNumber);
	lastGmFreqChange = (double)od->adjvppb/1.0E9;
	memcpy(&cd->lastGmFreqChangePk, &lastGmFreqChange, sizeof(double));
	cd->gmTimeBaseIndicator = od->timeBaseIndicator;
	cd->lastGmPhaseChange_nsec = od->lastGmPhaseChange.nsec;
	cd->gmsync = od->pp->gmsync;
	cd->domainNumber = od->pp->domainNumber;
	memcpy(cd->clockId, od->clockId, sizeof(ClockIdentity));
	cd->domainActive = (gcd.shm->head.active_domain==od->domainIndex);
	memcpy(cd->gmClockId, gcd.pdd[od->domainIndex].gmClockId, sizeof(ClockIdentity));
	return 0;
}

int gptpclock_set_thisClock(int clockIndex, uint8_t domainNumber, bool set_clock_para)
{
	oneclock_data_t *od, *mod;
	double adjrate;
	int64_t ts64;
	if(clockIndex==0){
		UB_LOG(UBL_ERROR,"%s:clockIndex=0 can't be thisClock\n", __func__);
		return -1;
	}
	if(!gcd.clds) return -1;
	if((od=get_clockod(clockIndex, domainNumber))==NULL) return -1;
	if((mod=get_clockod(0, domainNumber))==NULL) return -1;
	if(strcmp(od->pp->ptpdev, mod->pp->ptpdev)){
		UB_LOG(UBL_ERROR,
		       "%s:master clock and thisClock must be based on the same ptp clock\n",
			__func__);
		return -1;
	}

	gcd.pdd[od->domainIndex].thisClockIndex=clockIndex;
	// make sure the master clock(clockIndex=0) is PTPCLOCK_MASTER
	mod->mode=PTPCLOCK_MASTER;

	/* During the offset and adjrate are moved into thisClock from the master clock,
	   the master clock can't be read. So it must be locked by mutex.
	   clockIndex is never '0' in this section, and gcd.shm->head.mcmutex is
	   locked only when clockIndex==0 in other parts.
	   Make sure to keep the condition not to have deadlock.
	 */
	gptpclock_mutex_trylock(&gcd.shm->head.mcmutex);
	adjrate=od->adjrate;
	if((od->state == PTPCLOCK_RDWR) && !gptpclock_mode_slave_main(clockIndex, domainNumber)){
		//PTPCLOCK_SLAVE_MAIN
		if(!set_clock_para) goto mutexout;
		/* when thisClock was SLAVE_SUB before, it may have SW offset and adjrate
		   move them to HW offset and adjrate */
		if(od->offset64){
			gptpclock_setoffset64(od->offset64, clockIndex, domainNumber);
		}
		if(adjrate){
			gptpclock_setadj(adjrate*UB_SEC_NS, clockIndex, domainNumber);
		}

		/* move the offset in the master clock to thisClock */
		gptpclock_setoffset64(mod->offset64, clockIndex, domainNumber);
		UB_LOG(UBL_INFO, "%s:thisClock is clockIndex=%d, SLAVE_MAIN\n",
		       __func__, clockIndex);
	}else{
		//PTPCLOCK_SLAVE_SUB
		gptpclock_mode_slave_sub(clockIndex, domainNumber);

		if(!set_clock_para) goto mutexout;
		ts64 = od->offset64 + mod->offset64;
		od->offset64=ts64;
		UB_LOG(UBL_INFO, "%s:thisClock is clockIndex=%d, SLAVE_SUB\n",
		       __func__, clockIndex);
	}
	/* when set_clock_para==true, offset in the master clock has been moved to thisClock,
	   and it needs to be cleared */
	mod->offset64=0;
	mod->pp->offset64=mod->offset64+od->offset64;
mutexout:
	CB_THREAD_MUTEX_UNLOCK(&gcd.shm->head.mcmutex);
	GH_SET_GPTP_SHM;
	return 0;
}

int64_t gptpclock_d0ClockfromRT(int clockIndex)
{
        oneclock_data_t *od;
	int64_t ts1, ts2, ts3;
        GPTPCLOCK_FN_ENTRY(od, clockIndex, 0);
        if(gptpclock_getts_od(&ts1, od)){
		UB_LOG(UBL_ERROR, "%s:can't get ts1=TS(clk=%d,D=%d)\n",
                       __func__, od->clockIndex, od->pp->domainNumber);
                return 0;
        }
	ts2=ub_rt_gettime64();
        if(gptpclock_getts_od(&ts3, od)){
		UB_LOG(UBL_ERROR, "%s:can't get ts3=TS(clk=%d,D=%d)\n",
                       __func__, od->clockIndex, od->pp->domainNumber);
                return 0;
        }
	ts3=(ts3-ts1)/2;
	if(ts3 > od->ts2diff*10) {
		UB_LOG(UBL_WARN,"%s:gap of 2 ts is too big:clockIndex=%d, domainNumber=%d, %"
		       PRIi64"\n",__func__, od->clockIndex, od->pp->domainNumber, ts3);
	}
	UB_LOG(UBL_DEBUGV,"%s:%d, %d, %"PRIi64"\n",
	       __func__, od->clockIndex, od->pp->domainNumber, ts1-ts2+ts3);
	return ts1-ts2+ts3;
}
