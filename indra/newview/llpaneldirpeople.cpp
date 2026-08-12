/**
 * @file llpaneldirpeople.cpp
 * @brief People panel in the legacy Search directory.
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

#include "llpaneldirpeople.h"
#include "llviewerwindow.h"
#include "llsearcheditor.h"

// viewer project includes
#include "llqueryflags.h"
#include "llnotificationsutil.h"

// <FS:PP> Search by UUID
#include "llavatarnamecache.h"
#include "llscrolllistctrl.h"
// </FS:PP>

#include "llavataractions.h"

static LLPanelInjector<LLPanelDirPeople> t_panel_dir_people("panel_dir_people");

LLPanelDirPeople::LLPanelDirPeople()
:   LLPanelDirBrowser()
{
    mMinSearchChars = 3;
}

bool LLPanelDirPeople::postBuild()
{
    LLPanelDirBrowser::postBuild();

    //getChild<LLLineEditor>("name")->setKeystrokeCallback(boost::bind(&LLPanelDirBrowser::onKeystrokeName, _1, _2), NULL);

    // <FS:Ansariel> Port over search term history
    //childSetAction("Search", &LLPanelDirBrowser::onClickSearchCore, this);
    //setDefaultBtn( "Search" );
    // </FS:Ansariel>

    return true;
}

LLPanelDirPeople::~LLPanelDirPeople()
{
    // <FS:PP> Search by UUID
    if (mAvatarNameCallbackConnection.connected())
    {
        mAvatarNameCallbackConnection.disconnect();
    }
    // </FS:PP>
}

// virtual
void LLPanelDirPeople::performQuery()
{
    // <FS:PP> Improve query sanitization
    // if (childGetValue("name").asString().length() < mMinSearchChars)
    // {
    //     return;
    // }
    std::string search_text = childGetValue("name").asString();
    LLStringUtil::replaceChar(search_text, '.', ' ');
    LLStringUtil::trim(search_text);
    if (search_text.length() < mMinSearchChars)
    {
        return;
    }

    if (LLUUID::validate(search_text))
    {
        setupNewSearch();

        if (mAvatarNameCallbackConnection.connected())
        {
            mAvatarNameCallbackConnection.disconnect();
        }

        const LLUUID avatar_id(search_text);
        LLAvatarName av_name;
        if (LLAvatarNameCache::get(avatar_id, &av_name))
        {
            addUUIDAvatarResult(avatar_id, av_name);
        }
        else
        {
            mAvatarNameCallbackConnection = LLAvatarNameCache::get(avatar_id, boost::bind(&LLPanelDirPeople::onAvatarNameCallback, this, _1, _2));
        }
        return;
    }
    // </FS:PP>

    // filter short words out of the query string
    // and indidate if we did have to filter it
    // The shortest username is 2 characters long.
    const S32 SHORTEST_WORD_LEN = 2;
    bool query_was_filtered = false;
    std::string query_string = LLPanelDirBrowser::filterShortWords(
            // <FS:PP> Improve query sanitization
            // childGetValue("name").asString(),
            search_text,
            // </FS:PP>
            SHORTEST_WORD_LEN,
            query_was_filtered );

    // possible we threw away all the short words in the query so check length
    if ( query_string.length() < mMinSearchChars )
    {
        LLNotificationsUtil::add("SeachFilteredOnShortWordsEmpty");
        return;
    };

    // if we filtered something out, display a popup
    if ( query_was_filtered )
    {
        LLSD args;
        args["FINALQUERY"] = query_string;
        LLNotificationsUtil::add("SeachFilteredOnShortWords", args);
    };

    setupNewSearch();

    U32 scope = DFQ_PEOPLE;

    // send the message
    sendDirFindQuery(
        gMessageSystem,
        mSearchID,
        query_string,
        scope,
        mSearchStart);
}

// <FS:PP> Search by UUID
void LLPanelDirPeople::onAvatarNameCallback(const LLUUID& id, const LLAvatarName& av_name)
{
    if (mAvatarNameCallbackConnection.connected())
    {
        mAvatarNameCallbackConnection.disconnect();
    }
    addUUIDAvatarResult(id, av_name);
}

void LLPanelDirPeople::addUUIDAvatarResult(const LLUUID& id, const LLAvatarName& av_name)
{
    LLScrollListCtrl* list = getChild<LLScrollListCtrl>("results");
    if (!list)
    {
        return;
    }

    std::string avatar_name = av_name.getUserName();
    if (avatar_name.empty())
    {
        avatar_name = av_name.getDisplayName();
    }
    if (avatar_name.empty())
    {
        avatar_name = id.asString();
    }

    LLSD content;
    content["type"] = AVATAR_CODE;
    content["name"] = avatar_name;

    LLSD row;
    row["id"] = id;
    row["columns"][0]["column"] = "icon";
    row["columns"][0]["type"] = "icon";
    row["columns"][0]["value"] = "icon_avatar_offline.tga";
    row["columns"][1]["column"] = "name";
    row["columns"][1]["value"] = avatar_name;
    row["columns"][1]["font"] = "SANSSERIF";

    list->addElement(row);
    mResultsContents[id.asString()] = content;
    mHaveSearchResults = true;
    updateResultCount();
    list->setEnabled(true);
    list->selectFirstItem();
    onCommitList(nullptr, this);
}
// </FS:PP>

// <FS:Ansariel> Add "open profile" button
void LLPanelDirPeople::openProfile()
{
    LLAvatarActions::showProfile(mSelectedID);
}
// </FS:Ansariel>
