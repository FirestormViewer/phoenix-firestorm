#!/bin/bash

exportMutliArchDRIPath() {
	MULTIARCH="$(dpkg-architecture -a$1 -qDEB_HOST_MULTIARCH 2>/dev/null)"
	MULTIARCH_ERR=$?
	if [ $MULTIARCH_ERR -eq 0 ]; then
		echo "Multi-arch support detected for $1."
		MULTIARCH_GL_DRIVERS="/usr/lib/${MULTIARCH}/dri"
		if [ -z ${LIBGL_DRIVERS_PATH}  ]
		then
			export LIBGL_DRIVERS_PATH="${MULTIARCH_GL_DRIVERS}"
		else
			export LIBGL_DRIVERS_PATH="${LIBGL_DRIVERS_PATH}:${MULTIARCH_GL_DRIVERS}"
		fi
	fi
}


## Here are some configuration options for Linux Client Testers.
## These options are for self-assisted troubleshooting during this beta
## testing phase; you should not usually need to touch them.

## - Avoids using any FMOD STUDIO audio driver.
#export LL_BAD_FMODSTUDIO_DRIVER=x
## - Avoids using any OpenAL audio driver.
#export LL_BAD_OPENAL_DRIVER=x

## - Avoids using the FMOD Studio PulseAudio audio driver.
#export LL_BAD_FMOD_PULSEAUDIO=x
## - Avoids using the FMOD Studio ALSA audio driver.
#export LL_BAD_FMOD_ALSA=x

## - Avoids the optional OpenGL extensions which have proven most problematic
##   on some hardware.  Disabling this option may cause BETTER PERFORMANCE but
##   may also cause CRASHES and hangs on some unstable combinations of drivers
##   and hardware.
## NOTE: This is now disabled by default.
#export LL_GL_BASICEXT=x

## - Avoids *all* optional OpenGL extensions.  This is the safest and least-
##   exciting option.  Enable this if you experience stability issues, and
##   report whether it helps in the Linux Client Testers forum.
#export LL_GL_NOEXT=x

## - For advanced troubleshooters, this lets you disable specific GL
##   extensions, each of which is represented by a letter a-o.  If you can
##   narrow down a stability problem on your system to just one or two
##   extensions then please post details of your hardware (and drivers) to
##   the Linux Client Testers forum along with the minimal
##   LL_GL_BLACKLIST which solves your problems.
#export LL_GL_BLACKLIST=abcdefghijklmno

## - Some ATI/Radeon users report random X server crashes when the mouse
##   cursor changes shape.  If you suspect that you are a victim of this
##   driver bug, try enabling this option and report whether it helps:
#export LL_ATI_MOUSE_CURSOR_BUG=x

## Help fontconfig find its default configuration file, otherwise the viewer will stall
## with Fontconfig error: Cannot load default config file
# export FONTCONFIG_PATH=/etc/fonts

## - Enable NVidia's threaded optimization by default. If you experience
##   weird behavior starting with the 6.6.17 Firestorm release that you
##   can verify not to be present in earlier versions, comment out the
##   next line. Thanks to Jira user Pazako Karu on FIRE-33398 to make us
##   aware of this option!
export __GL_THREADED_OPTIMIZATIONS=1

if [ "`uname -m`" = "x86_64" ]; then
    echo '64-bit Linux detected.'
fi


## Everything below this line is just for advanced troubleshooters.
##-------------------------------------------------------------------

## - For advanced debugging cases, you can run the viewer under the
##   control of another program, such as strace, gdb, or valgrind.  If
##   you're building your own viewer, bear in mind that the executable
##   in the bin directory will be stripped: you should replace it with
##   an unstripped binary before you run.
#export LL_WRAPPER='gdb --args'
#export LL_WRAPPER='valgrind --smc-check=all --error-limit=no --log-file=secondlife.vg --leak-check=full --suppressions=/usr/lib/valgrind/glibc-2.5.supp --suppressions=secondlife-i686.supp'

## - Avoids an often-buggy X feature that doesn't really benefit us anyway.
export SDL_VIDEO_X11_DGAMOUSE=0

## - Works around a problem with misconfigured 64-bit systems not finding GL
exportMutliArchDRIPath "amd64"

if [ -z ${LIBGL_DRIVERS_PATH} ]
then
	export LIBGL_DRIVERS_PATH="/usr/lib64/dri:/usr/lib/dri:/usr/lib/x86_64-linux-gnu/dri"
else
	export LIBGL_DRIVERS_PATH="${LIBGL_DRIVERS_PATH}:/usr/lib64/dri:/usr/lib/dri:/usr/lib/x86_64-linux-gnu/dri"
fi

export LIBGL_DRIVERS_PATH="${LIBGL_DRIVERS_PATH}:/usr/lib64/xorg/modules/dri"

echo "LIBGL_DRIVERS_PATH is ${LIBGL_DRIVERS_PATH}"

## - The 'scim' GTK IM module widely crashes the viewer.  Avoid it.
if [ "$GTK_IM_MODULE" = "scim" ]; then
    export GTK_IM_MODULE=xim
fi
if [ "$XMODIFIERS" = "" ]; then
    ## IME is valid only for fcitx, not when using ibus
    export XMODIFIERS="@im=fcitx"
fi

## - Automatically work around the ATI mouse cursor crash bug:
## (this workaround is disabled as most fglrx users do not see the bug)
#if lsmod | grep fglrx &>/dev/null ; then
#	export LL_ATI_MOUSE_CURSOR_BUG=x
#fi


## Nothing worth editing below this line.
##-------------------------------------------------------------------

SCRIPTSRC=`readlink -f "$0" || echo "$0"`
RUN_PATH=`dirname "${SCRIPTSRC}" || echo .`
echo "Running from ${RUN_PATH}"
cd "${RUN_PATH}"

# Re-register hop:// and secondlife:// protocol handler every launch, for now.
test -x ./etc/register_hopprotocol.sh && ./etc/register_hopprotocol.sh
test -x ./etc/register_secondlifeprotocol.sh && ./etc/register_secondlifeprotocol.sh

# Re-register the application with the desktop system every launch, for now.
# AO: Disabled don't install by default
#./etc/refresh_desktop_app_entry.sh

## Before we mess with LD_LIBRARY_PATH, save the old one to restore for
##  subprocesses that care.
export SAVED_LD_LIBRARY_PATH="${LD_LIBRARY_PATH}"


if ! test -f FS_No_LD_Hacks.txt; then

export LD_LIBRARY_PATH="$PWD/lib:${LD_LIBRARY_PATH}"
# AO: experimentally removing to allow --settings on the command line w/o error. FIRE-1031
#export SL_OPT="`cat etc/gridargs.dat` $@"

# <FS:ND> [blerg] set LD_PRELOAD so plugins will pick up the correct sll libs, otherwise they will pick up the system versions.
LLCRYPTO="`pwd`/lib/libcrypto.so.1.0.0"
LLSSL="`pwd`/lib/libssl.so.1.0.0"
if [ -f ${LLCRYPTO} ]
then
	export LD_PRELOAD="${LD_PRELOAD}:${LLCRYPTO}"
fi
if [ -f ${LLSSL} ]
then
	export LD_PRELOAD="${LD_PRELOAD}:${LLSSL}"
fi

fi

FSJEMALLOC="$(pwd)/lib/libjemalloc.so"
if [ -f ${FSJEMALLOC} ]
then
	echo "Using jemalloc"
	export LD_PRELOAD="${LD_PRELOAD}:${FSJEMALLOC}"
fi

# Copy "$@" to ARGS array specifically to delete the --skip-gridargs switch.
# The gridargs.dat file is no more, but we still want to avoid breaking
# scripts that invoke this one with --skip-gridargs.
ARGS=()
for ARG in "$@"; do
    if [ "--skip-gridargs" != "$ARG" ]; then
        ARGS[${#ARGS[*]}]="$ARG"
    fi
done

# Run the program.
# Don't quote $LL_WRAPPER because, if empty, it should simply vanish from the
# command line. But DO quote "${ARGS[@]}": preserve separate args as
# individually quoted.
$LL_WRAPPER bin/do-not-directly-run-firestorm-bin "${ARGS[@]}"
LL_RUN_ERR=$?

# Handle any resulting errors
if [ $LL_RUN_ERR -ne 0 ]; then
	# generic error running the binary
	echo '*** Bad shutdown ($LL_RUN_ERR). ***'
fi
