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

#include <atomic>
#include <chrono>
#include <array>
#include <unordered_map>
#include "lluuid.h"
#include "lltimer.h"
#include "blockingconcurrentqueue.h"
#include "llapp.h"
#include "fstelemetry.h"

extern LLUUID gAgentID;
namespace FSPerfStats
{

   	enum class ObjType_t{
		OT_GENERAL=0, // Also Unknown. Used for n/a type stats such as scenery
		OT_AVATAR,
		OT_ATTACHMENT,
		OT_HUD,
        OT_COUNT
	};
   	enum class StatType_t{
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
    
    struct StatsRecord
    { 
        StatType_t  statType;
        ObjType_t   objType;
        LLUUID      objID;
        uint64_t    time;
    };

    class StatsRecorder{
        using Queue = moodycamel::BlockingConcurrentQueue<StatsRecord>;
    public:
        static inline StatsRecorder& getInstance()
        {
            static StatsRecorder instance;
            // volatile int dummy{};
            return instance;
        }

        static inline void send(const StatsRecord& u){StatsRecorder::getInstance().q.enqueue(u);};
        static inline void endFrame(){StatsRecorder::getInstance().q.enqueue(StatsRecord{});};

        static inline void enable()     { collectionEnabled=true; };
        static inline void disable()    { collectionEnabled=false; };
        static inline bool enabled()    { return(collectionEnabled); };

		static inline int getReadBufferIndex() { return (writeBuffer ^ 1); };
        // static inline const StatsTypeMatrix& getCurrentStatsMatrix(){ return statsDoubleBuffer[getReadBufferIndex()];}
        static inline uint64_t get(ObjType_t otype, LLUUID id, StatType_t type)
        {
            return statsDoubleBuffer[getReadBufferIndex()][static_cast<size_t>(otype)][id][static_cast<size_t>(type)];
        }
        static inline uint64_t getSceneStat(StatType_t type)
        {
            static const LLUUID null_id{};
            return statsDoubleBuffer[getReadBufferIndex()][static_cast<size_t>(ObjType_t::OT_GENERAL)][null_id][static_cast<size_t>(type)];
        }

        static inline uint64_t getSum(ObjType_t otype, StatType_t type)
        {
            return sum[getReadBufferIndex()][static_cast<size_t>(otype)][static_cast<size_t>(type)];
        }
        static inline uint64_t getMax(ObjType_t otype, StatType_t type)
        {
            return max[getReadBufferIndex()][static_cast<size_t>(otype)][static_cast<size_t>(type)];
        }
    private:
        StatsRecorder():q(100),t(&StatsRecorder::run)
        {
            // create a queue
            // create a thread to consume from the queue
            t.detach();
        }

// StatsArray is a uint64_t for each possible statistic type.
        using StatsArray    = std::array<uint64_t, static_cast<size_t>(FSPerfStats::StatType_t::STATS_COUNT)>;
        using StatsMap      = std::unordered_map<LLUUID, StatsArray, FSUUIDHash>;
        using StatsTypeMatrix = std::array<StatsMap, static_cast<size_t>(FSPerfStats::ObjType_t::OT_COUNT)>;
        using StatsSummaryArray = std::array<StatsArray, static_cast<size_t>(FSPerfStats::ObjType_t::OT_COUNT)>;

		static std::atomic<int> writeBuffer;
        static std::array<StatsTypeMatrix,2> statsDoubleBuffer;
        static std::array<StatsSummaryArray,2> max;
        static std::array<StatsSummaryArray,2> sum;
        static bool collectionEnabled;


        void processUpdate(const StatsRecord& upd)
        {
            FSZone;
            using ST = StatType_t;
            // Note: nullptr is used as the key for global stats
            constexpr auto period{500};
            if(upd.statType == StatType_t::RENDER_GEOMETRY && upd.objType == ObjType_t::OT_GENERAL && upd.objID == LLUUID{} && upd.time == 0)
            {
                toggleBuffer();
                return;
            }

            StatsMap& stm {statsDoubleBuffer[writeBuffer][static_cast<size_t>(upd.objType)]};
            auto& key{upd.objID};
            auto val {upd.time};
            auto type {upd.statType};
            FSZoneText(key.asString().c_str(), 36);
            FSZoneValue(val);
            auto& thisAsset = stm[key];
            thisAsset[static_cast<size_t>(type)] += val;
            thisAsset[static_cast<size_t>(ST::RENDER_COMBINED)] += val;
            FSZoneValue(thisAsset[static_cast<size_t>(type)]);
            sum[writeBuffer][static_cast<size_t>(upd.objType)][static_cast<size_t>(type)] += val;
            sum[writeBuffer][static_cast<size_t>(upd.objType)][static_cast<size_t>(ST::RENDER_COMBINED)] += val;
            FSZoneValue(static_cast<size_t>(upd.objType));
            FSZoneValue(statsDoubleBuffer[writeBuffer][static_cast<size_t>(upd.objType)][key][static_cast<size_t>(ST::RENDER_COMBINED)]);
            if(max[writeBuffer][static_cast<size_t>(upd.objType)][static_cast<size_t>(type)] < stm[key][static_cast<size_t>(type)])
            {
                max[writeBuffer][static_cast<size_t>(upd.objType)][static_cast<size_t>(type)] = stm[key][static_cast<size_t>(type)];
            }
            if(max[writeBuffer][static_cast<size_t>(upd.objType)][static_cast<size_t>(ST::RENDER_COMBINED)] < stm[key][static_cast<size_t>(ST::RENDER_COMBINED)])
            {
                max[writeBuffer][static_cast<size_t>(upd.objType)][static_cast<size_t>(ST::RENDER_COMBINED)] = stm[key][static_cast<size_t>(ST::RENDER_COMBINED)];
            }
        }

        static inline void toggleBuffer()
        {
            FSPlot("q size", static_cast<int64_t>(StatsRecorder::getInstance().q.size_approx()));
            FSZone;
            using ST = StatType_t;

            auto& statsMap = statsDoubleBuffer[writeBuffer][static_cast<size_t>(ObjType_t::OT_ATTACHMENT)];
            for(auto& stat_entry : statsMap)
            {
                auto val = stat_entry.second[static_cast<size_t>(ST::RENDER_COMBINED)];
                auto avg = statsDoubleBuffer[writeBuffer ^ 1][static_cast<size_t>(ObjType_t::OT_ATTACHMENT)][stat_entry.first][static_cast<size_t>(ST::RENDER_COMBINED)];
                stat_entry.second[static_cast<size_t>(ST::RENDER_COMBINED)] = avg + (val/100) - (avg/100);
            }
            if(enabled())
            {
                writeBuffer ^= 1;
            }; // note we are relying on atomic updates here. The risk is low and would cause minor errors in the stats display. 
            auto& statsTypeMatrix = statsDoubleBuffer[writeBuffer];
            for(auto& statsMap : statsTypeMatrix)
            {
                FSZoneN("Clear stats maps");
                for(auto& stat_entry : statsMap)
                {
                    std::fill_n(stat_entry.second.begin() ,static_cast<size_t>(ST::STATS_COUNT),0);
                }
                statsMap.clear();
            }
            for(int i=0; i< static_cast<size_t>(ObjType_t::OT_COUNT); i++)
            {
                FSZoneN("clear max/sum");
                max[writeBuffer][i].fill(0);
                sum[writeBuffer][i].fill(0);
            }
        }



        static void run()
        {
            StatsRecord upd;
            auto& instance {StatsRecorder::getInstance()};
            FSThreadName( "PerfStats" );

            while( !LLApp::isExiting() )
            {
                if(instance.q.wait_dequeue_timed(upd, std::chrono::milliseconds(5)))
                {
                    instance.processUpdate(upd);
                }
            }
        }

        Queue q;
        std::thread t;

        ~StatsRecorder() = default;
        StatsRecorder(const StatsRecorder&) = delete;
        StatsRecorder& operator=(const StatsRecorder&) = delete;

    };

    // std::chrono::duration<double> getTime(){

    // auto begin= std::chrono::system_clock::now();
    // for ( size_t i= 0; i <= tenMill; ++i){
    //     StatsRecorder::getInstance();
    // }
    // return std::chrono::system_clock::now() - begin;
    
    // };

	template <enum ObjType_t ObjType>
    class RecordTime
	{

    private:
        RecordTime(const RecordTime&) = delete;
        RecordTime() = delete;
        const StatType_t        type;
        const decltype(ObjType) objType;
        const LLUUID            objID;
        U64 start;
        RecordTime( StatType_t type ){};// 

    public:

        static inline void enable(){collectionEnabled=true;};
        static inline void disable(){collectionEnabled=false;};
        static inline bool enabled(){return(collectionEnabled);};


        RecordTime( const LLUUID id, StatType_t type ):start{LLTimer::getCurrentClockCount()}, type{type}, objType{ObjType}, objID{id}{};

        ~RecordTime()
        { 
            FSZoneC(tracy::Color::Red);
            auto val = LLTimer::getCurrentClockCount() - start;
            FSZoneValue(val);
            FSZoneValue(static_cast<U64>(objType));
            FSZoneText(objID.asString().c_str(), 36);
            StatsRecord stat{type, objType, objID, val};
            StatsRecorder::send(std::move(stat));
        };


	};

    inline double raw_to_ns(U64 raw)    { return (static_cast<double>(raw) * 1000000000.0) * get_timer_info().mClockFrequencyInv; };
    inline double raw_to_us(U64 raw)    { return (static_cast<double>(raw) *    1000000.0) * get_timer_info().mClockFrequencyInv; };
    inline double raw_to_ms(U64 raw)    { return (static_cast<double>(raw) *       1000.0) * get_timer_info().mClockFrequencyInv; };

    using RecordSceneTime = RecordTime<ObjType_t::OT_GENERAL>;
    using RecordAvatarTime = RecordTime<ObjType_t::OT_AVATAR>;
    using RecordAttachmentTime = RecordTime<ObjType_t::OT_ATTACHMENT>;
     
}// namespace FSPerfStats

// <FS:Beq> helper function
using RATptr = std::unique_ptr<FSPerfStats::RecordAttachmentTime>;
template <typename T>
static inline RATptr trackMyAttachment(const T * vobj)
{
	if( !vobj ){return nullptr;};
	const T* rootAtt{vobj};
	if( rootAtt->isAttachment() )
	{
	    FSZone;
		while( !rootAtt->isRootEdit() )
		{ 
			rootAtt = (T*)(rootAtt->getParent());
		}
		if( ((T*)(rootAtt->getParent()))->getID() == gAgentID )
		{
            #if TRACY_ENABLE
	        FSZoneNC( "trackMyAttachment:self", tracy::Color::Red );
			auto& str = rootAtt->getAttachmentItemName();
			FSZoneText(str.c_str(), str.size());
			FSZoneText( rootAtt->getAttachmentItemID().asString().c_str(), 36);
            #endif
			return( std::make_unique<FSPerfStats::RecordAttachmentTime>( rootAtt->getAttachmentItemID(), FSPerfStats::StatType_t::RENDER_GEOMETRY) );
		}
	}
	return nullptr;
};
// </FS:Beq>
#endif