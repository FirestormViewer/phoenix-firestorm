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

#include <chrono>
#include <array>
#include <unordered_map>

namespace FSTelemetry
{
    extern bool active;

   	enum class ObjStatType_t{
		RENDER_GEOMETRY=0,
		RENDER_SHADOWS,
		RENDER_COMBINED,
		STATS_COUNT
	};
   	enum class SceneStatType_t{
		RENDER_GEOMETRY=0,
		RENDER_SHADOWS,
		RENDER_HUDS,
		RENDER_UI,
		RENDER_COMBINED,
        RENDER_SWAP,
        RENDER_FRAME,
        RENDER_SLEEP,
        RENDER_LFS,
        RENDER_MESHREPO,
        RENDER_FPSLIMIT,
        RENDER_FPS,
		STATS_COUNT
	};
    
    using ObjStatType = ObjStatType_t;
    using SceneStatType = SceneStatType_t;

	class RecordSceneTime
	{
        using StatsEnum = SceneStatType;
        using StatsArray = std::array<uint64_t, static_cast<size_t>(StatsEnum::STATS_COUNT)>;
        // using StatsBlock = std::unordered_map<T, StatsArray>;

		static int writeBuffer;
        static std::array<StatsArray,2> stats;
        static bool collectionEnabled;

        RecordSceneTime(const RecordSceneTime&) = delete;
        RecordSceneTime() = delete;

        const StatsEnum type;
        std::chrono::steady_clock::time_point start;
    public:

        static inline void enable(){collectionEnabled=true;};
        static inline void disable(){collectionEnabled=false;};
        static inline bool enabled(){return(collectionEnabled);};

        RecordSceneTime(SceneStatType type):start{std::chrono::steady_clock::now()}, type{type} {}

        ~RecordSceneTime()
        { 
            auto val = std::chrono::duration<uint64_t, std::nano>(std::chrono::steady_clock::now() - start).count();
            stats[writeBuffer][static_cast<size_t>(type)] += val;
        };

        static inline void toggleBuffer()
        {
            if(enabled())
            {
                // stats[writeBuffer][static_cast<size_t>(SceneStatType::RENDER_FPS)] = LLTrace::get_frame_recording().getPeriodMeanPerSec(LLStatViewer::FPS,3); // last 3 Frames
                writeBuffer = (writeBuffer+1)%2;
            }; // not we are relying on atomic updates here. The risk is low and would cause minor errors in the stats display. 

            auto& statsArray = stats[writeBuffer];
            std::fill_n(statsArray.begin() ,static_cast<size_t>(SceneStatType::STATS_COUNT),0);
        }
		static inline int getReadBufferIndex(){return (writeBuffer+1)%2;};
        static inline StatsArray getCurrentStatsBuffer(){ return stats[getReadBufferIndex()];}
        static inline uint64_t get(StatsEnum type){return stats[getReadBufferIndex()][static_cast<size_t>(type)];}
	};
	
    template <typename T>
    class RecordObjectTime
	{
        using StatsEnum = ObjStatType;
        using StatsArray = std::array<uint64_t, static_cast<size_t>(StatsEnum::STATS_COUNT)>;
        using StatsBlock = std::unordered_map<T, StatsArray>;

		static int writeBuffer;
        static std::array<StatsBlock,2> stats;

		static std::array<StatsArray,2> max;
		static std::array<StatsArray,2> sum;
        static bool collectionEnabled;

        RecordObjectTime(const RecordObjectTime&) = delete;
        RecordObjectTime() = delete;
        const T key;
        const StatsEnum type;
        std::chrono::steady_clock::time_point start;

    public:
        static inline void enable(){collectionEnabled=true;};
        static inline void disable(){collectionEnabled=false;};
        static inline bool enabled(){return(collectionEnabled);};

        RecordObjectTime(T key, ObjStatType type):start{std::chrono::steady_clock::now()}, key{key}, type{type} {}

        ~RecordObjectTime()
        { 
            using ST = StatsEnum;
            // Note: nullptr is used as the key for global stats
            auto val = std::chrono::duration<uint64_t, std::nano>(std::chrono::steady_clock::now() - start).count();
            if(key)
            {
                stats[writeBuffer][key][static_cast<size_t>(type)] += val;
                stats[writeBuffer][key][static_cast<size_t>(ST::RENDER_COMBINED)] += val;
                if(max[writeBuffer][static_cast<size_t>(type)] < stats[writeBuffer][key][static_cast<size_t>(type)])
                {
                    max[writeBuffer][static_cast<size_t>(type)] = stats[writeBuffer][key][static_cast<size_t>(type)];
                }
                if(max[writeBuffer][static_cast<size_t>(ST::RENDER_COMBINED)] < stats[writeBuffer][key][static_cast<size_t>(ST::RENDER_COMBINED)])
                {
                    max[writeBuffer][static_cast<size_t>(ST::RENDER_COMBINED)] = stats[writeBuffer][key][static_cast<size_t>(ST::RENDER_COMBINED)];
                }
                sum[writeBuffer][static_cast<size_t>(type)] += val;
                sum[writeBuffer][static_cast<size_t>(ST::RENDER_COMBINED)] += val;
            }
        };
        static inline void toggleBuffer()
        {
            using ST = StatsEnum;
            if(enabled())
            {
                writeBuffer = (writeBuffer+1)%2;
            }; // note we are relying on atomic updates here. The risk is low and would cause minor errors in the stats display. 

            auto& statsMap = stats[writeBuffer];
            for(auto& stat_entry : statsMap)
            {
                std::fill_n(stat_entry.second.begin() ,static_cast<size_t>(ST::STATS_COUNT),0);
            }
            statsMap.clear();
            std::fill_n(max[writeBuffer].begin(),static_cast<size_t>(ST::STATS_COUNT),0);
            std::fill_n(sum[writeBuffer].begin(),static_cast<size_t>(ST::STATS_COUNT),0);
        }
		static inline int getReadbufferIndex(){return (writeBuffer+1)%2;};
        static inline StatsBlock& getCurrentStatsBuffer(){ return stats[(writeBuffer+1)%2]; }
        static inline uint64_t getMax(StatsEnum type){return max[(writeBuffer+1)%2][static_cast<size_t>(type)];}
        static inline uint64_t getSum(StatsEnum type){return sum[(writeBuffer+1)%2][static_cast<size_t>(type)];}
        static inline uint64_t getNum(){return stats[(writeBuffer+1)%2].size();}
        static inline uint64_t get(T key, StatsEnum type){return stats[(writeBuffer+1)%2][key][static_cast<size_t>(type)];}
	};
    
	static inline void toggleBuffer()
    {
        // RecordObjectTime<LLVOAvatar*>::toggleBuffer();
        RecordSceneTime::toggleBuffer();
    }

    template< typename T >
    int 	RecordObjectTime<T>::writeBuffer{0};

    template< typename T >
    bool 	RecordObjectTime<T>::collectionEnabled{true};

	template< typename T >
    std::array< typename RecordObjectTime< T >::StatsArray, 2 >	RecordObjectTime<T>::max;

	template< typename T >
    std::array< typename RecordObjectTime< T >::StatsArray, 2 >	RecordObjectTime<T>::sum;
	
    template< typename T >
	std::array< typename RecordObjectTime< T >::StatsBlock, 2 > RecordObjectTime< T >::stats{ {{}} };

}// namespace FSTelemetry

#endif