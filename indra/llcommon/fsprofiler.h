#pragma once
#ifndef FS_PROFILER_H_INCLUDED
#define FS_PROFILER_H_INCLUDED

// define a simple set of empty macros that allow us to build without the Tracy profiler installed in 3p
// this is similar to the profiler abstraction used by LL but as they have no plans to release that any time soon we'll replace it
// Just a minimal set at the moment will add locks/gpu/memory and other stuff later

#ifdef TRACY_ENABLE
#include "Tracy.hpp"
namespace FSProfiler
{
    extern bool active;
}

#define FSZone ZoneNamed( ___tracy_scoped_zone, FSProfiler::active)
#define FSZoneN( name ) ZoneNamedN( ___tracy_scoped_zone, name, FSProfiler::active)
#define FSZoneC(color) ZoneNamedC( ___tracy_scoped_zone, color, FSProfiler::active)
#define FSZoneNC(name, color) ZoneNamedNC( ___tracy_scoped_zone, name, color, FSProfiler::active)
#define FSPlot( name, value ) TracyPlot( name, value)
#define FSFrameMark FrameMark

#else

#define FSZone
#define FSZoneN( name ) 
#define FSZoneC(color) 
#define FSZoneNC(name, color)
#define FSPlot( name, value ) 
#define FSFrameMark 
#endif // TRACY_ENABLE
#endif