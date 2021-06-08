#pragma once
#ifndef FS_TELEMETRY_H_INCLUDED
#define FS_TELEMETRY_H_INCLUDED
/** 
 * @file fstelemetry.h
 * @brief fstelemetry Telemetry abstraction for FS
 *
 * $LicenseInfo:firstyear=2021&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2021, The Phoenix Firestorm Project, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

// define a simple set of empty macros that allow us to build without the Tracy profiler installed in 3p
// this is similar to the profiler abstraction used by LL but as they have no plans to release that any time soon we'll replace it
// Just a minimal set at the moment will add locks/gpu/memory and other stuff later.

// generic switch (in case we ever add others or incorporate commercial tools like RAD Games if LL were to share a license)
// turn off in the else statement below.
#define FS_HAS_TELEMETRY_SUPPORT

#ifdef TRACY_ENABLE // (Tracy open source telemetry)
#include "Tracy.hpp"

#define FSZone ZoneNamed( ___tracy_scoped_zone, FSTelemetry::active)
#define FSZoneN( name ) ZoneNamedN( ___tracy_scoped_zone, name, FSTelemetry::active)
#define FSZoneC( color ) ZoneNamedC( ___tracy_scoped_zone, color, FSTelemetry::active)
#define FSZoneNC( name, color ) ZoneNamedNC( ___tracy_scoped_zone, name, color, FSTelemetry::active)
#define FSPlot( name, value ) TracyPlot( name, value)
#define FSFrameMark FrameMark
#define FSThreadName( name ) tracy::SetThreadName( name )
#define FSTelemetryIsConnected TracyIsConnected

#else // (no telemetry)

// No we don't want no stinkin' telemetry. move along
#undef FS_HAS_TELEMETRY_SUPPORT

#define FSZone
#define FSZoneN( name ) 
#define FSZoneC( color ) 
#define FSZoneNC( name, color )
#define FSPlot( name, value ) 
#define FSFrameMark 
#define FSThreadName( name ) 
#define FSTelemetryIsConnected
#endif // TRACY_ENABLE

namespace FSTelemetry
{
    extern bool active;
}

#endif