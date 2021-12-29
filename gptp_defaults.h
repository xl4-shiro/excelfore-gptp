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
#ifndef __GPTP_DEFAULTS_H
#define __GPTP_DEFAULTS_H

#define DEFAULT_TEST_SYNC_REC_PORT -1 // set -1 for normal operation
#define DEFAULT_TEST_SYNC_SEND_PORT -1 // set -1 for normal operation

/* Number of clock instances depends on the number of port multiply by the
   number of domains. Number of port is static but number of domain can change
   dynamically. The value CONF_MAX_DOMAIN_NUMBER x CONF_MAX_PORT_NUMBER
   is used for clock instance limit. */
#define DEFAULT_MAX_DOMAIN_NUMBER 1
#define DEFAULT_MAX_PORT_NUMBER 8

/* 10.2.5.13 ptpPortEnabled
   Per-port configuration to enable or disable gPTP capability.
   Comma separated list of values that matches the number of ports (excluding port 0)
   value 0: ptpPortEnabled is false
   value 1: ptpPortEnabled is true
   When this configuration is not enabled, all ports defaults to gPTP capable port. */
#define DEFAULT_PTP_PORT_ENABLED "1,1,1,1,1,1,1,1" // max_length=32 use ""

#define DEFAULT_MASTER_PTPDEV "" // max_length=32 use "" for the first detected ptpdev

#define DEFAULT_TXTS_LOST_TIME 20000000 //20msec, give up if no TxTS in this time

/* to set up the second domain, set this number for the clockIndex which is used
   for the second domain. For the second domin, only thisClock and master clock
   are added. CONF_CMLDS_MODE must be set to use multiple domains.
   set -1 not to use the second domain. */
#define DEFAULT_FIRST_DOMAIN_THIS_CLOCK 1 // 1 to use the first ptpdevice
#define DEFAULT_SECOND_DOMAIN_THIS_CLOCK -1
#define DEFAULT_SECOND_DOMAIN_NUMBER 1

/* set 1 for single clock with multiple ports.  Switches are likely in that mode */
#define DEFAULT_SINGLE_CLOCK_MODE 0

/* AFTERSEND_GUARDTIME, is a guard time not to send the next packet in this time.
   This is needed for the other end. Some devices don't have queue for RxTs.
   if Sync and FollowUp comes in very short time, FollowUp RxTs overwrites Sync RxTs.
   Actually FollowUp RxTs is not needed, but many devices capture Ts for all PTP messages.
   For Intel i210, this value must be set for TxTs.  1000000 for Minnow board.
*/
#define DEFAULT_AFTERSEND_GUARDTIME 300000 // nsec unit

/* gptpnet_extra_timeout call use this value when 'toutns=0' */
#define DEFAULT_GPTPNET_EXTRA_TOUTNS 1000000 //1msec

/* absolute value of clock rate adjustment shouldn't go beyond this value */
#define DEFAULT_MAX_ADJUST_RATE_ON_CLOCK 1000000 //ppb unit

#define DEFAULT_IPC_NOTICE_PHASE_UPDATE 1
#define DEFAULT_IPC_NOTICE_FREQ_UPDATE 0

/* setting this value, TAS(Domain 0 only) becomes asCapable without PdelayResponse,
   the value should be estimated Pdelay in nsec. */
#define DEFAULT_NEIGHBOR_PROP_DELAY 0

/* -1 for nomal operation
   stting this value, BMCS is skipped and the port states are statically configured.
   value 0: port0 is SLAVE and the others are MASTER, which means this Device is GM.
   positive number 'N': port N is SLAVE and the other are MASTER, a connected device
   on port N is GM. */
#define DEFAULT_STATIC_PORT_STATE_SLAVE_PORT -1

/* 8.2.2 Timescale if PTP (1) is epoch is PTP epoch
   otherwise, ARB (0) if time scale is arbitrary
   Notes:
   - AVNU (gPTP.com.c.12.05a,gPTP.com.c.12.05a, and gPTP.com.c.12.05c) requires
   the value of this setting to be set to (1) */
#define DEFAULT_TIMESCALE_PTP 1

/* Option to conform to AVNU instead of 802.1AS specification in case of
   conflict.
   By default, set 0 such that 802.1AS behavior is followed for items where
   conforming to AVNU will violate the 802.1AS specifications.

   The following is a list of known AVNU requirements that violates 802.1AS:
   - gPTP.br.c.24.1 - Appending path trace TLV - over max length
     802.1AS requires PathTrace field to exist with length=0 in cases where TLV
	 pathTrace is empty.
	 AVNU on the other hand, requires the PathTrace field to be absent.
   - gPTP.com.c.18.1 / gPTP.com.c.15.7 - AVnu alliance requires in addition that
     devices cease transmit of PDelayReq when 3 successive request are responded
	 with multiple response.
 */
#define DEFAULT_FOLLOW_AVNU 0

// 10.3.2 systemIdentity
// PRIMARY is used for domainNumber=0, SECONDARY is used for the other domains
#define DEFAULT_PRIMARY_PRIORITY1 248 // 8.6.2.1 priority1, 255 for not GM-capable
#define DEFAULT_PRIMARY_CLOCK_CLASS 248 // 8.6.2.2 clockClass
#define DEFAULT_PRIMARY_CLOCK_ACCURACY 0x22 // 8.6.2.3 clockAccuracy, accurate to within 250ns
#define DEFAULT_PRIMARY_OFFSET_SCALED_LOG_VARIANCE 0x436A // 8.6.2.4 offsetScaledLogVariance
#define DEFAULT_PRIMARY_PRIORITY2 248 // 8.6.2.5 priority2

#define DEFAULT_SECONDARY_PRIORITY1 248 // 8.6.2.1 priority1, 255 for not GM-capable
#define DEFAULT_SECONDARY_CLOCK_CLASS 248 // 8.6.2.2 clockClass
#define DEFAULT_SECONDARY_CLOCK_ACCURACY 0x22 // 8.6.2.3 clockAccuracy, accurate to within 250ns
#define DEFAULT_SECONDARY_OFFSET_SCALED_LOG_VARIANCE 0x436A // 8.6.2.4 offsetScaledLogVariance
#define DEFAULT_SECONDARY_PRIORITY2 248 // 8.6.2.5 priority2

#define DEFAULT_TIME_SOURCE INTERNAL_OSCILLATOR

// Option to perform quick restart of BMCS on master's configuration change
// 0 - disabled (default) means that BMCS follows 802.1AS-Rev standards where slave waits for
// timeout before finding new master when most recent master changes configuration.
// 1 - enabled will perform quick switch when Announce from current master suddenly changes,
// this includes:
//  - disconnection of current master in upstream resulting to change in priority
//  - external configuration change of current master resulting to change in priority
#define DEFAULT_BMCS_QUICK_UPDATE_MODE 0

// 10.3.8.24 externalPortConfiguration
// VALUE_DISABLED means that port states are determined by BMCS
// VALUE_ENABLED means that port states are configured externally by the system
//   10.3.14.1.3 rcvdPortStateInd must also be configured externally for each port
//   10.3.14.1.4 portStateInd must also be configured externally for each port
#define DEFAULT_EXTERNAL_PORT_CONFIGURATION VALUE_DISABLED

// 10.7.2.2 Announce message transmission interval default value
#define DEFAULT_INITIAL_LOG_ANNOUNCE_INTERVAL 0

// 10.7.2.5 Interval for sending the gPTP capable TLV Signaling message
// The default value shall be TBD. The range shall be TBD.
#define DEFAULT_LOG_GPTP_CAPABLE_MESSAGE_INTERVAL 3
// 10.7.3.3 gPtpCapableReceiptTimeout, the value is still TBD
// this number of gPtpCapableMessageInterval cycle time
#define DEFAULT_GPTP_CAPABLE_RECEIPT_TIMEOUT 3

// 10.7.3.1 syncReceiptTimeout
#define DEFAULT_SYNC_RECEIPT_TIMEOUT 3

// 10.7.3.2 announceReceiptTimeout
#define DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT 3

// 11.2.15 Common Mean Link Delay Service
#define DEFAULT_CMLDS_MODE 0 // 0 for PTP-instance-specific, 1 for CMLDS

// 10.6.2.2.3
// In 802.1AS-2011 minorVersionPTP field doens't exist and the field is reserved.
// when 'the reserved fieds == all zero' is checked, this value may be set to 0
#define DEFAULT_MINOR_VERSION_PTP 1 // this should be 1

// 11.5.2.3 Sync message transmission interval default value
#define DEFAULT_LOG_SYNC_INTERVAL -3
#define DEFAULT_LOG_ANNOUNCE_INTERVAL 0
#define DEFAULT_LOG_PDELAYREQ_INTERVAL 0
#define DEFAULT_LOG_DELAYREQ_INTERVAL 3

// 11.5.3 allowedLostResponses
#define DEFAULT_ALLOWED_LOST_RESPONSE 9 // updated from 3 to 9 in 802.1AS-REV
// 11.5.4 allowedFaults
#define DEFAULT_ALLOWED_FAULTS 9
// Table 11-1-Value of neighborPropDelayThresh for various links
// Upper limit for peer delay in nanoseconds, computed values greater than this
// will mark the port as asCapable
//#define DEFAULT_NEIGHBOR_PROPDELAY_THRESH 800 // default for AVnu Test Plan
#define DEFAULT_NEIGHBOR_PROPDELAY_THRESH 40000 // for Intel i218,i219

// Lower limit for peer delay in nanoseconds, computed values lower than this
// will mark the port as non asCapable
#define DEFAULT_NEIGHBOR_PROPDELAY_MINLIMIT 0

// if this is a positive number, use a UDP connection for the IPC instaead of Unix Domain Socket
// the defined number becomes the UDP server port number
#define DEFAULT_IPC_UDP_PORT 0

// set Domain0's priority1 for 254(lowest) for this time after initialization
// E.G. setting 12, the first 12 seconds of Domain0 uses priority1=254
#define DEFAULT_INITIAL_SLAVE_TIME 0

// msec unit, criterion to set GM stable, INITIAL_* is used when all ports are not asCapable
#define DEFAULT_INITIAL_GM_STABLE_TIME 1000
#define DEFAULT_NORMAL_GM_STABLE_TIME 10000

// ptp clock offset and freq adjustment
// IIR filter coefficients
#define DEFAULT_FREQ_OFFSET_IIR_ALPHA_START_VALUE 2 // reciprocal number is used
#define DEFAULT_FREQ_OFFSET_IIR_ALPHA_STABLE_VALUE 10 // reciprocal number is used
#define DEFAULT_PHASE_OFFSET_IIR_ALPHA_START_VALUE 2 // reciprocal number is used
#define DEFAULT_PHASE_OFFSET_IIR_ALPHA_STABLE_VALUE 10 // reciprocal number is used

#define DEFAULT_FREQ_OFFSET_STABLE_PPB 100 // freq. is stable if delta of adj rate is less then this

// switch active domain automatically to stable domain
// 2: even the current active domain is stable, if any lower number of domain is stable
//    switch the active domain to the lowest number of stable domain
//    by this, the active domain eventually go to domain '0', and all devices use
//    domain '0' as the active domain.
// 1: if the current doamin becomes unstable, switch to the lowest number of stable domain
// 0: no switching, it is switched only by the IPC command
#define DEFAULT_ACTIVE_DOMAIN_AUTO_SWITCH 2

#define DEFAULT_TSN_SCHEDULE_ON 0 //1:use TSN scheduling, 0: no use
#define DEFAULT_TSN_SCHEDULE_ALIGNTIME 100000000 // 100msec
#define DEFAULT_TSN_SCHEDULE_CYCLETIME 10000000 // 10msec

// a file name to save debug log memory at the end of gptp2d run
#define DEFAULT_DEBUGLOG_MEMORY_FILE "/tmp/gptp2d_debugmem.log" // max_length=64

// size of the debug log memory. if this is '0', no debug log memory is used
#define DEFAULT_DEBUGLOG_MEMORY_SIZE 64 // Kbytes

// when the over ip mode is used, use this port number as the starting port
// each network port uses two ports: source port and destination port.
// SPN: this number, i:the index of network port
// if SPN is even, source=SPN+i*2, dest=SPN+1+i*2
// if SPN is odd, source=SPN+i*2, dest=SPN-1+i*2
#define DEFAULT_OVIP_MODE_STRT_PORTNO 5018

// for nomal use, set "". then it uses GPTP_MASTER_CLOCK_SHARED_MEM as hardcoded value.
// when the over ip mode is used, multiple gptp2d can be run.
// Only in such case, the shared mem name must be changed.
#define DEFAULT_MASTER_CLOCK_SHARED_MEM "" // max_length=32

// for the over ip mode testing, this clock rate(ppb unit) change is applied.
#define DEFAULT_PTPVFD_CLOCK_RATE 0

// low-pass-filter threshold value for calculating the average ts2diff
// (time spend to setup the clock value)
#define DEFAULT_MAX_CONSEC_TS_DIFF 500000 //500usec

// ts2diff(time spent to setup the clock value) is measured at the top,
// the value is compensated by this percentage to cancel cache effect
#define DEFAULT_TS2DIFF_CACHE_FACTOR 150

// This should be 0. When the shared memory is not available and only a single domain
// is used, setting this value to '1' makes the adjustment apply on HW clock.
// There is a risk of disrupted timestamps; happening errors might be covered in tolerance level.
#define DEFAULT_USE_HW_PHASE_ADJUSTMENT 0

// a small phase adjustment is done by a frequency change
#define DEFAULT_PHASE_ADJUSTMENT_BY_FREQ 1

// Setting to '1', the md_abnormal_hooks layer is activated.
// Even if this is activated, no actions happens until abnormal events are registered.
#define DEFAULT_ACTIVATE_ABNORMAL_HOOKS 0

// Setting to '1', set frequency adjustment rate '0' when this device becomes GM.
// this may make a jump of frequency from the previous GM to this new GM, but it
// can correct the rate when the previous GM has a bad rate.
#define DEFAULT_RESET_FREQADJ_BECOMEGM 0

// GHS INTEGRITY ptp clock info contains clock name and clock frequency increase.
// The clock info follow the format "clock_name:clock_freq" and may vary in the OS and HW.
// Refer to the INTEGRITY BSP to configure the gptp daemon with the correct info.
// Multiple clocks shall be separated by comma e.g. "PTP1:10000,PTP2:20000,PTP3:30000".
#define DEFAULT_INTEGRITY_CLOCK_INFO "IOD_AVB_PTP_Clock:7876923" // max_length=128

#endif
