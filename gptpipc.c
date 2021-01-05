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
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include "gptpbasetypes.h"
#include "gptp_defaults.h"
#include "ll_gptpsupport.h"
#include "gptpipc.h"

static char *selectedStateStr(uint8_t selectedState)
{
	switch(selectedState){
	case DisabledPort: return "Disabled";
	case MasterPort: return "Master";
	case PassivePort: return "Passive";
	case SlavePort: return "Slave";
	default: return "Unknonw";
	}
}

static void print_ipc_data(gptpipc_gptpd_data_t *rd)
{
	char *duplex="unknown";
	char astr[64];
	int i;
	double lastGmFreqChange;

	switch(rd->dtype){
	case GPTPIPC_GPTPD_NOTICE:
		printf("GPTPD_NOTICE domainNumber=%d portIndex=%d ",
		       rd->u.notice.domainNumber, rd->u.notice.portIndex);
		if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_NETDEV_DOWN)
			printf("DWON ");
		else if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_NETDEV_UP)
			printf("UP ");
		if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_PHASE_UPDATE){
			printf("PHASE_UPDATE ");
			printf("PhaseChange=%"PRIi64"sec %"PRIi64"nsec ",
			       rd->u.notice.lastGmPhaseChange_nsec/1000000000,
			       rd->u.notice.lastGmPhaseChange_nsec%1000000000);
		}
		if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_FREQ_UPDATE){
			memcpy(&lastGmFreqChange, &rd->u.notice.lastGmFreqChangePk, sizeof(double));
			printf("FREQ_UPDATE ");
			printf("FreqChange=%.9f ", lastGmFreqChange);
		}
		if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_GM_SYNCED)
			printf("GM_SYNC ");
		else if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_GM_UNSYNCED)
			printf("GM_UNSYNC ");
		if(rd->u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_GM_CHANGE){
			printf("GM_CHANGE new GM="UB_PRIhexB8,
			       UB_ARRAY_B8(
				       rd->u.notice.gmPriority.rootSystemIdentity.clockIdentity));
		}
		printf("\n");
		break;
	case GPTPIPC_GPTPD_NDPORTD:
		printf("GPTPD_PORTD ");
		if(rd->u.ndportd.nlstatus.up)
			printf("UP ");
		else
			printf("DOWN ");
		if(rd->u.ndportd.nlstatus.duplex==1)
			duplex="full";
		else if(rd->u.ndportd.nlstatus.duplex==2)
			duplex="half";

		printf("%s speed=%d duplex=%s portid=%s\n", rd->u.ndportd.nlstatus.devname,
		       rd->u.ndportd.nlstatus.speed, duplex,
		       ub_bsid2ssid(rd->u.ndportd.nlstatus.portid, astr));
		break;
	case GPTPIPC_GPTPD_CLOCKD:
		printf("GPTPD_CLOCKD ");
		printf("domainNumber=%d portIndex=%d ",
		       rd->u.clockd.domainNumber, rd->u.clockd.portIndex);
		if(rd->u.clockd.gmsync)
			printf("GM_SYNC\n");
		else
			printf("GM_UNSYNC\n");
		memcpy(&lastGmFreqChange, &rd->u.clockd.lastGmFreqChangePk, sizeof(double));
		printf("  gmTimeBaseIndicator=%u lastGmPhaseChange=%"PRIi64
		       ", lastGmFreqChange=%.9f\n",
		       rd->u.clockd.gmTimeBaseIndicator, rd->u.clockd.lastGmPhaseChange_nsec,
		       lastGmFreqChange);
		break;
	case GPTPIPC_GPTPD_GPORTD:
		printf("GPTPD_GPORTD ");
		printf("domainNumber=%d portIndex=%d asCapable=%s gmStable=%s state=%s\n",
		       rd->u.gportd.domainNumber, rd->u.gportd.portIndex,
		       rd->u.gportd.asCapable?"True":"False",
		       rd->u.gportd.gmStable?"True":"False",
		       selectedStateStr(rd->u.gportd.selectedState));
		if(!rd->u.gportd.asCapable){
			printf("\n");
			break;
		}
		printf("  GM="UB_PRIhexB8"\n", UB_ARRAY_B8(rd->u.gportd.gmClockId));
		for(i=0;i<rd->u.gportd.annPathSequenceCount;i++){
			if(i>=MAX_PATH_TRACE_N) break;
			printf("  path trace %d: "UB_PRIhexB8"\n",
			       i+1, UB_ARRAY_B8(rd->u.gportd.annPathSequence[i]));
		}
		break;
	case GPTPIPC_GPTPD_STATSD:
		printf("GPTPD_STATSD --- portIndex=%d\n", rd->u.statsd.portIndex);
		printf("pdelay_req_send=%d\n", rd->u.statsd.pdelay_req_send);
		printf("pdelay_resp_rec=%d\n", rd->u.statsd.pdelay_resp_rec);
		printf("pdelay_resp_rec_valid=%d\n", rd->u.statsd.pdelay_resp_rec_valid);
		printf("pdelay_resp_fup_rec=%d\n", rd->u.statsd.pdelay_resp_fup_rec);
		printf("pdelay_resp_fup_rec_valid=%d\n", rd->u.statsd.pdelay_resp_fup_rec_valid);
		printf("pdelay_req_rec=%d\n", rd->u.statsd.pdelay_req_rec);
		printf("pdelay_req_rec_valid=%d\n", rd->u.statsd.pdelay_req_rec_valid);
		printf("pdelay_resp_send=%d\n", rd->u.statsd.pdelay_resp_send);
		printf("pdelay_resp_fup_send=%d\n", rd->u.statsd.pdelay_resp_fup_send);
		break;
	case GPTPIPC_GPTPD_STATTD:
		printf("GPTPD_STATTD --- domainNumber=%d portIndex=%d\n",
		       rd->u.stattd.domainNumber, rd->u.stattd.portIndex);
		printf("sync_send=%d\n", rd->u.stattd.sync_send);
		printf("sync_fup_send=%d\n", rd->u.stattd.sync_fup_send);
		printf("sync_rec=%d\n", rd->u.stattd.sync_rec);
		printf("sync_rec_valid=%d\n", rd->u.stattd.sync_rec_valid);
		printf("sync_fup_rec=%d\n", rd->u.stattd.sync_fup_rec);
		printf("sync_fup_rec_valid=%d\n", rd->u.stattd.sync_fup_rec_valid);
		printf("signal_msg_interval_send=%d\n", rd->u.stattd.signal_msg_interval_send);
		printf("signal_gptp_capable_send=%d\n", rd->u.stattd.signal_gptp_capable_send);
		printf("signal_rec=%d\n", rd->u.stattd.signal_rec);
		printf("signal_msg_interval_rec=%d\n", rd->u.stattd.signal_msg_interval_rec);
		printf("signal_gptp_capable_rec=%d\n", rd->u.stattd.signal_gptp_capable_rec);
		break;
	default:
		printf("unknonw data\n");
		break;
	}
	fflush(stdout);
}

#define IPC_TOUT_MS 100
int send_ipc_request(int ipcfd, int domainNumber, int portIndex, gptp_ipc_command_t cmd)
{
	gptpipc_client_req_data_t cd;
	int res;
	memset(&cd, 0, sizeof(cd));
	cd.domainNumber=domainNumber;
	cd.domainIndex=-1;
	cd.portIndex=portIndex;
	cd.cmd=cmd;
	res=write(ipcfd, &cd, sizeof(cd));
	if(res!=sizeof(cd)){
		UB_LOG(UBL_ERROR, "%s:can't send to the IPC socket\n",__func__);
		return -1;
	}
	return 0;
}

static int gptpipc_socket_open(gptpipc_thread_data_t *ipcpd)
{
	if(!ipcpd->udpport){
		return cb_ipcsocket_init(&ipcpd->ipcfd, GPTP2D_IPC_CB_SOCKET_NODE,
					 ipcpd->pname, GPTP2D_IPC_CB_SOCKET_NODE);
	}else{
		if(!ipcpd->udpaddr)
			return cb_ipcsocket_udp_init(&ipcpd->ipcfd, "127.0.0.1",
						     "127.0.0.1", ipcpd->udpport);
		return cb_ipcsocket_udp_init(&ipcpd->ipcfd, NULL,
					     ipcpd->udpaddr, ipcpd->udpport);
	}
}

static void *gptpipc_thread_proc(void *ptr)
{
	int res;
	gptpipc_thread_data_t *ipcpd=(gptpipc_thread_data_t *)ptr;
	gptpipc_gptpd_data_t rd;
	bool get_response = false;
	int timeout_count = 0;
	int suppress_msg = 0;

	while(!ipcpd->ipcstop){
		if(!ipcpd->ipcfd){
			if(suppress_msg) ub_log_change(CB_COMBASE_LOGCAT, UBL_NONE, UBL_NONE);
			if(gptpipc_socket_open(ipcpd)) {
				ipcpd->ipcfd=0;
				UB_LOG(UBL_DEBUG, "cann't connect to gptp2d, "
				       "wait 100msec and try again\n");
				usleep(100000);
				if(suppress_msg) ub_log_return(CB_COMBASE_LOGCAT);
				suppress_msg=1;
				continue;
			}
			if(suppress_msg) ub_log_return(CB_COMBASE_LOGCAT);
			suppress_msg=0;
		}
		if(!get_response){
			if(send_ipc_request(ipcpd->ipcfd, 0, 1, GPTPIPC_CMD_REQ_NDPORT_INFO))
				goto reinit;
			if(send_ipc_request(ipcpd->ipcfd, 0, 1, GPTPIPC_CMD_REQ_GPORT_INFO))
				goto reinit;
			if(send_ipc_request(ipcpd->ipcfd, 0, 0, GPTPIPC_CMD_REQ_CLOCK_INFO))
				goto reinit;
			if(send_ipc_request(ipcpd->ipcfd, 0, 1, GPTPIPC_CMD_REQ_CLOCK_INFO))
				goto reinit;
		}
		res=cb_fdread_timeout(ipcpd->ipcfd, &rd, sizeof(rd), IPC_TOUT_MS);
		if(ipcpd->ipcstop) break;
		if(res<0) goto reinit;
		if(res==0) {
			if(ipcpd->query_interval &&
			   ++timeout_count > ipcpd->query_interval/IPC_TOUT_MS) get_response=false;
			continue;
		}
		if(res!=sizeof(rd)){
			UB_LOG(UBL_WARN, "%s:ipc received wrong size=%d\n", __func__, res);
			continue;
		}
		timeout_count=0;
		get_response=true;
		if(ipcpd->printdata) print_ipc_data(&rd);
		switch(rd.dtype){
		case GPTPIPC_GPTPD_NOTICE:
			UB_LOG(UBL_DEBUG,"%s:gptp IPC NOTICE\n", __func__);
			if(ipcpd->cb) ipcpd->cb(&rd, ipcpd->cbdata);
			break;
		case GPTPIPC_GPTPD_NDPORTD:
			UB_LOG(UBL_DEBUG,"%s:gptp IPC NDPORTD response\n", __func__);
			if(ipcpd->cb) ipcpd->cb(&rd, ipcpd->cbdata);
			break;
		case GPTPIPC_GPTPD_GPORTD:
			UB_LOG(UBL_DEBUG,"%s:gptp IPC GPORTD response\n", __func__);
			if(ipcpd->cb) ipcpd->cb(&rd, ipcpd->cbdata);
			break;
		case GPTPIPC_GPTPD_CLOCKD:
			UB_LOG(UBL_DEBUG,"%s:gptp IPC CLOCKD response\n", __func__);
			if(ipcpd->cb) ipcpd->cb(&rd, ipcpd->cbdata);
			break;
		case GPTPIPC_GPTPD_STATSD:
			UB_LOG(UBL_DEBUG,"%s:gptp IPC STATSD response\n", __func__);
			if(ipcpd->cb) ipcpd->cb(&rd, ipcpd->cbdata);
			break;
		case GPTPIPC_GPTPD_STATTD:
			UB_LOG(UBL_DEBUG,"%s:gptp IPC STATTD response\n", __func__);
			if(ipcpd->cb) ipcpd->cb(&rd, ipcpd->cbdata);
			break;
		}
		continue;
	reinit:
		UB_LOG(UBL_ERROR, "%s:ipcfd error, try to reconnect\n",__func__);
		close(ipcpd->ipcfd);
		ipcpd->ipcfd=0;
		continue;
	}
	if(ipcpd->ipcfd) {
		if(ipcpd->udpport)
			cb_ipcsocket_close(ipcpd->ipcfd, NULL, NULL);
		else
			cb_ipcsocket_close(ipcpd->ipcfd, GPTP2D_IPC_CB_SOCKET_NODE, ipcpd->pname);
	}
	ipcpd->ipcfd=0;
	UB_LOG(UBL_INFO, "closing gptpipc_thread_proc\n");
	return NULL;
}

#if defined(GHINTEGRITY)
#define THREAD_NORM_STACK 0
#define THREAD_SAME_PRI 190
#else
#define THREAD_NORM_STACK 0
#define THREAD_SAME_PRI 0
#endif
int gptpipc_init(gptpipc_thread_data_t *ipctd, int wait_toutsec)
{
	int tcount=0;
	const char *thread_name="avtpd_gptpd_ipc_thread";
	cb_xl4_thread_attr_t attr;

	cb_xl4_thread_attr_init(&attr, THREAD_SAME_PRI, THREAD_NORM_STACK, thread_name);
	if(CB_THREAD_CREATE(&ipctd->ipcthread, &attr, gptpipc_thread_proc, ipctd)){
		UB_LOG(UBL_ERROR,"%s:CB_THREAD_CREATE, %s\n", __func__, strerror(errno));
		return -1;
	}
	if(wait_toutsec){
		while(ipctd->ipcfd==0) {
			usleep(100000);
			tcount+=100;
			if(wait_toutsec>0 && tcount>wait_toutsec*1000) return -1;
		}
	}
	return 0;
}

int gptpipc_close(gptpipc_thread_data_t *ipctd)
{
	ipctd->ipcstop=true;
	CB_THREAD_JOIN(ipctd->ipcthread, NULL);
	ipctd->ipcstop=false; // set to 'false' for the next connection
	return 0;
}
