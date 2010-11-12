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
elif [ "$1" == "--config" ] ; then
	echo "configuring..."
	WANTS_BUILD=$FALSE
	WANTS_PACKAGE=$FALSE
fi

###
###  Main Logic
### 

if [ ! -d `dirname $LOG` ] ; then
	mkdir -p `dirname $LOG`
fi

pushd indra >/dev/null
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	./develop.py clean
	find . -name "*.pyc" -exec rm {} \;
fi

if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
	./develop.py -t Release | tee $LOG
	# work around LL bug
	mkdir -p ./build-darwin-i386/newview/Release/Firestorm.app
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
	echo -n "Setting build version to "
	buildVer=`hg summary | grep parent | sed 's/parent: //' | sed 's/:.*//'`
	echo "$buildVer."
	cp llcommon/llversionviewer.h llcommon/llversionviewer.h.build
	trap "mv llcommon/llversionviewer.h.build llcommon/llversionviewer.h" INT TERM EXIT
	sed -e "s#LL_VERSION_BUILD = [0-9][0-9]*#LL_VERSION_BUILD = ${buildVer}#"\
		llcommon/llversionviewer.h.build > llcommon/llversionviewer.h
	

	echo "Building in progress. Check $LOG for verbose status."
	# -sdk macosx10.6
	xcodebuild -project build-darwin-i386/SecondLife.xcodeproj \
		-alltargets -configuration Release GCC_VERSION=4.2 \
		-sdk macosx10.6 GCC_OPTIMIZATION_LEVEL=3 ARCHS=i386 \
		GCC_ENABLE_SSE3_EXTENSIONS=YES 2>&1 | tee $LOG | \
		grep -e "[(make.*Error)|(xcodebuild.*Error)] "
	trap - INT TERM EXIT
	mv llcommon/llversionviewer.h.build llcommon/llversionviewer.h
	echo "Complete."
fi
popd > /dev/null
