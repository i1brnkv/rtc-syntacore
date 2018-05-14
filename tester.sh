#! /bin/bash

build_test()
{
	echo " * trying to build module"
	cd ./src/
	make
	if [ -e rtc-syntacore.ko ]
	then
		echo " OK"
	else
		echo " FAIL"
		exit 1
	fi
	echo
	cd ../
}

insmod_test()
{
	echo " * trying to insert module "
	insmod ./src/rtc-syntacore.ko
	if [ -n "`lsmod | grep rtc_syntacore`" ]
	then
		echo " OK"
	else
		echo " FAIL"
		exit 1
	fi
	echo
}

cd `dirname $0`
build_test
insmod_test
