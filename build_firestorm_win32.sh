#!/bin/bash

###
### Constants
###

TRUE=0 # Map the shell's idea of truth to a variable for better documentation
FALSE=1
PYTHON="/cygdrive/c/Python26/python.exe"
LOG="`pwd`/logs/build_win32.log"

###
### Global Variables
###

WANTS_CLEAN=$TRUE
WANTS_CONFIG=$TRUE
WANTS_BUILD=$TRUE
WANTS_PACKAGE=$TRUE

###
### Helper Functions
###

if [ "$1" == "--rebuild" ] ; then
	echo "rebuilding..."
	WANTS_CLEAN=$FALSE
	WANTS_CONFIG=$FALSE
fi

###
###  Main Logic
### 

path=$WINPATH:/usr/local/bin:/usr/bin:/bin
if [ ! -f "$PYTHON" ] ; then
	echo "ERROR: You need to edit this file and set PYTHON at the top to point at the path of your python executable."
	exit 1
fi

pushd indra
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	$PYTHON develop.py clean
	find . -name "*.pyc" -exec rm {} \;
fi


if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
	$PYTHON develop.py -G vc80 -t Release configure -DPACKAGE:BOOL=ON -DLL_TESTS:BOOL=OFF  2>&1 | tee $LOG
	mkdir -p build-vc80/sharedlibs/RelWithDebInfo
	mkdir -p build-vc80/sharedlibs/Release
	cp fmod.dll ./build-vc80/sharedlibs/RelWithDebInfo
	cp fmod.dll ./build-vc80/sharedlibs/Release
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
        echo -n "Updating build version to "
        buildVer=`grep -o -e "LL_VERSION_BUILD = [0-9]\+" llcommon/llversionviewer.h | cut -f 3 -d " "`
        echo $((++buildVer))
        sed -e "s#LL_VERSION_BUILD = [0-9][0-9]*#LL_VERSION_BUILD = ${buildVer}#" llcommon/llversionviewer.h > llcommon/llversionviewer.h1
        mv llcommon/llversionviewer.h1 llcommon/llversionviewer.h


	echo "Building in progress... Check $LOG for verbose status"
	$PYTHON develop.py -G vc80 -t Release build  2>&1 | tee -a $LOG | grep Build
	echo "Complete"
fi
popd

