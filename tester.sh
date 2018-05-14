#! /bin/bash

set -e

build_test()
{
	echo " * trying to build module"
	cd ./src/
	make
	[ -e rtc-syntacore.ko ]
	echo " OK"
	echo
	cd ../
}

insmod_test()
{
	echo " * trying to insert module "
	insmod ./src/rtc-syntacore.ko
	[ -n "`lsmod | grep rtc_syntacore`" ]
	echo " OK"
	echo
}

cleanup()
{
	if [ $? -ne 0 ]
	then
		echo " FAIL"
		echo
	fi

	echo " * cleaning up"
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
