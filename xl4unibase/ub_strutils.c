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
/*
 * ub_strutils.c
 *
 * Copyright (C) 2019 Excelfore Corporation
 * Author: Shiro Ninomiya (shiro@excelfore.com)
 */
#include "unibase_private.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

void ub_hexdump(unsigned char *buf, int size, int addr)
{
	int i;
	int p=0;
	while(size){
		ub_console_print("%08X - ", addr);
		for(i=addr&0xf;i>0;i--) ub_console_print("   ");
		if((addr&0xf)>0x8) ub_console_print(" ");
		for(i=addr&0xf;i<0x10;i++){
			if(i==0x8) ub_console_print(" ");
			ub_console_print("%02x ", buf[p++]);
			if(! --size) break;
		}
		addr=(addr+0x10)&~0xf;
		ub_console_print("\n");
	}
}

char *ub_bmac2smac(ub_macaddr_t bmac, char *smac)
{
	sprintf(smac, UB_PRIhexB6, UB_ARRAY_B6(bmac));
	return smac;
}

uint8_t *ub_smac2bmac(const char *smac, ub_macaddr_t bmac)
{
	int i;
	char astr[18];
	for(i=0;i<18;i++){
		if(!smac[i]) break;
		astr[i]=tolower(smac[i]);
	}
	astr[17]=0;
	sscanf(astr,"%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
	       bmac, bmac+1, bmac+2, bmac+3, bmac+4, bmac+5);
	return bmac;
}

char *ub_bsid2ssid(ub_streamid_t bsid, char *ssid)
{
	sprintf(ssid, UB_PRIhexB8, UB_ARRAY_B8(bsid));
	return ssid;
}

uint8_t *ub_ssid2bsid(const char *ssid, ub_streamid_t bsid)
{
	int i;
	char astr[24];
	for(i=0;i<24;i++){
		if(!ssid[i]) break;
		astr[i]=tolower(ssid[i]);
	}
	astr[23]=0;
	sscanf(astr,"%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
	       bsid, bsid+1, bsid+2, bsid+3, bsid+4, bsid+5, bsid+6, bsid+7);
	return bsid;
}

static const char *white_spaces=" \t\n\r";
int ub_find_nospace(const char *astr, int maxn)
{
	int c;
	for(c=0;*astr!=0;c++,astr++){
		if(c==maxn) return -1;
		if(!strchr(white_spaces, *astr)) return c;
	}
	return -1;
}

int ub_find_space(const char *astr, int maxn)
{
	char inq;
	int c,esc;
	for(c=0,esc=0,inq=0;*astr!=0;c++,astr++){
		if(c==maxn) return -1;
		if(esc){
			esc=0;
			continue;
		}else if(*astr=='\\'){
			esc=1;
			continue;
		}

		if(inq){
			if(*astr!=inq) continue;
			inq=0;
			continue;
		}
		if(*astr=='"' || *astr=='\''){
			inq=*astr;
			continue;
		}
		if(strchr(white_spaces, *astr)) return c;
	}
	return -1;
}

int ub_command_line_parser(char *line, char *argv[], int maxargc)
{
	int i=0,c;
	int p=0;
	int linelen;

	if(!line) return 0;
	linelen=strlen(line);
	while(i<maxargc){
		if(p>=linelen) break;
		if(i==0) {
			argv[i++]=line;
			continue;
		}
		c=ub_find_nospace(line+p, linelen-p);
		if(c==-1) {
			// strip trailing spaces
			*(line+p)=0;
			break;
		}
		p+=c;
		argv[i++]=line+p;
		c=ub_find_space(line+p, linelen-p);
		if(c<=0) break;
		p+=c;
		*(line+p)=0;
		p+=1;
	}
	// if "...", '...', remove top and bottom of quotation mark
	for(c=0;c<i;c++){
		p=strlen(argv[c]);
		if((argv[c][0]=='"' && argv[c][p-1]=='"') ||
		   (argv[c][0]=='\'' && argv[c][p-1]=='\'')){
			argv[c][p-1]=0;
			argv[c]+=1;
			continue;
		}
	}
	return i;
}
