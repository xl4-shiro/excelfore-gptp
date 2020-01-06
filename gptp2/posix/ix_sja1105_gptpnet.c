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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <sys/un.h>
#include <linux/filter.h>
#include <linux/spi/spidev.h>
#include <fcntl.h>
#include <linux/ethtool.h>
#include "gptpnet.h"
#include "gptpclock.h"
#include "mdeth.h"

#define DEFAULT_SJA1105_PRIMARY_CPU_PORT 2
#define DEFAULT_SJA1105_CASCADE_PORT 4
#define DEFAULT_SJA1105_NUM_CASC 2
#define DEFAULT_SJA1105_NUM_DPORTS 5
#define DEFAULT_SJA1105_NUM_TPORTS ((DEFAULT_SJA1105_NUM_DPORTS-2)*DEFAULT_SJA1105_NUM_CASC+1)
#define DEFAULT_SJA1105_SPI_DEVICES {"/dev/spidev0.1","/dev/spidev0.0"}
#define DEFAULT_SJA1105_REC_QUEUES (DEFAULT_SJA1105_NUM_TPORTS*2)

#define GPTPNET_FRAME_SIZE (GPTP_MAX_PACKET_SIZE+sizeof(CB_ETHHDR_T))
#define GPTPD_IPC_MAX_CONNECTIONS 8

#define PTPCTRL_CORRCLK4TS_BIT (1<<2)
#define PTPCTRL_RESPTP (1<<3)
#define PTPCTRL_CASSYNC (1<<26)
#define PTPCTRL_PTPSTOPSCH (1<<29)
#define PTPCTRL_PTPSTRTSCH (1<<30)
#define PTPCTRL_VALID (1<<31)

static gptpnet_data_t *gpnetg;

static int config_spidev (int fd, int mode, int lsb, int bits, uint32_t speed);
static int spiOpen(const char* spidev);
static void spiClose(int spifd);
static size_t spiTransfer(int spifd, uint8_t* obuf, size_t onum, uint8_t* ibuf, size_t inum);
static uint32_t RdReg32(int spifd, uint32_t addr);
static void WrReg32(int spifd, uint32_t addr, uint32_t wval);
static uint64_t ptpSysemTime(int spifd);
static uint64_t ptpCascSyncTime(int spifd);
static void setMgmtL2AdLup(int spifd, uint8_t port);

extern char *PTPMsgType_debug[];

typedef struct sendbuf {
	CB_ETHHDR_T ehd;
	uint8_t pdata[GPTP_MAX_PACKET_SIZE];
} __attribute__((packed)) sendbuf_t;

typedef struct recmsg_queue {
	bool used;
	unsigned char buf[GPTPNET_FRAME_SIZE];
	event_data_recv_t edtrecv;
}recmsg_queue_t;

typedef struct swport {
	sendbuf_t sbuf;
	event_data_netlink_t nlstatus;
	uint64_t txtslost_time;
	uint32_t rxbytes;
	recmsg_queue_t *recd;
} swport_t;

typedef struct txts_event_data {
	int ndevIndex;
	int64_t defertout;
	event_data_txts_t edtxts;
} txts_event_data_t;

typedef struct meta_ts_data {
	uint8_t swid;
	uint8_t sportid;
	uint32_t ts;
}meta_ts_data_t;

struct gptpnet_data {
	int portfd;
	ub_macaddr_t srcmac;
	int mtusize;
	int spifd[DEFAULT_SJA1105_NUM_CASC];
	int64_t clkdiff[DEFAULT_SJA1105_NUM_CASC];
	char ifname[IFNAMSIZ];
	struct sockaddr_ll addr;
	gptpnet_cb_t cb_func;
	void *cb_data;
	int num_ports;
	swport_t swports[DEFAULT_SJA1105_NUM_TPORTS];
	int64_t event_ts64;
	int ipcfd;
	struct sockaddr_un ipc_address[GPTPD_IPC_MAX_CONNECTIONS];
	int64_t next_tout64;
	ub_esarray_cstd_t *txts_events;
	recmsg_queue_t recmsg_queue[DEFAULT_SJA1105_REC_QUEUES];
	int64_t last_event_timeout;
	uint32_t last_event_aligned;
	bool next_tout_needcb;
};

static recmsg_queue_t *get_recmsg_queue(gptpnet_data_t *gpnet)
{
	int i;
	for(i=0;i<DEFAULT_SJA1105_REC_QUEUES;i++){
		if(!gpnet->recmsg_queue[i].used){
			gpnet->recmsg_queue[i].used=true;
			return &gpnet->recmsg_queue[i];
		}
	}
	UB_LOG(UBL_ERROR, "%s:no data is available in the queue, force to clean up\n", __func__);
	for(i=1;i<DEFAULT_SJA1105_REC_QUEUES;i++) gpnet->recmsg_queue[i].used=true;
	return &gpnet->recmsg_queue[0];
}

static void ret_recmsg_queue(recmsg_queue_t *rdata)
{
	if(!rdata->used){
		UB_LOG(UBL_ERROR, "%s:realeasing not used rdata\n", __func__);
	}
	rdata->used=false;
}

#include "ix_gptpnet_common.c"

/* get_ndevIndex, get_swid_sportid need to be changed by cascade connection topology */
static int get_ndevIndex(int swid, int sportid)
{
	if(swid >= DEFAULT_SJA1105_NUM_CASC) return -1;
	if(sportid >= DEFAULT_SJA1105_NUM_DPORTS) return -1;
	if((swid < DEFAULT_SJA1105_NUM_CASC-1) && sportid >= DEFAULT_SJA1105_NUM_DPORTS-1)
		return -1;
	if(swid==0){
		if(sportid==DEFAULT_SJA1105_PRIMARY_CPU_PORT) return -1;
		if(sportid<DEFAULT_SJA1105_PRIMARY_CPU_PORT) return sportid;
		return sportid-1;
	}
	return swid*(DEFAULT_SJA1105_NUM_DPORTS-2)+sportid;
}

static int get_swid_sportid(int ndevIndex, uint8_t *swid, uint8_t *sportid)
{
	if(ndevIndex >= DEFAULT_SJA1105_NUM_TPORTS) return -1;
	*swid = ndevIndex / (DEFAULT_SJA1105_NUM_DPORTS-2);
	*sportid = ndevIndex % (DEFAULT_SJA1105_NUM_DPORTS-2);
	if(*swid == 0 && *sportid >= DEFAULT_SJA1105_PRIMARY_CPU_PORT) *sportid+=1;
	if(*swid == DEFAULT_SJA1105_NUM_CASC){
		*swid=DEFAULT_SJA1105_NUM_CASC-1;
		*sportid=DEFAULT_SJA1105_NUM_DPORTS-2;
	}
	return 0;
}

int gptp_clock_gettime(int spifd, int64_t *tp)
{
	uint64_t ts64;
	ts64=ptpSysemTime(spifd);
	*tp=ts64;
	return 0;
}

int gptp_clock_settime(int spifd, const int64_t *ts)
{
	return -1;
}

static void reset_sw_clocks(gptpnet_data_t *gpnet)
{
	int i;
	for(i=0;i<DEFAULT_SJA1105_NUM_CASC;i++){
		WrReg32(gpnet->spifd[i], 0x18, PTPCTRL_VALID | PTPCTRL_RESPTP |
			PTPCTRL_CORRCLK4TS_BIT );
	}
}

static int sw_clock_diff_cassync(gptpnet_data_t *gpnet)
{
	int i;
	uint64_t ts;
	int64_t dts;

	// assume, spifd[0] is CASMASTER, and the others are NOT CASMASTER
	// 1st round
	WrReg32(gpnet->spifd[0], 0x18, PTPCTRL_VALID | PTPCTRL_CASSYNC | PTPCTRL_CORRCLK4TS_BIT);
	ts=ptpCascSyncTime(gpnet->spifd[0]);
	for(i=1;i<DEFAULT_SJA1105_NUM_CASC;i++){
		dts=ptpCascSyncTime(gpnet->spifd[i])-ts;
		gpnet->clkdiff[i]=dts;
	}
	// 2nd round
	WrReg32(gpnet->spifd[0], 0x18, PTPCTRL_VALID | PTPCTRL_CASSYNC | PTPCTRL_CORRCLK4TS_BIT);
	ts=ptpCascSyncTime(gpnet->spifd[0]);
	for(i=1;i<DEFAULT_SJA1105_NUM_CASC;i++){
		dts=ptpCascSyncTime(gpnet->spifd[i])-ts;
		if(gpnet->clkdiff[i]!=dts) {
			if(!gpnet->clkdiff[i] || !dts || abs(gpnet->clkdiff[i]-dts)>10)
				return -1;
			// small diff(<=10) is okay
		}
		gpnet->clkdiff[i]*=8; // it is 8nsec unit
	}
	UB_LOG(UBL_INFO, "%s:CASSYNC works\n", __func__);
	return 0;
}

static int sw_clock_diff_swmes(gptpnet_data_t *gpnet)
{
	int i,j;
	for(i=1;i<DEFAULT_SJA1105_NUM_CASC;i++){
		uint64_t ts0, ts1, ts2;
		int64_t td;
		int64_t tsum=0;
		int64_t tdmax=-1000000000;
		int64_t tdmin=1000000000;
		for(j=0;j<10;j++){
			ts0=ptpSysemTime(gpnet->spifd[0]);
			ts1=ptpSysemTime(gpnet->spifd[i]);
			ts2=ptpSysemTime(gpnet->spifd[0]);
			td=ts1-(ts0+ts2)/2;
			tsum+=td;
			if(tdmax<td) tdmax=td;
			if(tdmin>td) tdmin=td;
		}
		// sum of 8 valuse
		gpnet->clkdiff[i]=(tsum-tdmax-tdmin)/8;
	}
	return 0;
}

static void set_sw_clock_diffs(gptpnet_data_t *gpnet)
{
	int i;

	gpnet->clkdiff[0]=0;
	if(!sw_clock_diff_cassync(gpnet)) goto printdiff;
	UB_LOG(UBL_INFO, "%s:CASSYNC doesn't work, go to software measurement,"
	       " which adds 500usec level inaccuracy\n", __func__);
	sw_clock_diff_swmes(gpnet);
printdiff:
	for(i=1;i<DEFAULT_SJA1105_NUM_CASC;i++){
		UB_LOG(UBL_INFO, "  clkdiff[%d]=%"PRIi64" nsec \n", i, gpnet->clkdiff[i]);
	}
}

static int portfd_open(gptpnet_data_t *gpnet, char *netdev)
{
	const ub_macaddr_t mcastmac={0x01,0x80,0xC2,0x00,0x00,0x0F};
	/*
	  The filter values can be obtained by the next command.
	  sudo tcpdump -i eth0 '((ether dst 01:80:c2:00:00:0e) or (ether dst 01:80:c2:01:00:0e)
	  or (ether dst 01:80:c2:03:00:0e) or (ether dst 01:80:c2:00:01:0e)
	  or (ether dst 01:80:c2:01:01:0e) or (ether dst 01:80:c2:02:01:0e)
	  or (ether dst 01:80:c2:03:01:0e) or (ether dst 01:80:c2:00:00:0f))' -dd

	  This must be adjusted for the SJA1105 cascade connections.
	*/
	struct sock_filter ptp_filter [] = {
		{ 0x20, 0, 0, 0x00000002 },
		{ 0x15, 7, 0, 0xc200000e }, // swid=0, sportid=0
		{ 0x15, 6, 0, 0xc201000e }, // swid=0, sportid=1
		{ 0x15, 5, 0, 0xc203000e }, // swid=0, sportid=3
		{ 0x15, 4, 0, 0xc200010e }, // swid=1, sportid=0
		{ 0x15, 3, 0, 0xc201010e }, // swid=1, sportid=1
		{ 0x15, 2, 0, 0xc202010e }, // swid=1, sportid=2
		{ 0x15, 1, 0, 0xc203010e }, // swid=1, sportid=3
		{ 0x15, 0, 3, 0xc200000f }, // meta frames
		{ 0x28, 0, 0, 0x00000000 },
		{ 0x15, 0, 1, 0x00000180 },
		{ 0x6, 0, 0, 0x00040000 },
		{ 0x6, 0, 0, 0x00000000 },
	};
	struct sock_fprog fcode = {0};
	cb_rawsock_paras_t cbrawp;

	strncpy(gpnet->ifname, netdev, IFNAMSIZ);
	memset(&cbrawp, 0, sizeof(cbrawp));
	cbrawp.dev=gpnet->ifname;
	cbrawp.proto=ETH_P_ALL;
	if(cb_rawsock_open(&cbrawp, &gpnet->portfd, &gpnet->addr, &gpnet->mtusize,
			   gpnet->srcmac)){
		UB_LOG(UBL_ERROR, "%s:can't open portfd socket: %s\n", __func__, strerror(errno));
		return -1;
	}
	if(cb_reg_multicast_address(gpnet->portfd, netdev, mcastmac, 0)) {
		UB_LOG(UBL_ERROR,"%s:failed to add multicast address\n", __func__);
		goto erexit;
	}
	fcode.len = sizeof(ptp_filter) / sizeof(struct sock_filter);
	fcode.filter = &ptp_filter[0];
	if(setsockopt(gpnet->portfd, SOL_SOCKET, SO_ATTACH_FILTER, &fcode, sizeof(fcode))){
		UB_LOG(UBL_ERROR,"%s:error in setsockopt: %s\n", __func__, strerror(errno));
		goto erexit;
	}
	return 0;
erexit:
	close(gpnet->portfd);
	gpnet->portfd=0;
	return -1;
}

static int netdev_init(gptpnet_data_t *gpnet, char *netdev)
{
	int i;
	const char *spi_devices[]=DEFAULT_SJA1105_SPI_DEVICES;

	if(portfd_open(gpnet, netdev)) return -1;

	for(i=0;i<DEFAULT_SJA1105_NUM_CASC;i++){
		gpnet->spifd[i]=spiOpen(spi_devices[i]);
		UB_LOG(UBL_DEBUG, "SJA1105 Device ID=0x%x\n", RdReg32(gpnet->spifd[i],0));
	}
	reset_sw_clocks(gpnet);
	set_sw_clock_diffs(gpnet);
	return 0;
}

static int oneport_init(gptpnet_data_t *gpnet, swport_t *swport, int swid, int sportid)
{
	ub_macaddr_t destmac = GPTP_MULTICAST_DEST_ADDR;
	uint8_t pide[2];
	swport->rxbytes=RdReg32(gpnet->spifd[swid],0x404+sportid*0x10);
	swport->txtslost_time = gptpconf_get_intitem(CONF_TXTS_LOST_TIME);

	memcpy(swport->sbuf.ehd.H_SOURCE, gpnet->srcmac, ETH_ALEN);
	memcpy(swport->sbuf.ehd.H_DEST, destmac, ETH_ALEN);
	swport->sbuf.ehd.H_PROTO = htons(ETH_P_1588);
	destmac[4]=swid;
	destmac[3]=sportid;
	if(cb_reg_multicast_address(gpnet->portfd, gpnet->ifname, destmac, 0)) {
		UB_LOG(UBL_ERROR,"failed to add multicast address");
		return -1;
	}
	if(!swid && !sportid){
		pide[0]=0xFF;
		pide[1]=0xFE;
	}else{
		pide[0]=swid;
		pide[1]=sportid;
	}
	eui48to64(gpnet->srcmac, swport->nlstatus.portid, pide);
	sprintf(swport->nlstatus.devname, "s%dp%d", swid, sportid);
	// there is no ptp device, set a dummy name which has spifd
	sprintf(swport->nlstatus.ptpdev, "ptpspi%d", gpnet->spifd[swid]);
	return 0;
}

static bool mii_status(gptpnet_data_t *gpnet, int ndevIndex)
{
	uint32_t rxb;
	uint8_t swid=0;
	uint8_t sportid=0;
	swport_t *swport;

	/* ??? need to check phy status */

	swport=&gpnet->swports[ndevIndex];
	get_swid_sportid(ndevIndex, &swid, &sportid);

	rxb=RdReg32(gpnet->spifd[swid],0x100900+sportid);
	rxb=(rxb>>3)&0x3;
	if(rxb==0){
		swport->nlstatus.speed = 10;
	}else if(rxb==1){
		swport->nlstatus.speed = 100;
	}else{
		swport->nlstatus.speed = 1000;
	}
	swport->nlstatus.duplex = DUPLEX_FULL;

	rxb=RdReg32(gpnet->spifd[swid],0x404+sportid*0x10);
	if(rxb!=swport->rxbytes){
		swport->nlstatus.up = true;
		swport->rxbytes=rxb;
		UB_LOG(UBL_INFO, "%s:UP ndevIndex=%d, speed=%d\n",
		       __func__, ndevIndex, swport->nlstatus.speed);
		return true;
	}
	return false;
}

static int check_dup_update(gptpnet_data_t *gpnet)
{
	int i;
	swport_t *swport;
	bool up;
	for(i=0;i<DEFAULT_SJA1105_NUM_TPORTS;i++){
		swport=&gpnet->swports[i];
		up=swport->nlstatus.up;
		if(!up && mii_status(gpnet, i)){
			gpnet->cb_func(gpnet->cb_data, i+1, GPTPNET_EVENT_DEVUP,
				       &gpnet->event_ts64, &swport->nlstatus);
		}
	}
	return 0;
}

static int onenet_activate(gptpnet_data_t *gpnet, int ndevIndex)
{
	swport_t *swport;
	bool dup;
	swport=&gpnet->swports[ndevIndex];
	dup=mii_status(gpnet, ndevIndex);
	if(!dup || !gpnet->cb_func || !swport->nlstatus.up) return 0;
	return gpnet->cb_func(gpnet->cb_data, ndevIndex+1, GPTPNET_EVENT_DEVUP,
			      &gpnet->event_ts64, &swport->nlstatus);
}

/* read the clock in SJA1105 */
static int ptp_clock_readreg(int spifd, uint32_t *clocklsb, uint32_t *clockmsb)
{
	// to use PTPTSCLK read from (0x1C, 0x1D)
	// to use PTPCLK(rate adjusted clock) read from (0x19, 0x1A)
	// msb is latched when lsb is read
	*clocklsb = RdReg32(spifd, 0x19);
	*clockmsb = RdReg32(spifd, 0x1A);
	return 0;
}

static uint64_t ptpSysemTime(int spifd)
{
	uint32_t clocklsb, clockmsb;
	ptp_clock_readreg(spifd, &clocklsb, &clockmsb);
	// the clock is 8nsec unit, shit 3 bits to convert to 1nsec unit
	return (((uint64_t)clockmsb << 32) | clocklsb)<<3;
}

/* 1 unit is 8nsec */
static uint64_t extend_ts2rt(gptpnet_data_t *gpnet, int swid, uint32_t ts)
{
	uint32_t clocklsb, clockmsb;
	ptp_clock_readreg(gpnet->spifd[swid], &clocklsb, &clockmsb);
	if(clocklsb < ts){
		return (((((uint64_t)clockmsb-1)<<32)|ts)<<3) - gpnet->clkdiff[swid];
	}
	return ((((uint64_t)clockmsb<<32)|ts)<<3) - gpnet->clkdiff[swid];
}

static int read_meta_data(uint8_t *data, meta_ts_data_t *metad, int *dvi)
{
	uint8_t *sad=data+6;

	if(UB_NON_ZERO_B6(sad)){
		int msgtype;
		char *tmsg="unknow";
		msgtype=PTP_HEAD_MSGTYPE(data+sizeof(CB_ETHHDR_T));
		if(msgtype<=15)	tmsg=PTPMsgType_debug[msgtype];
		UB_LOG(UBL_DEBUG, "%s, src="UB_PRIhexB6", msgtype=%s\n",
		       "not meta frame", UB_ARRAY_B6(sad), tmsg);
		return -1;
	}
	metad->sportid=data[20];
	metad->swid=data[21];
	if(data[18]||data[19]) {
		UB_LOG(UBL_ERROR,"%s:MAC Dest. b2:b1==%02X:%02X\n",__func__, data[18], data[19]);
		return -1;
	}
	if((*dvi=get_ndevIndex(metad->swid, metad->sportid))<0){
		UB_LOG(UBL_ERROR,"%s:invalid swid=%d, sportid=%d\n", __func__,
		       metad->swid, metad->sportid);
		return -1;
	}
	metad->ts=(data[14]<<24|data[15]<<16|data[16]<<8|data[17])&0xffffffff;
	return 0;
}

static uint64_t read_egress_timestamp(gptpnet_data_t *gpnet, uint8_t swid, uint8_t sportid)
{
	uint32_t reg0, reg1;
	uint32_t ts;

	reg0 = 0xC0 + (4 * sportid);
	reg1 = 0xC0 + (4 * sportid) + 1;
	if(!RdReg32(gpnet->spifd[swid], reg0)) {
		UB_LOG(UBL_DEBUG, "%s: TxTS is not available, swid=%d, sportid=%d\n",
		       __func__, swid, sportid);
		return 0;
	}
	ts = RdReg32(gpnet->spifd[swid], reg1);
	return extend_ts2rt(gpnet, swid, ts);
}

static int read_msg_data(gptpnet_data_t *gpnet, recmsg_queue_t *rdata)
{
	uint8_t swid, sportid;
	int dvi;
	uint8_t *buf=rdata->buf;

	if(buf[12]!=0x88 || buf[13]!=0xf7){
		if(buf[12]==0 || buf[13]==0x08){
			return 1; // this is meta frame
		}else{
			UB_LOG(UBL_DEBUG, "%s:not expected ethtype=0x%02X%02X, swid=%d, portid=%d\n",
			       __func__, buf[12], buf[13], buf[4], buf[3]);
			return -1;
		}
	}

	rdata->edtrecv.recbptr=buf+ETH_HLEN;
	rdata->edtrecv.domain=PTP_HEAD_DOMAIN_NUMBER(buf+ETH_HLEN);
	rdata->edtrecv.msgtype=PTP_HEAD_MSGTYPE(buf+ETH_HLEN);

	swid=buf[4];
	sportid=buf[3];
	dvi=get_ndevIndex(swid, sportid);
	if(dvi<0){
		UB_LOG(UBL_ERROR,"%s:invalid mswid=%d, msportid=%d\n",
		       __func__, buf[4], buf[3]);
		return -1;
	}
	if(rdata->edtrecv.msgtype>=16){
		UB_LOG(UBL_ERROR,"%s:invalid msgtype=%d\n", __func__, rdata->edtrecv.msgtype);
		return -1;
	}

	if(gpnet->swports[dvi].recd) {
		UB_LOG(UBL_WARN,"%s:portIndex=%d, rec buffer is busy, overwrite on msg=%s\n",
		       __func__, dvi+1,
		       PTPMsgType_debug[gpnet->swports[dvi].recd->edtrecv.msgtype]);
		ret_recmsg_queue(gpnet->swports[dvi].recd);
	}

	gpnet->swports[dvi].recd = rdata;
	UB_LOG(UBL_DEBUGV,"%s:portIndex=%d, received msgtype=%s\n", __func__, dvi+1,
	       PTPMsgType_debug[rdata->edtrecv.msgtype]);
	return 0; // received one PTP message
}

static int read_recdata(gptpnet_data_t *gpnet, struct msghdr *msg, int len,
			recmsg_queue_t *rdata)
{
	uint8_t *buf=rdata->buf;
	int dvi;
	meta_ts_data_t metad={0,};
	uint64_t ets;
	event_data_recv_t *edtrecv;
	int res;

	if (msg->msg_flags & MSG_TRUNC) {
		UB_LOG(UBL_ERROR,"received truncated message\n");
		return -1;
	}
	if (msg->msg_flags & MSG_CTRUNC) {
		UB_LOG(UBL_ERROR,"received truncated ancillary data\n");
		return -1;
	}

	if((res=read_msg_data(gpnet, rdata))<=0) {
		if(res==0) return 1; // return 1 to keep rdata
		return res;
	}

	if(buf[5]!=0x0f){
		int msgtype=PTP_HEAD_MSGTYPE(buf+ETH_HLEN);
		UB_LOG(UBL_ERROR,"%s:received non-metadata, buf[5]=0x%02X\n",
		       __func__, buf[5]);
		if(buf[5]==0x0e && msgtype<16){
			UB_LOG(UBL_ERROR,"%s:received msgtype=%s\n",
			       __func__, PTPMsgType_debug[msgtype]);
		}
		return -1;
	}

	res=read_meta_data(buf, &metad, &dvi);
	if(res){
		UB_LOG(UBL_ERROR,"%s:swid=%d, sportid=%d can't read as a metaframe\n",
		       __func__, metad.swid, metad.sportid);
		return -1;
	}

	if(!gpnet->swports[dvi].recd){
		UB_LOG(UBL_DEBUG, "%s:not expected meta data, portIndex=%d\n",
		       __func__, dvi+1);
		return -1;
	}

	edtrecv=&gpnet->swports[dvi].recd->edtrecv;
	ets=extend_ts2rt(gpnet, metad.swid, metad.ts);
	edtrecv->ts64=ets;

	if(!gpnet->cb_func) {
		res=-1;
		goto erexit;
	}
	UB_LOG(UBL_DEBUGV,"%s:portIndex=%d, received metadata for %s, RxTS=%"PRIi64"nsec\n",
	       __func__, dvi+1, PTPMsgType_debug[edtrecv->msgtype],edtrecv->ts64);

	gpnet->cb_func(gpnet->cb_data, dvi+1, GPTPNET_EVENT_RECV,
		       &gpnet->event_ts64, edtrecv);
	res=0;
erexit:
	ret_recmsg_queue(gpnet->swports[dvi].recd);
	gpnet->swports[dvi].recd=NULL;
	return res;
}

static int read_netdev_event(gptpnet_data_t *gpnet)
{
	struct iovec vec[1];
	struct msghdr msg;
	int res;
	recmsg_queue_t *rdata=get_recmsg_queue(gpnet);

	if(!rdata) return -1;
	memset(&rdata->edtrecv, 0, sizeof(event_data_recv_t));
	vec[0].iov_base = rdata->buf;
	vec[0].iov_len = sizeof(rdata->buf);
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;

	res = CB_SOCK_RECVMSG(gpnet->portfd, &msg, MSG_DONTWAIT);
	if(res>0){
		res=read_recdata(gpnet, &msg, res, rdata);
		if(res==1) return 0; // keep rdata
	}else if(res==0){
		UB_LOG(UBL_ERROR,"%s:recvmsg returned 0\n", __func__);
		res=-1;
	}else{
		if(errno==EAGAIN){
			res=1;
		}else{
			UB_LOG(UBL_ERROR,"%s:recvmsg error %s\n", __func__, strerror(errno));
		}
	}
	ret_recmsg_queue(rdata);
	return res;
}

static int gptpnet_check_deferred(gptpnet_data_t *gpnet, int ndevIndex)
{
	int i, en;
	txts_event_data_t *txtsevt;
	en=ub_esarray_ele_nums(gpnet->txts_events);
	for(i=0;i<en;i++){
		txtsevt=(txts_event_data_t *)ub_esarray_get_ele(gpnet->txts_events, i);
		if(txtsevt->ndevIndex==ndevIndex &&
		   txtsevt->defertout!=0) return 1;
	}
	return 0;
}

static int gptpnet_proc_deferred_txts(gptpnet_data_t *gpnet, int64_t *ts)
{
	txts_event_data_t txtsevt;
	int i, en;
	uint8_t swid=0, sportid=0;
	bool callcb=false;
	int64_t tstout;
	uint64_t ts64;

	en=ub_esarray_ele_nums(gpnet->txts_events);
	for(i=0;i<en;i++){
		if(ub_esarray_pop_ele(gpnet->txts_events, (ub_esarray_element_t *)&txtsevt)) return 0;
		if(txtsevt.defertout==0){
			callcb=true;
			break;
		}
		get_swid_sportid(txtsevt.ndevIndex, &swid, &sportid);
		ts64=read_egress_timestamp(gpnet, swid, sportid);
		if(ts64) {
			txtsevt.edtxts.ts64=ts64;
			callcb=true;
			UB_TLOG(UBL_DEBUG, "%s:portIndex=%d, process deferred TxTS event\n",
				 __func__, txtsevt.ndevIndex+1);
			break;
		}
		// TxTS is not yet available, check timeout
		tstout=txtsevt.defertout-*ts;
		if(tstout<0){
			UB_LOG(UBL_ERROR,
			       "portIndex=%d, missed TxTS for msgtype=%s\n",
			       txtsevt.ndevIndex,
			       PTPMsgType_debug[txtsevt.edtxts.msgtype]);
			continue;
		}
		// not yet timed out, push back into the queue
		ub_esarray_add_ele(gpnet->txts_events, (ub_esarray_element_t *)&txtsevt);
		gptpnet_extra_timeout(gpnet, 5000000); // next timeout happens in 5msec
		gpnet->next_tout_needcb=false;
		return 0;
	}
	if(callcb){
		// one call back in one loop, and this is returned
		gpnet->cb_func(gpnet->cb_data, txtsevt.ndevIndex+1, GPTPNET_EVENT_TXTS,
			       ts, &txtsevt.edtxts);
		UB_LOG(UBL_DEBUGV, "%s:done with callback\n",__func__);
		gptpnet_extra_timeout(gpnet, 1000000); // next timeout happens in 1msec
		gpnet->next_tout_needcb=true;
		return 0;
	}
	UB_LOG(UBL_DEBUGV, "%s:done without callback\n",__func__);
	return 0;
}

#define GPTPNET_INTERVAL_TIMEOUT 25000000
static int cb_timeout_call(gptpnet_data_t *gpnet, int64_t *ts)
{
	gpnet->last_event_timeout=*ts;
	gpnet->last_event_aligned=((*ts%UB_SEC_NS)/GPTPNET_INTERVAL_TIMEOUT) *
		GPTPNET_INTERVAL_TIMEOUT;
	gpnet->next_tout_needcb=false;
	gpnet->next_tout64=0;
	gpnet->cb_func(gpnet->cb_data, 0, GPTPNET_EVENT_TIMEOUT, ts, NULL);
	return 0;
}

static int gptpnet_catch_event(gptpnet_data_t *gpnet)
{
	fd_set rfds;
	int maxfd=0;
	int64_t ts64, tstout64;
	struct timeval tvtout;
	int res=0;
	static int64_t last_ts64=0;

	ts64=ub_mt_gettime64();
	tstout64=ts64-last_ts64;
	if(tstout64>=0){
		// every 10 seconds, print clock parameters for debug
		if(tstout64>10*UB_SEC_NS){
			gptpclock_print_clkpara(UBL_DEBUG);
			last_ts64=ts64;
		}
	}


	gptpnet_proc_deferred_txts(gpnet, &ts64);

	FD_ZERO(&rfds);
	if(gpnet->portfd) FD_SET(gpnet->portfd, &rfds);
	maxfd=UB_MAX(maxfd, gpnet->portfd);
	if(gpnet->ipcfd){
		FD_SET(gpnet->ipcfd, &rfds);
		maxfd=UB_MAX(maxfd, gpnet->ipcfd);
	}

	if(gpnet->next_tout64){
		tstout64=gpnet->next_tout64-ts64;
		if(tstout64<0){
			// 'next_tout64' passed already, call the callback
			UB_LOG(UBL_DEBUG,"%s:call missed or extra TIMEOUT CB\n", __func__);
			if(!gpnet->next_tout_needcb) {
				gpnet->next_tout64=0;
				return 0;
			}
			return cb_timeout_call(gpnet, &ts64);
		}
		// 'tstout' is used for the next timeout, without aligning
	} else {
		// tstout = ts(current time) - last_event_timeout
		tstout64=ts64-gpnet->last_event_timeout;
		if(tstout64<0){
			// negative tstout shouldn't happen
			UB_LOG(UBL_WARN, "%s:ts < last_event_timeout\n", __func__);
			gpnet->last_event_timeout=ts64;
		}else{
			if(tstout64>=GPTPNET_INTERVAL_TIMEOUT){
				// the interval time passed already, call the callback
				return cb_timeout_call(gpnet, &ts64);
			}
		}

        tstout64 = (gpnet->last_event_timeout/UB_SEC_NS)*UB_SEC_NS;
		tstout64 += gpnet->last_event_aligned  + GPTPNET_INTERVAL_TIMEOUT;
		// the next timeout point is 'tstout'
		gpnet->next_tout64=tstout64;
		tstout64=tstout64-ts64;
		if(tstout64<0){
			// the next aligned time point passed already, call the callback
			return cb_timeout_call(gpnet, &ts64);
		}
	}
	UB_NSEC2TV(tstout64, tvtout);
	res=select(maxfd+1, &rfds, NULL, NULL, &tvtout);
	if(res == -1){
		UB_LOG(UBL_ERROR,"%s:select error %s\n", __func__, strerror(errno));
		return -1;
	}
	ts64=ub_mt_gettime64();
	gpnet->event_ts64=ts64;

	if(res == 0){
		if(!gpnet->cb_func) return -1;
		check_dup_update(gpnet);
		return cb_timeout_call(gpnet, &ts64);
	}
	if(FD_ISSET(gpnet->portfd, &rfds)){
		while(!read_netdev_event(gpnet)) ;
	}
	if(FD_ISSET(gpnet->ipcfd, &rfds)){
		res|=read_ipc_event(gpnet);
	}
	return res;
}

static void sja1105_clock_adjtime(int spifd, int adjppb)
{
	uint32_t v;
	// 0.465661287308 = 2^-31 * 1e9
	v=0x80000000 + (int)((double)adjppb/0.465661287308);
	WrReg32(spifd, 0x1B, v);
}

static int sja1105_control_schedule(int spifd, bool start)
{
	uint32_t d;

	d = RdReg32(spifd, 0x18); // PTP Control Register 1
	if(d & PTPCTRL_PTPSTRTSCH){
		//PTPSTRTSCH is set, the schedule is running
		UB_LOG(UBL_INFO, "stop the TSN schedule\n");
		WrReg32(spifd, 0x18, PTPCTRL_VALID | PTPCTRL_PTPSTOPSCH |
			PTPCTRL_CORRCLK4TS_BIT);
	}
	if(!start) return 0;
	UB_LOG(UBL_INFO, "start the TSN schedule\n");
	WrReg32(spifd, 0x18, PTPCTRL_VALID | PTPCTRL_PTPSTRTSCH |
		PTPCTRL_CORRCLK4TS_BIT);
	return 0;
}

// aligntime:nsec unit, cycletime:nsec unit
static int sja1105_start_schedule(gptpnet_data_t *gpnet, int spifd,
				  uint32_t aligntime, uint32_t cycletime)
{
	uint64_t gts0, gts1;
	uint64_t ts0, ts1;
	uint64_t ts;

	if(cycletime==0) return sja1105_control_schedule(spifd, false);
	ts0=ptpSysemTime(spifd);
	gts0=gptpclock_getts64(0, gptpclock_active_domain());
	ts1=ptpSysemTime(spifd);
	if(ts1-ts0>500000){
		ts0=ptpSysemTime(spifd);
		gts0=gptpclock_getts64(0, gptpclock_active_domain());
		ts1=ptpSysemTime(spifd);
	}
	if(ts1-ts0>500000){
		UB_LOG(UBL_ERROR, "two ptpSysemTime distance is too far %"PRIu64"usec\n",
		       (ts1-ts0)/1000);
		return -1;
	}
	gts1=((gts0+aligntime)/aligntime)*aligntime; // align to aligntime
	if(gts1-gts0<100000){
		// when it is in 100usec, use the next aligned point
		gts1=((gts0+2*aligntime)/aligntime)*aligntime;
	}
	UB_LOG(UBL_INFO, "gptp time=%"PRIu64"usec, aligned time=%"PRIu64"usec\n",
	       gts0/1000, gts1/1000);
	ts = gts1 + (ts1+ts0)/2 - gts0;
	// the clock is 8nsec unit, shit 3 bits to convert to 1nsec unit
	ts = ts >> 3;
	WrReg32(spifd, 0x14, ts>>32); // PTPSCHTMU
	WrReg32(spifd, 0x13, ts&0xffffffff); // PTPSCMTML

	cycletime = cycletime >> 3;
	WrReg32(spifd, 0x1E, cycletime); // PTPCLKCORP

	return sja1105_control_schedule(spifd, true);
}

int gptpnet_tsn_schedule(gptpnet_data_t *gpnet, uint32_t aligntime, uint32_t cycletime)
{
	int i;
	for(i=0;i<DEFAULT_SJA1105_NUM_CASC;i++){
		sja1105_start_schedule(gpnet, gpnet->spifd[i], aligntime, cycletime);
	}
	return 0;
}

int gptp_clock_adjtime(int spifd, int adjppb)
{
	int i;
	for(i=0;i<DEFAULT_SJA1105_NUM_CASC;i++){
		if(gpnetg) sja1105_clock_adjtime(gpnetg->spifd[i], adjppb);
	}
	return 0;
}

gptpnet_data_t *gptpnet_init(gptpnet_cb_t cb_func, void *cb_data, char *netdev[],
			     int *num_ports, char *master_ptpdev)
{
	gptpnet_data_t *gpnet;
	int i,j;
	int res;
	swport_t *swport;

	if(!netdev || !netdev[0][0]){
		UB_LOG(UBL_ERROR,"%s:at least one netdev need\n",__func__);
		return NULL;
	}
	gpnet=malloc(sizeof(gptpnet_data_t));
	ub_assert(gpnet, __func__, "malloc");
	memset(gpnet, 0, sizeof(gptpnet_data_t));
	gpnet->num_ports=DEFAULT_SJA1105_NUM_TPORTS;
	*num_ports=DEFAULT_SJA1105_NUM_TPORTS;
	if(netdev_init(gpnet, netdev[0])) {
		free(gpnet);
		return NULL;
	}
	gpnet->txts_events=ub_esarray_init(16, sizeof(txts_event_data_t), 64);

	for(i=0;i<DEFAULT_SJA1105_NUM_CASC;i++){
		for(j=0;j<DEFAULT_SJA1105_NUM_DPORTS;j++){
			int ndevIndex=get_ndevIndex(i, j);
			if(ndevIndex<0) continue;
			swport=&gpnet->swports[ndevIndex];
			if(!(res=oneport_init(gpnet, swport, i, j))) continue;
			if(res<0) {
				UB_LOG(UBL_ERROR, "%s:swid=%d, sportid=%d, can't be opened\n",
				       __func__, i, j);
				continue;
			}
		}
	}
	gpnet->cb_func=cb_func;
	gpnet->cb_data=cb_data;
	gpnet->event_ts64=ub_mt_gettime64();
	gptpnet_ipc_init(gpnet);
	gpnetg=gpnet;
	return gpnet;
}

int gptpnet_activate(gptpnet_data_t *gpnet)
{
	int i;
	for(i=0;i<gpnet->num_ports;i++){
		onenet_activate(gpnet, i);
	}
	return 0;
}

int gptpnet_close(gptpnet_data_t *gpnet)
{
	int i;
	UB_LOG(UBL_DEBUGV, "%s:\n",__func__);
	if(!gpnet) return -1;
	if(gpnet->portfd) close(gpnet->portfd);
	ub_esarray_close(gpnet->txts_events);
	for(i=0;i<DEFAULT_SJA1105_NUM_CASC;i++){
		if(gpnet->spifd[i]) spiClose(gpnet->spifd[i]);
	}
	gptpnet_ipc_close(gpnet);
	free(gpnet);
	gpnetg=NULL;
	return 0;
}

int gptpnet_eventloop(gptpnet_data_t *gpnet, int *stoploop)
{
	while(!*stoploop){
		gptpnet_catch_event(gpnet);
	}
	return 0;
}

uint8_t *gptpnet_get_sendbuf(gptpnet_data_t *gpnet, int ndevIndex)
{
	return gpnet->swports[ndevIndex].sbuf.pdata;
}

#define TXTS_DEFER_TIMEOUT 30000000 //30msec
int gptpnet_send(gptpnet_data_t *gpnet, int ndevIndex, uint16_t length)
{
	char *msg;
	int msgtype;
	uint64_t ts64;
	swport_t *swport;
	int res;
	uint8_t swid;
	uint8_t sportid;
	txts_event_data_t txtsevt;
	int64_t ts;

	if(length>GPTP_MAX_PACKET_SIZE){
		UB_LOG(UBL_ERROR, "%s:length=%d is too big\n", __func__, length);
		return -1;
	}
	if(get_swid_sportid(ndevIndex, &swid, &sportid)<0) return -1;
	ts=ub_mt_gettime64();

	swport=&gpnet->swports[ndevIndex];
	msgtype=PTP_HEAD_MSGTYPE(swport->sbuf.pdata);
	if(msgtype<=15)
		msg=PTPMsgType_debug[msgtype];
	else
		msg="unknow";

	if(gptpnet_check_deferred(gpnet, ndevIndex)){
		UB_LOG(UBL_DEBUGV, "%s:process defered_txts, and defer msg=%s\n",__func__, msg);
		gptpnet_extra_timeout(gpnet, 5000000); // next timeout happens in 5msec
		gpnet->next_tout_needcb=true;
		return -1;
	}

	UB_LOG(UBL_DEBUGV, "SEND: pindex=%d msgtype=%s\n", ndevIndex+1, msg);
	setMgmtL2AdLup(gpnet->spifd[swid], sportid);
	if(swid!=0) setMgmtL2AdLup(gpnet->spifd[0], DEFAULT_SJA1105_CASCADE_PORT);
	RdReg32(gpnet->spifd[swid], 0xC0 + (4 * sportid)); // clear the flag:UPDATE_n
	res=write(gpnet->portfd, &swport->sbuf, length+sizeof(CB_ETHHDR_T));
	if(res!=length+sizeof(CB_ETHHDR_T)){
		UB_LOG(UBL_ERROR, "%s:pindex=%d, can't send, res=%d < %lu\n",
		       __func__, ndevIndex+1, res, length+sizeof(CB_ETHHDR_T));
		return -1;
	}
	if(msgtype>=8) return res;
	if(!gpnet->cb_func) return -1;
	memset(&txtsevt, 0, sizeof(txts_event_data_t));
	txtsevt.edtxts.msgtype=msgtype;
	txtsevt.edtxts.seqid=PTP_HEAD_SEQID(swport->sbuf.pdata);
	txtsevt.edtxts.domain=PTP_HEAD_DOMAIN_NUMBER(swport->sbuf.pdata);
	ts64=read_egress_timestamp(gpnet, swid, sportid);
	if(!ts64){
		UB_TLOG(UBL_DEBUG, "%s:pindex=%d, defer reading TxTS for msgtype=%s\n",
			 __func__, ndevIndex+1, msg);
		ts+=TXTS_DEFER_TIMEOUT;
		txtsevt.defertout=ts;
	}
	txtsevt.edtxts.ts64=ts64;
	txtsevt.ndevIndex=ndevIndex;
	ub_esarray_add_ele(gpnet->txts_events, (ub_esarray_element_t *)&txtsevt);
	return res;
}

char *gptpnet_ptpdev(gptpnet_data_t *gpnet, int ndevIndex)
{
	return gpnet->swports[ndevIndex].nlstatus.ptpdev;
}

int gptpnet_num_ports(gptpnet_data_t *gpnet)
{
	return gpnet->num_ports;
}

uint8_t *gptpnet_portid(gptpnet_data_t *gpnet, int ndevIndex)
{
	return gpnet->swports[ndevIndex].nlstatus.portid;
}

void gptpnet_create_clockid(gptpnet_data_t *gpnet, uint8_t *id,
			    int ndevIndex, int8_t domainNumber)
{
	memcpy(id, gpnet->swports[ndevIndex].nlstatus.portid, sizeof(ClockIdentity));
	if(domainNumber==0) return;
	id[3]+=domainNumber*0x10;
}

int gptpnet_get_nlstatus(gptpnet_data_t *gpnet, int ndevIndex, event_data_netlink_t *nlstatus)
{
	if(ndevIndex < 0 || ndevIndex >= gpnet->num_ports){
		UB_LOG(UBL_ERROR, "%s:ndevIndex=%d doesn't exist\n",__func__, ndevIndex);
		return -1;
	}
	memcpy(nlstatus, &gpnet->swports[ndevIndex].nlstatus, sizeof(event_data_netlink_t));
	return 0;
}

uint64_t gptpnet_txtslost_time(gptpnet_data_t *gpnet, int ndevIndex)
{
	/* give up to read TxTS, if it can't be captured in this time */
	return gpnet->swports[ndevIndex].txtslost_time;
}

void gptpnet_extra_timeout(gptpnet_data_t *gpnet, int toutns)
{
	if(toutns==0) toutns=gptpconf_get_intitem(CONF_GPTPNET_EXTRA_TOUTNS);
	gpnet->next_tout64=ub_mt_gettime64();
	gpnet->next_tout64+=toutns;
}

/*****************************************************************
  functions to access sja1105
 *****************************************************************/
typedef struct spi_config {
	int        mode;  // [0-3]  (-1 when not configured).
	int        lsb;   // {0,1}  (-1 when not configured).
	int        bits;  // [7...] (-1 when not configured).
	uint32_t   speed; // 0 when not configured.
} spi_config_t;

static int config_spidev (int fd, int mode, int lsb, int bits, uint32_t speed)
{
	spi_config_t  new_config = { -1, -1, -1, 0 };
	spi_config_t  config;
	uint8_t       byte;
	uint32_t      u32;

	new_config.mode = mode;
	new_config.lsb  = lsb;
	new_config.bits = bits;
	new_config.speed= speed;

	// Read the previous configuration.
	if (ioctl(fd, SPI_IOC_RD_MODE, &byte) < 0) {
		UB_LOG(UBL_ERROR, "SPI_IOC_RD_MODE failed\n");
		return -1;
	}
	config.mode = byte;
	if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &byte) < 0) {
		UB_LOG(UBL_ERROR, "SPI_IOC_RD_LSB_FIRST failed\n");
		return -1;
	}
	config.lsb = (byte == SPI_LSB_FIRST ? 1 : 0);
	if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, & byte) < 0) {
		UB_LOG(UBL_ERROR, "SPI_IOC_RD_BITS_PER_WORD failed\n");
		return -1;
	}
	config.bits = (byte == 0 ? 8 : byte);
	if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, & u32) < 0) {
		UB_LOG(UBL_ERROR, "SPI_IOC_RD_MAX_SPEED_HZ failed\n");
		return -1;
	}
	config.speed = u32;

	// Set the new configuration.
	if ((config.mode != new_config.mode) && (new_config.mode != -1)) {
		byte = new_config.mode;
		if (ioctl(fd, SPI_IOC_WR_MODE, & byte) < 0) {
			UB_LOG(UBL_ERROR, "SPI_IOC_WR_MODE failed\n");
			return -1;
		}
	}
	if ((config.lsb != new_config.lsb) && (new_config.lsb != -1)) {
		byte = (new_config.lsb ? SPI_LSB_FIRST : 0);
		if (ioctl(fd, SPI_IOC_WR_LSB_FIRST, & byte) < 0) {
			UB_LOG(UBL_ERROR, "SPI_IOC_WR_LSB_FIRST failed\n");
			return -1;
		}
	}
	if ((config.bits != new_config.bits) && (new_config.bits != -1)) {
		byte = new_config.bits;
		if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, & byte) < 0) {
			UB_LOG(UBL_ERROR, "SPI_IOC_WR_BITS_PER_WORD failed\n");
			return -1;
		}
	}
	if ((config.speed != new_config.speed) && (new_config.speed != 0)) {
		u32 = new_config.speed;
		if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, & u32) < 0) {
			UB_LOG(UBL_ERROR, "SPI_IOC_WR_MAX_SPEED_HZ failed\n");
			return -1;
		}
	}
	return 0;
}

static size_t spiTransfer(int spifd, uint8_t* obuf, size_t onum, uint8_t* ibuf, size_t inum)
{
	size_t res=-1;
	uint8_t tx_buffer[128];
	uint8_t rx_buffer[128];
	struct spi_ioc_transfer transfer;

	memset(&transfer,0,sizeof(transfer));
	memset(tx_buffer, 0, onum+inum);
	memcpy(tx_buffer,obuf, onum);
	transfer.rx_buf = (intptr_t)rx_buffer;
	transfer.tx_buf = (intptr_t)tx_buffer;
	transfer.len = onum+inum;

	if (ioctl(spifd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
		UB_LOG(UBL_ERROR, "Error reading num=%lu\n",inum);
		return res;
	}
	if (ibuf) {
		memcpy(ibuf,rx_buffer+onum,inum);
		res = inum;
	} else {
		res = onum;
	}
	return res;
}

/* for SPI commands, CPU endian 32-bit word can be pushed as it is */
#define MKRDCMD(addr,nwords) (((addr&0x1fffff) << 4) + ((nwords&0x7f) << 25))
#define MKWRCMD(addr) (((addr&0x1fffff) << 4) + (1<<31))

static uint32_t RdReg32(int spifd, uint32_t addr)
{
	uint32_t val = 0;
	uint32_t nxp_cmd;
	nxp_cmd = MKRDCMD(addr,1);
	spiTransfer(spifd, (uint8_t*)&nxp_cmd, sizeof(nxp_cmd), (uint8_t*)&val, sizeof(val));
	//UB_LOG(UBL_DEBUGV, "%s: spifd=%d, addr=0x%x, val=0x%08x\n", __func__, spifd, addr, val);
	return val;
}

static void WrReg32(int spifd, uint32_t addr, uint32_t wval)
{
	uint32_t nxp_cmd[2];
	nxp_cmd[0]=MKWRCMD(addr);
	nxp_cmd[1]=wval;
	spiTransfer(spifd, (uint8_t*)&nxp_cmd, sizeof(nxp_cmd), NULL, 0);
}

static void spiClose(int spifd)
{
	UB_LOG(UBL_DEBUGV, "%s:spifd=%d\n", __func__, spifd);
	if (spifd) close(spifd);
}

static int spiOpen(const char* spidev)
{
	int spifd;
	UB_LOG(UBL_DEBUGV, "%s:spidev=%s\n", __func__, spidev);
	spifd = open(spidev, O_RDONLY);
	if (spifd < 0) {
		UB_LOG(UBL_ERROR, "spi open failed on %s\n", spidev);
		return 0;
	}

	if (config_spidev (spifd, 1, 0, 32, 12000000)) {
		UB_LOG(UBL_ERROR, "spi config failed on %s\n", spidev);
		spiClose(spifd);
		return 0;
	}
	return spifd;
}

/* read the captured clock by CASSYNC in SJA1105 */
static uint64_t ptpCascSyncTime(int spifd)
{
	uint32_t clocklsb, clockmsb;

	clocklsb = RdReg32(spifd, 0x1F);
	clockmsb = RdReg32(spifd, 0x20);
	return ((uint64_t)clockmsb << 32) | clocklsb;
}

/* Set Management L2 Address Lookup Table, use only INDEX(b15-b6)=0 */
static void setMgmtL2AdLup(int spifd, uint8_t port)
{
	int i;
	uint32_t tbl[6]={
		0xDD800000, // VALID=1,RDWRSET=1,LOCKDES=1,VALIDENT=1,MGMTROUTE=1,HOSTCMD=b011
		0x00000000,
		0x00000000,
		0x00000040, // TSREG(b71)=0, TAKETS(b70)=1
		0x60308000, // MAC(b69-b22): 01:80:C2:00:00:0E
		0x03810000,  // bit(b21-b17): destports, p4-p0, MGMTVALID(b16)=1
	};
	tbl[5]|=((1<<port)<<17);
	for (i = 0; i < 6; i++) {
		WrReg32(spifd, 0x24 + i, tbl[5-i]);
	}
}
