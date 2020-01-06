#!/usr/bin/python
'''
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
'''
import kivy
kivy.require('1.9.0')
from kivy.app import App
from kivy.uix.button import Button
from kivy.uix.togglebutton import ToggleButton
from kivy.base import EventLoop
from kivy.uix.screenmanager import ScreenManager, Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.graphics import Color, Rectangle
from kivy.config import Config
from ConfigParser import SafeConfigParser
from subprocess import Popen, call, PIPE
import json
import socket, select, sys, os, re
import struct
import time
import argparse
import signal

SCREEN_SIZE=(1200, 900)
TASHP_SCREEN_SIZE=(400, 200)
SCREEN_BG_COLOR=[1,1,1,1]
DOMAIN_BOX_BG_COLOR=[0.2,0.2,0.2,1]
EMPHASIZE_COLOR=[1,0,0,1]
NORMAL_COLOR=[1,1,1,1]
DISABLED_COLOR=[0.5,0.5,0.5,1]
ACTIVE_DOMAIN_COLOR=[0,1,1,1]
STABLE_DOMAIN_TIME=3.0

READ_ONLY = select.POLLIN | select.POLLPRI | select.POLLHUP | select.POLLERR
READ_WRITE = READ_ONLY | select.POLLOUT
_const=None
_gpmon_scm=None

"""
format in 'gpmongui.conf'
--------------------------
[devices]
mac_list = [
	 "54:53:ed:25:41:91",
	 "00:19:B8:01:9C:11"
	]

name_list = [
	  "A",
	  "B"
	]
--------------------------
"""

"""
typedef struct gptpipc_client_req_data {
	gptp_ipc_command_t cmd;
	int domainNumber;
	int domainIndex;
	int portIndex;
} gptpipc_client_req_data_t;
"""
REQ_DATA_FMT="=iiii"

"""
typedef struct gptpipc_notice_data {
	uint32_t event_flags;
	int8_t domainNumber;
	int domainIndex;
	int portIndex;
	UInteger224 gmPriority;
} gptpipc_notice_data_t;

UInteger224 part is "BBBHB8BH8BHH"
"""
NOTICE_DATA_FMT="=IbiiBBBHB8BH8BHH"

"""
typedef struct gptpipc_data_netlink {
	bool up;
	char devname[IFNAMSIZ];
	char ptpdev[MAX_PTPDEV_NAME];
	uint32_t speed;
	uint32_t duplex;
	ClockIdentity portid;
} gptpipc_data_netlink_t;
typedef struct gptpipc_ndport_data {
	gptpipc_data_netlink_t nlstatus;
} gptpipc_ndport_data_t;
"""
NDPORT_DATA_FMT="=b16c32cII8B"

"""
typedef struct gptpipc_gport_data {
	uint8_t domainNumber;
	int portIndex;
	bool asCapable;
	bool portOper;
	ClockIdentity gmClockId;
        uint8_t annPathSequenceCount;
	ClockIdentity annPathSequence[MAX_PATH_TRACE_N];
} gptpipc_gport_data_t;
"""
GPORT_DATA_FMT="=Bibb8BB" # followed by '8B'*annPathSequenceCount

"""
typedef struct gptpipc_clock_data{
	bool gmsync;
	uint8_t domainNumber;
	int portIndex;
	uint16_t gmTimeBaseIndicator;
	int64_t lastGmPhaseChange_nsec;
	double lastGmFreqChange;
	bool domainActive;
	ClockIdentity clockId;
	ClockIdentity gmClockId;
} gptpipc_clock_data_t;
"""
CLOCK_DATA_FMT="=bBiHqdb8B8B"


class ReadConfig(object):
    def __init__(self, fname="gpmongui.conf"):
        parser = SafeConfigParser()
        self.mac_list=[]
        if not parser.read([fname]): return
        mac_slist=json.loads(parser.get('devices','mac_list'))
        self.name_list=[]
        for ma in mac_slist:
            print ma
            sl=ma[1].split(':')
            self.mac_list.append([int(s, 16) for s in sl])
            self.name_list.append(ma[0])
        self.listener_cmd=parser.get('external','listener')
        self.talker_cmd=parser.get('external','talker')

    def name_from_id(self, id, fmt="%s"):
        for c,m in enumerate(self.mac_list):
            found=True
            for i in range(6):
                f=0 if i<3 else 2
                if m[i]!=id[i+f]:
                    found=False
                    break
            if found: return fmt % self.name_list[c]
        return ""

class ClockIDDisplay(object):
    dconfig=ReadConfig()
    short_format=False
    def clockid_hexstr(self, d):
        if self.short_format:
            return "%02X:%02X%s" % (d[6],d[7], self.dconfig.name_from_id(d,"(%s)"))
        return "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X%s" % (
            d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7], self.dconfig.name_from_id(d,"(%s)"))

ciddisp=ClockIDDisplay()
b8_hexstr=ciddisp.clockid_hexstr

class ListenIpcSocket(object):
    def __init__(self, udpport=0, udpaddr="127.0.0.1", server_node="/tmp/gptp2d_ipc", **kwargs):
        self.udpport=udpport
        self.udpaddr=udpaddr
        self.server_node=server_node
        self.pcount=0
        self.sock=None
        self.ipc_connect()

    def ipc_connect(self):
        if self.sock: self.sock.close()
        if self.udpport==0:
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
            self.my_node = "%s_gpmongui" % self.server_node
            self.del_my_node()
            self.sock.bind(self.my_node)
        else:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.server_node=(self.udpaddr, self.udpport)

        try:
            self.sock.connect(self.server_node)
        except socket.error, msg:
            print("can't connect. Is gptp2d running? ", msg)
            self.sock.close()
            self.sock=None
            return False
        return True

    def del_my_node(self):
        try:
            os.unlink(self.my_node)
        except OSError:
            if os.path.exists(self.my_node):
                raise

    def req_ext_script(self, arg):
        if not self.sock: return
        if not self.udpport: return
        sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_RUN_EXT_SCRIPT'],
                       arg,0,0)
        self.sock.send(sd)

    def start(self):
        if not self.sock: return
        try:
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_ACTIVE_DOMAINT_SWITCH'],
                           -1,-1,0) # make sure to start with auto switch mode
            self.sock.send(sd)
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_GPORT_INFO'],-1,-1,0)
            self.sock.send(sd)
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_NDPORT_INFO'],-1,-1,0)
            self.sock.send(sd)
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_CLOCK_INFO'],-1,-1,0)
            self.sock.send(sd)
            # start with the disabled state
            _gpmon_scm.ipcsock.send_tashpaer_control(False)
        except socket.error, msg:
            print("can't connect. Is gptp2d running? ", msg)
            self.sock.close()
            self.sock=None

    def stop(self):
        if not self.sock: return
        print("Disalbe Time Aware Shaper")
        _gpmon_scm.ipcsock.send_tashpaer_control(False)
        self.sock.close()
        if self.udpport==0:
            self.del_my_node()

    def update(self, dispatch_fn):
        self.pcount+=1
        if not self.sock:
            if self.pcount==300:
                self.pcount=0
                if self.ipc_connect(): self.start()
            return

        poller=select.poll()
        poller.register(self.sock, READ_ONLY)
        events=poller.poll(0)
        if not events:
            if self.pcount==300:
                self.pcount=0
                sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_NDPORT_INFO'],0,-1,0)
                try:
                    self.sock.send(sd)
                except socket.error, msg:
                    print("can't send msg. Is gptp2d running? ", msg)
                    _gpmon_scm.activate(False)
                    self.ipc_connect()
            return
        self.pcount=0
        try:
            self.decode_rdata(self.sock.recv(512))
        except socket.error, msg:
            print("can't receive msg. Is gptp2d running? ", msg)
            _gpmon_scm.activate(False)
            return
        _gpmon_scm.activate(True)
        if _gpmon_scm.ipcsock.udpport: return
        for i in range(len(_gpmon_scm.domain_boxes)):
            if i==0: continue
            self.update_dc_diff(i)

    def update_dc_diff(self, domain=1):
        p=Popen(["gptpclock_monitor", "-o", "-d", "%d" % domain], stdout=PIPE)
        t=p.communicate(input=None)
        try:
            a=" %dus" % (int(t[0])/1000)
        except ValueError:
            return
        r=re.search(r" -?[0-9]*us$", _gpmon_scm.domain_boxes[domain].gmindl.text)
        if r:
            _gpmon_scm.domain_boxes[domain].gmindl.text = \
            	_gpmon_scm.domain_boxes[domain].gmindl.text[:r.start()] + a
        else:
            _gpmon_scm.domain_boxes[domain].gmindl.text = \
            	_gpmon_scm.domain_boxes[domain].gmindl.text + a

    def get_flags(self, d):
        rn=[]
        for k,v in _const.flags.iteritems():
            if d & v:
                rn.append(k)
        return ",".join(rn)

    def decode_rdata(self, rdata):
        rsize=len(rdata)
        fmt="i"
        dtype=struct.unpack(fmt, rdata[0:struct.calcsize(fmt)])
        dtype=dtype[0]
        rdata=rdata[struct.calcsize(fmt):-1]
        if dtype==_const.enums['GPTPIPC_GPTPD_NOTICE']:
            self.decode_notice(rdata)
	elif dtype==_const.enums['GPTPIPC_GPTPD_NDPORTD']:
            self.decode_ndportd(rdata)
	elif dtype==_const.enums['GPTPIPC_GPTPD_GPORTD']:
            self.decode_gportd(rdata)
	elif dtype==_const.enums['GPTPIPC_GPTPD_CLOCKD']:
            self.decode_clockd(rdata)
        else:
            print("unknown request %d" % dtype)

    def decode_notice(self, rdata):
        fmt=NOTICE_DATA_FMT
        rd=list(struct.unpack(fmt, rdata[0:struct.calcsize(fmt)]))
        eflags=rd.pop(0)
        fn=self.get_flags(eflags)
        domainNumber=rd.pop(0)
        domainIndex=rd.pop(0)
        portIndex=rd.pop(0)
        print("%s, domainNumber=%d, domainIndex=%d, portIndex=%d" % (
            fn, domainNumber, domainIndex, portIndex))
        if eflags & _const.flags['GPTPIPC_EVENT_CLOCK_FLAG_NETDEV_DOWN']:
            # NetDev Down means portOper=0 for all the domains
            _gpmon_scm.update_gportd(-1, portIndex, asCapable=None,
                                     portOper=0, gmClockId=None, gmPathIds=None)
            return
        if eflags & _const.flags['GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_DOWN']:
            _gpmon_scm.update_gportd(domainNumber, portIndex, asCapable=0,
                                     portOper=None, gmClockId=None, gmPathIds=None)
            return
        if eflags & _const.flags['GPTPIPC_EVENT_PORT_FLAG_AS_CAPABLE_UP']:
            _gpmon_scm.update_gportd(domainNumber, portIndex, asCapable=1,
                                     portOper=None, gmClockId=None, gmPathIds=None)
            return
        if eflags & _const.flags['GPTPIPC_EVENT_CLOCK_FLAG_ACTIVE_DOMAIN']:
            _gpmon_scm.update_active_domain(domainNumber, True)
            return

        if eflags & _const.flags['GPTPIPC_EVENT_CLOCK_FLAG_GM_CHANGE']:
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_CLOCK_INFO'],-1,-1,0)
            self.sock.send(sd)

        if eflags & _const.flags['GPTPIPC_EVENT_CLOCK_FLAG_NETDEV_UP']:
            # for network UP event, request for all domains
            domainNumber=-1
            domainIndex=-1
        sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_GPORT_INFO'],
                       domainNumber, domainIndex, 0)
        self.sock.send(sd)

    def decode_ndportd(self, rdata):
        fmt=NDPORT_DATA_FMT
        rd=list(struct.unpack(fmt, rdata[0:struct.calcsize(fmt)]))
        dev_up=rd.pop(0)
        ndev="".join([rd.pop(0) for i in range(16)])
        n=ndev.find("\0")
        ndev=ndev[0:n]
        ptpdev="".join([rd.pop(0) for i in range(32)])
        n=ptpdev.find("\0")
        ptpdev=ptpdev[0:n]
        speed=rd.pop(0)
        duplex=rd.pop(0)
        portid=rd[:8]
        print("%s, ndev=%s, ptpdev=%s, spped=%s, duplex=%s" % ("Up" if dev_up else "Down",
                                                               ndev, ptpdev, speed, duplex))
        print("  clockid=%s" % b8_hexstr(portid))

    def decode_gportd(self, rdata):
        fmt=GPORT_DATA_FMT
        rd=list(struct.unpack(fmt, rdata[0:struct.calcsize(fmt)]))
        rdata=rdata[struct.calcsize(fmt):-1]
        domainNumber=rd.pop(0)
        portIndex=rd.pop(0)
        asCapable=rd.pop(0)
        portOper=rd.pop(0)
        gmClockId=b8_hexstr([rd.pop(0) for i in range(8)])
        annPathSequenceCount=rd.pop(0)
        gmPathIds=[]

        print("domainNumber=%d, portIndex=%d, asCapable=%d, portOper=%d" \
              % (domainNumber, portIndex, asCapable, portOper))
        print("gmClockId=%s, annPathSequenceCount=%d" % (gmClockId, annPathSequenceCount))
        for i in range(annPathSequenceCount):
            fmt="8B"
            d=struct.unpack(fmt, rdata[0:8])
            rdata=rdata[8:-1]
            gmPathIds.append(b8_hexstr(d))
            print("  clockId in th path: %s" % b8_hexstr(d))
        _gpmon_scm.update_gportd(domainNumber, portIndex, asCapable,
                                 portOper, gmClockId, gmPathIds)


    def decode_clockd(self, rdata):
        fmt=CLOCK_DATA_FMT
        rd=list(struct.unpack(fmt, rdata[0:struct.calcsize(fmt)]))
        gmsync=rd.pop(0)
        domainNumber=rd.pop(0)
        portIndex=rd.pop(0)
        gmTimeBaseIndicator=rd.pop(0)
        lastGmPhaseChange_nsec=rd.pop(0)
        lastGmFreqChange=rd.pop(0)
        domainActive=rd.pop(0)
        clockId=b8_hexstr([rd.pop(0) for i in range(8)])
        gmClockId=b8_hexstr(rd)
        print("gmsync=%d, domainNumber=%d, active=%d, portIndex=%d, gmTimeBaseIndicator=%d" %
              (gmsync, domainNumber, domainActive, portIndex, gmTimeBaseIndicator))
        print("  lastGmPhaseChange_nsec=%ld, lastGmFreqChange=%f, clockId=%s, gmClockId=%s" %
              (lastGmPhaseChange_nsec, lastGmFreqChange, clockId, gmClockId))
        _gpmon_scm.update_clockd(domainNumber, portIndex, clockId)
        _gpmon_scm.update_active_domain(domainNumber, domainActive)

    def send_active_domain_switch(self, domain):
        sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_ACTIVE_DOMAINT_SWITCH'],
                       -1,domain,0)
        self.sock.send(sd)
        sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REQ_CLOCK_INFO'],-1,-1,0)
        self.sock.send(sd)

    def send_tashpaer_control(self, enable):
        if enable:
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_TSN_SCHEDULE_CONTROL'],1,1,0)
        else:
            sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_TSN_SCHEDULE_CONTROL'],0,0,0)
        self.sock.send(sd)

class ControlSwButton(Button):
    def __init__(self, text2=None, **kwargs):
        super(ControlSwButton, self).__init__(**kwargs)
        self.text1=self.text
        self.text2=text2

    def on_release(self):
        if not self.text2: return
        if self.text==self.text1:
            self.text=self.text2
        else:
            self.text=self.text1

class RestartExtSw(ControlSwButton):
    proc=None
    def __init__(self, extsw, **kwargs):
        if extsw=='vlistener':
            swname="Video Listener"
            self.pname="run_listener_1722"
        elif extsw=='vtalker':
            swname="Video Talker"
            self.pname="run_talker_1722"
        else:
            raise NameError('unknown external program')

        self.extsw=extsw
        super(RestartExtSw, self).__init__(text2="Stop %s" % swname, text="Start %s" % swname,
                                           **kwargs)

    def stop_running_proc(self):
        if self.proc:
            if _gpmon_scm.ipcsock.udpport:
                _gpmon_scm.ipcsock.req_ext_script(0)
            else:
                #self.proc.send_signal(signal.SIGINT)
                call(["killall","-SIGINT",self.pname])
                count=0
                while self.proc.poll()==None:
                    time.sleep(0.1)
                    count+=1
                    if count<10: continue
                    #self.proc.send_signal(signal.SIGKILL)
                    call(["killall","-SIGKILL",self.pname])
                    if count<20: continue
                    print "can't stop external process"
                    raise
            self.proc=None
            self.text=self.text1
            return True
        return False

    def on_release(self):
        if self.stop_running_proc(): return
        if self.extsw=='vlistener':
            if _gpmon_scm.ipcsock.udpport:
                _gpmon_scm.ipcsock.req_ext_script(1)
                self.proc=1
            else:
                self.proc=Popen(ciddisp.dconfig.listener_cmd.split())
        elif self.extsw=='vtalker':
            if _gpmon_scm.ipcsock.udpport:
                _gpmon_scm.ipcsock.req_ext_script(2)
                self.proc=2
            else:
                self.proc=Popen(ciddisp.dconfig.talker_cmd.split())
        self.text=self.text2

class SingleMultiDomainSw(ControlSwButton):
    def on_release(self):
        super(SingleMultiDomainSw, self).on_release()
        if self.text==self.text1:
            _gpmon_scm.ipcsock.send_active_domain_switch(-1)
            _gpmon_scm.active_domain_switch=-1
        else:
            _gpmon_scm.ipcsock.send_active_domain_switch(0)
            _gpmon_scm.active_domain_switch=0
        _gpmon_scm.update_active_domain_bg(_gpmon_scm.active_domain_switch)

class GpmonGmpathBox(BoxLayout):
    def __init__(self, pathid, **kwargs):
        super(GpmonGmpathBox, self).__init__(**kwargs)
        self.pathid=pathid
        self.label=Label(text=pathid, font_size=20)
        self.add_widget(self.label)
    def update_pathid(self, pathid):
        self.pathid=pathid
        self.label.text=pathid

class GpmonPortdBox(BoxLayout):
    def __init__(self, portIndex, **kwargs):
        super(GpmonPortdBox, self).__init__(**kwargs)
        self.portIndex=portIndex
        self.annPathSequenceCount=0
        self.path_boxes=[]
        self.label=Label(text="Port %d" % portIndex, font_size=30,
                         size_hint=(1,None), height=50)
        self.add_widget(self.label)
        self.own_clockid=""
        self.empty_box=BoxLayout()
        self.add_widget(self.empty_box)
        self.asCapable=0
        self.portOper=0

    def update_own_clockid(self, own_clockid):
        self.own_clockid=own_clockid
        self.parent.own_clockidl.text="Clock ID - %s" % own_clockid

    def emphasize_gm(self):
        if not self.asCapable or not self.portOper: return
        if self.own_clockid==self.parent.gmClockId:
            self.parent.gmindl.text="This clock is GM"
            self.parent.gmindl.color=EMPHASIZE_COLOR
        else:
            self.parent.gmindl.text="GM - %s" % self.parent.gmClockId
            self.parent.gmindl.color=NORMAL_COLOR
        for pb in self.path_boxes:
            if pb.pathid==self.parent.gmClockId:
                pb.label.color=EMPHASIZE_COLOR
            else:
                pb.label.color=NORMAL_COLOR

    def update_info(self, asCapable=None, portOper=None, gmPathIds=None):
        if asCapable!=None: self.asCapable=asCapable
        if portOper!=None: self.portOper=portOper
        if asCapable==0 or portOper==0:
            for pb in self.path_boxes:
                self.remove_widget(pb)
            self.label.color=DISABLED_COLOR
            self.path_boxes=[]
            return
        if self.asCapable==1 and self.portOper==1:
            self.label.color=NORMAL_COLOR
        if gmPathIds==None: return
        self.remove_widget(self.empty_box)
        gmPathIds.reverse()
        i=len(gmPathIds)
        # if the current paths are more than new paths, remove exceeding ones
        if len(gmPathIds)<len(self.path_boxes):
            for i in range(len(gmPathIds),len(self.path_boxes)):
                self.remove_widget(self.path_boxes[i])
            self.path_boxes=self.path_boxes[:len(gmPathIds)]

        # update paths
        for i, gpid in enumerate(gmPathIds):
            if i<len(self.path_boxes):
                self.path_boxes[i].update_pathid(gpid)
                continue
            gpb=GpmonGmpathBox(gpid, size_hint=(1,None), height=50)
            self.path_boxes.append(gpb)
            self.add_widget(gpb)
        self.add_widget(self.empty_box)

class GpmonBoxLayout(BoxLayout):
    def __init__(self, **kwargs):
        super(GpmonBoxLayout, self).__init__(**kwargs)
        self.bind(size=self.update_bg)

    def update_bg(self, box, size):
        self.canvas.before.clear()
        with self.canvas.before:
            Color(*DOMAIN_BOX_BG_COLOR)
            Rectangle(size=self.size, pos=self.pos)

    def update_active_bg(self, active):
        self.canvas.before.clear()
        with self.canvas.before:
            if active:
                Color(*DOMAIN_BOX_BG_COLOR)
            else:
                Color(*DISABLED_COLOR)
            Rectangle(size=self.size, pos=self.pos)

class GpmonDomainBox(GpmonBoxLayout):
    def __init__(self, domainNumber, **kwargs):
        super(GpmonDomainBox, self).__init__(**kwargs)
        self.portd_boxes=[]
        self.domainNumber=domainNumber
        self.own_clockidl=None
        self.gmindl=None
        self.domainl=None
        self.gmClockId=""
        self.activeDomain=False

    def get_portd(self, portIndex):
        for pb in self.portd_boxes:
            if pb.portIndex==portIndex: return pb
        return None

    def emphasize_gm(self):
        for pb in self.portd_boxes: pb.emphasize_gm()

    def update_gmUpdateTime(self):
        self.gmUpdateTime=time.time()

    def update_portd(self, portIndex, asCapable, portOper, gmClockId, gmPathIds):
        if gmClockId!=None:
            if self.gmClockId!=gmClockId:
                self.update_gmUpdateTime()
                self.gmClockId=gmClockId
        if portIndex<=0:
            for pb in self.portd_boxes:
                pb.update_info(asCapable, portOper, gmPathIds)
            self.emphasize_gm()
            return
        # portIndex > 0
        pb=self.get_portd(portIndex)
        if not pb:
            pb = GpmonPortdBox(portIndex, orientation='vertical')
            self.portd_boxes.append(pb)
            self.add_widget(pb)
        pb.update_info(asCapable, portOper, gmPathIds)
        self.emphasize_gm()

class GpmonMainPage(BoxLayout):
    def __init__(self, active=True, **kwargs):
        super(GpmonMainPage, self).__init__(**kwargs)
        with self.canvas.before:
            if active:
                Color(*SCREEN_BG_COLOR)
            else:
                Color(*DISABLED_COLOR)
            Rectangle(size=self.size)

class GpmonScreenManager(ScreenManager):
    def __init__(self, ctlsw=False, **kwargs):
        super(GpmonScreenManager, self).__init__(**kwargs)
        self.mainp=GpmonMainPage(orientation='vertical', spacing=5, size=self.size)
        if ctlsw:
            self.create_control_switches(ctlsw)
        screen=Screen(name='gpmon_active')
        screen.add_widget(self.mainp)
        self.domain_boxes=[] # keep BoxLayout for domains
        self.add_widget(screen)
        screen=Screen(name='gpmon_disactive')
        screen.add_widget(GpmonMainPage(size=self.size, active=False))
        self.add_widget(screen)
        self.current='gpmon_active'
        self.active=True

    def activate(self, active):
        if self.active==active: return
        if active:
            self.current='gpmon_active'
        else:
            self.current='gpmon_disactive'
        self.active=active

    def get_domaind(self, domainNumber):
        for db in self.domain_boxes:
            if db.domainNumber == domainNumber: return db
        return None

    def get_portd(self, domainNumber, portIndex):
        db=self.get_domaind(domainNumber)
        if db: return db.get_portd(portIndex)
        return None

    def create_new_domain(self, domainNumber):
        ddb=GpmonBoxLayout(orientation='vertical')
        dlb=BoxLayout(orientation='horizontal', size_hint=(1,None), height=50)
        domainl=Label(text="Domain %d" % domainNumber, font_size=30, size_hint=(0.2, 1))
        if domainNumber==0: domainl.color=ACTIVE_DOMAIN_COLOR
        dlb.add_widget(domainl)
        own_clockidl=Label(text="", font_size=20, size_hint=(0.3, 1))
        dlb.add_widget(own_clockidl)
        gmindl=Label(text="", font_size=20, size_hint=(0.5, 1))
        dlb.add_widget(gmindl)
        ddb.add_widget(dlb)
        db=GpmonDomainBox(domainNumber, orientation='horizontal')
        db.own_clockidl=own_clockidl
        db.gmindl=gmindl
        db.domainl=domainl
        self.domain_boxes.append(db)
        ddb.add_widget(db)
        self.mainp.add_widget(ddb)
        return db

    def create_control_switches(self, ctlsw):
        swb=GpmonBoxLayout(orientation='horizontal', size_hint=(1, None), height=50)
        self.csw1=SingleMultiDomainSw(text2="Multiple Domain Mode", text="Single Domain Mode",
                                      font_size=30)
        self.csw2=RestartExtSw(ctlsw, font_size=30)
        swb.add_widget(self.csw1)
        swb.add_widget(self.csw2)
        self.mainp.add_widget(swb)

    def update_active_domain(self, domainNumber, activeDomain):
        adb=self.get_domaind(domainNumber)
        if adb.activeDomain==activeDomain: return # no change
        for db in self.domain_boxes:
            if activeDomain:
                db.domainl.color=ACTIVE_DOMAIN_COLOR if db==adb else NORMAL_COLOR
            else:
                if db==adb: db.domainl.color=NORMAL_COLOR

    def update_gportd(self, domainNumber, portIndex, asCapable, portOper, gmClockId, gmPathIds):
        if domainNumber<0:
            for db in self.domain_boxes:
                db.update_portd(portIndex, asCapable, portOper, gmClockId, gmPathIds)
            return
        db=self.get_domaind(domainNumber)
        if not db:
            db=self.create_new_domain(domainNumber)
        db.update_portd(portIndex, asCapable, portOper, gmClockId, gmPathIds)

    def update_clockd(self, domainNumber, portIndex, clockId):
        if portIndex:
            db=self.get_domaind(domainNumber)
            if not db: return
            pb=db.get_portd(portIndex)
            if not pb: return
            pb.update_own_clockid(clockId)
            db.emphasize_gm()
            return
        db=self.get_domaind(domainNumber)
        if not db: return
        for pb in db.portd_boxes:
            pb.update_own_clockid(clockId)
        db.emphasize_gm()

    def update_active_domain_bg(self, active_domain_switch):
        for db in self.domain_boxes:
            if active_domain_switch>=0 and db.domainNumber!=active_domain_switch:
                db.parent.update_active_bg(False)
                db.update_active_bg(False)
            else:
                db.parent.update_active_bg(True)
                db.update_active_bg(True)

class TashpMainPage(BoxLayout):
    def __init__(self, active=True, **kwargs):
        super(TashpMainPage, self).__init__(**kwargs)
        with self.canvas.before:
            Color(*SCREEN_BG_COLOR)
            Rectangle(size=self.size)

class TashpScreenManager(ScreenManager):
    def __init__(self, iperfaddr, iperfport, **kwargs):
        super(TashpScreenManager, self).__init__(**kwargs)
        self.mainp=TashpMainPage(orientation='vertical', spacing=5, size=self.size)
        # top half has the shaper enable/disable button
        self.enable_btn=ToggleButton(text="Time Aware Shaper",
                                     font_size=40)
        self.enable_btn.bind(on_release=self.tashp_btn)
        # bottom half has side traffic radio buttons
        self.btmhp=BoxLayout(orientation='vertical')
        gmindl=Label(text="Side Traffic Control(Mbps)", font_size=20, size_hint=(1, 0.2),
                     color=[0,0,0,1])
        self.btmhp.add_widget(gmindl)
        self.stctl=BoxLayout(orientation='horizontal', spacing=2)
        for i in (0,200,400,800):
            btn=ToggleButton(text="%d" % i, group='sidetf', font_size=30)
            if i==0:
                self.nosidetf_btn=btn
                self.active_sidetf_btn=btn
                btn.state='down'
            btn.bind(on_release=self.sidetf_btn)
            self.stctl.add_widget(btn)
        self.btmhp.add_widget(self.stctl)
        # crate the main page
        self.mainp.add_widget(self.enable_btn)
        self.mainp.add_widget(self.btmhp)
        self.iperf_proc=None
        self.iperfaddr=iperfaddr
        self.iperfport=iperfport
        # create the main screen
        screen=Screen(name='tashp_screen')
        screen.add_widget(self.mainp)
        self.add_widget(screen)
        self.current='tashp_screen'

        # dummy definitions to be comptibel to GpmonMainPage
        self.domain_boxes=[]

    def stop_running_proc(self):
        pass

    def tashp_btn(self, btn):
        if btn.state=='down':
            tashp_state=True
        else:
            tashp_state=False
        _gpmon_scm.ipcsock.send_tashpaer_control(tashp_state)

    def sidetf_btn(self, btn):
        if btn.state!='down': btn.state='down'
        if btn.text!="0" and self.iperf_proc and self.active_sidetf_btn==btn: return
        if self.iperf_proc:
            if not self.iperf_proc.poll(): self.iperf_proc.terminate()
            self.iperf_proc.wait()
            self.iperf_proc=None
        if btn.text=="0": return
        self.iperf_proc=Popen(['iperf3', '-u', '-p', '%d' % self.iperfport,
                               '-c', '%s' % self.iperfaddr,
                               '-b', '%sM' % btn.text, '-t' '36000'],
                              stdin=PIPE)
        self.active_sidetf_btn=btn

    def update_gportd(self, domainNumber, portIndex, asCapable, portOper, gmClockId, gmPathIds):
        pass

    def activate(self, active):
        pass

    def update_clockd(self, domainNumber, portIndex, clockId):
        pass

    def update_active_domain(self, domainNumber, activeDomain):
        pass

    # start, stop, update are called from EventLoop
    def start(self):
        pass

    def stop(self):
        if self.iperf_proc and not self.iperf_proc.poll():
            self.iperf_proc.terminate()
            self.iperf_proc=None

    def update(self, dispatch_fn):
        if not self.iperf_proc: return
        if self.iperf_proc.poll():
            self.active_sidetf_btn.state='normal'
            self.sidetf_btn(self.nosidetf_btn)

class GpMonApp(App):
    def __init__(self, **kwargs):
        super(GpMonApp, self).__init__()
        self.kwargs=kwargs

    def build(self):
        global _gpmon_scm
        ctlsw = self.kwargs['ctlsw'] if self.kwargs.has_key('ctlsw') else False
        if self.kwargs.has_key('tashp') and self.kwargs['tashp']:
            Config.set('graphics', 'width', TASHP_SCREEN_SIZE[0])
            Config.set('graphics', 'height', TASHP_SCREEN_SIZE[1])
            _gpmon_scm=TashpScreenManager(iperfaddr=self.kwargs['iperfaddr'],
                                          iperfport=self.kwargs['iperfport'],
                                          size=TASHP_SCREEN_SIZE)
            EventLoop.add_input_provider(_gpmon_scm)
        else:
            Config.set('graphics', 'width', SCREEN_SIZE[0])
            Config.set('graphics', 'height', SCREEN_SIZE[1])
            _gpmon_scm=GpmonScreenManager(ctlsw=ctlsw, size=SCREEN_SIZE)
        _gpmon_scm.ipcsock=ListenIpcSocket(**self.kwargs)
        _gpmon_scm.active_domain_switch=-1
        EventLoop.add_input_provider(_gpmon_scm.ipcsock)
        return _gpmon_scm

class ReadGptpipcH(object):
    def __init__(self):
        self.scan_enum()
        self.scan_flags()

    def scan_enum(self):
        inf=open("gptpipc.h", "r")
        state=""
        eire1=re.compile(r"\s*(\S*)\s*=\s*([0-9]*).*")
        eire2=re.compile(r"\s*(\S*),.*")
        self.enums={}
        while True:
            line=inf.readline()
            if line=="": break
            if state=="":
                if line.find("typedef enum")>=0:
                    state="enum_in"
                    enum_count=0
                    continue
            elif state=="enum_in":
                if line.find("}")==0:
                    state=""
                    continue
                r=eire1.search(line)
                if r:
                    enum_count=int(r.group(2))
                else:
                    r=eire2.search(line)
                    if not r: break
                self.enums[r.group(1)]=enum_count
                enum_count+=1
        inf.close()

    def scan_flags(self):
        inf=open("gptpipc.h", "r")
        re1=re.compile(r"\s*#define\s*(\S*)\s*\(1\s*<<\s*(\S*)\)")
        self.flags={}
        while True:
            line=inf.readline()
            if line=="": break
            r=re1.search(line)
            if not r: continue
            self.flags[r.group(1)]=eval("1<<self.enums['%s']" % r.group(2))
        inf.close()

_const=ReadGptpipcH()

if __name__ == '__main__':
    opt_parser=argparse.ArgumentParser(prog='gpmontui.py')
    opt_parser.add_argument('--udpport', type=int, nargs='?', default=0,
                            help='USE udp mode connection with the indicated port number')
    opt_parser.add_argument('--udpaddr', nargs='?',
                            help='IPC server UDP address', default='127.0.0.1')
    opt_parser.add_argument('--short', action='store_true',
                            help='use short formt to display clock ID')
    opt_parser.add_argument('--ctlsw', action='store', nargs='?', default=False,
                            help='show control switch at the top')
    opt_parser.add_argument('--tashp', action='store_true',
                            help='control time aware shaper')
    opt_parser.add_argument('--iperfport', type=int, nargs='?', default=8317,
                            help='side traffic iperf target port')
    opt_parser.add_argument('--iperfaddr', nargs='?',
                            help='side traffic iperf target UDP address', default='192.168.2.101')
    kwargs={}
    options=opt_parser.parse_args(sys.argv[1:])
    if options:
        kwargs['udpport']=options.udpport
        kwargs['udpaddr']=options.udpaddr
        kwargs['ctlsw']=options.ctlsw
        kwargs['tashp']=options.tashp
        kwargs['iperfport']=options.iperfport
        kwargs['iperfaddr']=options.iperfaddr
        ciddisp.short_format=options.short

    GpMonApp(**kwargs).run()
    if _gpmon_scm: _gpmon_scm.stop_running_proc()
