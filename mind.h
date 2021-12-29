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
#ifndef __MIND_H_
#define __MIND_H_

#include "gptpbasetypes.h"

#define NUMBER_OF_PORTS 2

typedef struct PTPMsgAnnounce PTPMsgAnnounce;
typedef struct PTPMsgIntervalRequestTLV PTPMsgIntervalRequestTLV;
typedef struct PTPMsgGPTPCapableTLV PTPMsgGPTPCapableTLV;
typedef struct PerPortGlobal PerPortGlobal;

// 10.2 Time-synchronization state machines
// 10.2.2.1 MDSyncSend
typedef struct MDSyncSend {
	uint8_t domainNumber;
	ScaledNs followUpCorrectionField;
	PortIdentity sourcePortIdentity;
	int8_t logMessageInterval;
	Timestamp preciseOriginTimestamp;
	UScaledNs upstreamTxTime;
	double rateRatio;
	uint16_t gmTimeBaseIndicator;
	ScaledNs lastGmPhaseChange;
	double lastGmFreqChange;
} MDSyncSend;

// 10.2.2.2 MDSyncReceive
typedef struct MDSyncReceive {
	uint8_t domainNumber;
	ScaledNs followUpCorrectionField;
	PortIdentity sourcePortIdentity;
	int8_t logMessageInterval;
	Timestamp preciseOriginTimestamp;
	UScaledNs upstreamTxTime;
	double rateRatio;
	uint16_t gmTimeBaseIndicator;
	ScaledNs lastGmPhaseChange;
	double lastGmFreqChange;
} MDSyncReceive;

// 10.2.2.3 PortSyncSync
typedef struct PortSyncSync {
	uint8_t domainNumber;
	int16_t localPortNumber;
	UScaledNs syncReceiptTimeoutTime;
	ScaledNs followUpCorrectionField;
	PortIdentity sourcePortIdentity;
	int8_t logMessageInterval;
	Timestamp preciseOriginTimestamp;
	UScaledNs upstreamTxTime;
	double rateRatio;
	uint16_t gmTimeBaseIndicator;
	ScaledNs lastGmPhaseChange;
	double lastGmFreqChange;
	// As portNumber is not the same as portIndex, we keep portIndex separately
	int16_t localPortIndex;
	// Next send time relative to current time
	UScaledNs syncNextSendTimeoutTime;
	PerPortGlobal *local_ppg; // per-port-global for the localPort
} PortSyncSync;

// 10.2.3 Per-time-aware-system global variables
typedef struct PerTimeAwareSystemGlobal {
	bool BEGIN;
	UScaledNs clockMasterSyncInterval;
	ExtendedTimestamp clockSlaveTime;
	ExtendedTimestamp syncReceiptTime;
	UScaledNs syncReceiptLocalTime;
	double clockSourceFreqOffset;
	ScaledNs clockSourcePhaseOffset;
	uint16_t clockSourceTimeBaseIndicator;
	uint16_t clockSourceTimeBaseIndicatorOld;
	ScaledNs clockSourceLastGmPhaseChange;
	double clockSourceLastGmFreqChange;
	//UScaledNs currentTime; //we use gptpclock of (clockIndex,domainNumber)
	bool gmPresent;
	double gmRateRatio;
	uint16_t gmTimeBaseIndicator;
	ScaledNs lastGmPhaseChange;
	double lastGmFreqChange;
	TimeInterval localClockTickInterval;
	//UScaledNs localTime;  //we use gptpclock of (clockIndex,domainNumber)
	Enumeration2 selectedState[MAX_PORT_NUMBER_LIMIT];
	//ExtendedTimestamp masterTime; //we use gptpclock of (clockIndex=0,domainNumber)
	ClockIdentity thisClock;
	int8_t parentLogSyncInterval;
	bool instanceEnable;
	uint8_t domainNumber; // domainNumber is not defined in the standard, but we need here
	int thisClockIndex; // index of the gptpclock entity of 'thisClock'
	int8_t clockMasterLogSyncInterval;
	bool gm_stable_initdone;
	bool asCapableOrAll;
	// Flag to determine if AVNU is followed over 802.1AS
	bool conformToAvnu;
} PerTimeAwareSystemGlobal;

// 10.2.4 Per-port global variables
typedef struct PerPortGlobalForAllDomain {
	bool asymmetryMeasurementMode;
	double neighborRateRatio;
	UScaledNs neighborPropDelay;
	UScaledNs delayAsymmetry;
	bool computeNeighborRateRatio;
	bool computeNeighborPropDelay;
	bool portOper;
        bool useMgtSettableLogAnnounceInterval;
        int8_t mgtSettableLogAnnounceInterval;
	//14.8.25 useMgtSettableLogPdelayReqInterval
	bool useMgtSettableLogPdelayReqInterval;
	int8_t mgtSettableLogPdelayReqInterval;
	//when PdelayReq comes in CMLDS mode(SdoId=0x200), set this
	int8_t receivedNonCMLDSPdelayReq;
}PerPortGlobalForAllDomain;
struct PerPortGlobal {
	bool asCapable; // for domain!=0, somebody needs to set this
	uint32_t portEventFlags;
	UScaledNs syncReceiptTimeoutTimeInterval;
	int8_t currentLogSyncInterval;
	int8_t initialLogSyncInterval;
	UScaledNs syncInterval;
	bool ptpPortEnabled;
	uint16_t thisPort;
	// As portNumber is not the same as portIndex, we keep portIndex separately
	uint16_t thisPortIndex;
	bool syncLocked;
	bool neighborGptpCapable;
	bool syncSlowdown;
	UScaledNs oldAnnounceInterval;
	PerPortGlobalForAllDomain *forAllDomain;
	// 10.7.3.1 syncReceiptTimeout
	int8_t syncReceiptTimeout;
	// 10.4.1 gPtpCapableTransmit state machine needs this value
	int8_t logGptpCapableMessageInterval;
	// 10.7.3.3 gPtpCapableReceiptTimeout
	int8_t gPtpCapableReceiptTimeout;
	// 14.8.19 useMgtSettableLogSyncInterval
	bool useMgtSettableLogSyncInterval;
	// 14.8.20 mgtSettableLogSyncInterval
	int8_t mgtSettableLogSyncInterval;
	// 14.8.38 initialOneStepTxOper
	bool initialOneStepTxOper;
	// 14.8.39 currentOneStepTxOper
	bool currentOneStepTxOper;
	// 14.8.40 useMgtSettableOneStepTxOper
	bool useMgtSettableOneStepTxOper;
	// 14.8.41 mgtSettableOneStepTxOper
	bool mgtSettableOneStepTxOper;

};

// 10.2.6 SiteSyncSync state machine
typedef struct SiteSyncSyncSM {
	bool rcvdPSSync;
	PortSyncSync *rcvdPSSyncPtr;
	// 802.1AS-2020 10.2.7.1.3 txPSSyncPtrSSS
	PortSyncSync *txPSSyncPtrSSS;
} SiteSyncSyncSM;

// 10.2.7 PortSyncSyncReceive state machine
typedef struct PortSyncSyncReceiveSM {
	bool rcvdMDSync;
	MDSyncReceive *rcvdMDSyncPtr;
	PortSyncSync *txPSSyncPtr;
	double rateRatio;
} PortSyncSyncReceiveSM;

// 10.2.8 ClockMasterSyncSend state machine
typedef struct ClockMasterSyncSendSM {
	UScaledNs syncSendTime;
	PortSyncSync *txPSSyncPtr;
} ClockMasterSyncSendSM;

// 10.2.9 ClockMasterSyncOffset state machine
typedef struct ClockMasterSyncOffsetSM {
	bool rcvdSyncReceiptTime;
} ClockMasterSyncOffsetSM;

// 10.2.10 ClockMasterSyncReceive state machine
typedef struct ClockMasterSyncReceiveSM {
	bool rcvdClockSourceReq;
	// a pointer to the received ClockSourceTime.invoke function parameters.
	void *rcvdClockSourceReqPtr;
	bool rcvdLocalClockTick;
} ClockMasterSyncReceiveSM;

// 10.2.11 PortSyncSyncSend state machine
typedef struct PortSyncSyncSendSM {
	bool rcvdPSSync;
	PortSyncSync *rcvdPSSyncPtr;
	Timestamp lastPreciseOriginTimestamp;
	ScaledNs lastFollowUpCorrectionField;
	double lastRateRatio;
	UScaledNs lastUpstreamTxTime;
	UScaledNs lastSyncSentTime;
	uint16_t lastRcvdPortNum;
	uint16_t lastGmTimeBaseIndicator;
	ScaledNs lastGmPhaseChange;
	double lastGmFreqChange;
	MDSyncSend *txMDSyncSendPtr;
	UScaledNs syncReceiptTimeoutTime;
	uint8_t numberSyncTransmissions;
	UScaledNs interval1;
} PortSyncSyncSendSM;

// 10.2.12 ClockSlaveSync state machine
typedef struct ClockSlaveSyncSM {
	bool rcvdPSSync;
	bool rcvdLocalClockTick;
	PortSyncSync *rcvdPSSyncPtr;
} ClockSlaveSyncSM;


// 10.3 Best master clock selection
// 10.3.8 Per-time-aware-system global variables
typedef struct BmcsPerTimeAwareSystemGlobal {
	bool reselect[MAX_PORT_NUMBER_LIMIT];
	bool selected[MAX_PORT_NUMBER_LIMIT];
	uint16_t masterStepsRemoved;
	bool leap61;
	bool leap59;
	bool currentUtcOffsetValid;
	bool ptpTimescale;
	bool timeTraceable;
	bool frequencyTraceable;
	int16_t currentUtcOffset;
	Enumeration8 timeSource;
	bool sysLeap61;
	bool sysLeap59;
	bool sysCurrentUTCOffsetValid;
	bool sysPtpTimescale;
	bool sysTimeTraceable;
	bool sysFrequencyTraceable;
	int16_t sysCurrentUtcOffset;
	Enumeration8 sysTimeSource;
	UInteger224 systemPriority;
	UInteger224 gmPriority;
	UInteger224 lastGmPriority;
	// Addition of pathTrace length
	uint8_t pathTraceCount;
	ClockIdentity pathTrace[MAX_PATH_TRACE_N];
	Enumeration8 externalPortConfiguration;
	uint32_t lastAnnouncePort;
	int bmcsQuickUpdate;
} BmcsPerTimeAwareSystemGlobal;

// 10.3.9 Per-port global variables
typedef struct BmcsPerPortGlobal {
	UScaledNs announceReceiptTimeoutTimeInterval;
	bool announceSlowdown;
	UScaledNs oldAnnounceInterval;
	Enumeration2 infoIs;
	UInteger224 masterPriority;
	int8_t currentLogAnnounceInterval;
	int8_t initialLogAnnounceInterval;
	UScaledNs announceInterval;
	uint16_t messageStepsRemoved;
	bool newInfo;
	UInteger224 portPriority;
	uint16_t portStepsRemoved;
	PTPMsgAnnounce *rcvdAnnouncePtr;
	bool rcvdMsg;
	bool updtInfo;
	bool annLeap61;
	bool annLeap59;
	bool annCurrentUtcOffsetValid;
	bool annPtpTimescale;
	bool annTimeTraceable;
	bool annFrequencyTraceable;
	int16_t annCurrentUtcOffset;
	Enumeration8 annTimeSource;
	// ??? global pathTrace is updated only when portState is known
	// to be SlavePort, in the case when system is grandmaster (no SlavePort)
	// and the Announce received may convey transition of portState to SlavePort
	// a copy of the announce pathSequence should be used for global pathTrace
	uint8_t annPathSequenceCount;
	ClockIdentity annPathSequence[MAX_PATH_TRACE_N];
	// Additional from 10.7.3 Timeouts
	int8_t announceReceiptTimeout;
} BmcsPerPortGlobal;

// 10.3.9.4 infoIs
typedef enum {
        Received,
        Mine,
        Aged,
        Disabled
} BmcsInfoIs;

// 10.3.11.2.1 rcvInfo
typedef enum {
        RepeatedMasterInfo,
        SuperiorMasterInfo,
        InferiorMasterInfo,
        OtherInfo
} BmcsRcvdInfo;

// 10.3.10 PortAnnounceReceive state machine
typedef struct PortAnnounceReceiveSM {
	bool rcvdAnnounce;
} PortAnnounceReceiveSM;

// 10.3.11 PortAnnounceInformation state machine
typedef struct PortAnnounceInformationSM {
	UScaledNs announceReceiptTimeoutTime;
	UInteger224 messagePriority;
	Enumeration2 rcvdInfo;
} PortAnnounceInformationSM;

// 10.3.12 PortStateSelection state machine
typedef struct PortStateSelectionSMforAllDomain {
	bool asymmetryMeasurementModeChange;
} PortStateSelectionSMforAllDomain;
typedef struct PortStateSelectionSM {
	bool  systemIdentityChange;
	PortStateSelectionSMforAllDomain *forAllDomain;
} PortStateSelectionSM;

// 10.3.13 PortAnnounceInformationExt state machine
typedef struct PortAnnounceInformationExtSM {
	bool rcvdAnnounce;
	UInteger224 messagePriority;
} PortAnnounceInformationExtSM;

// 10.3.14 PortStateSettingExt state machine
typedef struct PortStateSettingExtSMforAllDomain {
	bool asymmetryMeasurementModeChangeThisPort;
} PortStateSettingExtSMforAllDomain;
typedef struct PortStateSettingExtSM {
	bool disabledExt;
	bool rcvdPortStateInd;
	Enumeration2 portStateInd;
	PortStateSettingExtSMforAllDomain *forAllDomain;
} PortStateSettingExtSM;

// 10.3.15 PortAnnounceTransmit state machine
typedef struct PortAnnounceTransmitSM {
	UScaledNs announceSendTime;
	uint8_t numberAnnounceTransmissions;
	UScaledNs interval2;
} PortAnnounceTransmitSM;

// 10.3.16 AnnounceIntervalSetting state machine
typedef struct AnnounceIntervalSettingSM {
	bool rcvdSignalingMsg2;
	PTPMsgIntervalRequestTLV *rcvdSignalingPtr;
} AnnounceIntervalSettingSM;

// 10.4 State machines related to signaling gPTP protocol capability
// 10.4.1 gPtpCapableTransmit state machine
typedef struct gPtpCapableTransmitSM {
	UScaledNs signalingMsgTimeInterval;
	UScaledNs intervalTimer;
	PTPMsgGPTPCapableTLV *txSignalingMsgPtr;
} gPtpCapableTransmitSM;

// 10.4.2 gPtpCapableReceive state machine
typedef struct gPtpCapableReceiveSM {
	bool rcvdGptpCapableTlv;
	PTPMsgGPTPCapableTLV *rcvdSignalingMsgPtr;
	UScaledNs gPtpCapableReceiptTimeoutInterval;
	UScaledNs timeoutTime;
} gPtpCapableReceiveSM;

// 10.6.2 Message formats - Header
typedef struct PTPMsgHeader {
	Nibble majorSdoId;
	Enumeration4 messageType;
	UInteger4 minorVersionPTP;
	UInteger4 versionPTP;
	uint16_t messageLength;
	uint8_t domainNumber;
	uint8_t minorSdoId;
	Octet2 flags;
	int64_t correctionField;
	Octet4 messageTypeSpecific;
	PortIdentity sourcePortIdentity;
	uint16_t sequenceId;
	uint8_t control;
	int8_t logMessageInterval;
} PTPMsgHeader;

// 10.6.3 Announce message
struct PTPMsgAnnounce {
	PTPMsgHeader header;
	int16_t currentUtcOffset;
	uint8_t grandmasterPriority1;
	ClockQuality grandmasterClockQuality;
	uint8_t grandmasterPriority2;
	ClockIdentity grandmasterIdentity;
	uint16_t stepsRemoved;
	Enumeration8 timeSource;
	Enumeration16 tlvType;
	uint16_t tlvLength;
	ClockIdentity pathSequence[MAX_PATH_TRACE_N];
};

// 10.6.4.3 Message interval request TLV definition
struct PTPMsgIntervalRequestTLV {
	Enumeration16 tlvType;
	uint16_t lengthField;
	Octet3 organizationId;
	Enumeration24 organizationSubType;
	int8_t linkDelayInterval;
	int8_t timeSyncInterval;
	int8_t announceInterval;
	Octet flags;
};

// 10.6.4.4 gPTP capable TLV
struct PTPMsgGPTPCapableTLV {
	Enumeration16 tlvType;
	uint16_t lengthField;
	Octet3 organizationId;
	Enumeration24 organizationSubType;
	int8_t logGptpCapableMessageInterval;
	Octet flags;
};

// 10.6.4.3.9 flags (Octet)
#define COMPUTE_NEIGHBOR_RATE_RATIO_BIT 0
#define COMPUTE_NEIGHBOR_PROP_DELAY_BIT 1
#define ONE_STEP_RECEIVE_CAPABLE_BIT 2

void ptas_glb_init(PerTimeAwareSystemGlobal **tasglb, uint8_t domainNumber);
void ptas_glb_close(PerTimeAwareSystemGlobal **tasglb);

void pp_glb_init(PerPortGlobal **ppglb, PerPortGlobalForAllDomain *forAllDomain,
		 uint16_t portIndex);
void pp_glb_close(PerPortGlobal **ppglb, int domainNumber);

void bmcs_ptas_glb_init(BmcsPerTimeAwareSystemGlobal **btasglb,
                        PerTimeAwareSystemGlobal *ptasglb);
void bmcs_ptas_glb_update(BmcsPerTimeAwareSystemGlobal **btasglb,
                          PerTimeAwareSystemGlobal *ptasglb, bool primary);
void bmcs_ptas_glb_close(BmcsPerTimeAwareSystemGlobal **btasglb);

void bmcs_pp_glb_init(BmcsPerPortGlobal **bppglb);
void bmcs_pp_glb_close(BmcsPerPortGlobal **bppglb);

#endif
