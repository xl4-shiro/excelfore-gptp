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
#include <errno.h>
#include "xl4combase/cb_ethernet.h"
#include "gptpnet.h"
#include "gptpclock.h"
#include "ll_gptpsupport.h"
#include "ix_timestamp.h"

static int read_txts(int dvi, struct msghdr *msg, int len, uint16_t ovip_port,
		     event_data_txts_t *edtxts)
{
	uint8_t *buf=msg->msg_iov[0].iov_base;

	if(len < 48) {
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg returned only %d bytes\n",
		       __func__, dvi, len);
		return -1;
	}
	if(ovip_port) buf+=ETH_HLEN+20+8;
	if(ntohs(*(uint16_t *)(buf + 12))!=ETH_P_1588){
		UB_LOG(UBL_DEBUG,
		       "%s:deviceIndex=%d, not ETH_P_1588 packet 0x%02X%02X\n",
		       __func__, dvi, buf[12], buf[13]);
		return -1;
	}

	edtxts->msgtype=PTP_HEAD_MSGTYPE(buf+ETH_HLEN);
	edtxts->seqid=PTP_HEAD_SEQID(buf+ETH_HLEN);
	edtxts->domain=PTP_HEAD_DOMAIN_NUMBER(buf+ETH_HLEN);
	if(edtxts->msgtype >= 8){
		UB_LOG(UBL_DEBUG,"deviceIndex=%d, msgtype:%d is not Event, ignore this\n",
		       dvi, edtxts->msgtype);
		return -1;
	}

	if(ll_txmsg_timestamp(msg, &edtxts->ts64)) return -1;
	if(ovip_port && edtxts->msgtype==0)
		edtxts->ts64+=gptpclock_d0ClockfromRT(dvi+1);
	return 0;
}

// return 0:got TxTS, -1:got data but no TxTS, -2:error to read, 1:no data
int ix_timestamp_txts(int fd, struct msghdr *msg, int dvi, uint16_t ovip_port,
		      event_data_txts_t *edtxts)
{
	int res;
	res = recvmsg(fd, msg, MSG_ERRQUEUE | MSG_DONTWAIT);
	if(res > 0){
		return read_txts(dvi, msg, res, ovip_port, edtxts); // return 0 or -1
	}else if(res == 0){
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg for EQ returned 0\n",
		       __func__, dvi);
		return -2;
	}else if(errno!=EAGAIN ) {
		UB_LOG(UBL_ERROR,"%s:deviceIndex=%d, recvmsg for EQ failed: %s\n",
		       __func__, dvi, strerror(errno));
		return -2;
	}
	return 1; // EAGAIN case
}
