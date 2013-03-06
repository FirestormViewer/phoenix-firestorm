#!/bin/bash

###
### Constants
###

TRUE=0  # Map the shell's idea of truth to a variable for better documentation
FALSE=1

#echo "DEBUG ARGS: $@"
#echo "DEBUG `pwd`"

# args ../indra
#                  <string>-DCMAKE_BUILD_TYPE:STRING=Release</string>
#                  <string>-DWORD_SIZE:STRING=32</string>
#                  <string>-DROOT_PROJECT_NAME:STRING=SecondLife</string>
#                  <string>-DINSTALL_PROPRIETARY=FALSE</string>
#                  <string>-DUSE_KDU=TRUE</string>
#                  <string>-DFMOD=TRUE</string>
#                  <string>-DLL_TESTS:BOOL=OFF</string>
#                  <string>-DPACKAGE:BOOL=OFF></string>


###
### Global Variables
###

WANTS_CLEAN=$FALSE
WANTS_CONFIG=$FALSE
WANTS_PACKAGE=$FALSE
WANTS_VERSION=$FALSE
WANTS_KDU=$FALSE
WANTS_FMOD=$FALSE
WANTS_BUILD=$FALSE
PLATFORM="darwin" # darwin, win32, win64, linux32, linux64
BTYPE="Release"
CHANNEL="" # will be overwritten later with platform-specific values unless manually specified.
LL_ARGS_PASSTHRU=""
JOBS="0"

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
    echo "  --build    : build firestorm"
    echo "  --version   : Update version number"
    echo "  --chan  [Release|Beta|Private]   : Private is the default, sets channel"
    echo "  --btype [Release|RelWithDebInfo] : Release is default, whether to use symbols"
    echo "  --kdu       : Build with KDU"
    echo "  --package   : Build installer"
    echo "  --fmod      : Build fmod"
    echo "  --platform  : darwin | win32 | win64 | linux32 | linux64"
    echo "  --jobs <num>: Build with <num> jobs in parallel (Linux and Darwin only)"
    echo
    echo "All arguments not in the above list will be passed through to LL's configure/build"
    echo
}

getArgs()
# $* = the options passed in from main
{
    if [ $# -gt 0 ]; then
      while getoptex "clean build config version package fmod jobs: platform: kdu help chan: btype:" "$@" ; do

          #insure options are valid
          if [  -z "$OPTOPT"  ] ; then
                showUsage
                exit 1
          fi

          case "$OPTOPT" in
          clean)    WANTS_CLEAN=$TRUE;;
          config)   WANTS_CONFIG=$TRUE;;
          version)  WANTS_VERSION=$TRUE;;
          chan)     CHANNEL="$OPTARG";;
          btype)    if [ \( "$OPTARG" == "Release" \) -o \( "$OPTARG" == "RelWithDebInfo" \) -o \( "$OPTARG" == "Debug" \) ] ; then
                      BTYPE="$OPTARG"
                    fi
                    ;;
          kdu)      WANTS_KDU=$TRUE;;
          fmod)    WANTS_FMOD=$TRUE;;
          package)    WANTS_PACKAGE=$TRUE;;
          build)    WANTS_BUILD=$TRUE;;
          platform)    PLATFORM="$OPTARG";;
          jobs)    JOBS="$OPTARG";;

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
           [ $WANTS_VERSION -ne $TRUE ] && [ $WANTS_BUILD -ne $TRUE ] && \
           [ $WANTS_PACKAGE -ne $TRUE ] ; then
        # the user didn't say what to do, so assume he wants to do a basic rebuild
              WANTS_CONFIG=$TRUE
          WANTS_BUILD=$TRUE
              WANTS_VERSION=$TRUE
        fi

    LOG="`pwd`/logs/build_$PLATFORM.log"
    if [ -r "$LOG" ] ; then
        rm -f `basename "$LOG"`/* #(remove old logfiles)
    fi
}

function b2a()
{
  if [ $1 -eq $TRUE ] ; then
    echo "true"
  else
    echo "false"
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
        #echo "$0: error: invalid option: $o"
    LL_ARGS_PASSTHRU="$LL_ARGS_PASSTHRU $o"
    return 0
    #showUsage
    #exit 1
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
if [ ! -d `dirname "$LOG"` ] ; then
        mkdir -p `dirname "$LOG"`
fi

echo -e "configure_firestorm.py" > $LOG
echo -e "       PLATFORM: '$PLATFORM'"       | tee -a $LOG
echo -e "         KDU: `b2a $WANTS_KDU`"     | tee -a $LOG
echo -e "        FMOD: `b2a $WANTS_FMOD`"    | tee -a $LOG
echo -e "     PACKAGE: `b2a $WANTS_PACKAGE`" | tee -a $LOG
echo -e "       CLEAN: `b2a $WANTS_CLEAN`"   | tee -a $LOG
echo -e "       BUILD: `b2a $WANTS_BUILD`"   | tee -a $LOG
echo -e "      CONFIG: `b2a $WANTS_CONFIG`"  | tee -a $LOG
echo -e "    PASSTHRU: $LL_ARGS_PASSTHRU"    | tee -a $LOG
echo -e "       BTYPE: $BTYPE"               | tee -a $LOG
if [ $PLATFORM == "linux32" -o $PLATFORM == "linux64" -o $PLATFORM == "darwin" ] ; then
    echo -e "        JOBS: $JOBS"                | tee -a $LOG
fi
echo -e "       Logging to $LOG"


if [ $PLATFORM == "win32" ] ; then
    FIND=/usr/bin/find
else
    FIND=find
fi


if [ -z $CHANNEL ] ; then
    if [ $PLATFORM == "darwin" ] ; then
        CHANNEL="private-`hostname -s` "
    else
        CHANNEL="private-`hostname`"
    fi
else
    CHANNEL=`echo $CHANNEL | sed -e "s/[^a-zA-Z0-9\-]*//g"` # strip out difficult characters from channel
fi

if [ \( $WANTS_CLEAN -eq $TRUE \) -a \( $WANTS_BUILD -eq $FALSE \) ] ; then
    echo "Cleaning $PLATFORM...."
    wdir=`pwd`
    pushd ..

    if [ $PLATFORM == "darwin" ] ; then
        rm -rf build-darwin-i386/*
        mkdir -p build-darwin-i386/logs

    elif [ $PLATFORM == "win32" ] ; then
        rm -rf build-vc100/*
        mkdir -p build-vc100/logs

    elif [ $PLATFORM == "linux32" ] ; then
        rm -rf build-linux-i686/*
        mkdir -p build-linux-i686/logs
    fi

    popd
fi

if [ \( $WANTS_VERSION -eq $TRUE \) -o \( $WANTS_CONFIG -eq $TRUE \) ] ; then
    echo "Versioning..."
    pushd ..
    buildVer=`hg summary | head -1 | cut -d " "  -f 2 | cut -d : -f 1 | grep "[0-9]*"`
    majorVer=`cat indra/Version | cut -d "=" -f 2 | cut -d "." -f 1`
    minorVer=`cat indra/Version | cut -d "=" -f 2 | cut -d "." -f 2`
    patchVer=`cat indra/Version | cut -d "=" -f 2 | cut -d "." -f 3`
    echo "Channel : Firestorm-${CHANNEL}"
    echo "Version : ${majorVer}.${minorVer}.${patchVer}.${buildVer}"
    python ./scripts/update_version_files.py --channel="Firestorm-$CHANNEL" --version=${majorVer}.${minorVer}.${patchVer}.${buildVer}
    popd
fi


if [ $WANTS_CONFIG -eq $TRUE ] ; then
    echo "Configuring $PLATFORM..."

    if [ $WANTS_KDU -eq $TRUE ] ; then
        KDU="-DUSE_KDU:BOOL=ON"
    else
        KDU="-DUSE_KDU:BOOL=OFF"
    fi
    if [ $WANTS_FMOD -eq $TRUE ] ; then
        FMOD="-DFMOD:BOOL=ON"
    else
        FMOD="-DFMOD:BOOL=OFF"
    fi
    if [ $WANTS_PACKAGE -eq $TRUE ] ; then
        PACKAGE="-DPACKAGE:BOOL=ON"
        # Also delete easy-to-copy resource files, insuring that we properly refresh resoures from the source tree
        if [ -d skins ] ; then
            echo "Removing select previously packaged resources, they will refresh at build time"
            for subdir in skins app_settings fs_resources ; do
                for resourcedir in `$FIND . -type d -name $subdir` ; do
                    rm -rf $resourcedir ;
                done
            done
        fi
    else
        PACKAGE="-DPACKAGE:BOOL=OFF"
    fi

    #make sure log directory exists.
    if [ ! -d "logs" ] ; then
        echo "Creating logging dir `pwd`/logs"
        mkdir -p "logs"
    fi

    if [ $PLATFORM == "darwin" ] ; then
        TARGET="Xcode"
    elif [ \( $PLATFORM == "linux32" \) -o \( $PLATFORM == "linux64" \) ] ; then
        TARGET="Unix Makefiles"
    elif [ \( $PLATFORM == "win32" \) ] ; then
        TARGET="Visual Studio 10"
    fi

    cmake -G "$TARGET" ../indra $FMOD $KDU $PACKAGE -DUNATTENDED:BOOL=ON -DLL_TESTS:BOOL=OFF -DWORD_SIZE:STRING=32 -DCMAKE_BUILD_TYPE:STRING=$BTYPE -DROOT_PROJECT_NAME:STRING=Firestorm $LL_ARGS_PASSTHRU | tee $LOG

    if [ $PLATFORM == "win32" ] ; then
    ../indra/tools/vstool/VSTool.exe --solution Firestorm.sln --startup firestorm-bin --workingdir firestorm-bin "..\\..\\indra\\newview" --config $BTYPE
    fi

fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
    echo "Building $PLATFORM..."
    if [ $PLATFORM == "darwin" ] ; then
        if [ $JOBS == "0" ] ; then
            JOBS=""
        else
            JOBS="-jobs $JOBS"
        fi
        if [ $OSTYPE == "darwin11" -o $OSTYPE == "darwin12" ] ; then
            xcodebuild -configuration $BTYPE -project Firestorm.xcodeproj $JOBS GCC_OPTIMIZATION_LEVEL=3 GCC_ENABLE_SSE3_EXTENSIONS=YES 2>&1 | tee -a $LOG
        else
            xcodebuild -configuration $BTYPE -project Firestorm.xcodeproj GCC_VERSION=4.2 GCC_OPTIMIZATION_LEVEL=3 GCC_ENABLE_SSE3_EXTENSIONS=YES 2>&1 | tee -a $LOG
        fi
    elif [ $PLATFORM == "linux32" -o $PLATFORM == "linux64" ] ; then
        if [ $JOBS == "0" ] ; then
            JOBS=`cat /proc/cpuinfo | grep processor | wc -l`
        fi
        make -j $JOBS | tee -a $LOG
    elif [ $PLATFORM == "win32" ] ; then
        msbuild.exe Firestorm.sln /flp:LogFile=logs\\FirestormBuild_win32.log /flp1:errorsonly;LogFile=logs\\FirestormBuild_win32.err /flp:LogFile=logs\\FirestormBuild_win32.log /p:Configuration=$BTYPE /p:Platform=Win32 /t:Build /p:useenv=true /verbosity:normal /toolsversion:4.0 /p:"VCBuildAdditionalOptions= /incremental"
    fi
fi

echo "Finished"
