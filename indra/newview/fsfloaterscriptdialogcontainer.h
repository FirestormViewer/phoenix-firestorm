/**
 * @file fsfloaterscriptdialogcontainer.h
 * @brief Multifloater containing active Script Dialog floaters in separate tab container.
 * @author minerjr@firestorm
 *
 * $LicenseInfo:firstyear=2026&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2026 The Phoenix Firestorm Project, Inc.
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


#ifndef FS_FLOATERSCRIPTDIALOGCONTAINER_H
#define FS_FLOATERSCRIPTDIALOGCONTAINER_H

#include "llmultifloater.h"
#include "lltransientdockablefloater.h"
#include "llnotificationptr.h"

class LLToastPanel;
class LLTabContainer;
class LLScriptFloater;

/////////////////////////////
// FSFloaterScriptContainer//
///////////////////////////

class FSFloaterScriptDialogContainer : public LLMultiFloater
{
public:
    FSFloaterScriptDialogContainer(const LLSD& seed);
    virtual ~FSFloaterScriptDialogContainer();

    /*virtual*/ bool postBuild() override;
    /*virtual*/ void onOpen(const LLSD& key) override;
    /*virtual*/ void onClose(bool app_quitting) override;
    void             onCloseFloater(const LLUUID& id);
    void             onClickMinimize();
    void             onClickCloseAll();

    void closeAllImpl();
    bool confirmCloseAllCallback(const LLSD& notification, const LLSD& response);

    /*virtual*/ void growToFit(S32 content_width, S32 content_height) override;
    /*virtual*/ void draw() override;
    /*virtual*/ void addFloater(LLFloater* floaterp, bool select_added_floater, LLTabContainer::eInsertionPoint insertion_point = LLTabContainer::END) override;

    // [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-11 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
    /*virtual*/ void removeFloater(LLFloater* floaterp) override;
    void removeFloater(const LLUUID& id);
    // [/SL:KB]
    bool hasFloater(LLFloater* floaterp) const;
    bool hasFloater(const LLUUID& id) const;
    LLScriptFloater* getFloater(const LLUUID& id) const;

    void addNewSession(LLFloater* floaterp);

    static FSFloaterScriptDialogContainer* findInstance();
    static FSFloaterScriptDialogContainer* getInstance();

    /*virtual*/ void setMinimized(bool b) override;

    void onNewMessageReceived(const LLSD& msg); // public so nearbychat can call it directly. TODO: handle via callback. -AO

    void addFlashingSession(const LLUUID& session_id);

    void tabOpen(LLFloater* opened_floater, bool from_click);

    void startFlashingTab(LLFloater* floater, const std::string& message);

private:
    LLFloater* getCurrentScriptFloater();

    typedef std::map<LLUUID, LLFloater*> avatarID_panel_map_t;
    avatarID_panel_map_t                 mSessions;
    boost::signals2::connection          mNewMessageConnection;

    void       checkFlashing();
    uuid_vec_t mFlashingSessions;

    bool mIsAddingNewSession;
    S32 mInitialHeight;
    S32 mInitialWidth;

    std::map<LLFloater*, bool> mFlashingTabStates;

    LLButton* mCloseAllBtn;

    /*virtual*/ void computeResizeLimits(S32& new_min_width, S32& new_min_height) override;
    // [SL:KB] - Patch: UI-TabRearrange | Checked: 2012-05-05 (Catznip-3.3.0)
protected:
    void onScriptTabRearrange(S32 tab_index, LLPanel* tab_panel);
    // [/SL:KB]
};

#endif // FS_FLOATERSCRIPTDIALOGCONTAINER_H
