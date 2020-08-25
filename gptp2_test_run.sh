#!/bin/sh
#
# Excelfore gptp - Implementation of gPTP(IEEE 802.1AS)
# Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
#
# This file is part of Excelfore-gptp.
#
# Excelfore-gptp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Excelfore-gptp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Excelfore-gptp.  If not, see
# <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
#
export UBL_GPTP="4,ubase:45,cbase:45,gptp:36"
if [ "$1" = "-v" ]; then
    VALGRIND_MEMCHECK="./libtool --mode=execute valgrind --leak-check=full"
fi
create_config_file()
{
    let suffix_no=$1
    let priority=$2
    let rate=$3
    let static_slave=$4
    let ovip_port=5018+${suffix_no}
    let ipc_port=${ovip_port}+500
    cat <<EOF  > gptp2_test${suffix_no}.conf
CONF_IPC_UDP_PORT ${ipc_port}
CONF_OVIP_MODE_STRT_PORTNO ${ovip_port}
CONF_PRIMARY_PRIORITY1 ${priority}
CONF_MASTER_CLOCK_SHARED_MEM "/gptp_mc_shm${suffix_no}"
CONF_PTPVFD_CLOCK_RATE ${rate}
CONF_DEBUGLOG_MEMORY_FILE "gptp2d_debugmem${suffix_no}.log"
CONF_TS2DIFF_CACHE_FACTOR 300
CONF_DEBUGLOG_MEMORY_SIZE 1024
CONF_LOG_GPTP_CAPABLE_MESSAGE_INTERVAL 1
CONF_STATIC_PORT_STATE_SLAVE_PORT ${static_slave}
EOF
}

start_gptp2d()
{
    create_config_file 0 246 $1 -1
    create_config_file 1 248 0 -1
    ./gptp2d -d cbeth0 -c gptp2_test0.conf &
    g1_pid=$!
    ./gptp2d -d cbeth1 -c gptp2_test1.conf &
    g2_pid=$!
}

stop_gptp2d()
{
    kill $g1_pid
    kill $g2_pid
}

rate_move_test()
{
    ppb=$1
    start_gptp2d $ppb
    duration=3
    sleep 1
    v1=`./gptpipcmon -d 0 -u 5519 -a 127.0.0.1 -o | sed -rn "s/GPTPD_CLOCKD domainNumber=0 portIndex=0 (.*)/\1/p"`
    if [ "${v1}" != "GM_SYNC" ]; then
	echo "can't get SYNC status"
	stop_gptp2d
	exit -1
    fi

    v1=`./gptpclock_monitor -s /gptp_mc_shm1 -o | sed -rn "s/.* ([^ ]*)$/\1/p"`
    sleep ${duration}
    v2=`./gptpclock_monitor -s /gptp_mc_shm1 -o | sed -rn "s/.* ([^ ]*)$/\1/p"`
    let rv=${v2}-${v1}
    let maxv=${duration}*$ppb+100000*${duration}
    let minv=${duration}*$ppb-100000*${duration}
    let rrate=${rv}/${duration}
    if [ ${rv} -gt ${maxv} -o ${rv} -lt ${minv} ]; then
	echo "the clock moved ${rv} nsec in ${duration} sec, rate is ${rrate} ppb, looks bad"
	stop_gptp2d
	exit -1
    fi
    echo "##### PASS #####: moved ${rrate} ppb for ${ppb} ppb config"
    stop_gptp2d
}

multi_port_close()
{
    kill ${g0_pid}
    for i in {1,3,5,7,9}; do
	kill ${g_pid[${i}]}
    done
}

multi_port_test()
{
    ppb=500000
    create_config_file 0 246 $ppb -1
    ./gptp2d -d cbeth0,cbeth1,cbeth2,cbeth3,cbeth4 -c gptp2_test0.conf &
    g0_pid=$!
    pn=5
    for i in {1,3,5,7,9}; do
	create_config_file $i 248 0 -1
	./gptp2d -d cbeth${pn} -c gptp2_test${i}.conf &
	let g_pid[${i}]=$!
	let pn=${pn}+1
    done
    sleep 3
    for i in {1,3,5,7,9}; do
	let up=5518+${i}
	v1=`./gptpipcmon -d 0 -u ${up} -a 127.0.0.1 -o | sed -rn "s/GPTPD_CLOCKD domainNumber=0 portIndex=0 (.*)/\1/p"`
	if [ "${v1}" != "GM_SYNC" ]; then
	    echo "can't get SYNC status, index=${i}"
	    multi_port_close
	    exit -1
	fi
    done
    echo "PASS: multi ports SYNCed"

    for i in {1,3,5,7,9}; do
	va[${i}]=`./gptpclock_monitor -s /gptp_mc_shm1 -o | sed -rn "s/.* ([^ ]*)$/\1/p"`
    done
    let duration=3
    sleep ${duration}
    for i in {1,3,5,7,9}; do
	vb[${i}]=`./gptpclock_monitor -s /gptp_mc_shm1 -o | sed -rn "s/.* ([^ ]*)$/\1/p"`
	let rv[${i}]=${vb[${i}]}-${va[${i}]}
	let rrate[${i}]=${rv[${i}]}/${duration}
    done
    let maxv=${duration}*$ppb+100000*${duration}
    let minv=${duration}*$ppb-100000*${duration}
    for i in {1,3,5,7,9}; do
	if [ ${rv[${i}]} -gt ${maxv} -o ${rv[${i}]} -lt ${minv} ]; then
	    echo "the clock moved ${rv[${i}]} nsec in ${duration} sec, "\
		 "rate is ${rrate[${i}]} ppb, looks bad"
	    multi_port_close
	    exit -1
	fi
	echo "PASS: moved ${rrate[${i}]} ppb for ${ppb} ppb config"
    done
    echo "##### PASS #####: multi ports test"
    multi_port_close
}

create_md_config_file()
{
    create_config_file $1 $2 $3 -1
    echo "CONF_MAX_DOMAIN_NUMBER 2" >> gptp2_test${1}.conf
    echo "CONF_SECOND_DOMAIN_THIS_CLOCK 1" >> gptp2_test${1}.conf
    echo "CONF_CMLDS_MODE 1" >> gptp2_test${1}.conf
    echo "CONF_SECONDARY_PRIORITY1 $4" >> gptp2_test${1}.conf
}

multi_domain_close()
{
    kill ${g0_pid}
    for i in {1,3}; do
	kill ${g_pid[${i}]}
    done
}

multi_domain_test()
{
    ppb=500000
    create_md_config_file 0 246 $ppb 248
    ${VALGRIND_MEMCHECK} ./gptp2d -d cbeth0,cbeth1 -c gptp2_test0.conf &
    g0_pid=$!
    create_md_config_file 1 248 0 246
    ${VALGRIND_MEMCHECK} ./gptp2d -d cbeth2 -c gptp2_test1.conf &
    g_pid[1]=$!
    create_md_config_file 3 248 0 248
    ${VALGRIND_MEMCHECK} ./gptp2d -d cbeth3 -c gptp2_test3.conf &
    g_pid[3]=$!

    sleep 10
    for i in {0,1,3}; do
	let up=5518+${i}
	./gptpipcmon -d 1 -u ${up} -a 127.0.0.1 -o > gptpipcmon${up}.log
	v1=`cat gptpipcmon${up}.log | sed -rn "s/GPTPD_CLOCKD domainNumber=0 portIndex=0 (.*)/\1/p"`
	v2=`cat gptpipcmon${up}.log | sed -rn "s/GPTPD_CLOCKD domainNumber=1 portIndex=0 (.*)/\1/p"`
	if [ "${v1}" != "GM_SYNC" -o  "${v2}" != "GM_SYNC" ]; then
	    echo "can't get SYNC status, index=${i}, ${v1}, ${v2}"
	    multi_domain_close
	    exit -1
	fi
	v1=`cat gptpipcmon${up}.log | sed -rn "s/.*lastGmFreqChange=([0-9.]*)/\1/p"`
	set $v1
	v=0
	case $i in
	    0)
		v=`echo "$4 > -0.0006 && $4 < -0.0004" | bc`
		;;
	    1)
		v=`echo "$2 > 0.0004 && $2 < 0.0006" | bc`
		;;
	    3)
		v=`echo "$2 > 0.0004 && $2 < 0.0006 && $4 > -0.0001 && $4 < 0.0001" | bc`
		;;
	esac
	if [ $v != 1 ]; then
	    echo "gptpipcmon${up}.log looks bad"
	    multi_domain_close
	    exit -1
	fi
    done
    echo "PASS: #### multi domains ####"
    multi_domain_close
}

static_config_test()
{
    create_config_file 0 246 0 0
    create_config_file 1 248 0 1
    ./gptp2d -d cbeth0 -c gptp2_test0.conf &
    g1_pid=$!
    ./gptp2d -d cbeth1 -c gptp2_test1.conf &
    g2_pid=$!
    pass_flag=true
    echo "--- master ---"
    if ./gptpipcmon -d 0 -u 5518 -a 127.0.0.1 -o | \
	    grep "domainNumber=0 portIndex=0 GM_SYNC" > /dev/null; then
	echo "Static Master immediately SYNC, PASS"
    else
	pass_flag=false
    fi
    echo "--- slave ---"
    if ./gptpipcmon -d 0 -u 5519 -a 127.0.0.1 -o | \
	    grep "domainNumber=0 portIndex=0 GM_UNSYNC" > /dev/null; then
	echo "Static Slave initially Not SYNC, PASS"
    else
	pass_flag=false
    fi
    sleep 2
    if ./gptpipcmon -d 0 -u 5519 -a 127.0.0.1 -o | \
	    grep "domainNumber=0 portIndex=0 GM_SYNC" > /dev/null; then
	echo "Static Slave SYNC in 2 sec, PASS"
    else
	pass_flag=false
    fi
    kill $g1_pid
    sleep 2
    if ./gptpipcmon -d 0 -u 5519 -a 127.0.0.1 -o | \
	    grep "domainNumber=0 portIndex=0 GM_UNSYNC" > /dev/null; then
	echo "Static Slave SYNC should be lost in 2 sec, PASS"
    else
	pass_flag=false
    fi

    if ${pass_flag}; then
	echo "PASS: #### static Master/Slave config test ####"
    fi
    kill $g2_pid
    if ! ${pass_flag}; then
	echo "FAIL: #### static Master/Slave config test ####"
	exit -1
    fi
}

rate_move_test -300000
rate_move_test 300000
multi_port_test
multi_domain_test
static_config_test
sleep 1
exit 0
