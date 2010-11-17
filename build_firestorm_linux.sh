#!/bin/bash

###
### Constants
###

TRUE=0 # Map the shell's idea of truth to a variable for better documentation
FALSE=1
LOG="`pwd`/logs/build_linux.log"

###
### Global Variables
###

WANTS_CLEAN=$TRUE
WANTS_CONFIG=$TRUE
WANTS_BUILD=$TRUE
WANTS_PACKAGE=$TRUE
WANTS_VERSION=$TRUE

###
### Helper Functions
###

if [ "$1" == "--rebuild" ] ; then
	echo "rebuilding..."
	WANTS_CLEAN=$FALSE
	WANTS_CONFIG=$FALSE
	WANTS_VERSION=$FALSE
elif [ "$1" == "--config" ] ; then
	echo "configuring..."
	WANTS_BUILD=$FALSE
	WANTS_PACKAGE=$FALSE
fi

###
###  Main Logic
### 

export CC=/usr/bin/gcc-4.3
export CXX=/usr/bin/g++-4.3
export CMAKE_CXX_FLAGS_RELEASE="-O3 -msse -msse2" 
if [ ! -d `dirname $LOG` ] ; then
        mkdir -p `dirname $LOG`
fi

pushd indra > /dev/null
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	./develop.py -t release clean
	find . -name "*.pyc" -exec rm {} \;
fi

if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
	./develop.py -t release | tee $LOG
fi

if [ $WANTS_VERSION -eq $TRUE ] ; then
	echo -n "Setting build version to "
	buildVer=`hg summary | grep parent | sed 's/parent: //' | sed 's/:.*//'`
	echo "$buildVer."
	cp llcommon/llversionviewer.h llcommon/llversionviewer.h.build
	trap "mv llcommon/llversionviewer.h.build llcommon/llversionviewer.h" INT TERM EXIT
	sed -e "s#LL_VERSION_BUILD = [0-9][0-9]*#LL_VERSION_BUILD = ${buildVer}#"\
		llcommon/llversionviewer.h.build > llcommon/llversionviewer.h
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
	echo "Building in progress. Check $LOG for verbose status."
	./develop.py -t release build 2>&1 | tee $LOG | grep -e "make.*Error "
	trap - INT TERM EXIT
	mv llcommon/llversionviewer.h.build llcommon/llversionviewer.h
	echo "Complete"
fi
popd > /dev/null
