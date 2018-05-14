#! /bin/bash

set -e

RC='\e[1;31m'
GC='\e[1;32m'
NC='\e[0m'

DEV="NA"

build_test()
{
	echo -e " ${GC}*${NC} trying to build module"
	cd ./src/
	make
	[ -e rtc-syntacore.ko ]
	echo -e " ${GC}OK${NC}"
	echo
	cd ../
}

insmod_test()
{
	echo -e " ${GC}*${NC} trying to insert module "
	insmod ./src/rtc-syntacore.ko
	[ -n "`lsmod | grep rtc_syntacore`" ]
	echo -e " ${GC}OK${NC}"
	echo
	DEV=`dmesg | grep rtc-syntacore | grep -o "rtc[0-9]\+" | tail -1`
}

hwclock_get_test()
{
	echo -e " ${GC}*${NC} trying to get time with \"hwclock -r -f /dev/${DEV}\""
	hwclock -r -f /dev/${DEV}
	echo -e " ${GC}OK${NC}"
	echo
}

timevalid_test()
{
	echo -e " ${GC}*${NC} comparing with system time"
	typeset -i T1=`cat /sys/class/rtc/rtc0/since_epoch`
	echo "/sys/class/rtc/rtc0/since_epoch ${T1} (system)"
	typeset -i T2=`cat /sys/class/rtc/${DEV}/since_epoch`
	echo "/sys/class/rtc/${DEV}/since_epoch ${T2} (our)"
	TDIF=$(( ${T1} - ${T2} ))
	[ ${TDIF} -ge -1 ]
	[ ${TDIF} -le 1 ]
	echo -e " ${GC}OK${NC}"
	echo
}

hwclock_set_test()
{
	echo -e " ${GC}*${NC} trying to set date to 1970-01-02 with hwclock"
	hwclock -f /dev/${DEV} --set --date="1970-01-02"
	echo -n "new time is "; hwclock -r -f /dev/${DEV}
	DATE=`hwclock -r -f /dev/${DEV} | cut -f 1 -d ' '`
	[ "${DATE}" = "1970-01-02" ]
	echo -e " ${GC}OK${NC}"
	echo
}

slow_speed_test()
{
	echo -e " ${GC}*${NC} trying to set time speed to 0.6"
	echo 0.6 > /proc/rtc-syntacore/speed
	typeset -i T1=`cat /sys/class/rtc/${DEV}/since_epoch`
	for i in `seq 10`
	do
		echo -n "$(( 11 - ${i} ))..."
		sleep 1
	done
	typeset -i T2=`cat /sys/class/rtc/${DEV}/since_epoch`
	TDIF=$(( ${T2} - ${T1} ))
	echo
	echo "in 10 real seconds left ${TDIF} seconds"
	[ ${TDIF} -eq 6 ]
	echo -e " ${GC}OK${NC}"
	echo
}

cleanup()
{
	if [ $? -ne 0 ]
	then
		echo -e " ${RC}FAIL${NC}"
		echo
	fi

	echo -e " ${GC}*${NC} cleaning up"
	if [ -n "`lsmod | grep rtc_syntacore`" ]
	then
		rmmod rtc_syntacore
	fi

	if [ -e ./src/rtc-syntacore.ko ]
	then
		cd ./src/
		make clean
	fi
}

trap cleanup EXIT

cd `dirname $0`
build_test
insmod_test
hwclock_get_test
timevalid_test
hwclock_set_test
slow_speed_test
