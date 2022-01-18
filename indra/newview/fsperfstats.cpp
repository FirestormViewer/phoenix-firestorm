/** 
 * @file fsperfstats.cpp
 * @brief Stats collection to support perf floater and auto tune
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

#include "llviewerprecompiledheaders.h"
#include "fsperfstats.h"
#include "llcontrol.h"
#include "pipeline.h"
#include "llagentcamera.h"
#include "llvoavatar.h"
#include "llworld.h"

extern LLControlGroup gSavedSettings;

namespace FSPerfStats
{
 #ifdef USAGE_TRACKING
    std::atomic<int64_t> inUse{0};
    std::atomic<int64_t> inUseAvatar{0};
    std::atomic<int64_t> inUseScene{0};
    std::atomic<int64_t> inUseAttachment{0};
    std::atomic<int64_t> inUseAttachmentRigged{0};
    std::atomic<int64_t> inUseAttachmentUnRigged{0};
#endif
    
    std::atomic<int64_t> tunedAvatars{0};
    U32 targetFPS; // desired FPS
    U64 renderAvatarMaxART_ns{(U64)(ART_UNLIMITED_NANOS)}; // highest render time we'll allow without culling features
    U32 fpsTuningStrategy{0}; // linked to FSTuningFPSStrategy
    U32 lastGlobalPrefChange{0}; 
    std::mutex bufferToggleLock{};
    bool autoTune{false};

    std::atomic<int> 	StatsRecorder::writeBuffer{0};
    bool 	            StatsRecorder::collectionEnabled{true};
    LLUUID              StatsRecorder::focusAv{LLUUID::null};
	std::array<StatsRecorder::StatsTypeMatrix,2>  StatsRecorder::statsDoubleBuffer{ {} };
    std::array<StatsRecorder::StatsSummaryArray,2> StatsRecorder::max{ {} };
    std::array<StatsRecorder::StatsSummaryArray,2> StatsRecorder::sum{ {} };


    StatsRecorder::StatsRecorder():q(1024*16),t(&StatsRecorder::run)
    {
        // create a queue
        // create a thread to consume from the queue

        FSPerfStats::targetFPS = gSavedSettings.getU32("FSTargetFPS");
        FSPerfStats::autoTune = gSavedSettings.getBOOL("FSAutoTuneFPS");

        updateRenderCostLimitFromSettings();

        t.detach();
    }

    // static
    void StatsRecorder::toggleBuffer()
    {
        FSZone;
        using ST = StatType_t;

        bool unreliable{false};
        static LLCachedControl<U32> smoothingPeriods(gSavedSettings, "FSPerfFloaterSmoothingPeriods");

        auto& sceneStats = statsDoubleBuffer[writeBuffer][static_cast<size_t>(ObjType_t::OT_GENERAL)][LLUUID::null];
        auto& lastStats = statsDoubleBuffer[writeBuffer ^ 1][static_cast<size_t>(ObjType_t::OT_GENERAL)][LLUUID::null];

        static constexpr std::initializer_list<StatType_t> sceneStatsToAvg = {
            StatType_t::RENDER_FRAME, 
            StatType_t::RENDER_DISPLAY, 
            StatType_t::RENDER_HUDS,
            StatType_t::RENDER_UI,
            StatType_t::RENDER_SWAP,
            // RENDER_LFS,
            // RENDER_MESHREPO,
            StatType_t::RENDER_IDLE };

        static constexpr std::initializer_list<StatType_t> avatarStatsToAvg = {
            StatType_t::RENDER_GEOMETRY, 
            StatType_t::RENDER_SHADOWS, 
            StatType_t::RENDER_COMBINED,
            StatType_t::RENDER_IDLE };


        if( sceneStats[static_cast<size_t>(StatType_t::RENDER_FPSLIMIT)] != 0 || sceneStats[static_cast<size_t>(StatType_t::RENDER_SLEEP)] != 0 )
        {
            unreliable = true;
            lastStats[static_cast<size_t>(StatType_t::RENDER_FPSLIMIT)] = sceneStats[static_cast<size_t>(StatType_t::RENDER_FPSLIMIT)];
            lastStats[static_cast<size_t>(StatType_t::RENDER_SLEEP)] = sceneStats[static_cast<size_t>(StatType_t::RENDER_SLEEP)];
            lastStats[static_cast<size_t>(StatType_t::RENDER_FRAME)] = sceneStats[static_cast<size_t>(StatType_t::RENDER_FRAME)]; //  bring over the total frame render time to deal with region crossing overlap issues
        }

        if(!unreliable)
        {
            // only use these stats when things are reliable. 

            for(auto & statEntry : sceneStatsToAvg)
            {
                auto avg = lastStats[static_cast<size_t>(statEntry)];
                auto val = sceneStats[static_cast<size_t>(statEntry)];
                sceneStats[static_cast<size_t>(statEntry)] = avg + (val/smoothingPeriods) - (avg/smoothingPeriods);
                // LL_INFOS("scenestats") << "Scenestat: " << static_cast<size_t>(statEntry) << " before=" << avg << " new=" << val << " newavg=" << statsDoubleBuffer[writeBuffer][static_cast<size_t>(ObjType_t::OT_GENERAL)][LLUUID::null][static_cast<size_t>(statEntry)] << LL_ENDL;
            }

            auto& statsMap = statsDoubleBuffer[writeBuffer][static_cast<size_t>(ObjType_t::OT_ATTACHMENT)];
            for(auto& stat_entry : statsMap)
            {
                auto val = stat_entry.second[static_cast<size_t>(ST::RENDER_COMBINED)];
                if(val>smoothingPeriods){
                    auto avg = statsDoubleBuffer[writeBuffer ^ 1][static_cast<size_t>(ObjType_t::OT_ATTACHMENT)][stat_entry.first][static_cast<size_t>(ST::RENDER_COMBINED)];
                    stat_entry.second[static_cast<size_t>(ST::RENDER_COMBINED)] = avg + (val/smoothingPeriods) - (avg/smoothingPeriods);
                }
            }


            auto& statsMapAv = statsDoubleBuffer[writeBuffer][static_cast<size_t>(ObjType_t::OT_AVATAR)];
            for(auto& stat_entry : statsMapAv)
            {
                for(auto& stat : avatarStatsToAvg)
                {
                    auto val = stat_entry.second[static_cast<size_t>(stat)];
                    if(val>smoothingPeriods)
                    {
                        auto avg = statsDoubleBuffer[writeBuffer ^ 1][static_cast<size_t>(ObjType_t::OT_AVATAR)][stat_entry.first][static_cast<size_t>(stat)];
                        stat_entry.second[static_cast<size_t>(stat)] = avg + (val/smoothingPeriods) - (avg/smoothingPeriods);
                    }
                }
            }

            // swap the buffers
            if(enabled())
            {
                std::lock_guard<std::mutex> lock{bufferToggleLock};
                writeBuffer ^= 1;
            }; // note we are relying on atomic updates here. The risk is low and would cause minor errors in the stats display. 
        }

        // clean the write maps in all cases.
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

        // and now adjust the visuals.
        if(autoTune)
        {
            updateAvatarParams();
        }
    }

    // clear buffers when we change region or need a hard reset.
    // static 
    void StatsRecorder::clearStatsBuffers()
    {
        FSZone;
        using ST = StatType_t;

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
        // swap the clean buffers in
        if(enabled())
        {
            std::lock_guard<std::mutex> lock{bufferToggleLock};
            writeBuffer ^= 1;
        }; 
        // repeat before we start processing new stuff
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

    // static 
    void StatsRecorder::updateSettingsFromRenderCostLimit()
    {
        static LLCachedControl<F32> maxRenderCost_us(gSavedSettings, "FSRenderAvatarMaxART");    
        if( (F32)maxRenderCost_us != log10( ( (F32)FSPerfStats::renderAvatarMaxART_ns )/1000 ) )
        {
            if( FSPerfStats::renderAvatarMaxART_ns != 0 )
            {
                gSavedSettings.setF32( "FSRenderAvatarMaxART", log10( ( (F32)FSPerfStats::renderAvatarMaxART_ns )/1000 ) );
            }
            else
            {
                gSavedSettings.setF32( "FSRenderAvatarMaxART",log10( FSPerfStats::ART_UNLIMITED_NANOS/1000 ) );
            }
        }        
    }

    // static
    void StatsRecorder::updateRenderCostLimitFromSettings()
    {
	    const auto newval = gSavedSettings.getF32("FSRenderAvatarMaxART");
	    if(newval < log10(FSPerfStats::ART_UNLIMITED_NANOS/1000))
	    {
    		FSPerfStats::renderAvatarMaxART_ns = pow(10,newval)*1000;
    	}
    	else
    	{
		    FSPerfStats::renderAvatarMaxART_ns = 0;
	    };        
    }

    //static
    int StatsRecorder::countNearbyAvatars(S32 distance)
    {
        const auto our_pos = gAgentCamera.getCameraPositionGlobal();

       	std::vector<LLVector3d> positions;
	    uuid_vec_t avatar_ids;
        LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, our_pos, distance);
        return positions.size();
	}

    // static
    void StatsRecorder::updateAvatarParams()
    {
        static LLCachedControl<F32> drawDistance(gSavedSettings, "RenderFarClip");
        static LLCachedControl<F32> userMinDrawDistance(gSavedSettings, "FSAutoTuneRenderFarClipMin");
        static LLCachedControl<F32> userTargetDrawDistance(gSavedSettings, "FSAutoTuneRenderFarClipTarget");
        static LLCachedControl<F32> impostorDistance(gSavedSettings, "FSAutoTuneImpostorFarAwayDistance");
        static LLCachedControl<bool> impostorDistanceTuning(gSavedSettings, "FSAutoTuneImpostorByDistEnabled");
        static LLCachedControl<U32> maxNonImpostors (gSavedSettings, "IndirectMaxNonImpostors");

        if(impostorDistanceTuning)
        {
            // if we have less than the user's "max Non-Impostors" avatars within the desired range then adjust the limit.
            // also adjusts back up again for nearby crowds.
            auto count = countNearbyAvatars(std::min(drawDistance, impostorDistance));
            if( count != maxNonImpostors )
            {
                gSavedSettings.setU32("IndirectMaxNonImpostors", (count < LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER)?count : LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER);
                LL_DEBUGS("AutoTune") << "There are " << count << "avatars within " << std::min(drawDistance, impostorDistance) << "m of the camera" << LL_ENDL;
            }
        }

        auto av_render_max_raw = FSPerfStats::StatsRecorder::getMax(ObjType_t::OT_AVATAR, FSPerfStats::StatType_t::RENDER_COMBINED);
        // Is our target frame time lower than current? If so we need to take action to reduce draw overheads.
        // cumulative avatar time (includes idle processing, attachments and base av)
        auto tot_avatar_time_raw = FSPerfStats::StatsRecorder::getSum(ObjType_t::OT_AVATAR, FSPerfStats::StatType_t::RENDER_COMBINED);
        // sleep time is basically forced sleep when window out of focus 
        auto tot_sleep_time_raw = FSPerfStats::StatsRecorder::getSceneStat(FSPerfStats::StatType_t::RENDER_SLEEP);
        // similar to sleep time, induced by FPS limit
        auto tot_limit_time_raw = FSPerfStats::StatsRecorder::getSceneStat(FSPerfStats::StatType_t::RENDER_FPSLIMIT);


        // the time spent this frame on the "doFrame" call. Treated as "tot time for frame"
        auto tot_frame_time_raw = FSPerfStats::StatsRecorder::getSceneStat(FSPerfStats::StatType_t::RENDER_FRAME);

        if( tot_sleep_time_raw != 0 )
        {
            // Note: we do not average sleep 
            // if at some point we need to, the averaging will need to take this into account or 
            // we forever think we're in the background due to residuals.
            LL_DEBUGS() << "No tuning when not in focus" << LL_ENDL;
            return;
        }

        // The frametime budget we have based on the target FPS selected
        auto target_frame_time_raw = (U64)llround((F64)LLTrace::BlockTimer::countsPerSecond()/(targetFPS==0?1:targetFPS));
        // LL_INFOS() << "Effective FPS(raw):" << tot_frame_time_raw << " Target:" << target_frame_time_raw << LL_ENDL;

        if( tot_limit_time_raw != 0)
        {
            // This could be problematic.
            tot_frame_time_raw -= tot_limit_time_raw;
        }
        // 1) Is the target frame tim lower than current?
        if( target_frame_time_raw <= tot_frame_time_raw )
        {
            // if so we've got work to do

            // how much of the frame was spent on non avatar related work?
            U32 non_avatar_time_raw = tot_frame_time_raw - tot_avatar_time_raw;

            // If the target frame time < non avatar frame time thne adjusting avatars is only goin gto get us so far.
            U64 target_avatar_time_raw;
            if(target_frame_time_raw < non_avatar_time_raw)
            {
                // we cannnot do this by avatar adjustment alone.
                if((gFrameCount - FSPerfStats::lastGlobalPrefChange) > 10) // give  changes a short time to take effect.
                {
                    if(FSPerfStats::fpsTuningStrategy == 1)
                    {
                        // 1 - hack the water to opaque. all non opaque have a significant hit, this is a big boost for (arguably) a minor visual hit.
                        // the other reflection options make comparatively little change and iof this overshoots we'll be stepping back up later
                        if(LLPipeline::RenderReflectionDetail != -2)
                        {
                            gSavedSettings.setS32("RenderReflectionDetail", -2);
                            FSPerfStats::lastGlobalPrefChange = gFrameCount;
                            return;
                        }
                        else // deliberately "else" here so we only do these in steps
                        {
                            // step down the DD by 10m per update
                            auto new_dd = (drawDistance-10>userMinDrawDistance)?(drawDistance - 10) : userMinDrawDistance;
                            if(new_dd != drawDistance)
                            {
                                gSavedSettings.setF32("RenderFarClip", new_dd);
                                FSPerfStats::lastGlobalPrefChange = gFrameCount;
                                return;
                            }
                        }
                    }
                }
                target_avatar_time_raw = 0;
            }
            else
            {
                // desired avatar budget.
                target_avatar_time_raw =  target_frame_time_raw - non_avatar_time_raw;
            }

            if( target_avatar_time_raw < tot_avatar_time_raw )
            {
                // we need to spend less time drawing avatars to meet our budget
                // Note: working in usecs now cos reasons.
                auto new_render_limit_ns {renderAvatarMaxART_ns};
                // max render this frame may be higher than the last (cos new entrants and jitter) so make sure we are heading in the right direction
                if(FSPerfStats::raw_to_ns(av_render_max_raw) < renderAvatarMaxART_ns)
                {
                    new_render_limit_ns = FSPerfStats::raw_to_ns(av_render_max_raw);
                }
                else
                {
                    new_render_limit_ns = renderAvatarMaxART_ns;
                }
                new_render_limit_ns -= FSPerfStats::ART_MIN_ADJUST_DOWN_NANOS;

                // bounce at the bottom to prevent "no limit" 
                new_render_limit_ns = std::max((U64)new_render_limit_ns, (U64)FSPerfStats::ART_MINIMUM_NANOS);

                // assign the new value
                renderAvatarMaxART_ns = new_render_limit_ns;
                // LL_DEBUGS() << "AUTO_TUNE: avatar_budget adjusted to:" << new_render_limit_ns << LL_ENDL;
            }
            // LL_DEBUGS() << "AUTO_TUNE: Target frame time:"<< FSPerfStats::raw_to_us(target_frame_time_raw) << "usecs (non_avatar is " << FSPerfStats::raw_to_us(non_avatar_time_raw) << "usecs) Max cost limited=" << renderAvatarMaxART_ns << LL_ENDL;
        }
        else if( FSPerfStats::raw_to_ns(target_frame_time_raw) > (FSPerfStats::raw_to_ns(tot_frame_time_raw) + renderAvatarMaxART_ns) )
        {
            if( FSPerfStats::tunedAvatars >= 0 )
            {
                // if we have more time to spare let's shift up little in the hope we'll restore an avatar.
                renderAvatarMaxART_ns += FSPerfStats::ART_MIN_ADJUST_UP_NANOS;
            }
            if( drawDistance < userTargetDrawDistance ) 
            {
                gSavedSettings.setF32("RenderFarClip", drawDistance + 10.);
            }
            if( (target_frame_time_raw * 1.5) > tot_frame_time_raw &&
                FSPerfStats::tunedAvatars == 0 &&
                drawDistance >= userTargetDrawDistance)
            {
                // if everything else is "max" and we have 50% headroom let's knock the water quality up a notch at a time.
                auto water = gSavedSettings.getS32("RenderReflectionDetail");
                gSavedSettings.setS32("RenderReflectionDetail", water+1);                
            }
        }
        updateSettingsFromRenderCostLimit();
   }
}