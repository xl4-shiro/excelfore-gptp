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
#include "mdeth.h"
#include "gptp_config.h"
#include "gptpnet.h"
#include "gptpclock.h"
#include <math.h>

void md_compose_head(PTPMsgHeader *head, MDPTPMsgHeader *phead)
{
	phead->majorSdoId_messageType = (head->majorSdoId << 4) | head->messageType;
	phead->minorVersionPTP_versionPTP = (head->minorVersionPTP << 4) |
		head->versionPTP;
	phead->messageLength_ns = htons(head->messageLength);
	phead->domainNumber = head->domainNumber;
	phead->minorSdoId = head->minorSdoId;
	memcpy(phead->flags, head->flags, 2);
	phead->correctionField_nll = UB_HTONLL((uint64_t)head->correctionField);
	memcpy(phead->messageTypeSpecific, head->messageTypeSpecific, 4);
	memcpy(phead->sourcePortIdentity.clockIdentity, head->sourcePortIdentity.clockIdentity,
	       sizeof(ClockIdentity));
	phead->sourcePortIdentity.portNumber_ns = htons(head->sourcePortIdentity.portNumber);
	phead->sequenceId_ns = htons(head->sequenceId);
	phead->control = head->control;
	phead->logMessageInterval = head->logMessageInterval;
}

void md_decompose_head(MDPTPMsgHeader *phead, PTPMsgHeader *head)
{
	head->majorSdoId = (phead->majorSdoId_messageType >> 4) & 0xf;
	head->messageType = phead->majorSdoId_messageType & 0xf;
	head->minorVersionPTP = (phead->minorVersionPTP_versionPTP >> 4) & 0xf;
	head->versionPTP = phead->minorVersionPTP_versionPTP & 0xf;
	head->messageLength = ntohs(phead->messageLength_ns);
	head->domainNumber = phead->domainNumber;
	head->minorSdoId = phead->minorSdoId;
	memcpy(head->flags, phead->flags, 2);
	head->correctionField = UB_NTOHLL((uint64_t)phead->correctionField_nll);
	memcpy(head->messageTypeSpecific, phead->messageTypeSpecific, 4);
	memcpy(head->sourcePortIdentity.clockIdentity, phead->sourcePortIdentity.clockIdentity,
	       sizeof(ClockIdentity));
	head->sourcePortIdentity.portNumber = ntohs(phead->sourcePortIdentity.portNumber_ns);
	head->sequenceId = ntohs(phead->sequenceId_ns);
	head->control = phead->control;
	head->logMessageInterval = phead->logMessageInterval;
}

void md_header_template(PTPMsgHeader *head, PTPMsgType msgtype, uint16_t len,
			PortIdentity *portId, uint16_t seqid, int8_t logMessageInterval)
{
	head->majorSdoId=1; // for CMLDS, this must be changed to '2'
	head->messageType=msgtype;
	head->minorVersionPTP=gptpconf_get_intitem(CONF_MINOR_VERSION_PTP);
	head->versionPTP=2;
	head->messageLength=len;
	head->domainNumber=0;
	head->minorSdoId=0;
	switch(msgtype){
		case SYNC: /* fall-through */
		case PDELAY_RESP:
			/* twoStepFlag bit is TRUE for Sync,Pdelay_Resp */
			head->flags[0]=0x2;
			break;
		default:
			head->flags[0]=0x0;
			break;
	}
	head->flags[1]=0x0|((gptpconf_get_intitem(CONF_TIMESCALE_PTP)&0x1)<<3);
	head->correctionField=0;
	memset(head->messageTypeSpecific,0,4);
	memcpy(&head->sourcePortIdentity, portId, sizeof(PortIdentity));
	head->sequenceId=seqid;
	switch(msgtype){
	case SYNC:
		head->control=0x0;
		break;
	case FOLLOW_UP:
		head->control=0x2;
		break;
	default:
		head->control=0x5;
		break;
	}
	head->logMessageInterval=logMessageInterval;
}

void *md_header_compose(gptpnet_data_t *gpnetd, int portIndex, PTPMsgType msgtype,
			uint16_t ssize, ClockIdentity thisClock, uint16_t thisPort,
			uint16_t seqid, int8_t logMessageInterval)
{
	PortIdentity portId;
	PTPMsgHeader head;
	uint8_t *sdata;

	if(!gpnetd) {
		UB_LOG(UBL_INFO, "%s:no gpnetd\n",__func__);
		return NULL;
	}
	sdata=gptpnet_get_sendbuf(gpnetd, portIndex-1);
	memset(sdata, 0, ssize);

	memcpy(portId.clockIdentity, thisClock, sizeof(ClockIdentity));
	portId.portNumber = thisPort;
	md_header_template(&head, msgtype, ssize, &portId, seqid, logMessageInterval);
	md_compose_head(&head, (MDPTPMsgHeader *)sdata);
	return sdata;
}

void md_followup_information_tlv_compose(MDFollowUpInformationTLV *tlv,
					 double rateRatio, uint16_t gmTimeBaseIndicator,
					 ScaledNs lastGmPhaseChange, double lastGmFreqChange)
{
	// 11.4.4.3 Follow_Up information TLV
	tlv->tlvType_ns = htons(0x3);
	tlv->lengthField_ns = htons(28);
	tlv->organizationId[0] = 0x00;
	tlv->organizationId[1] = 0x80;
	tlv->organizationId[2] = 0xC2;
	tlv->organizationSubType_nb[0] = 0;
	tlv->organizationSubType_nb[1] = 0;
	tlv->organizationSubType_nb[2] = 1;
	tlv->cumulativeScaledRateOffset_nl = htonl((int32_t)ldexp((rateRatio - 1.0), 41));
	tlv->gmTimeBaseIndicator_ns = htons(gmTimeBaseIndicator);
	tlv->lastGmPhaseChange.nsec_msb = htons(lastGmPhaseChange.nsec_msb);
	tlv->lastGmPhaseChange.nsec_nll = UB_HTONLL((uint64_t)lastGmPhaseChange.nsec);
	tlv->lastGmPhaseChange.subns_ns = htons(lastGmPhaseChange.subns);
	tlv->scaledLastGmFreqChange_nl = htonl((int32_t)ldexp(lastGmFreqChange, 41));
}

void md_entity_glb_init(MDEntityGlobal **mdeglb, MDEntityGlobalForAllDomain *forAllDomain)
{
	if(!*mdeglb){
		*mdeglb=malloc(sizeof(MDEntityGlobal));
		ub_assert(*mdeglb, __func__, "malloc error");
	}
	memset(*mdeglb, 0, sizeof(MDEntityGlobal));
	if(forAllDomain){
		(*mdeglb)->forAllDomain = forAllDomain;
	}else{
		(*mdeglb)->forAllDomain = malloc(sizeof(MDEntityGlobalForAllDomain));
		ub_assert((*mdeglb)->forAllDomain, __func__, "malloc error");
		memset((*mdeglb)->forAllDomain, 0, sizeof(MDEntityGlobalForAllDomain));
		(*mdeglb)->forAllDomain->currentLogPdelayReqInterval =
			gptpconf_get_intitem(CONF_LOG_PDELAYREQ_INTERVAL);
		(*mdeglb)->forAllDomain->initialLogPdelayReqInterval =
			gptpconf_get_intitem(CONF_LOG_PDELAYREQ_INTERVAL);
		(*mdeglb)->forAllDomain->pdelayReqInterval.nsec =
			LOG_TO_NSEC(gptpconf_get_intitem(CONF_LOG_PDELAYREQ_INTERVAL));
		(*mdeglb)->forAllDomain->allowedLostResponses =
			gptpconf_get_intitem(CONF_ALLOWED_LOST_RESPONSE);
		(*mdeglb)->forAllDomain->allowedFaults =
			gptpconf_get_intitem(CONF_ALLOWED_FAULTS);
		(*mdeglb)->forAllDomain->neighborPropDelayThresh.nsec =
			gptpconf_get_intitem(CONF_NEIGHBOR_PROPDELAY_THRESH);
		(*mdeglb)->forAllDomain->neighborPropDelayMinLimit.nsec =
			gptpconf_get_intitem(CONF_NEIGHBOR_PROPDELAY_MINLIMIT);
	}
	(*mdeglb)->syncSequenceId = (uint16_t)(rand() & 0xffff);
	(*mdeglb)->oneStepReceive = false;
	(*mdeglb)->oneStepTransmit = false;
	(*mdeglb)->oneStepTxOper = false;
}

void md_entity_glb_close(MDEntityGlobal **mdeglb, int domainIndex)
{
	if(!*mdeglb) return;
	if(domainIndex==0) free((*mdeglb)->forAllDomain);
	free(*mdeglb);
	*mdeglb=NULL;
}
