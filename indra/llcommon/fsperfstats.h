#pragma once
#ifndef FS_PERFSTATS_H_INCLUDED
#define FS_PERFSTATS_H_INCLUDED
/** 
 * @file fsperfstats.h
 * @brief Statistics collection to support autotune and perf flaoter.
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

#include <chrono>
#include <array>
#include <unordered_map>

namespace FSPerfStats
{
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
        RENDER_IDLE,
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
            constexpr auto period{500};
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

            // auto& statsMap = stats[writeBuffer];
            // for(auto& stat_entry : statsMap)
            // {
            //     auto val = stat_entry.second[static_cast<size_t>(ST::RENDER_COMBINED)];
            //     auto avg = stats[(writeBuffer+1)%2][stat_entry.first][static_cast<size_t>(ST::RENDER_COMBINED)];
            //     stat_entry.second[static_cast<size_t>(ST::RENDER_COMBINED)] = avg + (val/500) - (avg/500);
            // }
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
    template <typename T>
    class RecordAttachmentTime
	{
        using StatsEnum = ObjStatType;
        using StatsArray = std::array<uint64_t, static_cast<size_t>(StatsEnum::STATS_COUNT)>;
        using StatsBlock = std::unordered_map<T, StatsArray>;

		static int writeBuffer;
        static std::array<StatsBlock,2> stats;

		static std::array<StatsArray,2> max;
		static std::array<StatsArray,2> sum;
        static bool collectionEnabled;

        RecordAttachmentTime(const RecordAttachmentTime&) = delete;
        RecordAttachmentTime() = delete;
        const T key;
        const StatsEnum type;
        std::chrono::steady_clock::time_point start;

    public:
        static inline void enable(){collectionEnabled=true;};
        static inline void disable(){collectionEnabled=false;};
        static inline bool enabled(){return(collectionEnabled);};

        RecordAttachmentTime(T key, ObjStatType type):start{std::chrono::steady_clock::now()}, key{key}, type{type} {}

        ~RecordAttachmentTime()
        { 
            using ST = StatsEnum;
            // Note: nullptr is used as the key for global stats
            auto val = std::chrono::duration<uint64_t, std::nano>(std::chrono::steady_clock::now() - start).count();
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

    template< typename T >
    int 	RecordAttachmentTime<T>::writeBuffer{0};

    template< typename T >
    bool 	RecordAttachmentTime<T>::collectionEnabled{true};

	template< typename T >
    std::array< typename RecordAttachmentTime< T >::StatsArray, 2 >	RecordAttachmentTime<T>::max;

	template< typename T >
    std::array< typename RecordAttachmentTime< T >::StatsArray, 2 >	RecordAttachmentTime<T>::sum;
	
    template< typename T >
	std::array< typename RecordAttachmentTime< T >::StatsBlock, 2 > RecordAttachmentTime< T >::stats{ {{}} };
}// namespace FSPerfStats





#endif