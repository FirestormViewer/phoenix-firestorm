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

extern LLControlGroup gSavedSettings;

namespace FSPerfStats
{
 #ifdef USAGE_TRACKING
    std::atomic<int64_t> inUse{0};
    std::atomic<int64_t> inUseAvatar{0};
#endif
    std::atomic<int64_t> tunedAvatars{0};
    U32 targetFPS; // desired FPS
    U32 renderAvatarMaxART{50000}; // highest render time we'll allow without culling features
    U32 fpsTuningStrategy{0}; // linked to FSTuningFPSStrategy
    U32 lastGlobalPrefChange{0}; 
    std::mutex bufferToggleLock{};
    bool autoTune{false};

    U32 smoothingPeriods{1}; // number of frames to smooth over.


    std::atomic<int> 	StatsRecorder::writeBuffer{0};
    bool 	            StatsRecorder::collectionEnabled{true};
    LLUUID       StatsRecorder::focusAv{LLUUID::null};
	std::array<StatsRecorder::StatsTypeMatrix,2>  StatsRecorder::statsDoubleBuffer{ {} };
    std::array<StatsRecorder::StatsSummaryArray,2> StatsRecorder::max{ {} };
    std::array<StatsRecorder::StatsSummaryArray,2> StatsRecorder::sum{ {} };


    StatsRecorder::StatsRecorder():q(1024*16),t(&StatsRecorder::run)
    {
        // create a queue
        // create a thread to consume from the queue

        FSPerfStats::targetFPS = gSavedSettings.getU32("FSTargetFPS");
        FSPerfStats::autoTune = gSavedSettings.getBOOL("FSAutoTuneFPS");
        FSPerfStats::renderAvatarMaxART = gSavedSettings.getBOOL("FSRenderAvatarMaxART");
        FSPerfStats::smoothingPeriods = gSavedSettings.getU32("FSPerfFloaterSmoothingPeriods");

        t.detach();
    }

    // static
    void StatsRecorder::toggleBuffer()
    {
        FSZone;
        using ST = StatType_t;

        bool unreliable{false};

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
    void StatsRecorder::updateAvatarParams()
    {
        LLCachedControl<F32> drawDistance(gSavedSettings, "RenderFarClip");
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

        if(tot_sleep_time_raw != 0 || tot_limit_time_raw !=0 )
        {
            // Note: we do not average sleep and fpslimit, therefore we cannot reliably use them here.
            // if at some point we need to, the averaging will need to take this into account or 
            // we forever think we're in the background due to residuals.
            LL_DEBUGS() << "No tuning when not in focus" << LL_ENDL;
            return;
        }

        // LL_INFOS() << "Effective FPS:" << (1000000/FSPerfStats::raw_to_us(tot_frame_time_raw)) << " Target:" << targetFPS << LL_ENDL;
   
        // The frametime budget we have based on the target FPS selected
        auto target_frame_time_raw = (U64)llround(get_timer_info().mClockFrequency/(targetFPS==0?1:targetFPS));
        // LL_INFOS() << "Effective FPS(raw):" << tot_frame_time_raw << " Target:" << target_frame_time_raw << LL_ENDL;

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
                        // 1 - hack the water to opaque. all non opaque have a significan t hit, this is a big boost.
                        if(LLPipeline::RenderReflectionDetail != -2)
                        {
                            gSavedSettings.setS32("RenderReflectionDetail", -2);
                            FSPerfStats::lastGlobalPrefChange = gFrameCount;
                        }
                        else // deliberately "else" here so we only do these in steps
                        {
                            // step down the DD by 10m per update
                            auto new_dd = (drawDistance>42)?(drawDistance - 10) : 32;
                            gSavedSettings.setF32("RenderFarClip", new_dd);
                            FSPerfStats::lastGlobalPrefChange = gFrameCount;
                        }
                    }
                }
                // slam the avatar time to 0 "imposter all the things"
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
                U32 new_render_limit_us {0};
                // max render this frame may be higher than the last (cos new entrants and jitter) so make sure we are heading in the right direction
                if(FSPerfStats::raw_to_us(av_render_max_raw) < renderAvatarMaxART)
                {
                    new_render_limit_us = FSPerfStats::raw_to_us(av_render_max_raw);
                }
                else
                {
                    new_render_limit_us = renderAvatarMaxART;
                }
                new_render_limit_us -= 100;
                // bounce at the bottom to prevent "no limit"
                if(new_render_limit_us <= 0 || new_render_limit_us >1000000)
                {
                    new_render_limit_us = 100;
                }
                // assign the new value
                renderAvatarMaxART = new_render_limit_us;
                // LL_DEBUGS() << "AUTO_TUNE: avatar_budget adjusted to:" << new_render_limit_us << LL_ENDL;
            }
            // LL_DEBUGS() << "AUTO_TUNE: Target frame time:"<< FSPerfStats::raw_to_us(target_frame_time_raw) << "usecs (non_avatar is " << FSPerfStats::raw_to_us(non_avatar_time_raw) << "usecs) Max cost limited=" << renderAvatarMaxART << LL_ENDL;
        }
        else if( FSPerfStats::raw_to_us(target_frame_time_raw) > (FSPerfStats::raw_to_us(tot_frame_time_raw) + renderAvatarMaxART) )
        {
            if( FSPerfStats::tunedAvatars > 0 )
            {
                // if we have more time to spare let's shift up little in the hope we'll restore an avatar.
                renderAvatarMaxART += 10;
            }
            if(drawDistance < 180.) // TODO(Beq) make this less arbitrary
            {
                gSavedSettings.setF32("RenderFarClip", drawDistance + 10.);
            }
            if( (FSPerfStats::raw_to_us(target_frame_time_raw) * 1.5) > FSPerfStats::raw_to_us(tot_frame_time_raw) &&
                FSPerfStats::tunedAvatars == 0 &&
                drawDistance >= 128. )
            {
                // if everything else is "max" and we have 50% headroom let's knock the water quality up a notch at a time.
                auto water = gSavedSettings.getS32("RenderReflectionDetail");
                gSavedSettings.setS32("RenderReflectionDetail", water+1);                
            }
        }
    }
}