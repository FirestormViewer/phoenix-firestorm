#pragma once
#ifndef FS_TELEMETRY_H_INCLUDED
#define FS_TELEMETRY_H_INCLUDED

// define a simple set of empty macros that allow us to build without the Tracy profiler installed in 3p
// this is similar to the profiler abstraction used by LL but as they have no plans to release that any time soon we'll replace it
// Just a minimal set at the moment will add locks/gpu/memory and other stuff later.

#ifdef TRACY_ENABLE
#include "Tracy.hpp"

#define FSZone ZoneNamed( ___tracy_scoped_zone, FSTelemetry::active)
#define FSZoneN( name ) ZoneNamedN( ___tracy_scoped_zone, name, FSTelemetry::active)
#define FSZoneC( color ) ZoneNamedC( ___tracy_scoped_zone, color, FSTelemetry::active)
#define FSZoneNC( name, color ) ZoneNamedNC( ___tracy_scoped_zone, name, color, FSTelemetry::active)
#define FSPlot( name, value ) TracyPlot( name, value)
#define FSFrameMark FrameMark
#define FSTelemetryIsConnected TracyIsConnected

#else

#define FSZone
#define FSZoneN( name ) 
#define FSZoneC( color ) 
#define FSZoneNC( name, color )
#define FSPlot( name, value ) 
#define FSFrameMark 
#define FSTelemetryIsConnected
#endif // TRACY_ENABLE

namespace FSTelemetry
{
    extern bool active;
}

#endif