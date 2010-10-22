#!/bin/bash

###
### Constants
###

TRUE=0 # Map the shell's idea of truth to a variable for better documentation
FALSE=1
LOG="`pwd`/logs/build_macosx.log"

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


pushd indra
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	./develop.py clean
	find . -name "*.pyc" -exec rm {} \;
fi

if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
	./develop.py -t Release | tee $LOG
	mkdir -p /Users/unencrypted/code/firestorm/indra/build-darwin-i386/newview/Release/Firestorm.app  # work around LL bug
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
	echo "Building in progress... Check $LOG for verbose status"
	# -sdk macosx10.6
	xcodebuild -project build-darwin-i386/SecondLife.xcodeproj -alltargets -configuration Release GCC_OPTIMIZATION_LEVEL=3 ARCHS=i386 GCC_ENABLE_SSE3_EXTENSIONS=YES 2>&1 | tee $LOG | grep -e "make.*Error "
	echo "Complete"
fi
popd
