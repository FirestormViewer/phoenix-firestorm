/**
* @file fsfloaterscriptcontainer.h
* @brief Declaration of script dialog multi-floater container
* @author minerjr@firestorm
*
* $LicenseInfo:firstyear=2025&license=fsviewerlgpl$
* Phoenix Firestorm Viewer Source Code
* Copyright (C) 2025, Beq Janus
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

// This code is based on the fsfloaterimcontainer class, but modified for the purpose of grouping Script Dialog floaters instead of Instant Messages.
// Some methods are left in case in the future bugs are discovered and code needs to be updated to be more similar to the fsfloaterimcontainer class.

#ifndef FS_FLOATERSCRIPTCONTAINER_H
#define FS_FLOATERSCRIPTCONTAINER_H

#include "llmultifloater.h"
#include "lltransientdockablefloater.h"
#include "llnotificationptr.h"

class LLToastPanel;
class LLTabContainer;

/////////////////////////////
//FSFloaterScriptContainer//
///////////////////////////

class FSFloaterScriptContainer : public LLMultiFloater
{
public:
    FSFloaterScriptContainer(const LLSD& seed);
    virtual ~FSFloaterScriptContainer();

    /*virtual*/ bool postBuild();
    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ void onClose(bool app_quitting);
    void onDoubleClick();
    void onCloseFloater(LLUUID& id);
    /*virtual*/ void draw();

    /*virtual*/ void addFloater(LLFloater* floaterp,
        bool select_added_floater,
        LLTabContainer::eInsertionPoint insertion_point = LLTabContainer::END);
    // [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-11 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
    /*virtual*/ void removeFloater(LLFloater* floaterp);
    // [/SL:KB]
    bool hasFloater(LLFloater* floaterp);

    void addNewSession(LLFloater* floaterp);

    static FSFloaterScriptContainer* findInstance();
    static FSFloaterScriptContainer* getInstance();

    virtual void setVisible(bool b);
    /*virtual*/ void setMinimized(bool b);

    void onNewMessageReceived(const LLSD& msg); // public so nearbychat can call it directly. TODO: handle via callback. -AO

    static void reloadEmptyFloaters();
    void initTabs();

    void addFlashingSession(const LLUUID& session_id);

    void tabOpen(LLFloater* opened_floater, bool from_click);

    void startFlashingTab(LLFloater* floater, const std::string& message);

private:

    LLFloater*  getCurrentScriptFloater();

    typedef std::map<LLUUID, LLFloater*> avatarID_panel_map_t;
    avatarID_panel_map_t mSessions;
    boost::signals2::connection mNewMessageConnection;

    void checkFlashing();
    uuid_vec_t  mFlashingSessions;

    bool        mIsAddingNewSession;

    std::map<LLFloater*, bool> mFlashingTabStates;

    // [SL:KB] - Patch: UI-TabRearrange | Checked: 2012-05-05 (Catznip-3.3.0)
protected:
    void onScriptTabRearrange(S32 tab_index, LLPanel* tab_panel);
    // [/SL:KB]

};

#endif
