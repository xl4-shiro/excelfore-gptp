#!/usr/bin/env python3
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
read from file:fname
pick up lines between 'smark' and 'emark' not including 'smark' line or 'emark' line
call 'item_list' for each selected line
  'item_list' creates 'items' with '"prefix" "regex selected 1st group" "suffix"'
'print_items' prints each item

This function is used to create debug strings from C source codes,
especially to create a string array from enum definitions.

>>> from io import StringIO
>>> inf=StringIO('''
... typedef enum {
... 	SAMPLE_ENUM_ITEM0 = 1,
... 	SAMPLE_ENUM_ITEM1,
... 	SAMPLE_ENUM_ITEM2,
... } sample_type_t;
... ''')
>>> oi=oneItem(None, "enum", "sample_type", r"(\S[^\s^,]*)", inf)
>>> oi.print_items("sample_type_debug[3] = {")
sample_type_debug[3] = {
	"SAMPLE_ENUM_ITEM0",
	"SAMPLE_ENUM_ITEM1",
	"SAMPLE_ENUM_ITEM2",
};
<BLANKLINE>
"""
from __future__ import print_function
import re
import sys

class oneItem(object):
    def __init__(self, fname, smark, emark, regstr, inf=None):
        if inf:
            self.inf=inf
        else:
            self.inf=open(fname,"r")
        if smark and emark: self.scanfile(smark, emark)
        if regstr: self.item_list(regstr)

    def scanfile(self, smark, emark):
        state=0
        self.linebuf=[]
        while True:
            line=self.inf.readline()
            if line=="": break
            if state==0:
                if line.find(smark)>=0: state=1
                continue
            if state==1:
                if line.find(smark)>=0:
                    self.linebuf=[]
                    continue
                if line.find(emark)>=0: break
                self.linebuf.append(line)
                continue
        self.inf.close()

    def item_list(self, regstr, prefix='"', suffix='"'):
        rea=re.compile(regstr)
        self.items=[]
        for line in self.linebuf:
            ro=rea.search(line)
            if not ro: continue
            self.items.append("%s%s%s" % (prefix, ro.group(1), suffix))

    def print_items(self, first_line, last_line="};"):
        print(first_line)
        for i in self.items:
            print("	%s," % i)
        print(last_line)
        print()

if __name__ == "__main__":
    import doctest
    doctest.testmod(optionflags=doctest.NORMALIZE_WHITESPACE)
