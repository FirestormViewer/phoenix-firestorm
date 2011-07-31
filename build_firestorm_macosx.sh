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

WANTS_CLEAN=$FALSE
WANTS_CONFIG=$FALSE
WANTS_BUILD=$FALSE
WANTS_PACKAGE=$FALSE
WANTS_VERSION=$FALSE
WANTS_KDU=$FALSE
BTYPE="Release"
CHANNEL="private-`hostname -s`"

###
### Helper Functions
###

showUsage()
{
        echo
        echo "Usage: "
        echo "========================"
        echo
        echo "  --clean     : Remove past builds & configuration"
        echo "  --config    : General a new architecture-specific config"
        echo "  --version   : Update version number"
        echo "  --rebuild   : Build, reusing unchanged projects to save time"
        echo "  --chan [Release|Beta|Private] : Private is the default, sets channel"
        echo "  --btype [Release|RelWithDebInfo] : Release is default, whether to use symbols"
        echo "  --kdu       : Build with KDU"
}

getArgs()
# $* = the options passed in from main
{
        if [ $# -gt 0 ]; then
          while getoptex "clean config version kdu rebuild help chan: btype:" "$@" ; do

	      #insure options are valid
	      if [  -z "$OPTOPT"  ] ; then
                showUsage
                exit 1
              fi
        
              case "$OPTOPT" in
              clean)    WANTS_CLEAN=$TRUE;;
              config)   WANTS_CONFIG=$TRUE;;
              version)  WANTS_VERSION=$TRUE;;
              rebuild)  WANTS_BUILD=$TRUE
                        WANTS_VERSION=$TRUE
                        WANTS_PACKAGE=$TRUE;;
              chan)     CHANNEL="$OPTARG";;
              btype)    BTYPE="$OPTARG";;
              kdu)      WANTS_KDU=$TRUE;;
            
              help)     showUsage && exit 0;;
            
              -*)       showUsage && exit 1;;
              *)        showUsage && exit 1;;            
              esac
          done
          shift $[OPTIND-1]
          if [ $OPTIND -le 1 ] ; then
              showUsage && exit 1
          fi
        fi

        if [ $WANTS_CLEAN -ne $TRUE ] && [ $WANTS_CONFIG -ne $TRUE ] && \
        	[ $WANTS_BUILD -ne $TRUE ] && [ $WANTS_VERSION -ne $TRUE ] && \
        	[ $WANTS_PACKAGE -ne $TRUE ] ; then
        # the user didn't say what to do, so assume he wants to do everything
              WANTS_CLEAN=$TRUE
              WANTS_CONFIG=$TRUE
              WANTS_BUILD=$TRUE
              WANTS_VERSION=$TRUE
              WANTS_PACKAGE=$TRUE
        fi
}


function getoptex()
{
  let $# || return 1
  local optlist="${1#;}"
  let OPTIND || OPTIND=1
  [ $OPTIND -lt $# ] || return 1
  shift $OPTIND
  if [ "$1" != "-" -a "$1" != "${1#-}" ]
  then OPTIND=$[OPTIND+1]; if [ "$1" != "--" ]
  then
        local o
        o="-${1#-$OPTOFS}"
        for opt in ${optlist#;}
        do
          OPTOPT="${opt%[;.:]}"
          unset OPTARG
          local opttype="${opt##*[^;:.]}"
          [ -z "$opttype" ] && opttype=";"
          if [ ${#OPTOPT} -gt 1 ]
          then # long-named option
                case $o in
                  "--$OPTOPT")
                        if [ "$opttype" != ":" ]; then return 0; fi
                        OPTARG="$2"
                        if [ -z "$OPTARG" ];
                        then # error: must have an agrument
                          let OPTERR && echo "$0: error: $OPTOPT must have an argument" >&2
                          OPTARG="$OPTOPT";
                          OPTOPT="?"
                          return 1;
                        fi
                        OPTIND=$[OPTIND+1] # skip option's argument
                        return 0
                  ;;
                  "--$OPTOPT="*)
                        if [ "$opttype" = ";" ];
                        then  # error: must not have arguments
                          let OPTERR && echo "$0: error: $OPTOPT must not have arguments" >&2
                          OPTARG="$OPTOPT"
                          OPTOPT="?"
                          return 1
                        fi
                        OPTARG=${o#"--$OPTOPT="}
                        return 0
                  ;;
                esac
          else # short-named option
                case "$o" in
                  "-$OPTOPT")
                        unset OPTOFS
                        [ "$opttype" != ":" ] && return 0
                        OPTARG="$2"
                        if [ -z "$OPTARG" ]
                        then
                          echo "$0: error: -$OPTOPT must have an argument" >&2
                          OPTARG="$OPTOPT"
                          OPTOPT="?"
                          return 1
                        fi
                        OPTIND=$[OPTIND+1] # skip option's argument
                        return 0
                  ;;
                  "-$OPTOPT"*)
                        if [ $opttype = ";" ]
                        then # an option with no argument is in a chain of options
                          OPTOFS="$OPTOFS?" # move to the next option in the chain
                          OPTIND=$[OPTIND-1] # the chain still has other options
                          return 0
                        else
                          unset OPTOFS
                          OPTARG="${o#-$OPTOPT}"
                          return 0
                        fi
                  ;;
                esac
          fi
        done
        echo "$0: error: invalid option: $o"
	showUsage
	exit 1
  fi; fi
  OPTOPT="?"
  unset OPTARG
  return 1
}

function optlistex
{
  local l="$1"
  local m # mask
  local r # to store result
  while [ ${#m} -lt $[${#l}-1] ]; do m="$m?"; done # create a "???..." mask
  while [ -n "$l" ]
  do
        r="${r:+"$r "}${l%$m}" # append the first character of $l to $r
        l="${l#?}" # cut the first charecter from $l
        m="${m#?}"  # cut one "?" sign from m
        if [ -n "${l%%[^:.;]*}" ]
        then # a special character (";", ".", or ":") was found
          r="$r${l%$m}" # append it to $r
          l="${l#?}" # cut the special character from l
          m="${m#?}"  # cut one more "?" sign
        fi
  done
  echo $r
}

function getopt()
{
  local optlist=`optlistex "$1"`
  shift
  getoptex "$optlist" "$@"
  return $?
}


###
###  Main Logic
### 

getArgs $*

if [ ! -d `dirname $LOG` ] ; then
	mkdir -p `dirname $LOG`
fi

pushd indra >/dev/null
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	./develop.py clean
	find . -name "*.pyc" -exec rm {} \;
fi

if [ \( $WANTS_VERSION -eq $TRUE \) -o \( $WANTS_CONFIG -eq $TRUE \) ] ; then
        buildVer=`hg summary | head -1 | cut -d " "  -f 2 | cut -d : -f 1`
        majorVer=`cat Version | cut -d "=" -f 2 | cut -d "." -f 1`
        minorVer=`cat Version | cut -d "=" -f 2 | cut -d "." -f 2`
        patchVer=`cat Version | cut -d "=" -f 2 | cut -d "." -f 3`
        echo "Building $CHANN- ${majorVer}.${minorVer}.${patchVer}.${buildVer}"
        sed -e "s#LL_VERSION_BUILD = .*\$#LL_VERSION_BUILD = ${buildVer};#" \
            -e "s#LL_VERSION_MAJOR = .*\$#LL_VERSION_MAJOR = ${majorVer};#" \
            -e "s#LL_VERSION_MINOR = .*\$#LL_VERSION_MINOR = ${minorVer};#" \
            -e "s#LL_VERSION_PATCH = .*\$#LL_VERSION_PATCH = ${patchVer};#" \
            -e "s#LL_CHANNEL = .*\$#LL_CHANNEL = \"Firestorm-$CHANNEL\";#" llcommon/llversionviewer.cpp.in > llcommon/llversionviewer.cpp
        sed -e "s/@VERSION_VIEWER_MAJOR@/${majorVer}/" \
            -e "s/@VERSION_VIEWER_MINOR@/${minorVer}/" \
            -e "s/@VERSION_VIEWER_PATCH@/${patchVer}/" \
            -e "s/@VERSION_BUILD@/${buildVer}/" newview/Info-Firestorm.plist.in > newview/info-Firestorm.plist
        sed -e "s/@VERSION_VIEWER_MAJOR@/${majorVer}/" \
            -e "s/@VERSION_VIEWER_MINOR@/${minorVer}/" \
            -e "s/@VERSION_VIEWER_PATCH@/${patchVer}/" \
            -e "s/@VERSION_BUILD@/${buildVer}/" newview/English.lproj/InfoPlist.strings.in > newview/English.lproj/InfoPlist.strings
fi


if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
        if [ $WANTS_KDU -eq $TRUE ] ; then
		KDU="-DUSE_KDU:BOOL=ON"
	else
		KDU=""
	fi
	./develop.py -t $BTYPE configure $KDU -DPACKAGE:BOOL=ON -DLL_TESTS:BOOL=OFF -DVIEWER_CHANNEL:STRING=Firestorm-$CHANNEL -DVIEWER_LOGIN_CHANNEL:STRING=Firestorm-$CHANNEL 2>&1 | tee $LOG
	# LL build wants this directory to exist, but doesn't make it itself.
	if [ -f "./build-darwin-i386/SecondLife.xcodeproj" ]; then
		mkdir -p ./build-darwin-i386/newview/Release/Firestorm.app
	elif [ -f "./build-darwin-i686/SecondLife.xcodeproj" ]; then
		mkdir -p ./build-darwin-i686/newview/Release/Firestorm.app
	fi
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then

	echo "Building in progress. Check $LOG for verbose status."
	# -sdk macosx10.6
	PROJDIR="build-darwin-i386/SecondLife.xcodeproj"
	if [ ! -d $PROJDIR ]; then
		PROJDIR="build-darwin-i686/SecondLife.xcodeproj"
		if [ ! -d $PROJDIR ]; then
			echo "Could not find Xcode project file."
			exit 2
		fi
	fi
	xcodebuild -project $PROJDIR -alltargets -configuration $BTYPE \
		MACOSX_DEPLOYMENT_TARGET=10.4 GCC_VERSION=4.2 \
		-sdk macosx10.6 GCC_OPTIMIZATION_LEVEL=3 ARCHS=i386 \
		GCC_ENABLE_SSE3_EXTENSIONS=YES 2>&1 | tee $LOG | \
		grep -e "[(make.*Error)|(xcodebuild.*Error)] "
	trap - INT TERM EXIT
	echo "Complete."
fi
popd > /dev/null
