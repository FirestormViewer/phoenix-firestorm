#!/bin/bash

## Here are some configuration options for Linux Client Users.

## - Avoids using any OpenAL audio driver.
#export LL_BAD_OPENAL_DRIVER=x

## GL Driver Options
export mesa_glthread=true

# Cache NVIDIA display-hardware detection without requiring diagnostic tools.
# Require the NVIDIA driver too; Nouveau uses Mesa and does not need this policy.
has_nvidia_gpu=false
for pci_device in /sys/bus/pci/devices/*; do
    [[ -r "$pci_device/vendor" && -r "$pci_device/class" ]] || continue
    if [[ "$(< "$pci_device/vendor")" == "0x10de" &&
          "$(< "$pci_device/class")" == 0x03* &&
          "$(readlink -- "$pci_device/driver" 2>/dev/null)" == */nvidia ]]; then
        has_nvidia_gpu=true
        break
    fi
done

# Leave SDL backend selection and NVIDIA-specific variables alone on other GPUs.
if "$has_nvidia_gpu"; then
    # Detect the desktop session and read SDL's preferred backend once.
    is_wayland=false
    if [[ -n "${WAYLAND_DISPLAY:-}" || "${XDG_SESSION_TYPE:-}" == "wayland" ]]; then
        is_wayland=true
    fi
    sdl_video_driver="${SDL_VIDEO_DRIVER:-${SDL_VIDEODRIVER:-}}"

    # On Wayland, default to X11/XWayland without replacing an explicit backend.
    if "$is_wayland" && [[ -z "$sdl_video_driver" ]]; then
        export SDL_VIDEO_DRIVER=x11
        export SDL_VIDEODRIVER=x11
        sdl_video_driver=x11
    fi

    # Respect explicit backends; otherwise assume X11 outside Wayland sessions.
    is_using_x11_sdl=false
    if [[ -n "$sdl_video_driver" ]]; then
        if [[ "$sdl_video_driver" == "x11" ]]; then
            is_using_x11_sdl=true
        fi
    elif ! "$is_wayland"; then
        is_using_x11_sdl=true
    fi

    # Native Wayland and ambiguous fallback lists keep NVIDIA threading disabled.
    if "$is_using_x11_sdl"; then
        export __GL_THREADED_OPTIMIZATIONS=1
    else
        export __GL_THREADED_OPTIMIZATIONS=0
    fi
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
#export ASAN_OPTIONS="halt_on_error=0 detect_leaks=1 symbolize=1"
#export UBSAN_OPTIONS="print_stacktrace=1 print_summary=1 halt_on_error=0"

FSJEMALLOC="$(pwd)/lib/libjemalloc.so"
if [ -f ${FSJEMALLOC} ]
then
        echo "Using jemalloc"
        export LD_PRELOAD="${LD_PRELOAD}:${FSJEMALLOC}"
fi

## Nothing worth editing below this line.
##-------------------------------------------------------------------

SCRIPTSRC=`readlink -f "$0" || echo "$0"`
RUN_PATH=`dirname "${SCRIPTSRC}" || echo .`
echo "Running from ${RUN_PATH}"
cd "${RUN_PATH}"

# Re-register the secondlife:// protocol handler every launch, for now.
./etc/register_secondlifeprotocol.sh

# Re-register the application with the desktop system every launch, for now.
./etc/refresh_desktop_app_entry.sh

## Before we mess with LD_LIBRARY_PATH, save the old one to restore for
##  subprocesses that care.
export SAVED_LD_LIBRARY_PATH="${LD_LIBRARY_PATH}"

# Add our library directory
export LD_LIBRARY_PATH="$PWD/lib:${LD_LIBRARY_PATH}"

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
