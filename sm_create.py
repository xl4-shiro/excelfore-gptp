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
import sys
from io import StringIO

class read_sm_list(object):
    def __init__(self, fname="sm_list.txt"):
        inf=open(fname)
        started=False
        self.sm_items=[]
        self.itemline=""
        while True:
            line=inf.readline()
            if line=="": break;
            if not started:
                if line.find("--- START ---")==0:
                    started=True
            else:
                if line.find("--- END ---")==0: break
                line=line.rstrip()
                self.add_itemline(line)

    def add_itemline(self,line):
        if line.rfind('\\')==len(line)-1:
            self.itemline+=line[:len(line)-1]
            return
        self.itemline+=line
        self.sm_items.append(self.itemline.split())
        self.itemline=""

class one_init(object):
    def __init__(self, smitems):
        self.smitems=smitems
        self.sm_name=self.gen_sm_name()
        self.gvars=[]
        for v in self.smitems[5].split(','):
            self.mkvitems(v)
        self.states=['INIT']
        for i in self.smitems[6].split(','): self.states.append(i)
        self.states.append('REACTION')
        inf=open(self.smitems[3])
        self.temp_text=inf.read()
        inf.close()
        self.output_text()
        self.replace_text("__state_enum_data", self.create_enum)
        self.replace_text("__instance_data", self.create_data_type)
        self.replace_text("__allstate_condition", self.allstate_condition)
        self.replace_text("__static_functions", self.static_functions)
        self.replace_text("__state_procedures", self.state_procedures)
        self.replace_text("__init_args", self.dec_init_function)
        self.replace_text("__init_function", self.init_function)
        self.replace_text("__close_function", self.close_function)

    def mkvitems(self, v):
        if v=="gptpnet_data_t":
            self.gvars.append((v,"gpnetd"))
            return
        name=""
        cc=False
        lastu=-1
        ss=""
        ov=v
        if v[0:2]=="MD":
            name="md"
            v=v[2:]
        for i in range(len(v)):
            if v[i:i+1].isupper():
                if len(ss)==0: ss=v[i:i+1].lower()
                lastu=1
            else:
                if lastu==1: # last char is upper
                    name+=ss
                    ss=""
                lastu=0
        if len(ss)>0: name+=ss
        self.gvars.append((ov,name))

    def gen_sm_name(self):
        name=""
        cc=False
        lastu=-1
        smdname=self.smitems[1]
        ss=""
        for i in range(len(smdname)):
            if smdname[i:i+1].isupper():
                ss+=smdname[i:i+1].lower()
                lastu=1
            else:
                if lastu==1: # last char is upper
                    if len(name)>1: # not a top or second char
                        name+='_'
                    name+=ss[0:len(ss)-1]
                    ss=ss[-1]
                ss+=smdname[i:i+1]
                lastu=0
        if len(ss)>0: name+='_'+ss
        return name

    def output_text(self):
        self.output_text=self.temp_text.replace("SM_MACHINE", self.sm_name)

    def replace_text(self, mark, ofunc):
        inf=StringIO(self.output_text)
        ot=""
        while True:
            line=inf.readline()
            if line=="": break
            if line.find(mark)<0:
                ot+=line
            else:
                ot+=ofunc()
        inf.close()
        self.output_text=ot

    def create_enum(self):
        ot=""
        for i in self.states: ot+="\t%s,\n" % i
        return ot

    def create_data_type(self):
        ot=""
        for i in self.gvars: ot+="\t%s *%s;\n" % (i[0],i[1])
        ot+="\t%s %s;\n" % (self.sm_name+"_state_t", "state")
        ot+="\t%s %s;\n" % (self.sm_name+"_state_t", "last_state")
        ot+="\t%s %s;\n" % (self.smitems[1]+"SM", "*thisSM")
        if self.smitems[4].find("D")>=0:
            ot+="\tint domainIndex;\n"
        if self.smitems[4].find("P")>=0:
            ot+="\tint portIndex;\n"
        return ot

    def allstate_condition(self):
        return "\treturn %s;\n" % self.states[1]

    def static_functions(self):
        ot=""
        for i in self.states[1:len(self.states)-1]:
            ot+="static void *%s_proc(%s_data_t *sm)\n" % (i.lower(), self.sm_name)
            ot+="{\n"
            ot+=self.debug_msg("sm->", "%s:" % self.sm_name)
            ot+="\treturn NULL;\n"
            ot+="}\n\n"
            ot+="static %s_state_t %s_condition(%s_data_t *sm)\n" % \
                (self.sm_name, i.lower(), self.sm_name)
            ot+="{\n"
            ot+="\treturn %s;\n" % i
            ot+="}\n\n"
        return ot

    def state_procedures(self):
        ot=""
        ot+="\t\tcase %s:\n" % self.states[0]
        ot+="\t\t\tsm->state = %s;\n" % self.states[1]
        ot+="\t\t\tbreak;\n"
        for i in self.states[1:len(self.states)-1]:
            ot+="\t\tcase %s:\n" % i
            ot+="\t\t\tif(state_change)\n"
            ot+="\t\t\t\tretp=%s_proc(sm);\n" % i.lower()
            ot+="\t\t\tsm->state = %s_condition(sm);\n" % i.lower()
            ot+="\t\t\tbreak;\n"
        ot+="\t\tcase %s:\n" % self.states[len(self.states)-1]
        ot+="\t\t\tbreak;\n"
        return ot

    def dec_init_function(self):
        ot="void %s_sm_init(%s_data_t **sm,\n" % (self.sm_name, self.sm_name)
        if self.smitems[4]=="DP":
            ot+="\tint domainIndex, int portIndex,\n"
        elif self.smitems[4]=="D":
            ot+="\tint domainIndex,\n"
        elif self.smitems[4]=="P":
            ot+="\tint portIndex,\n"
        for i in self.gvars:
            ot+="\t%s *%s,\n" % (i[0],i[1])
        ot=ot[:len(ot)-2]+")\n"
        return ot

    def debug_msg(self, s, p=""):
        ot=""
        if self.smitems[4]=="DP":
            ot="\tUB_LOG(UBL_DEBUGV, \"%s%%s:domainIndex=%%d, portIndex=%%d\\n\",\n" % p
            ot+="\t\t__func__, %sdomainIndex, %sportIndex);\n" % (s, s)
        elif self.smitems[4]=="D":
            ot="\tUB_LOG(UBL_DEBUGV, \"%s%%s:domainIndex=%%d\\n\", __func__, %sdomainIndex);\n" % \
                (p, s)
        elif self.smitems[4]=="P":
            ot="\tUB_LOG(UBL_DEBUGV, \"%s%%s:portIndex=%%d\\n\", __func__, %sportIndex);\n" % \
                (p, s)
        return ot

    def init_function(self):
        ot=self.debug_msg("")
        ot+="\tINIT_SM_DATA(%s_data_t, %sSM, sm);\n" % (self.sm_name, self.smitems[1])
        for i in self.gvars:
            if(i[1][0]=="*"):
                ot+="\t(*sm)->%s = %s;\n" % (i[1][1:],i[1][1:])
            else:
                ot+="\t(*sm)->%s = %s;\n" % (i[1],i[1])
        if self.smitems[4].find("D")>=0:
            ot+="\t(*sm)->domainIndex = domainIndex;\n"
        if self.smitems[4].find("P")>=0:
            ot+="\t(*sm)->portIndex = portIndex;\n"
        return ot

    def close_function(self):
        ot=self.debug_msg("(*sm)->")
        ot+="\tCLOSE_SM_DATA(sm);\n"
	ot+="\treturn 0;\n"
        return ot

    def header(self):
        ot="#ifndef __%s_SM_H_\n" % self.sm_name.upper()
        ot+="#define __%s_SM_H_\n" % self.sm_name.upper()
        ot+="\n"
        ot+="typedef struct %s_data %s_data_t;\n\n" % (self.sm_name, self.sm_name)
        ot+="void *%s_sm(%s_data_t *sm, uint64_t cts64);\n\n" % (self.sm_name, self.sm_name)
        ot+=self.dec_init_function()
        ot=ot.rstrip()
        ot+=";\n\n"
        ot+="int %s_sm_close(%s_data_t **sm);\n\n" % (self.sm_name, self.sm_name)
        ot+="#endif"
        return ot


if __name__ == '__main__':
    header_mode=False
    if len(sys.argv)<2:
        print("sm_create.py number [h] -- number for template of the item")
    if len(sys.argv)>2:
        if sys.argv[2]=="h": header_mode=True
    item_num=int(sys.argv[1])
    sm_list=read_sm_list()
    si=0
    sl=len(sm_list.sm_items)
    if item_num>0 and item_num<=len(sm_list.sm_items):
        si=item_num-1
        sl=item_num
    for sm in sm_list.sm_items[si:sl]:
        onesm=one_init(sm)
        if header_mode:
            print(onesm.header())
        else:
            print(onesm.output_text)
