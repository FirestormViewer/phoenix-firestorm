/**
 * @file fsworldmapmessage.cpp
 * @brief FS specific extensions to world map handling for OpenSim
 *
 * $LicenseInfo:firstyear=2024&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2024, The Phoenix Firestorm Project, Inc.
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
 * 
 */
// potential workaround for hop://grid:port/Partial/x/y/z resolution
// 2024.04.30 humbletim

// notes:
//   - exact MapNameRequests are sent flagless here (not using LAYER_FLAG)
//     - this is to avoid triggering OpenSim code paths that modify result names
//     - only affects LLWorldMapMessage->sendNamedRegionRequest(name, callback, ...)
//     - in particular where a grid hosts overlapping names, hop Region matching may work better

#include "llviewerprecompiledheaders.h"

#ifdef OPENSIM
#include "fsworldmapmessage.h"

#include "llagent.h"
#include "llworldmap.h" // grid_to_region_handle
#include "fsgridhandler.h"
#include "llworldmapmessage.h"
#include "message.h"
#include <regex>

#define htxhop_log(format, ...) LL_DEBUGS("GridManager") << llformat(format, __VA_ARGS__) << LL_ENDL;

namespace hypergrid
{
    static inline std::string extract_region(const std::string& s)
    {
        static auto const& patterns = {
            std::regex { R"(/ ([^/:=]+)$)" }, // TODO: figure out where the spec lives for hop "slash space" embedding...
            std::regex { R"(([^/:=]+)$)" }, // TODO: figure out where the spec lives for hop "grid:port:region" embedding...
        };
        std::smatch match_results;
        std::string ls{ s };
        LLStringUtil::toLower(ls);
        for (const auto& pattern : patterns)
        {
            if (std::regex_search(ls, match_results, pattern))
            {
                return match_results[1].str();
            }
        }
        return {};
    }

    // helper to encapsulate Region Map Block responses
    struct MapBlock
    {
        S32 index{};
        U16 x_regions{}, y_regions{}, x_size{ REGION_WIDTH_UNITS }, y_size{ REGION_WIDTH_UNITS };
        std::string name{};
        U8 accesscode{};
        U32 region_flags{};
        LLUUID image_id{};

        inline U32 x_world() const { return (U32)(x_regions)*REGION_WIDTH_UNITS; }
        inline U32 y_world() const { return (U32)(y_regions)*REGION_WIDTH_UNITS; }
        inline U64 region_handle() const { return grid_to_region_handle(x_regions, y_regions); }

        // see: LLWorldMapMessage::processMapBlockReply
        MapBlock(LLMessageSystem* msg, S32 block)
            : index(block)
        {
            msg->getU16Fast(_PREHASH_Data, _PREHASH_X, x_regions, block);
            msg->getU16Fast(_PREHASH_Data, _PREHASH_Y, y_regions, block);
            msg->getStringFast(_PREHASH_Data, _PREHASH_Name, name, block);
            msg->getU8Fast(_PREHASH_Data, _PREHASH_Access, accesscode, block);
            msg->getU32Fast(_PREHASH_Data, _PREHASH_RegionFlags, region_flags, block);
            //        msg->getU8Fast(_PREHASH_Data, _PREHASH_WaterHeight, water_height, block);
            //        msg->getU8Fast(_PREHASH_Data, _PREHASH_Agents, agents, block);
            msg->getUUIDFast(_PREHASH_Data, _PREHASH_MapImageID, image_id, block);
            // <FS:CR> Aurora Sim
            if (msg->getNumberOfBlocksFast(_PREHASH_Size) > 0)
            {
                msg->getU16Fast(_PREHASH_Size, _PREHASH_SizeX, x_size, block);
                msg->getU16Fast(_PREHASH_Size, _PREHASH_SizeY, y_size, block);
            }
            if (x_size == 0 || (x_size % 16) != 0 || (y_size % 16) != 0)
            {
                x_size = 256;
                y_size = 256;
            }
            // </FS:CR> Aurora Sim
        }
    };

    constexpr U32 EXACT_FLAG = 0x00000000;
    constexpr U32 LAYER_FLAG = 0x00000002;

    // see: LLWorldMapMessage::sendNamedRegionRequest
    static void sendMapNameRequest(const std::string& region_name, U32 flags)
    {
        LLMessageSystem* msg = gMessageSystem;
        msg->newMessageFast(_PREHASH_MapNameRequest);
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
        msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
        msg->addU32Fast(_PREHASH_Flags, flags);
        msg->addU32Fast(_PREHASH_EstateID, 0); // Filled in on sim
        msg->addBOOLFast(_PREHASH_Godlike, false); // Filled in on sim
        msg->nextBlockFast(_PREHASH_NameData);
        msg->addStringFast(_PREHASH_Name, region_name);
        gAgent.sendReliableMessage();
    }

    struct AdoptedRegionNameQuery
    {
        std::string key{};
        std::string region_name{};
        hypergrid::url_callback_t arbitrary_callback;
        std::string arbitrary_slurl{};
        bool arbitrary_teleport{ false };
    };

    // map extracted region names => pending query entries
    static std::map<std::string, AdoptedRegionNameQuery> region_name_queries;
}

bool hypergrid::sendExactNamedRegionRequest(const std::string& region_name, const url_callback_t& callback, const std::string& callback_url, bool teleport)
{
    if (!LLGridManager::instance().isInOpenSim() || !callback)
    {
        return false;
    }

    auto key = extract_region(region_name);
    if (key.empty())
    {
        return false;
    }
    region_name_queries.try_emplace(key, AdoptedRegionNameQuery{ key, region_name, callback, callback_url, teleport });
    htxhop_log("Send Region Name '%s' (key: %s)", region_name.c_str(), key.c_str());
    sendMapNameRequest(region_name, EXACT_FLAG);
    return true;
}

bool hypergrid::processExactNamedRegionResponse(LLMessageSystem* msg, U32 agent_flags)
{
    if (!LLGridManager::instance().isInOpenSim() || !msg || agent_flags & LAYER_FLAG)
    {
        return false;
    }
    // NOTE: we assume only agent_flags have been read from msg so far
    S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_Data);

    std::vector<MapBlock> blocks;
    blocks.reserve(num_blocks);
    for (S32 b = 0; b < num_blocks; b++)
    {
        blocks.emplace_back(msg, b);
    }
    for (const auto& block : blocks)
    {
        htxhop_log("#%02d key='%s' block.name='%s' block.region_handle=%llu", block.index, extract_region(block.name).c_str(), block.name.c_str(), block.region_handle());
    }
    // special case: handle singular result w/empty name tho valid region handle AND singular pending query as a match
    // (might be that a landing area / redirect hop URL is coming back: "^hop://grid:port/$", which extract_region's into "")
    bool solo_result = blocks.size() == 2 && blocks[0].region_handle() && extract_region(blocks[0].name).empty() && !blocks[1].region_handle();
    if (solo_result && region_name_queries.size() == 1)
    {
        htxhop_log("applying first block as redirect; region_handle: %llu", blocks[0].region_handle());
        blocks[0].name = region_name_queries.begin()->second.region_name;
    }

    for (const auto& block : blocks)
    {
        auto key = extract_region(block.name);
        if (key.empty())
        {
            continue;
        }

        auto idx = region_name_queries.find(key);
        if (idx == region_name_queries.end())
        {
            continue;
        }

        auto pending{ idx->second };
        htxhop_log("Recv Region Name '%s' (key: %s) block.name='%s' block.region_handle=%llu)", pending.region_name.c_str(),
            pending.key.c_str(), block.name.c_str(), block.region_handle());
        region_name_queries.erase(idx);
        pending.arbitrary_callback(block.region_handle(), pending.arbitrary_slurl, block.image_id, pending.arbitrary_teleport);
        return true;
    }
    return false;
}
#endif
