/**
 * @file llpaneldirgroups.cpp
 * @brief Groups panel in the legacy Search directory.
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2025, Linden Research, Inc.
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

#include "llpaneldirgroups.h"

#include "llagent.h"
#include "llnotificationsutil.h" // <FS:TJ/>
#include "llqueryflags.h"
#include "llviewercontrol.h"
#include "llsearcheditor.h"

#include "llgroupactions.h"

static LLPanelInjector<LLPanelDirGroups> t_panel_dir_groups("panel_dir_groups");

LLPanelDirGroups::LLPanelDirGroups()
    : LLPanelDirBrowser()
{
    mMinSearchChars = 3;
}


bool LLPanelDirGroups::postBuild()
{
    LLPanelDirBrowser::postBuild();

    //getChild<LLLineEditor>("name")->setKeystrokeCallback(boost::bind(&LLPanelDirBrowser::onKeystrokeName, _1, _2), NULL);

    // <FS:Ansariel> Port over search term history
    //childSetAction("Search", &LLPanelDirBrowser::onClickSearchCore, this);
    //setDefaultBtn( "Search" );
    // </FS:Ansariel>

    return true;
}

LLPanelDirGroups::~LLPanelDirGroups()
{
}

// virtual
void LLPanelDirGroups::performQuery()
{
    // <FS:PP> Improve query sanitization
    // if (childGetValue("name").asString().length() < mMinSearchChars)
    std::string search_text = childGetValue("name").asString();
    LLStringUtil::trim(search_text);
    if (search_text.length() < mMinSearchChars)
    // </FS:PP>
    {
        return;
    }

    setupNewSearch();

    // groups
    U32 scope = DFQ_GROUPS;

    // Check group mature filter.
    // <FS:TJ> Fix legacy group search to better support maturity settings
    //if ( !gSavedSettings.getBOOL("ShowMatureGroups")  || gAgent.isTeen() )
    //{
    //    scope |= DFQ_FILTER_MATURE;
    //}
    bool adult_enabled = gAgent.canAccessAdult();
    bool mature_enabled = gAgent.canAccessMature();

    static LLUICachedControl<U32> search_maturity("FSSearchGroupMaturity", SIM_ACCESS_PG);
    if (!search_maturity())
    {
        LLNotificationsUtil::add("NoContentToSearch");
        return;
    }

    if (search_maturity >= SIM_ACCESS_PG)
    {
        scope |= DFQ_INC_PG;
    }
    if (mature_enabled && search_maturity >= SIM_ACCESS_MATURE)
    {
        scope |= DFQ_INC_MATURE;
    }
    if (adult_enabled && search_maturity >= SIM_ACCESS_ADULT)
    {
        scope |= DFQ_INC_ADULT;
    }
    // </FS:TJ>

    mCurrentSortColumn = "score";
    mCurrentSortAscending = false;

    // send the message
    sendDirFindQuery(
        gMessageSystem,
        mSearchID,
        // <FS:PP> Search by UUID
        // childGetValue("name").asString(),
        search_text,
        // </FS:PP>
        scope,
        mSearchStart);
}

// <FS:Ansariel> Add "open profile" button
void LLPanelDirGroups::openProfile()
{
    LLGroupActions::show(mSelectedID);
}
// </FS:Ansariel>
