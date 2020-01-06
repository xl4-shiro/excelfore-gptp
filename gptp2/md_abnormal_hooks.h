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
#ifndef __MD_ABNORMAL_HOOKS_H_
#define __MD_ABNORMAL_HOOKS_H_

#include "gptpnet.h"

typedef enum {
	MD_ABN_EVENT_NONE, // no event happens
	MD_ABN_EVENT_SKIP, // skip a message, sequenceID has no skip
	MD_ABN_EVENT_DUP, // send the same message twice
	MD_ABN_EVENT_BADSEQN, // add eventpara on sequenceID.
			      // if eventpara==0 invert lower 8 bits of SequenceID
	MD_ABN_EVENT_NOTS, // make No Timestamp error
	MD_ABN_EVENT_SENDER, // make send error
} md_abn_event_type;

typedef struct md_abn_event {
	int domainNumber;
	int ndevIndex;
	PTPMsgType msgtype;
	md_abn_event_type eventtype;
	float eventrate; // 0.0 to 1.0: possibility of happening of the event
	int repeat; // 0:repeat forever, 1:one time, 2:two times, ...
	int interval; // 0:every time, 1: ever other time, 2: ever two times, ...
	int eventpara; // integer parameter for the event
} md_abn_event_t;

typedef enum {
	MD_ABN_EVENTP_NONE,
	MD_ABN_EVENTP_SKIP,
	MD_ABN_EVENTP_DUPLICATE,
	MD_ABN_EVENTP_MANUPULATE,
	MD_ABN_EVENTP_SENDER,
} md_abn_eventp_t;

/**
 * @brief initialize and activate 'md_abnormal_gptpnet_send_hook' operation.
 */
void md_abnormal_init(void);

/**
 * @brief close and deactivate 'md_abnormal_gptpnet_send_hook' operation.
 */
void md_abnormal_close(void);

/**
 * @brief register an abnormal event
 */
int md_abnormal_register_event(md_abn_event_t *event);

/**
 * @brief deregister all of abnormal events
 */
int md_abnormal_deregister_all_events(void);

/**
 * @brief deregister all of abnormal events with 'msgtype'
 */
int md_abnormal_deregister_msgtype_events(PTPMsgType msgtype);

/**
 * @brief inject abnomal send events by calling this function just before 'gptpnet_send'
 * @param ndevIndex	index of a network device
 * @param domainNumber	domain Number
 * @result eventp type
 */
md_abn_eventp_t md_abnormal_gptpnet_send_hook(gptpnet_data_t *gpnet, int ndevIndex,
					      uint16_t length);

static inline int gptpnet_send_whook(gptpnet_data_t *gpnet, int ndevIndex, uint16_t length)
{
	switch(md_abnormal_gptpnet_send_hook(gpnet, ndevIndex, length)){
	case MD_ABN_EVENTP_NONE:
		break;
	case MD_ABN_EVENTP_SKIP:
		return -(length+sizeof(CB_ETHHDR_T));
	case MD_ABN_EVENTP_DUPLICATE:
		gptpnet_send(gpnet, ndevIndex, length);
		return gptpnet_send(gpnet, ndevIndex, length);
	case MD_ABN_EVENTP_MANUPULATE:
		break;
	case MD_ABN_EVENTP_SENDER:
		return -1;
	}
	return gptpnet_send(gpnet, ndevIndex, length);
}

/**
 * @brief check if the timestamp should be abandoned by an abnormal event
 * @param ndevIndex	index of a network device
 * @param domainNumber	domain Number
 * @result 0:no abnormal event, 1:hit with a registered abnormal event
 */
int md_abnormal_timestamp(PTPMsgType msgtype, int ndevIndex, int domainNumber);

#endif
