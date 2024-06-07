// Just a simple wrapper to easily import the single tracy source file and save
// all the pain of cross platform libraries in the Tracy 3p build.
#include "linden_common.h"

#if LL_PROFILER_CONFIGURATION == LL_PROFILER_CONFIG_TRACY || LL_PROFILER_CONFIGURATION == LL_PROFILER_CONFIG_TRACY_FAST_TIMER
	#include "TracyClient.cpp"
#endif // LL_PROFILER_CONFIGURATION

