/**
 * @file fspanelimcontrolpanel.h
 * @brief FSPanelChatControlPanel and related class definitions
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

// Original file: llpanelimcontrolpanel.h

#ifndef FS_PANELIMCONTROLPANEL_H
#define FS_PANELIMCONTROLPANEL_H

#include "llpanel.h"

class FSParticipantList;

class FSPanelChatControlPanel
    : public LLPanel
{
public:
    FSPanelChatControlPanel() :
        mSessionId(LLUUID()) {};
    ~FSPanelChatControlPanel() { }

    virtual void setSessionId(const LLUUID& session_id);
    const LLUUID& getSessionId() { return mSessionId; }

    virtual uuid_vec_t getParticipants() const;

private:
    LLUUID mSessionId;
};


class FSPanelIMControlPanel : public FSPanelChatControlPanel
{
public:
    FSPanelIMControlPanel() { }
    ~FSPanelIMControlPanel() { }
};


class FSPanelGroupControlPanel : public FSPanelChatControlPanel
{
public:
    FSPanelGroupControlPanel(const LLUUID& session_id);
    ~FSPanelGroupControlPanel();

    void setSessionId(const LLUUID& session_id) override;
    void draw() override;

    uuid_vec_t getParticipants() const override;

protected:
    LLUUID mGroupID;

    FSParticipantList* mParticipantList;
};

class FSPanelAdHocControlPanel : public FSPanelGroupControlPanel
{
public:
    FSPanelAdHocControlPanel(const LLUUID& session_id);
};

#endif // FS_PANELIMCONTROLPANEL_H
