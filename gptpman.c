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
#include <signal.h>
#include <stdio.h>
#include "gptpnet.h"
#include "gptpclock.h"
#include "mdeth.h"
#include "mind.h"
#include "md_pdelay_req_sm.h"
#include "md_pdelay_resp_sm.h"
#include "md_sync_receive_sm.h"
#include "md_sync_send_sm.h"
#include "announce_interval_setting_sm.h"
#include "port_sync_sync_receive_sm.h"
#include "port_sync_sync_send_sm.h"
#include "port_announce_receive_sm.h"
#include "port_announce_information_sm.h"
#include "port_announce_information_ext_sm.h"
#include "port_state_selection_sm.h"
#include "port_state_setting_ext_sm.h"
#include "port_announce_transmit_sm.h"
#include "site_sync_sync_sm.h"
#include "clock_master_sync_send_sm.h"
#include "clock_slave_sync_sm.h"
#include "clock_master_sync_receive_sm.h"
#include "clock_master_sync_offset_sm.h"
#include "gptp_capable_transmit_sm.h"
#include "gptp_capable_receive_sm.h"
#include "sync_interval_setting_sm.h"
#include "link_delay_interval_setting_sm.h"
#include "one_step_tx_oper_setting_sm.h"
#include "md_announce_send_sm.h"
#include "md_announce_receive_sm.h"
#include "md_signaling_send_sm.h"
#include "md_signaling_receive_sm.h"
#include "gm_stable_sm.h"
#include "gptpman.h"
#include "md_abnormal_hooks.h"

extern char *PTPMsgType_debug[];
extern char *gptpnet_event_debug[];

#define DOMAIN_DATA_EXIST(di) ((di>=0) && (di<gpmand->max_domains) && gpmand->tasds[di].tasglb)
#define PORT_DATA_EXIST(di,pi) (DOMAIN_DATA_EXIST(di) && (pi>=0) && (pi<gpmand->max_ports) && \
				gpmand->tasds[di].ptds[pi].ppglb)

// per-port data
typedef struct gptpsm_ptd{
	PerPortGlobal *ppglb;
	BmcsPerPortGlobal *bppglb;
	MDEntityGlobal *mdeglb;
	md_pdelay_req_data_t *mdpdreqd;
	md_pdelay_resp_data_t *mdpdrespd;
	md_sync_receive_data_t *mdsrecd;
	md_sync_send_data_t *mdssendd;
	announce_interval_setting_data_t *aisetd;
	port_sync_sync_receive_data_t *pssrecd;
	port_sync_sync_send_data_t *psssendd;
	port_announce_receive_data_t *parecd;
	port_announce_information_data_t *painfd;
	port_announce_information_ext_data_t *paiextd;
	port_announce_transmit_data_t *patransd;
	port_state_setting_ext_data_t *pssextd;
	bool cmldsLinkPortEnabled; // 11.2.16.1
	gptp_capable_transmit_data_t *gctransd;
	gptp_capable_receive_data_t *gcrecd;
	sync_interval_setting_data_t *sisetd;
	link_delay_interval_setting_data_t *ldisetd;
	one_step_tx_oper_setting_data_t *ostxopd;
	md_announce_send_data_t *mdansendd;
	md_announce_receive_data_t *mdanrecd;
	md_signaling_send_data_t *mdsigsendd;
	md_signaling_receive_data_t *mdsigrecd;
} gptpsm_ptd_t;

// per-time-aware-system data
typedef struct gptpsm_tasd{
	gptpsm_ptd_t *ptds;
	PerTimeAwareSystemGlobal *tasglb;
	BmcsPerTimeAwareSystemGlobal *btasglb;
	site_sync_sync_data_t *sssd;
	clock_master_sync_send_data_t *cmssendd;
	clock_slave_sync_data_t *cssd;
	clock_master_sync_receive_data_t *cmsrecd;
	clock_master_sync_offset_data_t *cmsoffsetd;
	port_state_selection_data_t *pssd;
	gm_stable_data_t *gmsd;
	PerPortGlobal **ppglbl;
	BmcsPerPortGlobal **bppglbl;
} gptpsm_tasd_t;

/*
  tasds[0] (0 is always domainNumber=0)
    ptds[0],ptds[1],...,ptds[max_ports]
  tasds[1] (domainNumber is tasds[1].domainNumber)
    ptds[0],ptds[1],...,ptds[max_ports]
  tasds[2] (domainNumber is tasds[2].domainNumber)
    ptds[0],ptds[1],...,ptds[max_ports]
  ...
  portIndex is enumerated, but domain number is not.
*/

// per-entire-system data
struct gptpman_data {
	gptpsm_tasd_t *tasds;
	gptpnet_data_t *gpnetd;
	int max_ports;
	int max_domains;
	FILE *extcmdstdin;
};

static void set_asCapable(PerTimeAwareSystemGlobal *tasg, gptpsm_ptd_t *ptd)
{
	if(ptd->mdeglb->forAllDomain->asCapableAcrossDomains ||
	    ptd->ppglb->neighborGptpCapable){
		if(ptd->ppglb->asCapable) return;
		ptd->ppglb->asCapable = true;
		UB_LOG(UBL_INFO,
		       "set asCapable for domainNumber=%d, portIndex=%d\n",
		       tasg->domainNumber, ptd->ppglb->thisPortIndex);
		ptd->ppglb->portEventFlags|=GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_UP;
		ptd->ppglb->portEventFlags&=~GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_DOWN;
	}else{
		if(!ptd->ppglb->asCapable) return;
		ptd->ppglb->asCapable = false;
		UB_LOG(UBL_INFO,
		       "reset asCapable for domainNumber=%d, portIndex=%d\n",
		       tasg->domainNumber, ptd->ppglb->thisPortIndex);
		ptd->ppglb->portEventFlags|=GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_DOWN;
		ptd->ppglb->portEventFlags&=~GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_UP;
	}
}

static int portSyncSync_for_all(gptpman_data_t *gpmand, int domainIndex,
				void *smret, uint64_t cts64)
{
	int pi;
	void *smretp;
	for(pi=1;pi<gpmand->max_ports;pi++){
		smretp=port_sync_sync_send_sm_portSyncSync(
			gpmand->tasds[domainIndex].ptds[pi].psssendd, smret, cts64);
		if(smretp) md_sync_send_sm_mdSyncSend(
			gpmand->tasds[domainIndex].ptds[pi].mdssendd, smretp, cts64);

		// send smret to BMCS PortAnnounceInformation
		port_announce_information_sm_SyncReceiptTimeoutTime(
		gpmand->tasds[domainIndex].ptds[pi].painfd, smret);
	}

	// send the same smret to ClockSlaveSync
	clock_slave_sync_sm_portSyncSync(
		gpmand->tasds[domainIndex].cssd, smret, cts64);
	clock_master_sync_receive_sm_ClockSourceReq(
		gpmand->tasds[domainIndex].cmsrecd, cts64);
	clock_master_sync_offset_sm_SyncReceiptTime(
		gpmand->tasds[domainIndex].cmsoffsetd, cts64);

	return 0;
}

static int get_domain_index(gptpman_data_t *gpmand, uint8_t domainNumber)
{
	int i;
	if(domainNumber==0) return 0; // 0 is always index 0
	for(i=0;i<gpmand->max_domains;i++){
		if(gpmand->tasds[i].tasglb &&
		   gpmand->tasds[i].tasglb->domainNumber == domainNumber) return i;
	}
	return -1;
}

static int sm_bmcs_perform(gptpman_data_t *gpmand, int domainIndex,
		                   int portIndex, uint64_t cts64)
{
	void *smret;
	gptpsm_tasd_t *tasd = &gpmand->tasds[domainIndex];

	if(tasd->btasglb->externalPortConfiguration==VALUE_DISABLED){
		do{
			port_announce_information_sm(tasd->ptds[portIndex].painfd, cts64);
			if((smret=port_state_selection_sm(tasd->pssd, cts64))){
				gm_stable_gm_change(tasd->gmsd, smret, cts64);
				// update clock source after GM change
				clock_master_sync_receive_sm_ClockSourceReq(tasd->cmsrecd, cts64);
			}
		}while(tasd->ptds[portIndex].bppglb->updtInfo);
	}else{ // btasglb->externalPortConfiguration==VALUE_ENABLED
		smret = port_announce_information_ext_sm(tasd->ptds[portIndex].paiextd, cts64);
		if(smret) port_state_setting_ext_sm_messagePriority(tasd->ptds[portIndex].pssextd, smret);
		port_state_setting_ext_sm(tasd->ptds[portIndex].pssextd, cts64);
	}

	smret = port_announce_transmit_sm(tasd->ptds[portIndex].patransd, cts64);
	if(smret) md_announce_send_sm_mdAnnouncSend(tasd->ptds[portIndex].mdansendd, smret, cts64);
	return 0;
}

static int sm_bmcs_domain_port_update(gptpman_data_t *gpmand, int domainIndex,
		                              int portIndex, uint64_t cts64)
{
	// update state machine on event occurence
	// ie. portOper = true, asCapable = true
	announce_interval_setting_sm(gpmand->tasds[domainIndex].ptds[portIndex].aisetd, cts64);
	sm_bmcs_perform(gpmand, domainIndex, portIndex, cts64);
	return 0;
}

static int sm_close_for_domain_zero_port(gptpman_data_t *gpmand, int pi)
{
	SM_CLOSE(md_pdelay_resp_sm_close, gpmand->tasds[0].ptds[pi].mdpdrespd);
	SM_CLOSE(md_pdelay_req_sm_close, gpmand->tasds[0].ptds[pi].mdpdreqd);
	SM_CLOSE(link_delay_interval_setting_sm_close, gpmand->tasds[0].ptds[pi].ldisetd);
	return 0;
}

static int sm_close_for_domain_port(gptpman_data_t *gpmand, int di, int pi)
{
	if(!DOMAIN_DATA_EXIST(di) ||
	   (di!=0 && !gpmand->tasds[di].tasglb->domainNumber)){
		UB_LOG(UBL_DEBUG, "%s:di=%d, pi=%d, not initialized\n", __func__, di, pi);
		return -1;
	}
	if(di==0) sm_close_for_domain_zero_port(gpmand, pi);
	SM_CLOSE(port_announce_information_ext_sm_close, gpmand->tasds[di].ptds[pi].paiextd);
	SM_CLOSE(port_announce_information_sm_close, gpmand->tasds[di].ptds[pi].painfd);
	if(pi==0) return 0;
	SM_CLOSE(announce_interval_setting_sm_close, gpmand->tasds[di].ptds[pi].aisetd);
	SM_CLOSE(port_state_setting_ext_sm_close, gpmand->tasds[di].ptds[pi].pssextd);
	SM_CLOSE(port_announce_receive_sm_close, gpmand->tasds[di].ptds[pi].parecd);
	SM_CLOSE(port_announce_transmit_sm_close, gpmand->tasds[di].ptds[pi].patransd);
	SM_CLOSE(md_sync_receive_sm_close, gpmand->tasds[di].ptds[pi].mdsrecd);
	SM_CLOSE(md_sync_send_sm_close, gpmand->tasds[di].ptds[pi].mdssendd);
	SM_CLOSE(port_sync_sync_receive_sm_close, gpmand->tasds[di].ptds[pi].pssrecd);
	SM_CLOSE(port_sync_sync_send_sm_close, gpmand->tasds[di].ptds[pi].psssendd);
	SM_CLOSE(gptp_capable_transmit_sm_close, gpmand->tasds[di].ptds[pi].gctransd);
	SM_CLOSE(gptp_capable_receive_sm_close, gpmand->tasds[di].ptds[pi].gcrecd);
	SM_CLOSE(sync_interval_setting_sm_close, gpmand->tasds[di].ptds[pi].sisetd);
	SM_CLOSE(one_step_tx_oper_setting_sm_close, gpmand->tasds[di].ptds[pi].ostxopd);
	SM_CLOSE(md_announce_send_sm_close, gpmand->tasds[di].ptds[pi].mdansendd);
	SM_CLOSE(md_announce_receive_sm_close, gpmand->tasds[di].ptds[pi].mdanrecd);
	SM_CLOSE(md_signaling_send_sm_close, gpmand->tasds[di].ptds[pi].mdsigsendd);
	SM_CLOSE(md_signaling_receive_sm_close, gpmand->tasds[di].ptds[pi].mdsigrecd);
	return 0;
}

static int gptpnet_cb_devup(gptpman_data_t *gpmand, int portIndex,
			    event_data_netlink_t *ed, uint64_t cts64)
{
	char *dup;
	int di;
	gptpipc_gptpd_data_t ipcd;

	switch(ed->duplex){
	case 1: dup="full";
		if(ed->speed<100) break;
		gpmand->tasds[0].ptds[portIndex].ppglb->forAllDomain->portOper=true;

		for(di=0;di<gpmand->max_domains;di++){
			if(!DOMAIN_DATA_EXIST(di)) continue;
			if(di!=0 && !gpmand->tasds[di].tasglb->domainNumber) continue;
			sm_bmcs_domain_port_update(gpmand, di, portIndex, cts64);
		}

		break;
	case 2: dup="half"; break;
	default: dup="unknown"; break;
	}
	UB_LOG(UBL_INFO, "index=%d speed=%d, duplex=%s, ptpdev=%s\n",
	       portIndex, (int)ed->speed, dup, ed->ptpdev);

	if(ed->speed<100 || ed->duplex!=1){
		UB_LOG(UBL_WARN,"!!! Full duplex link with "
				"Speed above 100 Mbps needed for gptp to run !!!\n");
	}

	memset(&ipcd, 0, sizeof(ipcd));
	ipcd.dtype=GPTPIPC_GPTPD_NOTICE;
	ipcd.u.notice.event_flags=GPTPIPC_EVENT_CLOCK_FLAG_NETDEV_UP;
	ipcd.u.notice.portIndex=portIndex;
	gptpnet_ipc_notice(gpmand->gpnetd, &ipcd, sizeof(ipcd));
	return 0;
}

static int gptpnet_cb_devdown(gptpman_data_t *gpmand, int portIndex,
			      event_data_netlink_t *ed, uint64_t cts64)
{
	gptpipc_gptpd_data_t ipcd;
	int di;
	UB_LOG(UBL_INFO, "%s:portIndex=%d\n", __func__, portIndex);

	memset(&ipcd, 0, sizeof(ipcd));
	ipcd.dtype=GPTPIPC_GPTPD_NOTICE;
	ipcd.u.notice.event_flags=GPTPIPC_EVENT_CLOCK_FLAG_NETDEV_DOWN;
	ipcd.u.notice.portIndex=portIndex;
	gptpnet_ipc_notice(gpmand->gpnetd, &ipcd, sizeof(ipcd));
	gpmand->tasds[0].ptds[portIndex].ppglb->forAllDomain->portOper=false;

	gpmand->tasds[0].ptds[portIndex].mdeglb->forAllDomain->asCapableAcrossDomains=false;
	gpmand->tasds[0].ptds[portIndex].ppglb->forAllDomain->receivedNonCMLDSPdelayReq=0;
	for(di=0;di<gpmand->max_domains;di++){
		if(!gpmand->tasds[di].ptds[portIndex].ppglb->asCapable) continue;
		gpmand->tasds[di].ptds[portIndex].ppglb->asCapable = false;
		gpmand->tasds[di].ptds[portIndex].ppglb->portEventFlags|=
			GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_DOWN;
		gpmand->tasds[di].ptds[portIndex].ppglb->portEventFlags&=
			~GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_UP;
	}
	return 0;
}

// return 1 when TxTS is expected after this call
static int gptpnet_cb_timeout(gptpman_data_t *gpmand, uint64_t cts64)
{
	int pi, di;
	void *smret;

	for(di=0;di<gpmand->max_domains;di++){
		if(!DOMAIN_DATA_EXIST(di)) continue;

		if(di==0){
			for(pi=1;pi<gpmand->max_ports;pi++){
				md_pdelay_req_sm(gpmand->tasds[di].ptds[pi].mdpdreqd, cts64);
				md_pdelay_resp_sm(gpmand->tasds[di].ptds[pi].mdpdrespd, cts64);
			}
		}
		smret=clock_master_sync_send_sm(gpmand->tasds[di].cmssendd, cts64);
		if(smret) smret=site_sync_sync_sm_portSyncSync(
			gpmand->tasds[di].sssd, smret, cts64);
		if(smret) portSyncSync_for_all(gpmand, di, smret, cts64);

		gpmand->tasds[di].tasglb->asCapableOrAll=false;
		for(pi=1;pi<gpmand->max_ports;pi++){
			set_asCapable(gpmand->tasds[di].tasglb,  &gpmand->tasds[di].ptds[pi]);
			gpmand->tasds[di].tasglb->asCapableOrAll |=
				gpmand->tasds[di].ptds[pi].ppglb->asCapable;
			smret=gptp_capable_transmit_sm(gpmand->tasds[di].ptds[pi].gctransd, cts64);
			if(smret) md_signaling_send_sm_mdSignalingSend(
					gpmand->tasds[di].ptds[pi].mdsigsendd, smret, cts64);

			// The next 4 md_* calls are to check and send defered msg.
			// However, ensure that this are called after the calling
			// set_asCapable above so that no defered messages are not sent
			// after asCapable has been toggled to false already.
			md_sync_send_sm(gpmand->tasds[di].ptds[pi].mdssendd, cts64);
			md_announce_send_sm(gpmand->tasds[di].ptds[pi].mdansendd, cts64);
			md_signaling_send_sm(gpmand->tasds[di].ptds[pi].mdsigsendd, cts64);

			md_sync_receive_sm(gpmand->tasds[di].ptds[pi].mdsrecd, cts64);
			// asCapable might be set, inform other state machines
			if(di!=0 && !gpmand->tasds[di].tasglb->domainNumber) continue;
			sm_bmcs_domain_port_update(gpmand, di, pi, cts64);
		}
		gm_stable_sm(gpmand->tasds[di].gmsd, cts64);
	}
	return 0;
}

// return 1 when TxTS is expected after this call
static int gptpnet_cb_recv(gptpman_data_t *gpmand, int portIndex,
			   event_data_recv_t *ed, uint64_t cts64)
{
	int di=0;
	int pi;
	char *msg;
	void *smret;
	uint32_t stype;
	if(ed->domain!=0){
		/* get domainIndex from  domainNumber */
		if((di=get_domain_index(gpmand, ed->domain))<0) {
			UB_LOG(UBL_DEBUG, "%s:domainNumber=%d is not yet initialized\n",
			       __func__, ed->domain);
			return 0;
		}
	}
	if(ed->msgtype<=15)
		msg=PTPMsgType_debug[ed->msgtype];
	else
		msg="unknown";

	UB_LOG(UBL_DEBUGV, "RECV: pindex=%d msgtype=%s, TS=%"PRIi64"nsec\n",
	       portIndex, msg, ed->ts64);

	switch(ed->msgtype){
	case SYNC:
		/* in a TAS, all the TSs work based on 'thisClock', so when a TS is not
		 * based on 'thisClock', it must be converted.
		 * When this TAS is synced to GM, 'thisClock' has the same freq. rate as GM,
		 * but the phase is different. The phase sync happens in the TAS master clock,
		 * which is gptpclock entity of (ClockIndex=0, tasglb->domainNumber)
		 */
		pi=gptpconf_get_intitem(CONF_SINGLE_CLOCK_MODE)?1:portIndex;
		gptpclock_tsconv(&ed->ts64, pi, 0,
				 gpmand->tasds[di].tasglb->thisClockIndex,
				 gpmand->tasds[di].tasglb->domainNumber);
		md_sync_receive_sm_recv_sync(gpmand->tasds[di].ptds[portIndex].mdsrecd,
					     ed, cts64);
		return 0;
	case PDELAY_REQ:
		if(di!=0) goto not_allowed_domain;
		md_pdelay_resp_sm_recv_req(gpmand->tasds[0].ptds[portIndex].mdpdrespd,
					   ed, cts64);
		return 0;
	case PDELAY_RESP:
		if(di!=0) goto not_allowed_domain;
		md_pdelay_req_sm_recv_resp(gpmand->tasds[0].ptds[portIndex].mdpdreqd,
					   ed, cts64);
		return 0;
	case FOLLOW_UP:
		smret=md_sync_receive_sm_recv_fup(
			gpmand->tasds[di].ptds[portIndex].mdsrecd, ed, cts64);
		if(smret) smret=port_sync_sync_receive_sm_recvMDSync(
			gpmand->tasds[di].ptds[portIndex].pssrecd, smret, cts64);
		if(smret) smret=site_sync_sync_sm_portSyncSync(
			gpmand->tasds[di].sssd, smret, cts64);
		if(smret) portSyncSync_for_all(gpmand, di, smret, cts64);
		return 0;
	case PDELAY_RESP_FOLLOW_UP:
		if(di!=0) goto not_allowed_domain;
		md_pdelay_req_sm_recv_respfup(gpmand->tasds[0].ptds[portIndex].mdpdreqd,
					      ed, cts64);
		// update asCapable for all domains
		gpmand->tasds[di].tasglb->asCapableOrAll=false;
		for(di=0;di<gpmand->max_domains;di++){
			if(!DOMAIN_DATA_EXIST(di)) continue;
			set_asCapable(gpmand->tasds[di].tasglb,
				      &gpmand->tasds[di].ptds[portIndex]);
			gpmand->tasds[di].tasglb->asCapableOrAll |=
				gpmand->tasds[di].ptds[portIndex].ppglb->asCapable;
		}
		return 0;
	case ANNOUNCE:
		smret=md_announce_receive_sm_mdAnnounceRec(
			gpmand->tasds[0].ptds[portIndex].mdanrecd, ed, cts64);
		if(smret){
			port_announce_receive_sm_recv_announce(
				gpmand->tasds[di].ptds[portIndex].parecd, smret, cts64);
			sm_bmcs_perform(gpmand, di, portIndex, cts64);
		}
		return 0;
	case SIGNALING:
		smret=md_signaling_receive_sm_mdSignalingRec(
			gpmand->tasds[di].ptds[portIndex].mdsigrecd, ed, cts64);
		if(!smret) return 0;
		stype=((PTPMsgIntervalRequestTLV *)smret)->organizationSubType;
		if(stype==2){
			sync_interval_setting_SignalingMsg1(
				gpmand->tasds[di].ptds[portIndex].sisetd,
				(PTPMsgIntervalRequestTLV *)smret, cts64);
			link_delay_interval_setting_SignalingMsg1(
				gpmand->tasds[di].ptds[portIndex].ldisetd,
				(PTPMsgIntervalRequestTLV *)smret, cts64);

		}else if(stype==4){
			gptp_capable_receive_rcvdSignalingMsg(
				gpmand->tasds[di].ptds[portIndex].gcrecd,
				(PTPMsgGPTPCapableTLV *)smret, cts64);
		}else{
			UB_LOG(UBL_WARN,"%s:unknown signaling message, stype=%u\n",
			       __func__, (unsigned int)stype);
		}
		return 0;
	default:
		return 0;
	}
not_allowed_domain:
	UB_LOG(UBL_WARN, "%s:msgtype=%s is not allowed for dominNumber=%d",
	       __func__, msg, ed->domain);
	return 0;
}

static int gptpnet_cb_txts(gptpman_data_t *gpmand, int portIndex,
			   event_data_txts_t *ed, uint64_t cts64)
{
	int di=0;
	char *msg;
	if(ed->domain!=0){
		/* get domainIndex from  domainNumber */
		if((di=get_domain_index(gpmand, ed->domain))<0) {
			UB_LOG(UBL_WARN, "%s:domainNumber=%d is not yet initialized\n",
			       __func__, ed->domain);
			return 0;
		}
	}
	if(ed->msgtype<=15)
		msg=PTPMsgType_debug[ed->msgtype];
	else
		msg="unknown";

	switch(ed->msgtype){
	case SYNC:
		md_sync_send_sm_txts(gpmand->tasds[di].ptds[portIndex].mdssendd,
				     ed, cts64);
		return 0;
	case PDELAY_REQ:
		if(di!=0) goto not_allowed_domain;
		md_pdelay_req_sm_txts(gpmand->tasds[0].ptds[portIndex].mdpdreqd,
				      ed, cts64);
		return 0;
	case PDELAY_RESP:
		if(di!=0) goto not_allowed_domain;
		md_pdelay_resp_sm_txts(gpmand->tasds[0].ptds[portIndex].mdpdrespd,
				       ed, cts64);
		return 0;
	case FOLLOW_UP:
	case PDELAY_RESP_FOLLOW_UP:
	case ANNOUNCE:
	default:
		UB_LOG(UBL_WARN, "%s: msgtype=%s shouldn't have TxTS\n", __func__, msg);
		return 0;
	}
not_allowed_domain:
	UB_LOG(UBL_WARN, "%s:msgtype=%s is not allowed for dominNumber=%d",
	       __func__, msg, ed->domain);
	return 0;
}

static int ipc_respond_one_ndport(gptpnet_data_t *gpnetd, int portIndex, struct sockaddr *addr)
{
	gptpipc_gptpd_data_t pd;
	memset(&pd, 0, sizeof(pd));
	pd.dtype=GPTPIPC_GPTPD_NDPORTD;
	if(gptpnet_get_nlstatus(gpnetd, portIndex-1, &pd.u.ndportd.nlstatus))
		return -1;
	gptpnet_ipc_respond(gpnetd, addr, &pd, sizeof(pd));
	return 0;
}

static int ipc_respond_one_clock(gptpnet_data_t *gpnetd,
				 PerTimeAwareSystemGlobal *tasglb,
				 int portIndex, int domainNumber, struct sockaddr *addr)
{
	gptpipc_gptpd_data_t pd;
	memset(&pd, 0, sizeof(pd));
	pd.dtype=GPTPIPC_GPTPD_CLOCKD;
	pd.u.clockd.portIndex=portIndex;
	pd.u.clockd.adjppb=gptpclock_get_adjppb(portIndex, domainNumber);
	if(gptpclock_get_ipc_clock_data(portIndex, domainNumber, &pd.u.clockd)){
		UB_LOG(UBL_WARN, "%s: portIndex=%d, domainNumber=%d, no clock data\n",
		       __func__, portIndex, domainNumber);
		return -1;
	}
	if(portIndex==0){
		pd.u.clockd.gmTimeBaseIndicator=tasglb->clockSourceTimeBaseIndicator;
		memcpy(&pd.u.clockd.lastGmFreqChangePk, &tasglb->clockSourceLastGmFreqChange,
		       sizeof(double));
		pd.u.clockd.lastGmPhaseChange_nsec=tasglb->clockSourceLastGmPhaseChange.nsec;
	}
	gptpnet_ipc_respond(gpnetd, addr, &pd, sizeof(pd));
	return 0;
}

static int ipc_respond_one_gport(gptpnet_data_t *gpnetd,
				 PerPortGlobal *ppglb, BmcsPerPortGlobal *bppglb,
				 PerTimeAwareSystemGlobal *tasglb,
				 BmcsPerTimeAwareSystemGlobal *btasglb,
				 int portIndex, int domainNumber, int domainIndex,
				 struct sockaddr *addr)
{
	gptpipc_gptpd_data_t pd;
	memset(&pd, 0, sizeof(pd));
	pd.dtype=GPTPIPC_GPTPD_GPORTD;
	pd.u.gportd.portIndex=portIndex;
	pd.u.gportd.domainNumber=domainNumber;
	pd.u.gportd.asCapable=ppglb->asCapable;
	pd.u.gportd.portOper=ppglb->forAllDomain->portOper;
	pd.u.gportd.selectedState=tasglb->selectedState[portIndex];
	pd.u.gportd.gmStable=gptpclock_get_gmstable(domainIndex);
	memcpy(&pd.u.gportd.gmClockId, btasglb->gmPriority.rootSystemIdentity.clockIdentity,
	       sizeof(ClockIdentity));
	pd.u.gportd.annPathSequenceCount=bppglb->annPathSequenceCount;
	if(bppglb->annPathSequenceCount>=MAX_PATH_TRACE_N) return -1;
	memcpy(&pd.u.gportd.annPathSequence, &bppglb->annPathSequence,
	       bppglb->annPathSequenceCount * sizeof(ClockIdentity));
	gptpnet_ipc_respond(gpnetd, addr, &pd, sizeof(pd));
	return 0;
}

static int ipc_respond_statsd_info(gptpman_data_t *gpmand, int pi, int resetcmd,
				   struct sockaddr *addr)
{
	gptpipc_gptpd_data_t pd;
	md_pdelay_req_stat_data_t *prsd;
	md_pdelay_resp_stat_data_t *ppsd;
	if(pi<0 || pi>=gpmand->max_ports) return -1;
	if(resetcmd){
		md_pdelay_req_stat_reset(gpmand->tasds[0].ptds[pi].mdpdreqd);
		md_pdelay_resp_stat_reset(gpmand->tasds[0].ptds[pi].mdpdrespd);
		return 0;
	}
	memset(&pd, 0, sizeof(pd));
	pd.dtype=GPTPIPC_GPTPD_STATSD;
	prsd=md_pdelay_req_get_stat(gpmand->tasds[0].ptds[pi].mdpdreqd);
	pd.u.statsd.portIndex=pi;
	pd.u.statsd.pdelay_req_send=prsd->pdelay_req_send;
	pd.u.statsd.pdelay_resp_rec=prsd->pdelay_resp_rec;
	pd.u.statsd.pdelay_resp_rec_valid=prsd->pdelay_resp_rec_valid;
	pd.u.statsd.pdelay_resp_fup_rec=prsd->pdelay_resp_fup_rec;
	pd.u.statsd.pdelay_resp_fup_rec_valid=prsd->pdelay_resp_fup_rec_valid;

	ppsd=md_pdelay_resp_get_stat(gpmand->tasds[0].ptds[pi].mdpdrespd);
	pd.u.statsd.pdelay_req_rec=ppsd->pdelay_req_rec;
	pd.u.statsd.pdelay_req_rec_valid=ppsd->pdelay_req_rec_valid;
	pd.u.statsd.pdelay_resp_send=ppsd->pdelay_resp_send;
	pd.u.statsd.pdelay_resp_fup_send=ppsd->pdelay_resp_fup_send;

	gptpnet_ipc_respond(gpmand->gpnetd, addr, &pd, sizeof(pd));
	return 0;
}
static int ipc_respond_stattd_info(gptpman_data_t *gpmand, int di, int pi, int resetcmd,
				   struct sockaddr *addr)
{
	gptpipc_gptpd_data_t pd;
	md_sync_send_stat_data_t *sssd;
	md_sync_receive_stat_data_t *srsd;
	md_signaling_send_stat_data_t *gssd;
	md_signaling_receive_stat_data_t *grsd;

	if(di<0 || di>=gpmand->max_domains) return -1;
	if(pi<0 || pi>=gpmand->max_ports) return -1;
	if(resetcmd){
		md_sync_send_stat_reset(gpmand->tasds[di].ptds[pi].mdssendd);
		md_sync_receive_stat_reset(gpmand->tasds[di].ptds[pi].mdsrecd);
		return 0;
	}
	memset(&pd, 0, sizeof(pd));
	pd.dtype=GPTPIPC_GPTPD_STATTD;
	sssd=md_sync_send_get_stat(gpmand->tasds[di].ptds[pi].mdssendd);
	pd.u.stattd.portIndex=pi;
	pd.u.stattd.domainNumber=gpmand->tasds[di].tasglb->domainNumber;
	pd.u.stattd.sync_send=sssd->sync_send;
	pd.u.stattd.sync_fup_send=sssd->sync_fup_send;

	srsd=md_sync_receive_get_stat(gpmand->tasds[di].ptds[pi].mdsrecd);
	pd.u.stattd.sync_rec=srsd->sync_rec;
	pd.u.stattd.sync_fup_rec=srsd->sync_fup_rec;
	pd.u.stattd.sync_rec_valid=srsd->sync_rec_valid;
	pd.u.stattd.sync_fup_rec_valid=srsd->sync_fup_rec_valid;

	gssd=md_signaling_send_get_stat(gpmand->tasds[di].ptds[pi].mdsigsendd);
	pd.u.stattd.signal_msg_interval_send=gssd->signal_msg_interval_send;
	pd.u.stattd.signal_gptp_capable_send=gssd->signal_gptp_capable_send;

	grsd=md_signaling_receive_get_stat(gpmand->tasds[di].ptds[pi].mdsigrecd);
	pd.u.stattd.signal_rec=grsd->signal_rec;
	pd.u.stattd.signal_msg_interval_rec=grsd->signal_msg_interval_rec;
	pd.u.stattd.signal_gptp_capable_rec=grsd->signal_gptp_capable_rec;

	gptpnet_ipc_respond(gpmand->gpnetd, addr, &pd, sizeof(pd));
	return 0;
}

static int get_domain_index_ipc(gptpman_data_t *gpmand, gptpipc_client_req_data_t *reqdata)
{
	if(reqdata->domainNumber==-1){
		if(reqdata->domainIndex>=0){
			if(reqdata->domainIndex>=gpmand->max_domains) return -1;
			if(!gpmand->tasds[reqdata->domainIndex].tasglb) return -1;
			reqdata->domainNumber=
				gpmand->tasds[reqdata->domainIndex].tasglb->domainNumber;
			return reqdata->domainIndex;
		}
		return -2; // the both are -1
	}
	reqdata->domainIndex=get_domain_index(gpmand, reqdata->domainNumber);
	return reqdata->domainIndex;
}

static int run_ext_script(FILE **extstdin, int arg)
{
#ifndef SYSTEM
	return 0;
#else
	char cmdstring[64];
	FILE *pfp;
	if(arg==0){
		snprintf(cmdstring, 64, "%s %d > /dev/null", GPTPIPC_EXT_SCRIPT, arg);
	}else{
		snprintf(cmdstring, 64, "%s %d > /tmp/runfromgptpd.log", GPTPIPC_EXT_SCRIPT, arg);
	}
	pfp=POPEN(cmdstring, "w"); // this command is sure to terminate previously running one
	if(*extstdin) PCLOSE(*extstdin);
	*extstdin=pfp;
	return 0;
#endif
}

static int ipc_register_abnormal_event(gptpipc_client_req_data_t *reqdata)
{
	md_abn_event_t aevent;

	memset(&aevent,0,sizeof(aevent));
	aevent.domainNumber=reqdata->domainNumber;
	aevent.ndevIndex=reqdata->portIndex-1;
	aevent.msgtype=(PTPMsgType)reqdata->u.abnd.msgtype;
	aevent.eventtype=(md_abn_event_type)reqdata->u.abnd.eventtype;
	aevent.eventrate=reqdata->u.abnd.eventrate;
	aevent.repeat=reqdata->u.abnd.repeat;
	aevent.interval=reqdata->u.abnd.interval;
	aevent.eventpara=reqdata->u.abnd.eventpara;
	if(reqdata->u.abnd.subcmd==0){
		return md_abnormal_register_event(&aevent);
	}
	if(reqdata->u.abnd.msgtype==-1){
		return md_abnormal_deregister_all_events();
	}
	return md_abnormal_deregister_msgtype_events(aevent.msgtype);
}

static int ipc_clock_master_clock_notice(gptpnet_data_t *gpnetd,
					 BmcsPerTimeAwareSystemGlobal *btasglb,
					 PerTimeAwareSystemGlobal *tasglb, int di)
{
	gptpipc_gptpd_data_t ipcd;
	int do_phase=gptpconf_get_intitem(CONF_IPC_NOTICE_PHASE_UPDATE);
	uint32_t aligntime, cycletime;
	// master clock (clockIndex=0) for PHASE_UPDATE and GM_SYNC
	memset(&ipcd, 0, sizeof(ipcd));
	ipcd.dtype=GPTPIPC_GPTPD_NOTICE;
	ipcd.u.notice.event_flags=gptpclock_get_event_flags(0, di);
	if(!ipcd.u.notice.event_flags) return 0;

	if(ipcd.u.notice.event_flags && GPTPIPC_EVENT_CLOCK_FLAG_GM_CHANGE){
		memcpy(&ipcd.u.notice.gmPriority, &btasglb->gmPriority,
		       sizeof(UInteger224));
	}

	if(gptpconf_get_intitem(CONF_TSN_SCHEDULE_ON) &&
	   (ipcd.u.notice.event_flags & GPTPIPC_EVENT_CLOCK_FLAG_PHASE_UPDATE)){
		aligntime=gptpconf_get_intitem(CONF_TSN_SCHEDULE_ALIGNTIME);
		cycletime=gptpconf_get_intitem(CONF_TSN_SCHEDULE_CYCLETIME);
		gptpnet_tsn_schedule(gpnetd, aligntime, cycletime);
	}

	if(ipcd.u.notice.event_flags == GPTPIPC_EVENT_CLOCK_FLAG_PHASE_UPDATE){
		if(!do_phase) return 0;
	}

	ipcd.u.notice.domainNumber=tasglb->domainNumber;
	ipcd.u.notice.domainIndex=di;

	ipcd.u.notice.gmTimeBaseIndicator=tasglb->clockSourceTimeBaseIndicator;
	memcpy(&ipcd.u.notice.lastGmFreqChangePk, &tasglb->clockSourceLastGmFreqChange,
	       sizeof(double));
	ipcd.u.notice.lastGmPhaseChange_nsec=tasglb->clockSourceLastGmPhaseChange.nsec;

	return gptpnet_ipc_notice(gpnetd, &ipcd, sizeof(ipcd));
}

static int ipc_clock_this_clock_notice(gptpnet_data_t *gpnetd, int di, uint8_t domainNumber,
				       int thisClockIndex)
{
	gptpipc_gptpd_data_t ipcd;
	int do_freq=gptpconf_get_intitem(CONF_IPC_NOTICE_FREQ_UPDATE);
	// thisClock for FREQ_UPDATE
	memset(&ipcd, 0, sizeof(ipcd));
	ipcd.u.notice.event_flags=gptpclock_get_event_flags(thisClockIndex, di);
	if(!ipcd.u.notice.event_flags) return 0;
	if((ipcd.u.notice.event_flags & ~GPTPIPC_EVENT_CLOCK_FLAG_FREQ_UPDATE) || do_freq){
		ipcd.u.notice.domainNumber=domainNumber;
		ipcd.u.notice.domainIndex=di;
		ipcd.u.notice.portIndex=thisClockIndex;
		gptpnet_ipc_notice(gpnetd, &ipcd, sizeof(ipcd));
	}
	return 1;
}

static int ipc_port_state_notice(gptpnet_data_t *gpnetd, int di, int pi, uint8_t domainNumber,
				 uint32_t flags)
{
	gptpipc_gptpd_data_t ipcd;
	memset(&ipcd, 0, sizeof(ipcd));
	ipcd.u.notice.event_flags=flags;
	ipcd.u.notice.domainNumber=domainNumber;
	ipcd.u.notice.domainIndex=di;
	ipcd.u.notice.portIndex=pi;
	gptpnet_ipc_notice(gpnetd, &ipcd, sizeof(ipcd));
	return 0;
}

static int ipc_clock_notice(gptpman_data_t *gpmand)
{
	int di, pi;
	for(di=0;di<gpmand->max_domains;di++){
		if(!DOMAIN_DATA_EXIST(di)) continue;
		ipc_clock_master_clock_notice(gpmand->gpnetd,
					      gpmand->tasds[di].btasglb,
					      gpmand->tasds[di].tasglb, di);
		ipc_clock_this_clock_notice(gpmand->gpnetd, di,
					    gpmand->tasds[di].tasglb->domainNumber,
					    gpmand->tasds[di].tasglb->thisClockIndex);

		for(pi=1;pi<gpmand->max_ports;pi++){
			if(!gpmand->tasds[di].ptds[pi].ppglb) continue;
			if(!gpmand->tasds[di].ptds[pi].ppglb->portEventFlags) continue;
			ipc_port_state_notice(gpmand->gpnetd, di, pi,
					      gpmand->tasds[di].tasglb->domainNumber,
					      gpmand->tasds[di].ptds[pi].ppglb->portEventFlags);
			gpmand->tasds[di].ptds[pi].ppglb->portEventFlags=0;
		}
	}
	return 0;
}

static int gptpnet_cb(void *cb_data, int portIndex, gptpnet_event_t event,
		      int64_t *event_ts64, void *event_data)
{
	gptpman_data_t *gpmand=(gptpman_data_t*)cb_data;
	uint64_t cts64=*event_ts64;
	int res=0;

	UB_TLOG(UBL_DEBUGV, "index=%d event=%s\n", portIndex, gptpnet_event_debug[event]);
	switch(event){
	case GPTPNET_EVENT_NONE:
		break;
	case GPTPNET_EVENT_TIMEOUT:
		gptpnet_cb_timeout(gpmand, cts64);
		break;
	case GPTPNET_EVENT_DEVUP:
		res = gptpnet_cb_devup(gpmand, portIndex,
					(event_data_netlink_t *)event_data, cts64);
		break;
	case GPTPNET_EVENT_DEVDOWN:
		res = gptpnet_cb_devdown(gpmand, portIndex,
					  (event_data_netlink_t *)event_data, cts64);
		break;
	case GPTPNET_EVENT_RECV:
		res = gptpnet_cb_recv(gpmand, portIndex,
				       (event_data_recv_t *)event_data, cts64);
		break;
	case GPTPNET_EVENT_TXTS:
		res = gptpnet_cb_txts(gpmand, portIndex,
				       (event_data_txts_t *)event_data, cts64);
		break;
	}
	ub_log_flush();
	if(res) return res;
	ipc_clock_notice(gpmand);
	return 0;
}

static int gptpnet_ipc_cb(void *cbdata, uint8_t *rdata, int size, struct sockaddr *addr)
{
	int di,ddi,pi,ppi;
	int resetcmd=0;
	uint32_t aligntime, cycletime;
	gptpipc_client_req_data_t *reqdata=(gptpipc_client_req_data_t *)rdata;
	gptpman_data_t *gpmand=(gptpman_data_t*)cbdata;

	if(size < 16) {
		UB_LOG(UBL_INFO,"%s:wrong received size:%d\n",__func__, size);
		return -1;
	}

	switch(reqdata->cmd){
	case GPTPIPC_CMD_DISCONNECT:
		gptpnet_ipc_client_remove(gpmand->gpnetd, addr);
		return 0;
	case GPTPIPC_CMD_REQ_NDPORT_INFO:
		if(reqdata->portIndex>0){
			return ipc_respond_one_ndport(gpmand->gpnetd, reqdata->portIndex,
						      addr);
		}else{
			for(pi=1;pi<gpmand->max_ports;pi++)
				ipc_respond_one_ndport(gpmand->gpnetd, pi, addr);
		}
		return 0;
	case GPTPIPC_CMD_REQ_CLOCK_INFO:
		di=get_domain_index_ipc(gpmand, reqdata);
		if(di==-1) return -1;
		if(di>=0){
			if(!DOMAIN_DATA_EXIST(di)) return -1;
			return ipc_respond_one_clock(gpmand->gpnetd,
						     gpmand->tasds[di].tasglb,
						     reqdata->portIndex,
						     reqdata->domainNumber,
						     addr);
		}
		for(di=0;di<gpmand->max_domains;di++){
			if(!DOMAIN_DATA_EXIST(di)) continue;
			// clockIndex=0 is the master clock and IPC needs only that info.
			ipc_respond_one_clock(gpmand->gpnetd,
					      gpmand->tasds[di].tasglb,
					      0, gpmand->tasds[di].tasglb->domainNumber, addr);
		}
		return 0;
	case GPTPIPC_CMD_REQ_GPORT_INFO:
		di=get_domain_index_ipc(gpmand, reqdata);
		if(di==-1) return 0;
		// for a specific domain and a apecific port
		if(di>=0 && reqdata->portIndex!=0){
			if(!PORT_DATA_EXIST(di, reqdata->portIndex)) return -1;
			return ipc_respond_one_gport(
				gpmand->gpnetd,
				gpmand->tasds[di].ptds[reqdata->portIndex].ppglb,
				gpmand->tasds[di].ptds[reqdata->portIndex].bppglb,
				gpmand->tasds[di].tasglb,
				gpmand->tasds[di].btasglb,
				reqdata->portIndex,
				reqdata->domainNumber, di, addr);
		}
		ppi=di;
		for(di=0;di<gpmand->max_domains;di++){
			if(ppi>=0 && ppi!=di) continue; // skip for a specific domain
			for(pi=1;pi<gpmand->max_ports;pi++){
				if(!PORT_DATA_EXIST(di,pi)) continue;
				ipc_respond_one_gport(
					gpmand->gpnetd,
					gpmand->tasds[di].ptds[pi].ppglb,
					gpmand->tasds[di].ptds[pi].bppglb,
					gpmand->tasds[di].tasglb,
					gpmand->tasds[di].btasglb,
					pi, gpmand->tasds[di].tasglb->domainNumber, di,
					addr);
			}
		}
		return 0;
	case GPTPIPC_CMD_ACTIVE_DOMAINT_SWITCH:
		gptpclock_active_domain_switch(reqdata->domainIndex);
		return 0;
	case GPTPIPC_CMD_RUN_EXT_SCRIPT:
		// use reqdata->domainNumber as single argument
		run_ext_script(&gpmand->extcmdstdin, reqdata->domainNumber);
		return 0;
	case GPTPIPC_CMD_TSN_SCHEDULE_CONTROL:
		// use reqdata->domainNumber
		// reqdata->domainNumber=0: to stop,
		// reqdata->domainNumber=1: to start
		aligntime=gptpconf_get_intitem(CONF_TSN_SCHEDULE_ALIGNTIME);
		if(reqdata->domainNumber==0)
			cycletime=0;
		else
			cycletime=gptpconf_get_intitem(CONF_TSN_SCHEDULE_CYCLETIME);
		gptpnet_tsn_schedule(gpmand->gpnetd, aligntime, cycletime);
		return 0;
	case GPTPIPC_CMD_REQ_STAT_INFO_RESET:
		resetcmd=1;
		// fall through
	case GPTPIPC_CMD_REQ_STAT_INFO:
		ddi=get_domain_index_ipc(gpmand, reqdata);
		if(reqdata->domainNumber>=0 && ddi<0) return -1;
		ppi=reqdata->portIndex;
		for(di=0;di<gpmand->max_domains;di++){
			if(ddi>=0 && ddi!=di) continue;
			for(pi=1;pi<gpmand->max_ports;pi++){
				if(ppi>0 && ppi!=pi) continue;
				if(di==0) ipc_respond_statsd_info(gpmand, pi, resetcmd,
								  addr);
				ipc_respond_stattd_info(gpmand, di, pi, resetcmd,
							addr);
			}
		}
		return 0;
	case GPTPIPC_CMD_REG_ABNORMAL_EVENT:
		return ipc_register_abnormal_event(reqdata);
	default:
		return -1;
	}
}

static int stopgptp;
static void signal_handler(int sig)
{
	stopgptp=1;
}

static int domain_zero_port_sm_init(gptpsm_tasd_t *tasd, gptpnet_data_t *gpnetd, int pi)
{
	md_pdelay_req_sm_init(&tasd->ptds[pi].mdpdreqd, pi, gpnetd,
			      tasd->tasglb, tasd->ptds[pi].ppglb, tasd->ptds[pi].mdeglb);
	md_pdelay_resp_sm_init(&tasd->ptds[pi].mdpdrespd, pi, gpnetd,
			       tasd->tasglb, tasd->ptds[pi].ppglb);
	link_delay_interval_setting_sm_init(&tasd->ptds[pi].ldisetd, pi,
					    tasd->tasglb, tasd->ptds[pi].ppglb,
					    tasd->ptds[pi].mdeglb);
	return 0;
}

static int domain_port_sm_init(gptpsm_tasd_t *tasd, gptpnet_data_t *gpnetd, int di, int pi)
{
	port_announce_information_sm_init(&tasd->ptds[pi].painfd, di, pi,
			tasd->tasglb, tasd->ptds[pi].ppglb,
			tasd->btasglb, tasd->ptds[pi].bppglb);
	port_announce_information_ext_sm_init(&tasd->ptds[pi].paiextd, di, pi,
			tasd->tasglb, tasd->ptds[pi].ppglb,
			tasd->btasglb, tasd->ptds[pi].bppglb);
	if(pi==0) return 0;
	md_sync_receive_sm_init(&tasd->ptds[pi].mdsrecd,di, pi,	tasd->tasglb,
			tasd->ptds[pi].ppglb, tasd->ptds[pi].mdeglb);
	port_sync_sync_receive_sm_init(&tasd->ptds[pi].pssrecd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	port_sync_sync_send_sm_init(&tasd->ptds[pi].psssendd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	md_sync_send_sm_init(&tasd->ptds[pi].mdssendd, di, pi, gpnetd, tasd->tasglb,
			tasd->ptds[pi].ppglb, tasd->ptds[pi].mdeglb);
	port_announce_receive_sm_init(&tasd->ptds[pi].parecd, di, pi,
			tasd->tasglb, tasd->ptds[pi].ppglb,
			tasd->btasglb, tasd->ptds[pi].bppglb);
	port_announce_transmit_sm_init(&tasd->ptds[pi].patransd, di, pi,
			tasd->tasglb, tasd->ptds[pi].ppglb,
			tasd->btasglb, tasd->ptds[pi].bppglb);
	gptp_capable_transmit_sm_init(&tasd->ptds[pi].gctransd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	gptp_capable_receive_sm_init(&tasd->ptds[pi].gcrecd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	sync_interval_setting_sm_init(&tasd->ptds[pi].sisetd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	announce_interval_setting_sm_init(&tasd->ptds[pi].aisetd, di, pi,
			tasd->tasglb, tasd->ptds[pi].ppglb,
			tasd->ptds[pi].bppglb);
	one_step_tx_oper_setting_sm_init(&tasd->ptds[pi].ostxopd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb, tasd->ptds[pi].mdeglb);
	md_announce_send_sm_init(&tasd->ptds[pi].mdansendd, di, pi, gpnetd, tasd->tasglb,
			tasd->ptds[pi].ppglb, tasd->ptds[pi].bppglb);
	md_announce_receive_sm_init(&tasd->ptds[pi].mdanrecd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	md_signaling_send_sm_init(&tasd->ptds[pi].mdsigsendd, di, pi, gpnetd, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	md_signaling_receive_sm_init(&tasd->ptds[pi].mdsigrecd, di, pi, tasd->tasglb,
			tasd->ptds[pi].ppglb);
	return 0;
}

static int gptpman_port_init(gptpman_data_t *gpmand, uint8_t di, int pi)
{
	gptpsm_tasd_t *tasd=&gpmand->tasds[di];
	if(di==0){
		md_entity_glb_init(&tasd->ptds[pi].mdeglb, NULL);
		pp_glb_init(&tasd->ptds[pi].ppglb, NULL, pi);
	}else{
		md_entity_glb_init(&tasd->ptds[pi].mdeglb,
				   gpmand->tasds[0].ptds[pi].mdeglb->forAllDomain);
		pp_glb_init(&tasd->ptds[pi].ppglb,
			    gpmand->tasds[0].ptds[pi].ppglb->forAllDomain, pi);
	}
	bmcs_pp_glb_init(&tasd->ptds[pi].bppglb);
	if(di==0){
		domain_zero_port_sm_init(tasd, gpmand->gpnetd, pi);
	}
	domain_port_sm_init(tasd, gpmand->gpnetd, di, pi);
	if(pi!=0){
		port_state_setting_ext_sm_init(&gpmand->tasds[di].ptds[pi].pssextd, di, pi,
					       gpmand->tasds[di].tasglb, gpmand->tasds[di].ppglbl,
					       gpmand->tasds[di].btasglb, gpmand->tasds[di].bppglbl,
					       gpmand->max_ports,
					       &gpmand->tasds[0].ptds[pi].pssextd);
	}

	tasd->ptds[pi].cmldsLinkPortEnabled = true;
	tasd->ppglbl[pi] = tasd->ptds[pi].ppglb;
	tasd->bppglbl[pi] = tasd->ptds[pi].bppglb;
	return 0;
}

/* allocate data in gptpsm_tasd_t */
int gptpman_domain_init(gptpman_data_t *gpmand, uint8_t domainNumber)
{
	int di;
	int pi;

	if(domainNumber==0){
		di=0;
	}else{
		for(di=1;di<gpmand->max_domains;di++){
			if(gpmand->tasds[di].tasglb &&
			   gpmand->tasds[di].tasglb->domainNumber == domainNumber){
				UB_LOG(UBL_ERROR,"%s:data of domainNumber=%d already exists\n",
				       __func__, domainNumber);
				return -1;
			}
			if(!gpmand->tasds[di].tasglb) break;
		}
	}
	if(di>=gpmand->max_domains){
		UB_LOG(UBL_ERROR,"%s:no space for a new domain\n", __func__);
		return -1;
	}
	ptas_glb_init(&gpmand->tasds[di].tasglb, domainNumber);

	bmcs_ptas_glb_init(&gpmand->tasds[di].btasglb, gpmand->tasds[di].tasglb);

	site_sync_sync_sm_init(&gpmand->tasds[di].sssd, di, gpmand->tasds[di].tasglb);
	clock_master_sync_send_sm_init(&gpmand->tasds[di].cmssendd, di, gpmand->tasds[di].tasglb);
	clock_slave_sync_sm_init(&gpmand->tasds[di].cssd, di, gpmand->tasds[di].tasglb);
	clock_master_sync_receive_sm_init(&gpmand->tasds[di].cmsrecd, di,
					  gpmand->tasds[di].tasglb);
	clock_master_sync_offset_sm_init(&gpmand->tasds[di].cmsoffsetd, di,
					 gpmand->tasds[di].tasglb);

	for(pi=0;pi<gpmand->max_ports;pi++){
		gptpman_port_init(gpmand, di, pi);
	}

	gm_stable_sm_init(&gpmand->tasds[di].gmsd, di, gpmand->tasds[di].tasglb);
	port_state_selection_sm_init(&gpmand->tasds[di].pssd, di,
			gpmand->tasds[di].tasglb, gpmand->tasds[di].ppglbl,
			gpmand->tasds[di].btasglb, gpmand->tasds[di].bppglbl,
			gpmand->max_ports,
			&gpmand->tasds[0].pssd);
	return 0;
}

static int all_sm_close(gptpman_data_t *gpmand)
{
	int di, pi;
	for(di=0;di<gpmand->max_domains;di++){
		for(pi=0;pi<gpmand->max_ports;pi++){
			if(gpmand->tasds[di].ptds && gpmand->tasds[di].ptds[pi].bppglb){
				bmcs_pp_glb_close(&gpmand->tasds[di].ptds[pi].bppglb);
				pp_glb_close(&gpmand->tasds[di].ptds[pi].ppglb, di);
				md_entity_glb_close(&gpmand->tasds[di].ptds[pi].mdeglb, di);
			}
			sm_close_for_domain_port(gpmand, di, pi);
		}
		SM_CLOSE(site_sync_sync_sm_close, gpmand->tasds[di].sssd);
		SM_CLOSE(clock_master_sync_send_sm_close, gpmand->tasds[di].cmssendd);
		SM_CLOSE(clock_slave_sync_sm_close, gpmand->tasds[di].cssd);
		SM_CLOSE(clock_master_sync_receive_sm_close, gpmand->tasds[di].cmsrecd);
		SM_CLOSE(clock_master_sync_offset_sm_close, gpmand->tasds[di].cmsoffsetd);
		SM_CLOSE(gm_stable_sm_close, gpmand->tasds[di].gmsd);
		SM_CLOSE(port_state_selection_sm_close, gpmand->tasds[di].pssd);

		if(gpmand->tasds[di].ptds) free(gpmand->tasds[di].ptds);
		if(gpmand->tasds[di].ppglbl) free(gpmand->tasds[di].ppglbl);
		if(gpmand->tasds[di].bppglbl) free(gpmand->tasds[di].bppglbl);
		ptas_glb_close(&gpmand->tasds[di].tasglb);
		bmcs_ptas_glb_close(&gpmand->tasds[di].btasglb);

	}
	return 0;
}

static int gptpman_close(gptpman_data_t *gpmand)
{
	free(gpmand->tasds);
	free(gpmand);
	return 0;
}

static int init_domain_clock(gptpman_data_t *gpmand, int domainIndex, int domainNumber,
			     int thisClockIndex)
{
	int i;
	struct timespec ts;
	char *ptpdev;
	ClockIdentity clkid;
	int num_clocks;
	// clocks are added for the domain
	memset(&ts, 0, sizeof(ts));
	num_clocks=gptpconf_get_intitem(CONF_SINGLE_CLOCK_MODE)?2:gpmand->max_ports;
	for(i=1;i<num_clocks;i++){
		// for domainIndex!=0, add only thisClock
		if(domainIndex && i!=thisClockIndex) continue;
		// for domainIndex==0, add clocks for all ports
		ptpdev=gptpnet_ptpdev(gpmand->gpnetd, i-1);
		// actually every netdevice has assigned ptpdevice, done in gptpnet_init
		if(!ptpdev[0]) continue;
		gptpnet_create_clockid(gpmand->gpnetd, clkid, i-1, domainNumber);
		if(gptpclock_add_clock(i, ptpdev, domainIndex, domainNumber, clkid)){
			UB_LOG(UBL_ERROR,"%s:clock can't be added, ptpdev=%s\n",
			       __func__, ptpdev);
			return -1;
		}
	}

	/* add the master clock for the domainIndex.
	   get ptpdev from the network device of thisClockIndex. */
	gptpnet_create_clockid(gpmand->gpnetd, clkid, thisClockIndex-1, domainNumber);

	ptpdev=gptpnet_ptpdev(gpmand->gpnetd, thisClockIndex-1);
	if(gptpclock_add_clock(0, ptpdev, domainIndex, domainNumber, clkid)){
		UB_LOG(UBL_ERROR,"%s:master clock can't be added, ptpdev=%s\n",
		       __func__, ptpdev);
		return -1;
	}
	return 0;
}

static int set_domain_thisClock(PerTimeAwareSystemGlobal *tasglb, int domainNumber,
				int thisClockIndex)
{
	uint8_t *clockid;
	tasglb->thisClockIndex=thisClockIndex;
	gptpclock_set_thisClock(tasglb->thisClockIndex, domainNumber, false);

	/* initialize adjustment rate of the master clock to 0 */
	gptpclock_setadj(0, thisClockIndex, domainNumber);

	clockid=gptpclock_clockid(tasglb->thisClockIndex, domainNumber);
	if(!clockid) return -1;
	memcpy(tasglb->thisClock, clockid, sizeof(ClockIdentity));
	return 0;
}

static int static_domains_init(gptpman_data_t *gpmand, char *inittm)
{
	/* for domainIndex=0 and domainNumber=0, initialize here.
	   For other domains, it will be done at a receive
	   or an action to create a new domain */
	int this_ci;
	int di, dn;

	di=0; // domainIndex=0
	dn=0; // domainNumber=0
	this_ci=gptpconf_get_intitem(CONF_FIRST_DOMAIN_THIS_CLOCK);
	if(gptpman_domain_init(gpmand, dn)) return -1;
	if(init_domain_clock(gpmand, di, dn, this_ci) ){
		UB_LOG(UBL_ERROR,"%s:domain clock init failed, di=%d dn=%d\n",
		       __func__, di, dn);
		return -1;
	}
	set_domain_thisClock(gpmand->tasds[di].tasglb, dn, this_ci);
	if(inittm){
		gptpclock_setadj(0, this_ci, dn); // Freq. adj=0
		gptpclock_settime_str(inittm, 0, dn); // Phase set to inittm
	}
	bmcs_ptas_glb_update(&gpmand->tasds[di].btasglb,
			     gpmand->tasds[di].tasglb, true);

	di=1;
	dn=gptpconf_get_intitem(CONF_SECOND_DOMAIN_NUMBER);
	this_ci=gptpconf_get_intitem(CONF_SECOND_DOMAIN_THIS_CLOCK);
	if(this_ci<0) return 0;
	if(gptpman_domain_init(gpmand, dn)) return -1;
	if(init_domain_clock(gpmand, di, dn, this_ci) ){
		UB_LOG(UBL_ERROR,"%s:domain clock init failed, di=%d dn=%d\n",
		       __func__, di, dn);
		return -1;
	}
	set_domain_thisClock(gpmand->tasds[di].tasglb, dn, this_ci);
	if(inittm){
		gptpclock_setadj(0, this_ci, dn); // Freq. adj=0
		gptpclock_settime_str(inittm, 0, dn); // Phase set to inittm
	}
	bmcs_ptas_glb_update(&gpmand->tasds[di].btasglb,
			     gpmand->tasds[di].tasglb, false);

	return 0;
}

int gptpman_run(char *netdevs[], int max_ports, int max_domains, char *inittm)
{
	int i, ports_limit;
	gptpman_data_t *gpmand;
	struct sigaction sigact;
	int res=-1;

	gpmand=malloc(sizeof(gptpman_data_t));
	ub_assert(gpmand!=NULL, __func__, "malloc error");
	memset(gpmand, 0, sizeof(gptpman_data_t));

	// provide seed to pseudo random generator rand()
	srand(time(NULL));

	if(!max_domains) max_domains=gptpconf_get_intitem(CONF_MAX_DOMAIN_NUMBER);
	gpmand->max_domains=max_domains;
	ports_limit=gptpconf_get_intitem(CONF_MAX_PORT_NUMBER);
	gptpclock_init(gpmand->max_domains, ports_limit);

	/* a network device which has master ptpdev becomes the first device,
	   so that clockIndex=1 is safe to use for thisClockIndex */
	gpmand->gpnetd=gptpnet_init(gptpnet_cb, gptpnet_ipc_cb, gpmand, netdevs, &max_ports,
				    gptpconf_get_item(CONF_MASTER_PTPDEV));
	if(!gpmand->gpnetd) goto erexit;
	// increment max_ports to add the ClockMaster port as port=0
	if(++max_ports>=MAX_PORT_NUMBER_LIMIT){
		UB_LOG(UBL_ERROR, "%s:max_ports number is too big, must be less than %d\n",
		       __func__,MAX_PORT_NUMBER_LIMIT);
		goto erexit;
	}

	gpmand->max_ports=max_ports;

	gpmand->tasds=malloc(max_domains * sizeof(gptpsm_tasd_t));
	ub_assert(gpmand->tasds, __func__, "malloc error");
	memset(gpmand->tasds, 0, max_domains * sizeof(gptpsm_tasd_t));

	// reserve ptds(port data array) for the number of domain
	for(i=0;i<max_domains;i++){
		gpmand->tasds[i].ptds=malloc(max_ports * sizeof(gptpsm_ptd_t));
		ub_assert(gpmand->tasds[i].ptds, __func__, "malloc error");
		memset(gpmand->tasds[i].ptds, 0, max_ports * sizeof(gptpsm_ptd_t));
		// create per-port global lists
		gpmand->tasds[i].ppglbl=malloc(max_ports * sizeof(PerPortGlobal*));
		ub_assert(gpmand->tasds[i].ppglbl, __func__, "malloc error");
		memset(gpmand->tasds[i].ppglbl, 0, max_ports * sizeof(PerPortGlobal*));
		gpmand->tasds[i].bppglbl=malloc(max_ports * sizeof(BmcsPerPortGlobal*));
		ub_assert(gpmand->tasds[i].bppglbl, __func__, "malloc error");
		memset(gpmand->tasds[i].bppglbl, 0, max_ports * sizeof(BmcsPerPortGlobal*));
	}

	if(static_domains_init(gpmand, inittm)) goto erexit;

	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler=signal_handler;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	if(gptpnet_activate(gpmand->gpnetd)) goto erexit;
	if(gptpconf_get_intitem(CONF_ACTIVATE_ABNORMAL_HOOKS)) md_abnormal_init();
	GPTP_READY_NOTICE;
	gptpnet_eventloop(gpmand->gpnetd, &stopgptp);
	all_sm_close(gpmand);
	md_abnormal_close();
	res=0;
erexit:
	if(gpmand->gpnetd) gptpnet_close(gpmand->gpnetd);
	gptpclock_close();
	gptpman_close(gpmand);
	return res;
}
