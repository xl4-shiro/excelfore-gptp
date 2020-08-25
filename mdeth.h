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
#ifndef __MDETH_H_
#define __MDETH_H_

#include "mind.h"
#include "gptpnet.h"

/************************************************
  data types directly point to network data
*************************************************/
typedef struct MDPortIdentity {
	ClockIdentity clockIdentity;
	uint16_t portNumber_ns;
} __attribute__((packed)) MDPortIdentity;

typedef struct MDClockQuality {
	uint8_t clockClass;
	Enumeration8 clockAccuracy;
	uint16_t offsetScaledLogVariance_ns;
} __attribute__((packed)) MDClockQuality;

typedef struct MDScaledNs {
	int16_t nsec_msb;
	int64_t nsec_nll;
	uint16_t subns_ns;
} __attribute__((packed)) MDScaledNs;

typedef struct MDUScaledNs {
	uint16_t nsec_msb;
	uint64_t nsec_nll;
	uint16_t subns_ns;
} __attribute__((packed)) MDUScaledNs;

typedef struct MDTimestamp {
	uint16_t seconds_msb_ns;
	uint32_t seconds_lsb_nl;
	uint32_t nanoseconds_nl;
} __attribute__((packed)) MDTimestamp;

// 11.4.2 Header
typedef struct MDPTPMsgHeader {
	uint8_t majorSdoId_messageType;
	uint8_t minorVersionPTP_versionPTP;
	uint16_t messageLength_ns;
	uint8_t domainNumber;
	uint8_t minorSdoId;
	uint8_t flags[2];
	int64_t correctionField_nll;
	uint8_t messageTypeSpecific[4];
	MDPortIdentity sourcePortIdentity;
	uint16_t sequenceId_ns;
	uint8_t control;
	int8_t logMessageInterval;
} __attribute__((packed)) MDPTPMsgHeader;

// 11.4.4.3 Follow_Up information TLV
typedef struct MDFollowUpInformationTLV {
	uint16_t tlvType_ns;
	uint16_t lengthField_ns;
	uint8_t organizationId[3];
	uint8_t organizationSubType_nb[3];
	int32_t cumulativeScaledRateOffset_nl;
	uint16_t gmTimeBaseIndicator_ns;
	MDScaledNs lastGmPhaseChange;
	int32_t scaledLastGmFreqChange_nl;
} __attribute__((packed)) MDFollowUpInformationTLV;

// 11.4.3.1 General Sync message specifications
typedef struct MDPTPMsgSync {
	MDPTPMsgHeader head;
	uint8_t reserved[10];
} __attribute__((packed)) MDPTPMsgSync;

typedef struct MDPTPMsgSyncOneStep {
	MDPTPMsgHeader head;
	MDTimestamp originTimestamp;
	MDFollowUpInformationTLV FUpInfoTLV;
} __attribute__((packed)) MDPTPMsgSyncOneStep;

// 11.4.4 Follow_Up
// 11.4.4.1 General Follow_Up message specifications
typedef struct MDPTPMsgFollowUp {
	MDPTPMsgHeader head;
	MDTimestamp  preciseOriginTimestamp;
	MDFollowUpInformationTLV FUpInfoTLV;
} __attribute__((packed)) MDPTPMsgFollowUp;

// 11.4.5 Pdelay_Req message
typedef struct MDPTPMsgPdelayReq {
	MDPTPMsgHeader head;
	uint8_t reserved0[10];
	uint8_t reserved1[10];
} __attribute__((packed)) MDPTPMsgPdelayReq;

// 11.4.6 Pdelay_Resp message
typedef struct MDPTPMsgPdelayResp {
	MDPTPMsgHeader head;
	MDTimestamp requestReceiptTimestamp;
	MDPortIdentity requestingPortIdentity;
} __attribute__((packed)) MDPTPMsgPdelayResp;

// 11.4.7 Pdelay_Resp_Follow_Up message
typedef struct MDPTPMsgPdelayRespFollowUp {
	MDPTPMsgHeader head;
	MDTimestamp requestOriginTimestamp;
	MDPortIdentity requestingPortIdentity;
} __attribute__((packed)) MDPTPMsgPdelayRespFollowUp;

// 10.6.3 Announce message, reformat to MD
typedef struct MDPTPMsgAnnounce {
	MDPTPMsgHeader head;
	uint8_t reserved0[10];
	int16_t currentUtcOffset_ns;
	uint8_t reserved1[1];
	uint8_t grandmasterPriority1;
	MDClockQuality grandmasterClockQuality;
	uint8_t grandmasterPriority2;
	ClockIdentity grandmasterIdentity;
	uint16_t stepsRemoved_ns;
	uint8_t timeSource;
	uint16_t tlvType_ns;
	uint16_t tlvLength_ns;
	ClockIdentity pathSequence[MAX_PATH_TRACE_N];
} __attribute__((packed)) MDPTPMsgAnnounce;

// 10.6.4.3 Message interval request TLV definition, reformat to MD
typedef struct MDPTPMsgIntervalRequestTLV {
	MDPTPMsgHeader head;
	MDPortIdentity targetPortIdentity;
	uint16_t tlvType_ns;
	uint16_t lengthField_ns;
	uint8_t organizationId[3];
	uint8_t organizationSubType_nb[3];
	int8_t linkDelayInterval;
	int8_t timeSyncInterval;
	int8_t announceInterval;
	uint8_t flags;
	uint8_t reserved[2];
} __attribute__((packed)) MDPTPMsgIntervalRequestTLV;

// 10.6.4.4 gPTP capable TLV, reformat to MD
typedef struct MDPTPMsgGPTPCapableTLV {
	MDPTPMsgHeader head;
	MDPortIdentity targetPortIdentity;
	uint16_t tlvType_ns;
	uint16_t lengthField_ns;
	uint8_t organizationId[3];
	uint8_t organizationSubType_nb[3];
	uint8_t logGptpCapableMessageInterval;
	uint8_t flags;
	uint8_t reserved[4];
} __attribute__((packed)) MDPTPMsgGPTPCapableTLV;


/************************************************
  data types from the 802.1AS-Rev
*************************************************/
// 11.2 State machines for MD entity specific to full-duplex, point-to-point links
// 11.2.9
typedef struct MDTimestampReceive {
	UScaledNs timestamp;
} MDTimestampReceive;

// 11.2.12 MD entity global variables
// the instance is per port but only one for domains
typedef struct MDEntityGlobalForAllDomain{
	int8_t currentLogPdelayReqInterval;
	int8_t initialLogPdelayReqInterval;
	UScaledNs pdelayReqInterval;
	uint16_t allowedLostResponses;
	uint16_t allowedFaults;
	bool isMeasuringDelay;
	UScaledNs neighborPropDelayThresh;
	bool asCapableAcrossDomains;
}MDEntityGlobalForAllDomain;
typedef struct MDEntityGlobal{
	uint16_t syncSequenceId;
	bool oneStepReceive;
	bool oneStepTransmit;
	bool oneStepTxOper;
	MDEntityGlobalForAllDomain *forAllDomain;
} MDEntityGlobal;

// 11.2.13 MDSyncReceiveSM state machine
typedef struct MDSyncReceiveSM {
	UScaledNs followUpReceiptTimeoutTime;
	bool rcvdSync;
	bool rcvdFollowUp;
	MDPTPMsgSync *rcvdSyncPtr;
	MDPTPMsgFollowUp *rcvdFollowUpPtr;
	MDSyncReceive *txMDSyncReceivePtr;
	UScaledNs upstreamSyncInterval;
} MDSyncReceiveSM;

// 11.2.14 MDSyncSendSM state machine
typedef struct MDSyncSendSM {
	bool rcvdMDSync;
	MDPTPMsgSync *txSyncPtr;
	bool rcvdMDTimestampReceive;
	MDTimestampReceive *rcvdMDTimestampReceivePtr;
	MDPTPMsgFollowUp *txFollowUpPtr;
} MDSyncSendSM;

// 11.2.15 Common Mean Link Delay Service
// 11.2.16 Common Mean Link Delay Service global variables
typedef struct CMLinkDelayServiceGlobal {
	bool cmldsLinkPortEnabled;
} CMLinkDelayServiceGlobal;

// 11.2.17 MDPdelayReq state machine
typedef struct MDPdelayReqSM {
	UScaledNs pdelayIntervalTimer;
	bool rcvdPdelayResp;
	MDPTPMsgPdelayResp *rcvdPdelayRespPtr;
	bool rcvdPdelayRespFollowUp;
	MDPTPMsgPdelayRespFollowUp *rcvdPdelayRespFollowUpPtr;
	MDPTPMsgPdelayReq *txPdelayReqPtr;
	bool rcvdMDTimestampReceive;
	uint16_t pdelayReqSequenceId;
	bool initPdelayRespReceived;
	uint16_t lostResponses;
	bool neighborRateRatioValid;
	uint16_t detectedFaults;
	bool portEnabled0;
} MDPdelayReqSM;

// 11.2.18 MDPdelayResp state machine
typedef struct MDPdelayRespSM {
	bool rcvdPdelayReq;
	bool rcvdMDTimestampReceive;
	MDPTPMsgPdelayResp *txPdelayRespPtr;
	MDPTPMsgPdelayRespFollowUp *txPdelayRespFollowUpPtr;
	bool portEnabled1;
	MDPTPMsgPdelayReq *rcvdPdelayReqPtr; // not in the standard
} MDPdelayRespSM;

// 11.2.19 SyncIntervalSetting state machine
typedef struct SyncIntervalSettingSM {
	bool rcvdSignalingMsg1;
	PTPMsgIntervalRequestTLV *rcvdSignalingPtr;
} SyncIntervalSettingSM;

// 11.2.20 LinkDelayIntervalSetting state machine
typedef struct LinkDelayIntervalSettingSM {
	bool rcvdSignalingMsg1;
	PTPMsgIntervalRequestTLV *rcvdSignalingPtr;
	bool portEnabled3;
} LinkDelayIntervalSettingSM;

// 11.2.21 OneStepTxOperSetting state machine
typedef struct OneStepTxOperSettingSM {
	bool rcvdSignalingMsg4;
	PTPMsgIntervalRequestTLV *rcvdSignalingPtr;
} OneStepTxOperSettingSM;

// for announce state machines
typedef struct MDAnnounceSendSM {
	bool rcvdTxAnnounce;
	PTPMsgAnnounce *txAnnouncePtr;
} MDAnnounceSendSM;

typedef struct MDAnnounceReceiveSM {
	bool rcvdRxAnnounce;
	void *rxAnnouncePtr;
} MDAnnounceReceiveSM;

// 11.4 Message formats
typedef enum {
	SYNC = 0,
	DELAY_REQ,
	PDELAY_REQ = 2,
	PDELAY_RESP = 3,
	RESERVED_04,
	RESERVED_05,
	RESERVED_06,
	RESERVED_07,
	FOLLOW_UP = 8,
	DELAY_RESP,
	PDELAY_RESP_FOLLOW_UP = 10,
	ANNOUNCE,
	SIGNALING,
	MANAGEMENT,
	RESERVED_14,
	RESERVED_15,
} PTPMsgType;

/************************************************
  macros
*************************************************/
#define GET_TWO_STEP_FLAG(x) ((x).flags[0] & 0x2)
#define SET_TWO_STEP_FLAG(x) ((x).flags[0] |= 0x2)
#define RESET_TWO_STEP_FLAG(x) ((x).flags[0] &= ~0x2)

/************************************************
  functions
*************************************************/
void md_compose_head(PTPMsgHeader *head, MDPTPMsgHeader *phead);
void *md_header_compose(gptpnet_data_t *gptpnet, int portIndex, PTPMsgType msgtype,
			uint16_t ssize, ClockIdentity thisClock, uint16_t thisPort,
			uint16_t seqid, int8_t logMessageInterval);
void md_decompose_head(MDPTPMsgHeader *phead, PTPMsgHeader *head);
void md_header_template(PTPMsgHeader *head, PTPMsgType msgtype, uint16_t len,
			PortIdentity *portId, uint16_t seqid, int8_t logMessageInterval);

void md_entity_glb_init(MDEntityGlobal **mdeglb, MDEntityGlobalForAllDomain *forAllDomain);
void md_entity_glb_close(MDEntityGlobal **mdeglb, int domainIndex);

void md_followup_information_tlv_compose(MDFollowUpInformationTLV *tlv,
					 double rateRatio, uint16_t gmTimeBaseIndicator,
					 ScaledNs lastGmPhaseChange, double lastGmFreqChange);

#endif
