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

cd `dirname $0`
build_test
