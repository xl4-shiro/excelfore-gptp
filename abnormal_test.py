#!/usr/bin/env python3
'''
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
'''
from __future__ import print_function
import os, sys, time
from datetime import datetime
import subprocess, select, socket
import logging, traceback
import struct
import getopt

logging.basicConfig()
logger=logging.getLogger('abnormal_test')

class ReadGptpipcHabn(object):
    # The way in ReadGptpipcH of gpmongui should be taken,
    # but here we cheat the data by hardcoding,
    # The values should be checked with gptpipc.h and md_abnormal_hooks.h
    def __init__(self):
        self.enums={}
        self.enums['GPTPIPC_CMD_REG_ABNORMAL_EVENT']=9
        self.enums['REGISTER']=0
        self.enums['DEREGISTER']=1
        self.reg_eventtype()
        self.reg_msgtype()

    def reg_eventtype(self):
        self.evttstrs=['EVENT_NONE','EVENT_SKIP','EVENT_DUP','EVENT_BADSEQN',
                       'EVENT_NOTS','EVENT_SENDER']
        self.evtsstrs=['none','skip','dup','bads','nots','sender']
        self.evttints=[0,1,2,3,4,5]
        for s,i in zip(self.evttstrs,self.evttints): self.enums[s]=i

    def evttypeindex(self, sstr):
        for i,s in enumerate(self.evtsstrs):
            if s==sstr: return i
        return -1

    def reg_msgtype(self):
        self.msgtstrs=['SYNC','PDELAY_REQ','PDELAY_RESP','FOLLOW_UP','PDELAY_RESP_FOLLOW_UP',
                       'ANNOUNCE','SIGNALING','MANAGEMENT']
        self.msgsstrs=['sync','pdreq','pdres','fup','pdrfup','anno','sign','mang']
        self.msginterval=[125,1000,1000,125,1000,1000,8000,8000]
        self.msgtints=[0,2,3,8,10,11,12,13]
        for s,i in zip(self.msgtstrs,self.msgtints): self.enums[s]=i

    def msgtypeindex(self, sstr):
        for i,s in enumerate(self.msgsstrs):
            if s==sstr: return i
        return -1

_const=ReadGptpipcHabn()

class gptp2d_proc(object):
    def __init__(self, gptp2dpath, suffix_no):
        self.suffix_no=suffix_no
        self.priority=246+suffix_no
        self.ovip_port=5018+suffix_no
        self.ipc_port=self.ovip_port+500
        self.conffname="gptp2_test%d.conf" % suffix_no
        self.gptp2dpath=gptp2dpath
        self.create_config_file()
        self.proc=None
        self.ipcsock=None

    def create_config_file(self):
        with open(self.conffname, "w") as wf:
            wf.write("CONF_IPC_UDP_PORT %d\n" % self.ipc_port)
            wf.write("CONF_OVIP_MODE_STRT_PORTNO %d\n" % self.ovip_port)
            wf.write("CONF_PRIMARY_PRIORITY1 %d\n" % self.priority)
            wf.write("CONF_MASTER_CLOCK_SHARED_MEM \"/gptp_mc_shm%d\"\n" % self.suffix_no)
            wf.write("CONF_DEBUGLOG_MEMORY_FILE \"gptp2d_debugmem%d.log\"\n" % self.suffix_no)
            wf.write("CONF_TS2DIFF_CACHE_FACTOR 300\n")
            wf.write("CONF_DEBUGLOG_MEMORY_SIZE 1024\n")
            wf.write("CONF_LOG_GPTP_CAPABLE_MESSAGE_INTERVAL 1\n")
            wf.write("CONF_ACTIVATE_ABNORMAL_HOOKS 1\n")

    def run(self):
        args=["stdbuf", "-o0", self.gptp2dpath, "-d", "cbeth%d" % self.suffix_no,
              "-c", self.conffname ]
        try:
            self.proc=subprocess.Popen(args, stdout=subprocess.PIPE, stdin=subprocess.PIPE)
        except:
            self.proc=None
            logger.info("%s:can't run %s" % (self.confsection, " ".join(args)))
            return -1
        return 0

    def close(self):
        self.ipcsock_open()
        if self.proc:
            self.proc.terminate()
            self.proc.wait()
            self.proc=None

    def ipcsock_open(self):
        self.ipcsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.ipcsock.connect(("127.0.0.1", self.ipc_port))
        except socket.error as msg:
            logger.info("can't connect. IPC %s:%s" % msg)
            self.ipcsock_close()
            return -1
        return 0

    def ipcsock_close(self):
        if self.ipcsock: self.ipcsock.close()
        self.ipcsock=None

"""
typedef struct gptpipc_client_req_data {
	gptp_ipc_command_t cmd;
	int domainNumber;
	int domainIndex;
	int portIndex;
	// gptpipc_client_req_abnormal_t
	int32_t subcmd;
	int32_t msgtype;
	int32_t eventtype;
	float eventrate;
	int32_t repeat;
	int32_t interval;
	int32_t eventpara;
} gptpipc_client_req_data_t;
"""
REQ_DATA_FMT="=iiiiiiifiii"

class test_procs(object):
    def __init__(self, gp0, gp1):
        self.gp0=gp0
        self.gp1=gp1
        self.kids=('consolein', 'gp0stdo', 'gp1stdo', 'gp0ipc', 'gp1ipc' )
        self.kfds=(sys.stdin, gp0.proc.stdout, gp1.proc.stdout, gp0.ipcsock, gp1.ipcsock)

    def data_poll(self, polltoutms=0):
        rvals={}
        for kid in self.kids: rvals[kid]=False
        dp=select.poll()
        for kfd in self.kfds:
            if kfd: dp.register(kfd, select.POLLIN)
        pres=dp.poll(polltoutms)
        if not pres: return None
        for pr in pres:
            if pr[1]!=select.POLLIN:
                logger.warning("unexpected event: %d" % pr[1])
                return -1
            for kfd,kid in zip(self.kfds, self.kids):
                if pr[0]==kfd.fileno(): rvals[kid]=True
        return rvals

    def event_monitor(self, polltoutms=0, mongp0=False, mongp1=True,
                      mongp0ipc=False, mongp1ipc=False):
        prs=self.data_poll(polltoutms)
        if prs==None: return 1
        if prs==-1: return -1
        monios=[]
        if mongp1: monios.append('gp1stdo')
        if mongp0: monios.append('gp0stdo')
        if mongp1ipc: monios.append('gp1ipc')
        if mongp0ipc: monios.append('gp10pc')
        for kid,kfd in zip(self.kids, self.kfds):
            if not prs[kid]: continue
            if kid=='consolein' or kid=='gp0stdo' or kid=='gp1stdo':
                aline=kfd.readline().rstrip()
            else:
                aline=kfd.recv(4096).rstrip()

            if kid in monios:
                print("%s:%s" % (kid,aline))
            if kid=='consolein':
                logger.info("console input")
                return -1
        return 0

    def event_init(self, eventconf, starttm):
        eventconf['mtindex']=_const.msgtypeindex(eventconf['msgtype'])
        eventconf['evtindex']=_const.evttypeindex(eventconf['evttype'])
        if eventconf['mtindex']==-1: return -1
        if eventconf['evtindex']==-1: return -1
        eventconf['startts']=starttm+eventconf['startrun']

        if eventconf['duration']==0.0:
            eventconf['stopts']=eventconf['startts'] + \
                                 eventconf['repeat']*(eventconf['interval']+1)* \
                    _const.msginterval[mtindex]/1000.0 + \
                    _const.msginterval[mtindex]/1000.0 + 1
        else:
            eventconf['stopts']=eventconf['startts'] + eventconf['duration']
        eventconf['endts']=eventconf['stopts']+eventconf['endrun']
        eventconf['state']=0
        return 0

    def start_ipc(self, eventconf, startdt):
        if time.time()<eventconf['startts'] or eventconf['state']!=0: return 0
        dt=datetime.now()-startdt
        print('-'*60)
        print("!!!!! START %d.%03dsec: %s %s prob=%.02f repeat=%d interval=%d" %
              (dt.seconds, dt.microseconds/1000, eventconf['evttype'], eventconf['msgtype'],
               eventconf['prob'], eventconf['repeat'], eventconf['interval']) )
        sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REG_ABNORMAL_EVENT'],
                       0, 0, 1, _const.enums['REGISTER'],
                       _const.msgtints[eventconf['mtindex']],
                       _const.evttints[eventconf['evtindex']],
                       eventconf['prob'], eventconf['repeat'] , eventconf['interval'],
                       eventconf['eventpara'])
        self.gp0.ipcsock.send(sd)
        eventconf['state']=1
        return 1

    def stop_ipc(self, eventconf, startdt):
        if time.time()<eventconf['stopts'] or eventconf['state']!=1: return 0
        dt=datetime.now()-startdt
        print('-'*60)
        print("!!!!! STOP skipping %d.%03dsec" % (dt.seconds, dt.microseconds/1000))
        sd=struct.pack(REQ_DATA_FMT, _const.enums['GPTPIPC_CMD_REG_ABNORMAL_EVENT'],
                       0, 0, 1, _const.enums['DEREGISTER'],
                       _const.msgtints[eventconf['mtindex']],
                       _const.evttints[eventconf['evtindex']],
                       1.0, 0, 0, 0)
        self.gp0.ipcsock.send(sd)
        eventconf['state']=2
        return 1

    def testrun(self, runconf, eventconfs):
        startdt=datetime.now()
        starttm=time.time()
        for eventconf in eventconfs:
            if self.event_init(eventconf, starttm):
                print("invalid event ", eventconf)
                return
        endm=False
        while not endm:
            res=self.event_monitor(100, mongp0=(runconf['verbose']&1),
                                   mongp1=(runconf['verbose']&2))
            if res==-1: return
            if res==0: continue
            endm=True
            for eventconf in eventconfs:
                self.start_ipc(eventconf, startdt)
                self.stop_ipc(eventconf, startdt)
                endm = endm and (time.time()>eventconf['endts'])

def print_usage(argv):
    print("%s [options]" % argv[0][argv[0].rfind('/')+1:])
    print("  -h|--help: print this help")
    print("  -p|--proc: gptp2d program in the executable path")
    print("  -v|--verbose:Print which console output 0:No, 1:Master, 2:Slave, 3:Both, default=3")
    print("  -m|--msgtype: %s" % '|'.join(_const.msgsstrs))
    print("  -e|--evttype: %s" % '|'.join(_const.evtsstrs))
    print("  -r|--repeat: repeat times, 0:repeat every time forever, default=0")
    print("  -i|--interval: interval times, default=5")
    print("  -n|--eventpara: event parameter, default=0")
    print("  -d|--duration: ative time for registered events")
    print("  -a|--startrun: start timing, default=2.0")
    print("  -t|--endrun: running time after stop, default=2.0")
    print("  -s|--prob: probability, default=1.0")
    print("  -b|--menu: 0:run a single event 1:run in menu, default=1")

def get_runtime_options(argv):
    try:
        opts, args = getopt.getopt(argv, "hp:v:m:e:r:i:s:t:d:b:n:a:",
                                   ["help", "proc=", "verbose=", "msgtype=", "evttype=",
                                    "repeat=", "interval=", "prob=", "endrun=", "duration=",
                                    "menu=", "eventpara=", 'startrun='])
    except getopt.GetoptError as err:
        print("wrong option in %s" % " ".join(sys.argv))
        print_usage(sys.argv)
        return (None,None)
    runconf={'verbose':3, 'proc':'gptp2d', 'menu':1}
    eventconf={'msgtype':'sync', 'evttype':'skip', 'repeat':0, 'interval':0,
               'prob':1.0, 'eventpara':0, 'endrun':2.0, 'duration':0.0, 'startrun':2.0}
    for o, a in opts:
        if o in ("-h", "--help"):
            print_usage(sys.argv)
            sys.exit()
        elif o in ("-p", "--proc"):
            runconf['proc'] = a
        elif o in ("-v", "--verbose"):
            runconf['verbose'] = int(a)
        elif o in ("-b", "--menu"):
            runconf['menu'] = int(a)
        elif o in ("-m", "--msgtype"):
            eventconf['msgtype'] = a
        elif o in ("-e", "--evttype"):
            eventconf['evttype'] = a
        elif o in ("-r", "--repeat"):
            eventconf['repeat'] = int(a)
        elif o in ("-i", "--interval"):
            eventconf['interval'] = int(a)
        elif o in ("-s", "--prob"):
            eventconf['prob'] = float(a)
        elif o in ("-a", "--startrun"):
            eventconf['startrun'] = float(a)
        elif o in ("-t", "--endrun"):
            eventconf['endrun'] = float(a)
        elif o in ("-d", "--duration"):
            eventconf['duration'] = float(a)
        elif o in ("-n", "--eventpara"):
            eventconf['eventpara'] = int(a)
        else:
            assert False, "unhandled option"
    return (runconf, eventconf)

def run_single_test(runconf, eventconfs):
    gp0=gptp2d_proc(runconf['proc'], 0)
    gp1=gptp2d_proc(runconf['proc'], 1)
    try:
        gp0.run()
        gp1.run()
        while gp0.ipcsock_open(): time.sleep(0.1)
        while gp1.ipcsock_open(): time.sleep(0.1)
        tp=test_procs(gp0, gp1)
        tp.testrun(runconf, eventconfs)
    except:
        print("Error: %s" % sys.exc_info()[0])
        print('-'*60)
        traceback.print_exc(file=sys.stdout)
        print('-'*60)
    gp0.close()
    gp1.close()
    return 0

class MenuItems(object):
    '''
    1st item: title of 'run'
    2nd item: a list of the affected state machines
    3rd item: a list of eventconf(processed through 'get_runtime_options')
    4th item: comment
    '''
    menuitems=(
        ('Normal Run',
         ['all'],
         [["-m", "sync", "-e", "none", "-d", "10"]], # "sync" is dummy
         '''
Run without any error injection
'''
        ),
        ('missing Sync',
         ['md_sync_receive_sm', 'port_sync_sync_receive_sm',
          'port_announce_information_sm'],
         [["-m", "sync", "-e", "skip", "-d", "5"]],
         '''
Skip sending Sync from Master side.
After 3 SYNC interval time, port_announce_information should detect AGED status,
and GM change happens.  Because of another Announce, GM change happens agaian.
'''
        ),
        ('missing Sync and Announce',
         ['md_sync_receive_sm', 'port_sync_sync_receive_sm',
          'port_announce_information_sm'],
         [["-m", "sync", "-e", "skip", "-d", "5"],["-m", "anno", "-e", "skip", "-d", "5"]],
         '''
         Skip sending Sync and Announce from Master side.
After 3 SYNC interval time, port_announce_information should detect AGED status,
and GM change happens.  No more GM change happens.
'''
        ),
        ('missing Sync, SyncFup and Announce',
         ['md_sync_receive_sm', 'port_sync_sync_receive_sm',
          'port_announce_information_sm'],
         [["-m", "sync", "-e", "skip", "-d", "5"],["-m", "fup", "-e", "skip", "-d", "5"],
          ["-m", "anno", "-e", "skip", "-d", "5"]],
          '''
Skip sending Sync, FollowUp and Announce from Master side.
After 3 SYNC interval time, GM change happens.
'''
         ),
         ('add 1 on Sync sequenceId, one time',
          ['md_sync_receive_sm'],
          [["-m", "sync", "-e", "bads", "-n", "1", "-r", "1", "-d", "5"]],
          '''
add 1 on Sync squenceId, it happens just once.
'''
         ),
        ('add 1 on SyncFup sequenceId, one time',
         ['md_sync_receive_sm'],
         [["-m", "fup", "-e", "bads", "-n", "1", "-r", "1", "-d", "5"]],
         '''
add 1 on FollowUp squenceId, it happens just once.
'''
        ),
        ('add 1 on SyncFup sequenceId, every time',
         ['md_sync_receive_sm' 'port_sync_sync_receive_sm',
          'port_announce_information_sm'],
         [["-m", "fup", "-e", "bads", "-n", "1", "-d", "5"]],
         '''
add 1 on FollowUp squenceId, it happens every time.
After 3 SYNC interval time, GM change happens.
Because of another Announce, GM change happens agaian.
'''
        ),
        ('missing PDelayReq',
         ['md_pdelay_resp_sm'],
         [["-m", "pdreq", "-e", "skip", "-d", "15", "-t", "5"]],
         '''
Skip sending PDelayResquest from Master side.
After 9 seconds of timeout(allowedLostResponses), the Master side get 'reset asCapable'.
It makes GM cahnge.
'''
        ),
        ('missing PDelayResponse',
         ['md_pdelay_req_sm'],
         [["-m", "pdres", "-e", "skip", "-d", "15"]],
         '''
Skip sending PDelayResponse from Master side.
After 9 seconds of timeout(allowedLostResponses),
the console should show 'reset asCapable'
'''
        ),
        ('add 1 on PDelayResponse sequenceId, one time',
         ['md_pdelay_req_sm'],
         [["-m", "pdres", "-e", "bads", "-n", "1", "-d", "15", "-r", "1"]],
         '''
add 1 on PDelayResponse squenceId, it happens just once.
'''
        ),
        ('add 1 on PDelayResponse sequenceId, every time',
         ['md_pdelay_req_sm'],
         [["-m", "pdres", "-e", "bads", "-n", "1", "-d", "15"]],
         '''
add 1 on PDelayResponse squenceId, it happens every time.
After 9 seconds of timeout(allowedLostResponses),
the console should show 'reset asCapable'
'''
        ),
        ('Abandon SYNC TxTS, 2 times',
         ['md_sync_send_sm'],
         [["-m", "sync", "-e", "nots", "-d", "5", "-r", "2"]],
         '''
Abandon Sync TxTS, 2 times
This doesn't make 'GM change'
'''
        ),
        ('Abandon SYNC TxTS, every time',
         ['md_sync_send_sm'],
         [["-m", "sync", "-e", "nots", "-d", "5"]],
         '''
Abandon Sync TxTS, 3 times
This makes 'GM change'
'''
        ),
        ('Abandon PDelayReq TxTS, 3 times',
         ['md_pdelay_req_sm'],
         [["-m", "pdreq", "-e", "nots", "-d", "5", "-r", "3"]],
         '''
Abandon PDelayReq TxTS, 3 times
'''
        ),
        ('Abandon PDelayResp TxTS, 3 times',
         ['md_pdelay_resp_sm'],
         [["-m", "pdres", "-e", "nots", "-d", "5", "-r", "3"]],
         '''
Abandon PDelayReq TxTS, 3 times
'''
        ),
        ('Sync send error',
         ['md_sync_send_sm'],
         [["-m", "sync", "-e", "sender", "-d", "5"]],
         '''
Sync send error (gptpnet_send returns -1)
'''
        ),
    )

class testMenu(object):
    def __init__(self):
        self.menuitems=[]
        self.generate_menu()

    def generate_menu(self):
        for mi in MenuItems.menuitems:
            menuitem={}
            menuitem['title']=mi[0]
            menuitem['statemachines']=mi[1]
            menuitem['argss']=mi[2]
            menuitem['comment']=mi[3]
            self.menuitems.append(menuitem)

    def show_menu(self, withcomment=False):
        print("-"*30)
        for i,mi in enumerate(self.menuitems):
            print("%d: %s" % (i+1, mi['title']))
            if not withcomment: continue
            print("%s%s" % (" "*4, ",".join(mi['statemachines'])))
            for aline in mi['comment'].split('\n'):
                if aline=="": continue
                print("%s%s" % (" "*4, aline))
            print("-"*30)
        while True:
            print("\nSelect a number from the menu('0':quit, 'c':menu with comment) ?", end='')
            aline=sys.stdin.readline()
            if aline[0]=='c': return -2
            try:
                mn=int(aline)
                if mn<len(self.menuitems)+1: return mn-1
            except:
                pass
            print("select from 0 to %d" % len(self.menuitems))


    def run_item(self, runconf, mi):
        eventconfs=[]
        for args in self.menuitems[mi]['argss']:
            (a, eventconf)=get_runtime_options(args)
            eventconfs.append(eventconf)
        return run_single_test(runconf, eventconfs)

    def run_menu(self, runconf):
        withcomment=False
        while True:
            mi=self.show_menu(withcomment)
            if mi==-2:
                withcomment=True
                continue
            if mi<0: return
            self.run_item(runconf, mi)

if __name__ == "__main__":
    logger.setLevel(logging.INFO)
    (runconf, eventconf)=get_runtime_options(sys.argv[1:])
    if runconf==None: sys.exit(2)
    os.environ['UBL_GPTP']='45,ubase:45,cbase:45,gptp:47'
    if runconf['menu']==0:
        res=run_single_test(runconf, [eventconf])
        sys.exit(res)

    tm=testMenu()
    tm.run_menu(runconf)
