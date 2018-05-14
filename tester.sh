#! /bin/bash

set -e

RC='\e[1;31m'
GC='\e[1;32m'
NC='\e[0m'

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
