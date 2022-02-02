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
#include <mutex>
#include "lluuid.h"
#include "llfasttimer.h"
#include "blockingconcurrentqueue.h"
#include "llapp.h"
#include "fstelemetry.h"
#include "pipeline.h"

// Additional logging options. These can skew inworld numbers so onyl use for debugging and tracking issues
#ifdef  FS_HAS_TELEMETRY_SUPPORT
// USAGE_TRACKING - displays overlapping stats that may imply double counting.
// ATTACHMENT_TRACKING - displays detailed tracking info for Avatar and Attachment. very heavy overhead.
// #define USAGE_TRACKING
// #define ATTACHMENT_TRACKING
#else
#undef USAGE_TRACKING
#undef ATTACHMENT_TRACKING
#endif

extern U32 gFrameCount;
extern LLUUID gAgentID;
namespace FSPerfStats
{
#ifdef USAGE_TRACKING
    extern std::atomic<int64_t> inUse;
    extern std::atomic<int64_t> inUseAvatar;
    extern std::atomic<int64_t> inUseScene;
    extern std::atomic<int64_t> inUseAttachment;
    extern std::atomic<int64_t> inUseAttachmentRigged;
    extern std::atomic<int64_t> inUseAttachmentUnRigged;
#endif
// Note if changing these, they should correspond with the log range of the correpsonding sliders
    constexpr U64 ART_UNLIMITED_NANOS{50000000};
    constexpr U64 ART_MINIMUM_NANOS{100000};
    constexpr U64 ART_MIN_ADJUST_UP_NANOS{10000};
    constexpr U64 ART_MIN_ADJUST_DOWN_NANOS{10000}; 

    constexpr F32 PREFERRED_DD{180};

    extern std::atomic<int64_t> tunedAvatars;
    extern U32 targetFPS; // desired FPS
    extern U64 renderAvatarMaxART_ns;
    extern U32 lastGlobalPrefChange;
    extern std::mutex bufferToggleLock;
    extern bool autoTune;

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
        RENDER_DISPLAY,
        RENDER_SLEEP,
        RENDER_LFS,
        RENDER_MESHREPO,
        RENDER_FPSLIMIT,
        RENDER_FPS,
        RENDER_IDLE,
        RENDER_DONE, // toggle buffer & clearbuffer (see processUpdate for hackery)
		STATS_COUNT
	};

    struct StatsRecord
    { 
        StatType_t  statType;
        ObjType_t   objType;
        LLUUID      avID;
        LLUUID      objID;
        uint64_t    time;
        bool        isRigged;
        bool        isHUD;
    };

    struct Tunables
    {
        static constexpr U32 Nothing{0};
        static constexpr U32 NonImposters{1};
        static constexpr U32 ReflectionDetail{2};
        static constexpr U32 FarClip{4};

        U32 tuningFlag{0};
        U32 nonImposters;
        S32 reflectionDetail;
        F32 farClip;

        void updateFarClip(F32 nv){farClip=nv; tuningFlag |= FarClip;};
        void updateNonImposters(U32 nv){nonImposters=nv; tuningFlag |= NonImposters;};
        void updateReflectionDetail(S32 nv){reflectionDetail=nv; tuningFlag |= ReflectionDetail;};

        void applyUpdates();
        void resetChanges(){tuningFlag=Nothing;};
    };

    extern Tunables tunables;

    class StatsRecorder{
        using Queue = moodycamel::BlockingConcurrentQueue<StatsRecord>;
    public:

        static inline StatsRecorder& getInstance()
        {
            static StatsRecorder instance;
            // volatile int dummy{};
            return instance;
        }
        static inline void setFocusAv(const LLUUID& avID){focusAv = avID;};
        static inline const LLUUID& getFocusAv(){return (focusAv);};
        static inline void send(StatsRecord&& u){StatsRecorder::getInstance().q.enqueue(u);};
        static void endFrame(){StatsRecorder::getInstance().q.enqueue(StatsRecord{StatType_t::RENDER_DONE, ObjType_t::OT_GENERAL, LLUUID::null, LLUUID::null, 0});};
        static void clearStats(){StatsRecorder::getInstance().q.enqueue(StatsRecord{StatType_t::RENDER_DONE, ObjType_t::OT_GENERAL, LLUUID::null, LLUUID::null, 1});};

        static inline void setEnabled(bool on_or_off){collectionEnabled=on_or_off;};
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
            return statsDoubleBuffer[getReadBufferIndex()][static_cast<size_t>(ObjType_t::OT_GENERAL)][LLUUID::null][static_cast<size_t>(type)];
        }

        static inline uint64_t getSum(ObjType_t otype, StatType_t type)
        {
            return sum[getReadBufferIndex()][static_cast<size_t>(otype)][static_cast<size_t>(type)];
        }
        static inline uint64_t getMax(ObjType_t otype, StatType_t type)
        {
            return max[getReadBufferIndex()][static_cast<size_t>(otype)][static_cast<size_t>(type)];
        }
        static void updateSettingsFromRenderCostLimit();
        static void updateRenderCostLimitFromSettings();
        static void updateAvatarParams();
    private:
        StatsRecorder();

        static int countNearbyAvatars(S32 distance);
// StatsArray is a uint64_t for each possible statistic type.
        using StatsArray    = std::array<uint64_t, static_cast<size_t>(FSPerfStats::StatType_t::STATS_COUNT)>;
        using StatsMap      = std::unordered_map<LLUUID, StatsArray, FSUUIDHash>;
        using StatsTypeMatrix = std::array<StatsMap, static_cast<size_t>(FSPerfStats::ObjType_t::OT_COUNT)>;
        using StatsSummaryArray = std::array<StatsArray, static_cast<size_t>(FSPerfStats::ObjType_t::OT_COUNT)>;

		static std::atomic<int> writeBuffer;
		static LLUUID focusAv;
        static std::array<StatsTypeMatrix,2> statsDoubleBuffer;
        static std::array<StatsSummaryArray,2> max;
        static std::array<StatsSummaryArray,2> sum;
        static bool collectionEnabled;


        void processUpdate(const StatsRecord& upd)
        {
            FSZone;
            // LL_INFOS("perfstats") << "processing update:" << LL_ENDL;
            using ST = StatType_t;
            // Note: nullptr is used as the key for global stats
            #ifdef FS_HAS_TELEMETRY_SUPPORT
            static char avstr[36];
            static char obstr[36];
            #endif

            if(upd.statType == StatType_t::RENDER_DONE && upd.objType == ObjType_t::OT_GENERAL && upd.time == 0)
            {
                // LL_INFOS("perfstats") << "End of Frame Toggle Buffer:" << gFrameCount << LL_ENDL;
                toggleBuffer();
                return;
            }
            if(upd.statType == StatType_t::RENDER_DONE && upd.objType == ObjType_t::OT_GENERAL && upd.time == 1)
            {
                // LL_INFOS("perfstats") << "New region - clear buffers:" << gFrameCount << LL_ENDL;
                clearStatsBuffers();
                return;
            }

            auto ot{upd.objType};
            auto& key{upd.objID};
            auto& avKey{upd.avID};
            auto type {upd.statType};
            auto val {upd.time};

            #ifdef FS_HAS_TELEMETRY_SUPPORT
            FSZoneText(key.toStringFast(obstr),36);
            FSZoneText(avKey.toStringFast(avstr),36);
            FSZoneValue(val);
            #endif

            if(ot == ObjType_t::OT_GENERAL)
            {
                // LL_INFOS("perfstats") << "General update:" << LL_ENDL;
                doUpd(key, ot, type,val);
                return;
            }

            if(ot == ObjType_t::OT_AVATAR)
            {
                // LL_INFOS("perfstats") << "Avatar update:" << LL_ENDL;
                doUpd(avKey, ot, type, val);
                return;
            }

            if(ot == ObjType_t::OT_ATTACHMENT)
            {
                if( !upd.isRigged && !upd.isHUD )
                {
                    // For all attachments that are not rigged we add them to the avatar (for all avatars) cost.
                    doUpd(avKey, ObjType_t::OT_AVATAR, type, val);
                }
                if( avKey == focusAv )
                {
                // For attachments that are for the focusAv (self for now) we record them for the attachment/complexity view
                    if(upd.isHUD)
                    {
                        ot = ObjType_t::OT_HUD;
                    }
                    // LL_INFOS("perfstats") << "frame: " << gFrameCount << " Attachment update("<< (type==StatType_t::RENDER_GEOMETRY?"GEOMETRY":"SHADOW") << ": " << key.asString() << " = " << val << LL_ENDL;
                    doUpd(key, ot, type, val);
                }
                else
                {
                    // LL_INFOS("perfstats") << "frame: " << gFrameCount << " non-self Att update("<< (type==StatType_t::RENDER_GEOMETRY?"GEOMETRY":"SHADOW") << ": " << key.asString() << " = " << val << " for av " << avKey.asString() << LL_ENDL;
                }
            }

        }

        static inline void doUpd(const LLUUID& key, ObjType_t ot, StatType_t type, uint64_t val)
        {
            FSZone;
            using ST = StatType_t;
            StatsMap& stm {statsDoubleBuffer[writeBuffer][static_cast<size_t>(ot)]};
            auto& thisAsset = stm[key];

            thisAsset[static_cast<size_t>(type)] += val;
            thisAsset[static_cast<size_t>(ST::RENDER_COMBINED)] += val;

            sum[writeBuffer][static_cast<size_t>(ot)][static_cast<size_t>(type)] += val;
            sum[writeBuffer][static_cast<size_t>(ot)][static_cast<size_t>(ST::RENDER_COMBINED)] += val;

            if(max[writeBuffer][static_cast<size_t>(ot)][static_cast<size_t>(type)] < thisAsset[static_cast<size_t>(type)])
            {
                max[writeBuffer][static_cast<size_t>(ot)][static_cast<size_t>(type)] = thisAsset[static_cast<size_t>(type)];
            }
            if(max[writeBuffer][static_cast<size_t>(ot)][static_cast<size_t>(ST::RENDER_COMBINED)] < thisAsset[static_cast<size_t>(ST::RENDER_COMBINED)])
            {
                max[writeBuffer][static_cast<size_t>(ot)][static_cast<size_t>(ST::RENDER_COMBINED)] = thisAsset[static_cast<size_t>(ST::RENDER_COMBINED)];
            }
        }

        static void toggleBuffer();
        static void clearStatsBuffers();

        // thread entry
        static void run()
        {
            StatsRecord upd[10];
            auto& instance {StatsRecorder::getInstance()};
            FSThreadName( "PerfStats" );

            while( enabled() && !LLApp::isExiting() )
            {
                auto count = instance.q.wait_dequeue_bulk_timed(upd, 10, std::chrono::milliseconds(10));
                if(count)
                {
                    // LL_INFOS("perfstats") << "processing " << count << " updates." << LL_ENDL;
                    for(auto i =0; i < count; i++)
                    {
                        instance.processUpdate(upd[i]);
                    }
                }
            }
        }

        Queue q;
        std::thread t;

        ~StatsRecorder() = default;
        StatsRecorder(const StatsRecorder&) = delete;
        StatsRecorder& operator=(const StatsRecorder&) = delete;

    };

	template <enum ObjType_t ObjTypeDiscriminator>
    class RecordTime
	{

    private:
        RecordTime(const RecordTime&) = delete;
        RecordTime() = delete;
        U64 start;
    public:
        StatsRecord stat;

        RecordTime( const LLUUID& av, const LLUUID& id, StatType_t type, bool isRiggedAtt=false, bool isHUDAtt=false):
                    start{LLTrace::BlockTimer::getCPUClockCount64()},
                    stat{type, ObjTypeDiscriminator, std::move(av), std::move(id), 0, isRiggedAtt, isHUDAtt}
        {
            FSZoneC(tracy::Color::Orange);
        #ifdef USAGE_TRACKING
            if(stat.objType == FSPerfStats::ObjType_t::OT_ATTACHMENT)
            {
                if(!stat.isRigged && FSPerfStats::inUseAvatar){FSZoneText("OVERLAP AVATAR",14);}

                FSPlotSq("InUse", (int64_t)FSPerfStats::inUse, (int64_t)FSPerfStats::inUse+1);
                FSPerfStats::inUse++;
                FSPlotSq("InUseAttachment", (int64_t)FSPerfStats::inUseAttachment, (int64_t)FSPerfStats::inUseAttachment+1);
                FSPerfStats::inUseAttachment++;
                if (stat.isRigged)
                {
                    FSPlotSq("InUseAttachmentRigged", (int64_t)FSPerfStats::inUseAttachmentRigged,(int64_t)FSPerfStats::inUseAttachmentRigged+1);
                    FSPerfStats::inUseAttachmentRigged++;
                }
                else
                {
                    FSPlotSq("InUseAttachmentUnRigged", (int64_t)FSPerfStats::inUseAttachmentUnRigged,(int64_t)FSPerfStats::inUseAttachmentUnRigged+1);
                    FSPerfStats::inUseAttachmentUnRigged++;
                }            
            }
        #endif

        };

        template < ObjType_t OD = ObjTypeDiscriminator,
                   std::enable_if_t<OD == ObjType_t::OT_GENERAL> * = nullptr>
        RecordTime( StatType_t type ):RecordTime<ObjTypeDiscriminator>(LLUUID::null, LLUUID::null, type )
        {
            FSZone;
            #ifdef USAGE_TRACKING
            FSPlotSq("InUseScene", (int64_t)FSPerfStats::inUseScene, (int64_t)FSPerfStats::inUseScene+1);
            FSPerfStats::inUseScene++;
            FSPlotSq("InUse", (int64_t)FSPerfStats::inUse, (int64_t)FSPerfStats::inUse+1);
            FSPerfStats::inUse++;
            #endif
        };

        template < ObjType_t OD = ObjTypeDiscriminator,
                   std::enable_if_t<OD == ObjType_t::OT_AVATAR> * = nullptr>
        RecordTime( const LLUUID & av, StatType_t type ):RecordTime<ObjTypeDiscriminator>(std::move(av), LLUUID::null, type)
        {
            FSZoneC(tracy::Color::Purple);

        #ifdef USAGE_TRACKING
            if(FSPerfStats::inUseAvatar){FSZoneText("OVERLAP AVATAR",14);}

            FSPlotSq("InUseAv", (int64_t)FSPerfStats::inUseAvatar, (int64_t)FSPerfStats::inUseAvatar+1);
            FSPerfStats::inUseAvatar++;
            FSPlotSq("InUse", (int64_t)FSPerfStats::inUse, (int64_t)FSPerfStats::inUse+1);
            FSPerfStats::inUse++;

        #endif

        };

        ~RecordTime()
        { 
            if(!FSPerfStats::StatsRecorder::enabled())
            {
                return;
            }
            
            
            FSZoneC(tracy::Color::Red);
            
        #ifdef USAGE_TRACKING
            FSPlotSq("InUse", (int64_t)FSPerfStats::inUse,(int64_t)FSPerfStats::inUse-1);
            --FSPerfStats::inUse;
            if (stat.objType == FSPerfStats::ObjType_t::OT_ATTACHMENT)
            {
                FSPlotSq("InUseAttachment", (int64_t)FSPerfStats::inUseAttachment,(int64_t)FSPerfStats::inUseAttachment-1);
                --FSPerfStats::inUseAttachment;
                if (stat.isRigged)
                {
                    FSPlotSq("InUseAttachmentRigged", (int64_t)FSPerfStats::inUseAttachmentRigged,(int64_t)FSPerfStats::inUseAttachmentRigged-1);
                    --FSPerfStats::inUseAttachmentRigged;
                }
                else
                {
                    FSPlotSq("InUseAttachmentUnRigged", (int64_t)FSPerfStats::inUseAttachmentUnRigged,(int64_t)FSPerfStats::inUseAttachmentUnRigged-1);
                    --FSPerfStats::inUseAttachmentUnRigged;
                }
            }
            if (stat.objType == FSPerfStats::ObjType_t::OT_GENERAL)
            {
                FSPlotSq("InUseScene", (int64_t)FSPerfStats::inUseScene,(int64_t)FSPerfStats::inUseScene-1);
                --FSPerfStats::inUseScene;
            }
            if( stat.objType == FSPerfStats::ObjType_t::OT_AVATAR )
            {
                FSPlotSq("InUseAv", (int64_t)FSPerfStats::inUseAvatar, (int64_t)FSPerfStats::inUseAvatar-1);
                --FSPerfStats::inUseAvatar;
            }
        #endif
            stat.time = LLTrace::BlockTimer::getCPUClockCount64() - start;
            
        #ifdef ATTACHMENT_TRACKING
            static char obstr[36];
            static char avstr[36];
            FSZoneValue(static_cast<U64>(stat.objType));
            FSZoneText(stat.avID.toStringFast(avstr), 36);
            FSZoneText(stat.objID.toStringFast(obstr), 36);
            FSZoneValue(stat.time);
        #endif
            
            StatsRecorder::send(std::move(stat));
        };


	};

    
    inline double raw_to_ns(U64 raw)    { return (static_cast<double>(raw) * 1000000000.0) / (F64)LLTrace::BlockTimer::countsPerSecond(); };
    inline double raw_to_us(U64 raw)    { return (static_cast<double>(raw) *    1000000.0) / (F64)LLTrace::BlockTimer::countsPerSecond(); };
    inline double raw_to_ms(U64 raw)    { return (static_cast<double>(raw) *       1000.0) / (F64)LLTrace::BlockTimer::countsPerSecond(); };

    using RecordSceneTime = RecordTime<ObjType_t::OT_GENERAL>;
    using RecordAvatarTime = RecordTime<ObjType_t::OT_AVATAR>;
    using RecordAttachmentTime = RecordTime<ObjType_t::OT_ATTACHMENT>;
    using RecordHudAttachmentTime = RecordTime<ObjType_t::OT_HUD>;
     
};// namespace FSPerfStats

// helper functions
using RATptr = std::unique_ptr<FSPerfStats::RecordAttachmentTime>;

template <typename T>
static inline void trackAttachments(const T * vobj, bool isRigged, RATptr* ratPtrp)
{
	if( !vobj ){ ratPtrp->reset(); return;};
	
    const T* rootAtt{vobj};
	if( rootAtt->isAttachment() )
	{
	    FSZone;

		while( !rootAtt->isRootEdit() )
		{
			rootAtt = (T*)(rootAtt->getParent());
		}

        auto avPtr = (T*)(rootAtt->getParent()); 
        if(!avPtr){ratPtrp->reset(); return;}

        auto& av = avPtr->getID();
        auto& obj = rootAtt->getAttachmentItemID();
        if(!*ratPtrp || (*ratPtrp)->stat.objID != obj || (*ratPtrp)->stat.avID != av )
        {
            #if TRACY_ENABLE && defined(ATTACHMENT_TRACKING)            
            FSZoneNC( "trackAttachments:new", tracy::Color::Red );
            auto& str = rootAtt->getAttachmentItemName();
            FSZoneText(str.c_str(), str.size());
            FSZoneText(isRigged?"Rigged  ":"Unrigged",8);
            static char avStr[36];
            av.toStringFast(avStr);
            static char obStr[4];
            obj.toShortString(obStr);
            FSZoneText( avStr, 36);
            FSZoneText( obStr, 4);
            #endif
            if(*ratPtrp){ratPtrp->reset();}; // deliberately reset to ensure destruction before construction of replacement.
            *ratPtrp = std::make_unique<FSPerfStats::RecordAttachmentTime>( av, 
                                                                            obj,
                                                                            ( (LLPipeline::sShadowRender)?FSPerfStats::StatType_t::RENDER_SHADOWS : FSPerfStats::StatType_t::RENDER_GEOMETRY ), 
                                                                            isRigged, 
                                                                            rootAtt->isHUDAttachment());
        }
	}
	return;
};

#endif