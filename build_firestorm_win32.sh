#!/bin/bash

###
### Constants
###

TRUE=0 # Map the shell's idea of truth to a variable for better documentation
FALSE=1
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

pushd indra
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	cmd /c "develop.py clean"
	find . -name "*.pyc" -exec rm {} \;
fi


if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
	cmd /c "develop.py -G vc80 configure -DLL_TESTS:BOOL=OFF" | tee $LOG
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
	echo "Building in progress... Check $LOG for verbose status"
	cmd /c "develop.py build" | tee -a $LOG
	echo "Complete"
fi
popd

