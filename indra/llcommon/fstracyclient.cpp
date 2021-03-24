//
//          Tracy profiler
//         ----------------
//
// For fast integration, compile and
// link with this source file (and none
// other) in your executable (or in the
// main DLL / shared object on multi-DLL
// projects).
//

// Define TRACY_ENABLE to enable profiler.
#include "common/TracySystem.cpp"

#ifdef TRACY_ENABLE

#ifdef LL_WINDOWS
#  pragma warning(push, 0)
#endif

#include "common/tracy_lz4.cpp"
#include "client/TracyProfiler.cpp"
#include "client/TracyCallstack.cpp"
#include "client/TracySysTime.cpp"

// Black magic on Windows to deal with windows entry points that are not found in a standard LL windows build
// This is less invasive than changing the 3p library itself.
// TODO: work out why the windows trace subsystem stuff is not defined in our builds, this then goes away (hopefully)
#if defined(LL_WINDOWS) && !defined(__CYGWIN__)
#define __CYGWIN__
#include "client/TracySysTrace.cpp"
#undef __CYGWIN__
#else
#include "client/TracySysTrace.cpp"
#endif

#include "common/TracySocket.cpp"
#include "client/tracy_rpmalloc.cpp"
#include "client/TracyDxt1.cpp"

#if TRACY_HAS_CALLSTACK == 2 || TRACY_HAS_CALLSTACK == 3 || TRACY_HAS_CALLSTACK == 4 || TRACY_HAS_CALLSTACK == 6
#  include "libbacktrace/alloc.cpp"
#  include "libbacktrace/dwarf.cpp"
#  include "libbacktrace/fileline.cpp"
#  include "libbacktrace/mmapio.cpp"
#  include "libbacktrace/posix.cpp"
#  include "libbacktrace/sort.cpp"
#  include "libbacktrace/state.cpp"
#  if TRACY_HAS_CALLSTACK == 4
#    include "libbacktrace/macho.cpp"
#  else
#    include "libbacktrace/elf.cpp"
#  endif
#endif

#ifdef LL_WINDOWS
#  pragma comment(lib, "ws2_32.lib")
#  pragma comment(lib, "dbghelp.lib")
#  pragma warning(pop)
#endif

#endif