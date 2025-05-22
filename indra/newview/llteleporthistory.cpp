/**
 * @file llteleporthistory.cpp
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

#include "llteleporthistory.h"

#include "llparcel.h"
#include "llsdserialize.h"

#include "llagent.h"
#include "llvoavatarself.h"
#include "llslurl.h"
#include "llviewercontrol.h"        // for gSavedSettings
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llworldmap.h"
#include "llagentui.h"
#include "llwindow.h"
#include "llviewerwindow.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"

#include "llviewernetwork.h" // <FS/> Access to GridManager
#include "lfsimfeaturehandler.h" // <FS/> Access to hyperGridURL
#include "llworldmapmessage.h" // <FS/> Access to sendNamedRegionRequest

// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b)
#include "rlvhandler.h"
// [/RLVa:KB]


//////////////////////////////////////////////////////////////////////////////
// LLTeleportHistoryItem
//////////////////////////////////////////////////////////////////////////////

const std::string& LLTeleportHistoryItem::getTitle() const
{
    return gSavedSettings.getBOOL("NavBarShowCoordinates") ? mFullTitle : mTitle;
}

//////////////////////////////////////////////////////////////////////////////
// LLTeleportHistory
//////////////////////////////////////////////////////////////////////////////

LLTeleportHistory::LLTeleportHistory():
    mCurrentItem(-1),
    mRequestedItem(-1),
    mGotInitialUpdate(false),
    mTeleportHistoryStorage(NULL)
{
    mTeleportFinishedConn = LLViewerParcelMgr::getInstance()->
        setTeleportFinishedCallback(boost::bind(&LLTeleportHistory::updateCurrentLocation, this, _1));
    mTeleportFailedConn = LLViewerParcelMgr::getInstance()->
        setTeleportFailedCallback(boost::bind(&LLTeleportHistory::onTeleportFailed, this));
}

LLTeleportHistory::~LLTeleportHistory()
{
    mTeleportFinishedConn.disconnect();
    mTeleportFailedConn.disconnect();
}

void LLTeleportHistory::goToItem(int idx)

{
    // Validate specified index.
    if (idx < 0 || idx >= (int)mItems.size())
    {
        LL_WARNS() << "Invalid teleport history index (" << idx << ") specified" << LL_ENDL;
        dump();
        return;
    }

    if (idx == mCurrentItem)
    {
        LL_WARNS() << "Will not teleport to the same location." << LL_ENDL;
        dump();
        return;
    }

    // <FS> [FIRE-35355] OpenSim global position is dependent on the Grid you are on
    #ifdef OPENSIM
    if (LLGridManager::getInstance()->isInOpenSim())
    {
        if (mItems[mCurrentItem].mRegionID != mItems[idx].mRegionID)
        {
            LLSLURL slurl = mItems[idx].mSLURL;
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

                if (mRequestedItem != -1)
                {
                    return; // We already have a request in progress and don't want to spam the server
                }

                mRequestedItem = idx;

                LLWorldMapMessage::getInstance()->sendNamedRegionRequest(
                    slurl.getRegion(),
                    boost::bind(&LLTeleportHistory::regionNameCallback, this, idx, _1, _2, _3, _4),
                    slurl.getSLURLString(),
                    true
                );

                return; // The teleport will occur in the callback with the correct global position
            }
        }
    }
    #endif
    // </FS>

    // Attempt to teleport to the requested item.
    gAgent.teleportViaLocation(mItems[idx].mGlobalPos);
    mRequestedItem = idx;
}

void LLTeleportHistory::onTeleportFailed()
{
    // Are we trying to teleport within the history?
    if (mRequestedItem != -1)
    {
        // Not anymore.
        mRequestedItem = -1;
    }
}

void LLTeleportHistory::handleLoginComplete()
{
    if( mGotInitialUpdate )
    {
        return;
    }
    updateCurrentLocation(gAgent.getPositionGlobal());
}

static void on_avatar_name_update_title(const LLAvatarName& av_name)
{
    if (gAgent.getRegion() && gViewerWindow && gViewerWindow->getWindow())
    {
        std::string region = gAgent.getRegion()->getName();
        std::string username = av_name.getUserName();

        // this first pass simply displays username and region name
        // but could easily be extended to include other details like
        // X/Y/Z location within a region etc.
        std::string new_title = STRINGIZE(username << " @ " << region);
        gViewerWindow->getWindow()->setTitle(new_title);
    }
}

void LLTeleportHistory::updateCurrentLocation(const LLVector3d& new_pos)
{
    if (!gAgent.getRegion()) return;

    if (!mTeleportHistoryStorage)
    {
        mTeleportHistoryStorage = LLTeleportHistoryStorage::getInstance();
    }
    if (mRequestedItem != -1) // teleport within the history in progress?
    {
        mCurrentItem = mRequestedItem;
        mRequestedItem = -1;
    }
    else
    {
        //EXT-7034
        //skip initial update if agent avatar is no valid yet
        //this may happen when updateCurrentLocation called while login process
        //sometimes isAgentAvatarValid return false and in this case new_pos
        //(which actually is gAgent.getPositionGlobal() ) is invalid
        //if this position will be saved then teleport back will teleport user to wrong position
        if ( !mGotInitialUpdate && !isAgentAvatarValid() )
        {
            return ;
        }

        // If we're getting the initial location update
        // while we already have a (loaded) non-empty history,
        // there's no need to purge forward items or add a new item.

        if (mGotInitialUpdate || mItems.size() == 0)
        {
            // Purge forward items (if any).
            if(mItems.size())
                mItems.erase (mItems.begin() + mCurrentItem + 1, mItems.end());

            // Append an empty item to the history and make it current.
//          mItems.push_back(LLTeleportHistoryItem("", LLVector3d()));
//          mCurrentItem++;
// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b) | Added: RLVa-1.2.1b
            // Only append a new item if the list is currently empty or if not @showloc=n restricted and the last entry wasn't zero'ed out
            if ( (mItems.size() == 0) || ((!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) && (!mItems.back().mGlobalPos.isExactlyZero())) )
            {
                mItems.push_back(LLTeleportHistoryItem("", LLVector3d()));
                mCurrentItem++;
            }
// [RLVa:KB]
        }

        // Update current history item.
        if (mCurrentItem < 0 || mCurrentItem >= (int) mItems.size()) // sanity check
        {
            LL_WARNS() << "Invalid current item. (this should not happen)" << LL_ENDL;
            llassert(!"Invalid current teleport history item");
            return;
        }

// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b) | Added: RLVa-1.2.1b
        if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
        {
// [/RLVa:KB]
            LLVector3 new_pos_local = gAgent.getPosAgentFromGlobal(new_pos);
            mItems[mCurrentItem].mFullTitle = getCurrentLocationTitle(true, new_pos_local);
            mItems[mCurrentItem].mTitle = getCurrentLocationTitle(false, new_pos_local);
            mItems[mCurrentItem].mGlobalPos = new_pos;
            mItems[mCurrentItem].mRegionID = gAgent.getRegion()->getRegionID();
// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1b) | Added: RLVa-1.2.1b
        }
        else
        {
            mItems[mCurrentItem] = LLTeleportHistoryItem(RlvStrings::getString(RlvStringKeys::Hidden::Parcel), LLVector3d::zero);
        }
// [/RLVa:KB]

        // <FS> [FIRE-35355] OpenSim global position is dependent on the Grid you are on,
        // so we need to store the slurl so we can request the global position later
        #ifdef OPENSIM
        if (LLGridManager::getInstance()->isInOpenSim())
        {
            auto regionp = gAgent.getRegion();
            if (regionp)
            {
                LLVector3 new_pos_local = gAgent.getPosAgentFromGlobal(new_pos);
                LLSLURL slurl = LLSLURL(LFSimFeatureHandler::instance().hyperGridURL(), regionp->getName(), new_pos_local);
                mItems[mCurrentItem].mSLURL = slurl;
            }
        }
        #endif
        // </FS>
    }

    //dump(); // LO - removing the dump from happening every time we TP.

    if (!mGotInitialUpdate)
        mGotInitialUpdate = true;

    // update Viewer window title with username and region name
    // if we are in "non-interactive mode" (SL-15999) or the debug
    // setting to allow it is enabled (may be useful in other situations)
    if (gNonInteractive || gSavedSettings.getBOOL("UpdateAppWindowTitleBar"))
    {
        LLAvatarNameCache::get(gAgent.getID(), boost::bind(&on_avatar_name_update_title, _2));
    }

    // Signal the interesting party that we've changed.
    onHistoryChanged();
}

boost::signals2::connection LLTeleportHistory::setHistoryChangedCallback(history_callback_t cb)
{
    return mHistoryChangedSignal.connect(cb);
}

void LLTeleportHistory::onHistoryChanged()
{
    mHistoryChangedSignal();
}

void LLTeleportHistory::purgeItems()
{
    if (mItems.size() == 0) // no entries yet (we're called before login)
    {
        // If we don't return here the history will get into inconsistent state, hence:
        // 1) updateCurrentLocation() will malfunction,
        //    so further teleports will not properly update the history;
        // 2) mHistoryChangedSignal subscribers will be notified
        //    of such an invalid change. (EXT-6798)
        // Both should not happen.
        return;
    }

    if (mItems.size() > 0)
    {
        mItems.erase(mItems.begin(), mItems.end()-1);
    }
    // reset the count
    mRequestedItem = -1;
    mCurrentItem = 0;

    onHistoryChanged();
}

// static
std::string LLTeleportHistory::getCurrentLocationTitle(bool full, const LLVector3& local_pos_override)
{
    std::string location_name;
    LLAgentUI::ELocationFormat fmt = full ? LLAgentUI::LOCATION_FORMAT_NO_MATURITY : LLAgentUI::LOCATION_FORMAT_NORMAL;

    if (!LLAgentUI::buildLocationString(location_name, fmt, local_pos_override)) location_name = "Unknown";
    return location_name;
}

void LLTeleportHistory::dump() const
{
    LL_INFOS() << "Teleport history dump (" << mItems.size() << " items):" << LL_ENDL;

    for (size_t i=0; i<mItems.size(); i++)
    {
        std::stringstream line;
        line << ((i == mCurrentItem) ? " * " : "   ");
        line << i << ": " << mItems[i].mTitle;
        line << " REGION_ID: " << mItems[i].mRegionID;
        line << ", pos: " << mItems[i].mGlobalPos;
        LL_INFOS() << line.str() << LL_ENDL;
    }
}

// <FS> [FIRE-35355] Callback for OpenSim so we can teleport to the correct global position on another grid
void LLTeleportHistory::regionNameCallback(int idx, U64 region_handle, const LLSLURL& slurl, const LLUUID& snapshot_id, bool teleport)
{
    if (region_handle)
    {
        // Sanity checks again just in case since time has passed since the request was made
        if (idx < 0 || idx >= (int)mItems.size())
        {
            LL_WARNS() << "Invalid teleport history index (" << idx << ") specified" << LL_ENDL;
            return;
        }

        if (idx == mCurrentItem)
        {
            LL_WARNS() << "Will not teleport to the same location." << LL_ENDL;
            return;
        }

        LLVector3d origin_pos = from_region_handle(region_handle);
        LLVector3d global_pos(origin_pos + LLVector3d(slurl.getPosition()));

        // Attempt to teleport to the target grids region
        gAgent.teleportViaLocation(global_pos);
    }
    else
    {
        LL_WARNS() << "Invalid teleport history region handle" << LL_ENDL;
        onTeleportFailed();
    }
}
// </FS>
