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
#include <stdlib.h>
#include "gptpnet.h"
#include "mdeth.h"
#include "md_abnormal_hooks.h"

extern char *PTPMsgType_debug[];

typedef struct event_data {
	md_abn_event_t evd;
	int repeat_count;
	int interval_count;
} event_data_t;

typedef struct md_abnormal_data {
	ub_esarray_cstd_t *events;
} md_abnormal_data_t;

#define MD_EVENT_ARRAY_EXPUNIT 2
#define MD_EVENT_ARRAY_MAXUNIT 16

static md_abnormal_data_t *gmdabnd;

static int event_by_rate(event_data_t *event)
{
	float v;

	if(event->evd.eventrate<1.0){
		v=(float)random()/(float)RAND_MAX;
		if(v>event->evd.eventrate) return 0;
	}
	return 1;
}

static int event_happen(event_data_t *event)
{
	if(!event->evd.repeat) return event_by_rate(event); // if repeat=0, happen every time

	if(event->repeat_count == 0){
		event->repeat_count++;
		return event_by_rate(event);
	}

	if(!event->evd.interval){
		if(event->repeat_count++ < event->evd.repeat) return 1;
		return 0;
	}

	if(event->interval_count++ < event->evd.interval) return 0;
	event->interval_count=0;
	if(event->repeat_count++  < event->evd.repeat) return 1;
	return 0;
}

static md_abn_eventp_t proc_event(event_data_t *event, uint8_t *dbuf, bool domain_aware)
{
	if(domain_aware && (event->evd.domainNumber != PTP_HEAD_DOMAIN_NUMBER(dbuf)))
		return MD_ABN_EVENTP_NONE;
	switch(event->evd.eventtype){
	case MD_ABN_EVENT_SKIP:
		if(!event_happen(event)) break;
		return MD_ABN_EVENTP_SKIP;
	case MD_ABN_EVENT_DUP:
		if(!event_happen(event)) break;
		return MD_ABN_EVENTP_DUPLICATE;
	case MD_ABN_EVENT_BADSEQN:
		if(!event_happen(event)) break;
		if(event->evd.eventpara==0){
			*(dbuf+31)^=0xff; //invert lower 8 bits of SequenceID
		}else{
			uint16_t n;
			n=ntohs(*(uint16_t*)(dbuf+30))+event->evd.eventpara;
			*(uint16_t*)(dbuf+30)=htons(n);
		}
		return MD_ABN_EVENTP_MANUPULATE;
	case MD_ABN_EVENT_SENDER:
		if(!event_happen(event)) break;
		return MD_ABN_EVENTP_SENDER;
	default:
		break;
	}
	return MD_ABN_EVENTP_NONE;
}

void md_abnormal_init(void)
{
	if(gmdabnd) md_abnormal_close();

	gmdabnd=malloc(sizeof(md_abnormal_data_t));
	ub_assert(gmdabnd, __func__, "malloc error");
	memset(gmdabnd, 0, sizeof(md_abnormal_data_t));
	gmdabnd->events=ub_esarray_init(MD_EVENT_ARRAY_EXPUNIT, sizeof(event_data_t),
					MD_EVENT_ARRAY_MAXUNIT);
	return;
}

void md_abnormal_close(void)
{
	if(!gmdabnd) return;
	if(gmdabnd->events) ub_esarray_close(gmdabnd->events);
	free(gmdabnd);
	gmdabnd=NULL;
	return;
}

int md_abnormal_register_event(md_abn_event_t *event)
{
	event_data_t *nevent;
	if(!gmdabnd) return -1;
	if(event->msgtype>15) return -1;
	nevent=(event_data_t *)ub_esarray_get_newele(gmdabnd->events);
	if(!nevent) return -1;
	memset(nevent, 0, sizeof(event_data_t));
	memcpy(&nevent->evd, event, sizeof(md_abn_event_t));
	UB_LOG(UBL_INFO, "%s:dn=%d, ni=%d, msgtype=%s, eventtype=%d,"
	       "eventrate=%f, repeat=%d interval=%d eventpara=%d\n",__func__,
	       event->domainNumber,
	       event->ndevIndex,
	       PTPMsgType_debug[event->msgtype],
	       event->eventtype,
	       event->eventrate,
	       event->repeat,
	       event->interval,
	       event->eventpara);
	return 0;
}

int md_abnormal_deregister_all_events(void)
{
	int i;
	int elen;
	if(!gmdabnd) return -1;
	UB_LOG(UBL_DEBUG, "%s:\n",__func__);
	elen=ub_esarray_ele_nums(gmdabnd->events);
	for(i=elen-1;i>=0;i--) ub_esarray_del_index(gmdabnd->events, i);
	return 0;
}

int md_abnormal_deregister_msgtype_events(PTPMsgType msgtype)
{
	int i;
	event_data_t *event;
	int elen;
	if(!gmdabnd) return -1;
	if(msgtype>15) return -1;
	UB_LOG(UBL_DEBUG, "%s:msgtype=%s\n",__func__, PTPMsgType_debug[msgtype]);
	elen=ub_esarray_ele_nums(gmdabnd->events);
	for(i=elen-1;i>=0;i--) {
		event=(event_data_t *)ub_esarray_get_ele(gmdabnd->events, i);
		if(event->evd.msgtype==msgtype){
			ub_esarray_del_index(gmdabnd->events, i);
		}
	}
	return 0;
}

md_abn_eventp_t md_abnormal_gptpnet_send_hook(gptpnet_data_t *gpnet, int ndevIndex,
					      uint16_t length)
{
	int i;
	md_abn_eventp_t res=MD_ABN_EVENTP_NONE;
	event_data_t *event;
	int elen;
	uint8_t *dbuf;
	PTPMsgType msgtype;
	if(!gmdabnd) return res;
	dbuf=gptpnet_get_sendbuf(gpnet, ndevIndex);
	msgtype=(PTPMsgType)PTP_HEAD_MSGTYPE(dbuf);
	elen=ub_esarray_ele_nums(gmdabnd->events);
	for(i=0;i<elen;i++) {
		event=(event_data_t *)ub_esarray_get_ele(gmdabnd->events, i);
		if(event->evd.msgtype!=msgtype) continue;
		if(event->evd.ndevIndex!=ndevIndex) continue;
		switch(msgtype){
		case SYNC:
			res=proc_event(event, dbuf, true);
			break;
		case PDELAY_REQ:
			res=proc_event(event, dbuf, false);
			break;
		case PDELAY_RESP:
			res=proc_event(event, dbuf, false);
			break;
		case FOLLOW_UP:
			res=proc_event(event, dbuf, true);
			break;
		case PDELAY_RESP_FOLLOW_UP:
			res=proc_event(event, dbuf, false);
			break;
		case ANNOUNCE:
			res=proc_event(event, dbuf, true);
			break;
		case SIGNALING:
			res=proc_event(event, dbuf, true);
			break;
		case DELAY_REQ:
		case DELAY_RESP:
		case MANAGEMENT:
		default:
			break;
		}
	}
	return res;
}

int md_abnormal_timestamp(PTPMsgType msgtype, int ndevIndex, int domainNumber)
{
	int i, elen;
	event_data_t *event;

	if(!gmdabnd) return 0;
	elen=ub_esarray_ele_nums(gmdabnd->events);
	for(i=0;i<elen;i++) {
		event=(event_data_t *)ub_esarray_get_ele(gmdabnd->events, i);
		if(event->evd.msgtype!=msgtype) continue;
		if(event->evd.ndevIndex!=ndevIndex) continue;
		if(event->evd.eventtype!=MD_ABN_EVENT_NOTS) continue;
		if(domainNumber>=0 && event->evd.domainNumber!=domainNumber) continue;
		if(event_happen(event)){
			UB_LOG(UBL_DEBUG, "%s:%s timestamp must be abandoned\n",
			       __func__, PTPMsgType_debug[msgtype]);
			return 1;
		}
		return 0;
	}
	return 0;
}
