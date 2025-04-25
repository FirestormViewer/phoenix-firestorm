/**
 * @file fspanelimcontrolpanel.cpp
 * @brief LLPanelAvatar and related class implementations
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

// Original file: llpanelimcontrolpanel.cpp

#include "llviewerprecompiledheaders.h"

#include "fspanelimcontrolpanel.h"

#include "fsparticipantlist.h"
#include "llagent.h"
#include "llimview.h"
#include "llspeakers.h"

void FSPanelChatControlPanel::setSessionId(const LLUUID& session_id)
{
    //Method is called twice for AdHoc and Group chat. Second time when server init reply received
    mSessionId = session_id;
}

uuid_vec_t FSPanelChatControlPanel::getParticipants() const
{
    LLIMModel::LLIMSession* im_session = LLIMModel::instance().findIMSession(mSessionId);
    if (im_session && im_session->isP2PSessionType())
    {
        return { im_session->mOtherParticipantID, gAgentID };
    }

    return {};
}

FSPanelGroupControlPanel::FSPanelGroupControlPanel(const LLUUID& session_id):
mParticipantList(nullptr)
{
}

FSPanelGroupControlPanel::~FSPanelGroupControlPanel()
{
    delete mParticipantList;
    mParticipantList = nullptr;
}

// virtual
void FSPanelGroupControlPanel::draw()
{
    // Need to resort the participant list if it's in sort by recent speaker order.
    if (mParticipantList)
    {
        mParticipantList->update();
    }
    FSPanelChatControlPanel::draw();
}

void FSPanelGroupControlPanel::setSessionId(const LLUUID& session_id)
{
    FSPanelChatControlPanel::setSessionId(session_id);

    mGroupID = session_id;

    // for group and Ad-hoc chat we need to include agent into list
    if (!mParticipantList)
    {
        LLSpeakerMgr* speaker_manager = LLIMModel::getInstance()->getSpeakerManager(session_id);
        if (!speaker_manager)
            return;

        mParticipantList = new FSParticipantList(speaker_manager, getChild<LLAvatarList>("grp_speakers_list"), true,false);
    }
}

uuid_vec_t FSPanelGroupControlPanel::getParticipants() const
{
    return mParticipantList->getAvatarIds();
}

FSPanelAdHocControlPanel::FSPanelAdHocControlPanel(const LLUUID& session_id)
    : FSPanelGroupControlPanel(session_id)
{
}
