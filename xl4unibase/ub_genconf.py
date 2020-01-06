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
import re
import sys
import getopt

class DefaultHeader(object):
    def __init__(self, fname="defaults.h"):
        inf=open(fname)
        recomp=re.compile(r"^\s*#define\s+DEFAULT_(\S+)\s+(\S+).*")
        recomps=re.compile(r"^\s*#define\s+DEFAULT_(\S+)\s+(\".*\").*max_length *= *([0-9]*).*")
        recompss=re.compile(r"^\s*#define\s+DEFAULT_(\S+)\s+(\".*\").*")
        slen=0
        self.default_array=[]
        while True:
            line=inf.readline()
            if line=="": break
            r=recomp.match(line)
            if not r: continue
            if r.group(2)[0]=='"':
                r=recomps.match(line)
                if r:
                    slen=int(r.group(3))
                else:
                    r=recompss.match(line)
                    if not r: continue

            self.default_array.append(("CONF_%s" % r.group(1), r.group(2),
                                       self.get_value(r.group(2)), slen))

    def get_value(self, vstr):
        if vstr[0]=='"':
            try:
                a=eval(vstr)
                return ("string",a,None)
            except:
                pass

        if vstr.find(":")>0:
            a=[]
            s=0
            while True:
               i=vstr[s:].find(":")
               if i<0:
                   a.append(int(vstr[s:],16))
                   break
               a.append(int(vstr[s:s+i],16))
               s=s+i+1
            return ("bytes",a,None)

        try:
            if vstr.find("0x")==0:
                a=int(vstr,16)
                base=16
            else:
                a=int(vstr)
                base=10
            if a<0x80000000 and a>=-0x80000000:
                return ("int",a,base)
            else:
                return ("int64",a,base)
        except:
            pass

        return ("label",vstr,None)

    def print_enum(self, outf, pf):
        outf.write("typedef enum {\n")
        for item in self.default_array:
            outf.write("\t%s,\n" % item[0])
        outf.write("\tCONF_ENUM_LAST_ITEM\n")
        outf.write("} %s_config_item_t;\n" % pf)
        outf.write("\n")

    def print_default_defs(self, outf):
        for item in self.default_array:
            if item[2][0]=='int':
                if item[2][2]==16:
                    outf.write("static int32_t value_%s=0x%X;\n" % (item[0], item[2][1]))
                else:
                    outf.write("static int32_t value_%s=%d;\n" % (item[0], item[2][1]))
            elif item[2][0]=='int64':
                if item[2][2]==16:
                    outf.write("static int64_t value_%s=0x%X;\n" % (item[0], item[2][1]))
                else:
                    outf.write("static int64_t value_%s=%d;\n" % (item[0], item[2][1]))
            elif item[2][0]=='string':
                sa=["'%c'" % i for i in item[2][1]]
                sa.append("0");
                if item[3]==0:
                    outf.write("static char value_%s[]={%s};\n" % \
                        (item[0], ",".join(sa)))
                else:
                    outf.write("static char value_%s[%d]={%s};\n" % \
                        (item[0], item[3], ",".join(sa)))
            elif item[2][0]=='bytes':
                outf.write("static uint8_t value_%s[]={%s};\n" % \
                           (item[0], ",".join(hex(i) for i in item[2][1])))
            elif item[2][0]=='label':
                outf.write("static int32_t value_%s=%s;\n" % (item[0], item[2][1]))
        print

    def print_get_function(self, outf, pf):
        outf.write("void *%sconf_get_item(%s_config_item_t item)\n" % (pf, pf))
        outf.write("{\n")
        outf.write("\tswitch(item) {\n")
        for item in self.default_array:
            outf.write("\tcase %s:\n" % item[0])
            if item[2][0]=='int':
                outf.write("\t\treturn (void*)&value_%s;\n" % (item[0]))
            elif item[2][0]=='int64':
                outf.write("\t\treturn (void*)&value_%s;\n" % (item[0]))
            elif item[2][0]=='string':
                outf.write("\t\treturn (void*)value_%s;\n" % (item[0]))
            elif item[2][0]=='bytes':
                outf.write("\t\treturn (void*)value_%s;\n" % (item[0]))
            elif item[2][0]=='label':
                outf.write("\t\treturn (void*)&value_%s;\n" % (item[0]))
        outf.write("\tdefault:\n")
        outf.write("\t\treturn NULL;\n")
        outf.write("\t}\n")
        outf.write("}\n")
        outf.write("\n")
        outf.write("int32_t %sconf_get_intitem(%s_config_item_t item)\n" % (pf, pf))
        outf.write("{\n")
	outf.write("\treturn *((int32_t *)%sconf_get_item(item));\n" % pf)
        outf.write("}\n")
        outf.write("\n")
        outf.write("int64_t %sconf_get_lintitem(%s_config_item_t item)\n" % (pf, pf))
        outf.write("{\n")
	outf.write("\treturn *((int64_t *)%sconf_get_item(item));\n" % pf)
        outf.write("}\n")
        outf.write("\n")

    def print_set_function(self, outf, pf):
        outf.write("int %sconf_set_item(%s_config_item_t item, void *v)\n" % (pf, pf))
        outf.write("{\n")
        outf.write("\tswitch(item) {\n")
        for item in self.default_array:
            outf.write("\tcase %s:\n" % item[0])
            if item[2][0]=='int':
                outf.write("\t\tvalue_%s=*((int32_t*)v);\n" % (item[0]))
                outf.write("\t\tbreak;\n")
            elif item[2][0]=='int64':
                outf.write("\t\tvalue_%s=*((int64_t*)v);\n" % (item[0]))
                outf.write("\t\tbreak;\n")
            elif item[2][0]=='string':
                outf.write("\t\tif(sizeof(value_%s)>0)\n" % item[0])
                outf.write("\t\t\tstrncpy(value_%s, (char *)v, sizeof(value_%s)-1);\n" %
                           (item[0],item[0]))
                outf.write("\t\tbreak;\n")
            elif item[2][0]=='bytes':
                outf.write("\t\tmemcpy(value_%s, (uint8_t *)v, sizeof(value_%s));\n" %
                           (item[0],item[0]))
                outf.write("\t\tbreak;\n")
            elif item[2][0]=='label':
                outf.write("\t\tvalue_%s=*((int32_t*)v);\n" % (item[0]))
                outf.write("\t\tbreak;\n")
        outf.write("\tdefault:\n")
        outf.write("\t\treturn -1;\n")
        outf.write("\t}\n")
        outf.write("\treturn 0;\n")
        outf.write("}\n")
        outf.write("\n")

    def print_config_item_strings(self, outf):
        outf.write("static char *config_item_strings[]={\n")
        for item in self.default_array:
            outf.write("\t\"%s\",\n" % item[0])
        outf.write("};\n")
        outf.write("\n")

    def print_get_item_num_function(self, outf, pf):
        outf.write("%s_config_item_t %sconf_get_item_num(char *istr)\n" % (pf, pf))
        outf.write("{\n")
        outf.write("\tint i;\n")
        outf.write("\tfor(i=0;i<CONF_ENUM_LAST_ITEM;i++)\n")
        outf.write("\t\tif(!strcmp(istr, config_item_strings[i])) return (%s_config_item_t)i;\n"
                   % pf)
        outf.write("\treturn CONF_ENUM_LAST_ITEM;\n")
        outf.write("}\n")
        outf.write("\n")

    def print_set_stritem_function(self, outf, pf):
        outf.write("int %sconf_set_stritem(char *istr, void *v)\n" % pf)
        outf.write("{\n")
        outf.write("\tint item;\n")
        outf.write("\titem=%sconf_get_item_num(istr);\n" % pf)
        outf.write("\tif(item<0) return -1;\n")
        outf.write("\treturn %sconf_set_item(item, v);\n" % pf)
        outf.write("}\n")
        outf.write("\n")

    def print_unittest_items(self, outf, pf):
        for item in self.default_array:
            if item[2][0].find('int')==0:
                if item[2][0]=='int64':
                    lintsuf='l'
                    lint=64
                    cast='(int64_t)'
                else:
                    lintsuf=''
                    lint=32
                    cast='(int32_t)'
                outf.write("\tif(%sconf_get_%sintitem(%s) != %s){\n" % \
                           (pf, lintsuf, item[0], item[0].replace('CONF','DEFAULT')))
                outf.write('\t\tprintf("%s default=%%"PRIi%d", configured=%%"PRIi%d"\\n",\n' % \
                    (item[0].replace('CONF_',''),lint,lint))
                outf.write('\t\t\t%s%s,\n' % (cast, item[0].replace('CONF','DEFAULT')))
                outf.write('\t\t\t%sconf_get_%sintitem(%s));\n' % (pf, lintsuf, item[0]))
                outf.write('\t\tres++;\n')
                outf.write('\t}\n')
            elif item[2][0]=='string':
                outf.write("\tif(strcmp((char*)%sconf_get_item(%s), %s)){\n" % \
                    (pf, item[0], item[0].replace('CONF','DEFAULT')))
                outf.write('\t\tprintf("%s default=%%s, configured=%%s\\n",\n' % \
                    item[0].replace('CONF_',''))
                outf.write('\t\t\t%s,\n' % item[0].replace('CONF','DEFAULT'))
                outf.write('\t\t\t(char*)%sconf_get_item(%s));\n' % (pf, item[0]))
                outf.write('\t\tres++;\n')
                outf.write('\t}\n')
            elif item[2][0]=='bytes':
                outf.write("\t{\n")
                dbs=["0x%X" % v for v in item[2][1]]
                outf.write("\t\tuint8_t db[]={%s};\n" % ','.join(dbs))
                outf.write("\t\tif(memcmp(%sconf_get_item(%s), db, %d)){\n" % \
                           (pf, item[0], len(item[2][1])))
                outf.write('\t\t\tprintf("%s difference in default and configured\\n");\n' % \
                           item[0].replace('CONF_',''))
                outf.write('\t\t\tres++;\n')
                outf.write('\t\t}\n')
                outf.write('\t}\n')
            elif item[2][0]=='label':
                outf.write("\n")

def print_file_head(outf, hfile):
    outf.write("/* Don't edit this file.  This is an automatically generated file. */\n")
    outf.write("#include <string.h>\n")
    outf.write('#include "%s"\n' % hfile[hfile.rfind('/')+1:])

def print_header_file_head(outf, pf, exth):
    outf.write("/* Don't edit this file.  This is an automatically generated file. */\n")
    outf.write("#ifndef __%s_CONFING_H_\n" % pf.upper())
    outf.write("#define __%s_CONFING_H_\n" % pf.upper())
    outf.write("#include <inttypes.h>\n")
    if exth:
        for f in exth.split(','):
            outf.write('#include "%s"\n' % f)

def print_header_file_foot(outf):
    outf.write("#endif\n")

def print_unittest_head(outf, pf, ifile, hfile):
    outf.write("/* Don't edit this file.  This is an automatically generated file. */\n")
    outf.write("#include <stdio.h>\n")
    outf.write("#include <string.h>\n")
    outf.write("#include <inttypes.h>\n")
    outf.write('#include "%s"\n' % ifile[ifile.rfind('/')+1:])
    outf.write('#include "%s"\n' % hfile[hfile.rfind('/')+1:])
    outf.write("\n")
    outf.write("int %sconf_values_test(void)\n" % pf)
    outf.write("{\n")
    outf.write("\tint res=0;\n")

def print_usage():
    pname=sys.argv[0][sys.argv[0].rfind('/')+1:]
    print "%s [options]" % pname
    print "    -h|--help: this help"
    print "    -i|--input: input file name"
    print "    -c|--cfile: output c source file name"
    print "    -d|--hfile: output header file name"
    print "    -v|--vfile: output values test file name"
    print "    -e|--exth: extra header files(comma separated list)"

def set_options():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hi:c:d:v:p:e:",
                                   ["help", "input=", "cfile=", "hfile=",
                                    "vfile=", "prefix=", "exth="])
    except getopt.GetoptError as err:
        # print help information and exit:
        print str(err)  # will print something like "option -a not recognized"
        print_usage()
        sys.exit(1)
    res={'input':None, 'prefix':None, 'exth':None}
    for o, a in opts:
        if o in ("-h", "--help"):
            print_usage()
            sys.exit(0)
        elif o in ("-i", "--input"):
            res['input'] = a
        elif o in ("-c", "--cfile"):
            res['cfile'] = a
        elif o in ("-d", "--hfile"):
            res['hfile'] = a
        elif o in ("-v", "--vfile"):
            res['vfile'] = a
        elif o in ("-p", "--prefix"):
            res['prefix'] = a
        elif o in ("-e", "--exth"):
            res['exth'] = a
        else:
            assert False, "unhandled option"
    if not res['prefix']:
        print "need 'prefix' option"
        print_usage()
        sys.exit(1)
    return res

if __name__ == "__main__":
    options=set_options()
    pf=options['prefix']
    dh=DefaultHeader(fname=options['input'])
    if options['hfile']:
        outf=open(options['hfile'], "w")
        print_header_file_head(outf, pf, options['exth'])
        dh.print_enum(outf, pf)
        outf.write("void *%sconf_get_item(%s_config_item_t item);\n" % (pf, pf))
        outf.write("int32_t %sconf_get_intitem(%s_config_item_t item);\n" % (pf, pf))
        outf.write("int64_t %sconf_get_lintitem(%s_config_item_t item);\n" % (pf, pf))
        outf.write("int %sconf_set_item(%s_config_item_t item, void *v);\n" % (pf, pf))
        outf.write("%s_config_item_t %sconf_get_item_num(char *istr);\n" % (pf, pf))
        outf.write("int %sconf_set_stritem(char *istr, void *v);\n" % pf)
        outf.write("\n")
        print_header_file_foot(outf)
        outf.close()

    if not options['hfile']: options['hfile']=options['cfile'][:-1]+"h"
    if options['vfile']:
        outf=open(options['vfile'], "w")
        print_unittest_head(outf, pf, options['input'], options['hfile'])
        dh.print_unittest_items(outf, pf)
        outf.write("\treturn res;\n")
        outf.write("}\n")
        outf.close()

    if options['cfile']:
        outf=open(options['cfile'], "w")
        print_file_head(outf, options['hfile'])
        dh.print_config_item_strings(outf)
        dh.print_default_defs(outf)
        dh.print_get_function(outf, pf)
        dh.print_set_function(outf, pf)
        dh.print_get_item_num_function(outf, pf)
        dh.print_set_stritem_function(outf, pf)
        outf.close()

    sys.exit(0)
