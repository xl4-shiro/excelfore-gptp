#!/usr/bin/env python
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
"""
convert wireshark captured pcap/pcapng file on a virtual port to protocol
viewable format. (remove udp header)
this program relies on python-scapy

scapy 2.3.3 mis-treats timestamps in a way of the old 'pcap' format
2.4.3 works fine, it can be installed by 'pip install --pre scapy[basic]'
"""
from __future__ import print_function
import getopt
import sys
import struct
from pprint import pprint
from scapy.all import rdpcap, wrpcap, Ether, Raw

def print_usage(argv):
    print("%s [options]" % argv[0][argv[0].rfind('/')+1:])
    print("  -h|--help: print this help")
    print("  -i|--input input_pcap_file_name")
    print("  -o|--output output_pcap_file_name")
    print("  -r|--range port number range:comma separated rage like '5242-5249,5252-5259'")

def get_runtime_options():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hi:o:r:",
                                   ["help", "input=", "output=", "range="])
    except getopt.GetoptError as err:
        print("wrong option in %s" % " ".join(sys.argv))
        print_usage(sys.argv)
        sys.exit(2)
    runconf={'input':None, 'output':None, 'range':[(5242,5249),(5252,5259)]}
    for o, a in opts:
        if o in ("-h", "--help"):
            print_usage(sys.argv)
            sys.exit()
        elif o in ("-i", "--input"):
            runconf['input'] = a
        elif o in ("-o", "--output"):
            runconf['output'] = a
        elif o in ("-r", "--range"):
            rs=[]
            for r in a.split(','):
                pr=r.split('-')
                rs.append((int(pr[0]), int(pr[len(pr)-1])))
            runconf['range'] = rs
        else:
            assert False, "unhandled option"
    return runconf

if __name__ == '__main__':
    runconf=get_runtime_options()
    pcapackets=rdpcap(runconf['input'])
    plist=[]
    count=0
    for p in pcapackets:
        if not 'UDP' in p: continue
        vpd=False
        for r in runconf['range']:
            if p['UDP'].dport >= r[0] and p['UDP'].dport <= r[1]:
                vpd=True
                break
        if not vpd: continue
        d=[]
        for i in range(6): d.append("%02x" % ord(p['Raw'].load[i]))
        dst=":".join(d)
        d=[]
        for i in range(6): d.append("%02x" % ord(p['Raw'].load[i+6]))
        src=":".join(d)
        pro=ord(p['Raw'].load[12])*0x100 + ord(p['Raw'].load[13])
        np=Ether(dst=dst, src=src, type=pro)/Raw(load=p['Raw'].load[14:])
        np.time=p.time
        plist.append(np)
    if runconf['output']:
        wrpcap(runconf['output'], plist)
