/**
 * @file llteleporthistorystorage.cpp
 * @brief Teleport history
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llteleporthistorystorage.h"

#include "llsd.h"
#include "llsdserialize.h"
#include "lldir.h"
#include "llteleporthistory.h"
#include "llagent.h"
#include "llfloaterreg.h"
#include "llfloaterworldmap.h"
// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b)
#include "rlvactions.h"
// [/RLVa:KB]

#include "llviewernetwork.h" // <FS/> Access to GridManager
#include "lfsimfeaturehandler.h" // <FS/> Access to hyperGridURL
#include "llworldmapmessage.h" // <FS/> Access to sendNamedRegionRequest
#include "llregionhandle.h" // <FS/> Access to from_region_handle

// Max offset for two global positions to consider them as equal
const F64 MAX_GLOBAL_POS_OFFSET = 5.0f;

LLTeleportHistoryPersistentItem::LLTeleportHistoryPersistentItem(const LLSD& val)
{
    mTitle = val["title"].asString();
    mGlobalPos.setValue(val["global_pos"]);
    mDate = val["date"];
    // <FS:TJ> Fix Teleport and Location History for OpenSim
    if (val.has("slurl") && !val["slurl"].asString().empty())
    {
        mSLURL = LLSLURL(val["slurl"].asString());
    }
    else
    {
        mSLURL = LLSLURL();
    }
    // </FS:TJ>
}

LLSD LLTeleportHistoryPersistentItem::toLLSD() const
{
    LLSD val;

    val["title"]        = mTitle;
    val["global_pos"]   = mGlobalPos.getValue();
    val["date"]     = mDate;
    val["slurl"]    = mSLURL.isValid() ? mSLURL.getSLURLString() : std::string(); // <FS:TJ/> Fix Teleport and Location History for OpenSim

    return val;
}

struct LLSortItemsByDate
{
    bool operator()(const LLTeleportHistoryPersistentItem& a, const LLTeleportHistoryPersistentItem& b)
    {
        return a.mDate < b.mDate;
    }
};

LLTeleportHistoryStorage::LLTeleportHistoryStorage() :
    mFilename("teleport_history.txt")
{
    mItems.clear();
    LLTeleportHistory *th = LLTeleportHistory::getInstance();
    if (th)
        th->setHistoryChangedCallback(boost::bind(&LLTeleportHistoryStorage::onTeleportHistoryChange, this));

    load();
}

LLTeleportHistoryStorage::~LLTeleportHistoryStorage()
{
}

void LLTeleportHistoryStorage::onTeleportHistoryChange()
{
    LLTeleportHistory *th = LLTeleportHistory::getInstance();
    if (!th)
        return;

    // Hacky sanity check. (EXT-6798)
    if (th->getItems().size() == 0)
    {
        llassert(!"Inconsistent teleport history state");
        return;
    }

    const LLTeleportHistoryItem &item = th->getItems()[th->getCurrentItemIndex()];
// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b) | Added: RLVa-1.2.1b
    // Make sure we don't attempt to save zero'ed out teleport history items
    if (item.mGlobalPos.isExactlyZero())
    {
        return;
    }
// [/RLVa:KB]

    // <FS:TJ> Fix Teleport and Location History for OpenSim
    //addItem(item.mTitle, item.mGlobalPos);
    addItem(item.mTitle, item.mGlobalPos, item.mSLURL);
    // </FS:TJ>
    save();
}

void LLTeleportHistoryStorage::purgeItems()
{
    mItems.clear();
    mHistoryChangedSignal(-1);
}

// <FS:TJ> Fix Teleport and Location History for OpenSim
//void LLTeleportHistoryStorage::addItem(const std::string title, const LLVector3d& global_pos)
void LLTeleportHistoryStorage::addItem(const std::string title, const LLVector3d& global_pos, const LLSLURL& slurl)
{
    //addItem(title, global_pos, LLDate::now());
    addItem(title, global_pos, LLDate::now(), slurl);
}
// </FS:TJ>


bool LLTeleportHistoryStorage::compareByTitleAndGlobalPos(const LLTeleportHistoryPersistentItem& a, const LLTeleportHistoryPersistentItem& b)
{
    return a.mTitle == b.mTitle && (a.mGlobalPos - b.mGlobalPos).length() < MAX_GLOBAL_POS_OFFSET;
}

// <FS:TJ> Fix Teleport and Location History for OpenSim
//void LLTeleportHistoryStorage::addItem(const std::string title, const LLVector3d& global_pos, const LLDate& date)
void LLTeleportHistoryStorage::addItem(const std::string title, const LLVector3d& global_pos, const LLDate& date, const LLSLURL& slurl)
{
// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b) | Added: RLVa-1.2.1b
    if (!RlvActions::canShowLocation())
    {
        return;
    }
// [/RLVa:KB]

    // <FS:TJ> Fix Teleport and Location History for OpenSim
    //LLTeleportHistoryPersistentItem item(title, global_pos, date);
    LLTeleportHistoryPersistentItem item(title, global_pos, date, slurl);

    slurl_list_t::iterator item_iter = std::find_if(mItems.begin(), mItems.end(),
                                boost::bind(&LLTeleportHistoryStorage::compareByTitleAndGlobalPos, this, _1, item));

    // If there is such item already, remove it, since new item is more recent
    S32 removed_index = -1;
    if (item_iter != mItems.end())
    {
        // When teleporting via history it's possible that there can be
        // an offset applied to the position, so each new teleport can
        // be a meter higher than the last.
        // Avoid it by preserving original position.
        item.mGlobalPos = item_iter->mGlobalPos;

        removed_index = (S32)(item_iter - mItems.begin());
        mItems.erase(item_iter);
    }

    mItems.push_back(item);

    // Check whether sorting is needed
    if (mItems.size() > 1)
    {
        item_iter = mItems.end();

        item_iter--;
        item_iter--;

        // If second to last item is more recent than last, then resort items
        if (item_iter->mDate > item.mDate)
        {
            removed_index = -1;
            std::sort(mItems.begin(), mItems.end(), LLSortItemsByDate());
        }
    }

    mHistoryChangedSignal(removed_index);
}

void LLTeleportHistoryStorage::removeItem(S32 idx)
{
    if (idx < 0 || idx >= (S32)mItems.size())
        return;

    mItems.erase (mItems.begin() + idx);
}

void LLTeleportHistoryStorage::save()
{
        // build filename for each user
    std::string resolvedFilename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, mFilename);

    // open the history file for writing
    llofstream file(resolvedFilename.c_str());
    if (!file.is_open())
    {
        LL_WARNS() << "can't open teleport history file \"" << mFilename << "\" for writing" << LL_ENDL;
        return;
    }

    for (size_t i=0; i<mItems.size(); i++)
    {
        LLSD s_item = mItems[i].toLLSD();
        file << LLSDOStreamer<LLSDNotationFormatter>(s_item) << std::endl;
    }

    file.close();
}

void LLTeleportHistoryStorage::load()
{
        // build filename for each user
    std::string resolved_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, mFilename);

    // open the history file for reading
    llifstream file(resolved_filename.c_str());
    if (!file.is_open())
    {
        LL_WARNS() << "can't load teleport history from file \"" << mFilename << "\"" << LL_ENDL;
        return;
    }

    // remove current entries before we load over them
    mItems.clear();

    // the parser's destructor is protected so we cannot create in the stack.
    LLPointer<LLSDParser> parser = new LLSDNotationParser();
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            LL_WARNS() << "Teleport history contains empty line."<< LL_ENDL;
            continue;
        }

        LLSD s_item;
        std::istringstream iss(line);
        if (parser->parse(iss, s_item, line.length()) == LLSDParser::PARSE_FAILURE)
        {
            LL_INFOS() << "Parsing saved teleport history failed" << LL_ENDL;
            break;
        }

        mItems.push_back(s_item);
    }

    file.close();

    std::sort(mItems.begin(), mItems.end(), LLSortItemsByDate());

    mHistoryChangedSignal(-1);
}

void LLTeleportHistoryStorage::dump() const
{
    LL_INFOS() << "Teleport history storage dump (" << mItems.size() << " items):" << LL_ENDL;

    for (size_t i=0; i<mItems.size(); i++)
    {
        std::stringstream line;
        line << i << ": " << mItems[i].mTitle;
        line << " global pos: " << mItems[i].mGlobalPos;
        line << " date: " << mItems[i].mDate;
        line << " slurl: " << mItems[i].mSLURL.asString(); // <FS:TJ/> Fix Teleport and Location History for OpenSim

        LL_INFOS() << line.str() << LL_ENDL;
    }
}

boost::signals2::connection LLTeleportHistoryStorage::setHistoryChangedCallback(history_callback_t cb)
{
    return mHistoryChangedSignal.connect(cb);
}

void LLTeleportHistoryStorage::goToItem(S32 idx)
{
    // Validate specified index.
    if (idx < 0 || idx >= (S32)mItems.size())
    {
        LL_WARNS() << "Invalid teleport history index (" << idx << ") specified" << LL_ENDL;
        dump();
        return;
    }

    // <FS:TJ> Fix Teleport and Location History for OpenSim
#ifdef OPENSIM
    if (LLGridManager::getInstance()->isInOpenSim())
    {
        LLSLURL slurl = LLSLURL(mItems[idx].mSLURL);
        std::string grid = slurl.getGrid();
        std::string current_grid = LFSimFeatureHandler::instance().hyperGridURL();
        std::string gatekeeper = LLGridManager::getInstance()->getGatekeeper(grid);

        // Requesting region information from the server is only required when changing grid
        if (slurl.isValid() && grid != current_grid)
        {
            if (!gatekeeper.empty())
            {
                slurl = LLSLURL(gatekeeper + ":" + slurl.getRegion(), slurl.getPosition(), true);
            }

            LLWorldMapMessage::getInstance()->sendNamedRegionRequest(
                slurl.getRegion(),
                boost::bind(&LLTeleportHistoryStorage::regionNameCallback, this, idx, _1, _2, _3, _4),
                slurl.getSLURLString(),
                true
            );

            return; // The teleport will occur in the callback with the correct global position
        }
    }
#endif
    // </FS:TJ>

    // Attempt to teleport to the requested item.
    gAgent.teleportViaLocation(mItems[idx].mGlobalPos);
}

void LLTeleportHistoryStorage::showItemOnMap(S32 idx)
{
    // Validate specified index.
    if (idx < 0 || idx >= (S32)mItems.size())
    {
        LL_WARNS() << "Invalid teleport history index (" << idx << ") specified" << LL_ENDL;
        dump();
        return;
    }

    LLVector3d landmark_global_pos = mItems[idx].mGlobalPos;

    LLFloaterWorldMap* worldmap_instance = LLFloaterWorldMap::getInstance();
    if (!landmark_global_pos.isExactlyZero() && worldmap_instance)
    {
        worldmap_instance->trackLocation(landmark_global_pos);
        LLFloaterReg::showInstance("world_map", "center");
    }
}

// <FS:TJ> Fix Teleport and Location History for OpenSim
void LLTeleportHistoryStorage::regionNameCallback(int idx, U64 region_handle, const LLSLURL& slurl, const LLUUID& snapshot_id, bool teleport)
{
    if (region_handle)
    {
        // Sanity checks again just in case since time has passed since the request was made
        if (idx < 0 ||idx >= (int)mItems.size())
        {
            LL_WARNS() << "Invalid teleport history storage index (" << idx << ") specified" << LL_ENDL;
            return;
        }

        LLVector3d origin_pos = from_region_handle(region_handle);
        LLVector3d global_pos(origin_pos + LLVector3d(slurl.getPosition()));

        // Attempt to teleport to the target grids region
        gAgent.teleportViaLocation(global_pos);
    }
    else
    {
        LL_WARNS() << "Invalid teleport history storage region handle" << LL_ENDL;
    }
}
// </FS:TJ>
