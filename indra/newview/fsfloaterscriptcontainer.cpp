/**
* @file fsfloaterscriptcontainer.cpp
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

#include "llviewerprecompiledheaders.h"
#include "llviewercontrol.h"
#include "llchiclet.h"
#include "llchicletbar.h"
#include "llfloaterreg.h"
#include "llscriptfloater.h"
#include "fsfloaterscriptcontainer.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "lltoolbarview.h"
#include "llcontrol.h"

// This code is based on the fsfloaterimcontainer class, but modified for the purpose of grouping Script Dialog floaters instead of Instant Messages.
// Some methods are left in case in the future bugs are discovered and code needs to be updated to be more similar to the fsfloaterimcontainer class.

//
// FSFloaterScriptContainer
//
FSFloaterScriptContainer::FSFloaterScriptContainer(const LLSD& seed)
    :   LLMultiFloater(seed),
    mIsAddingNewSession(false)
{
    mAutoResize = false;
    LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::DOCKED, this);
}

FSFloaterScriptContainer::~FSFloaterScriptContainer()
{
    mNewMessageConnection.disconnect();
    LLTransientFloaterMgr::getInstance()->removeControlView(LLTransientFloaterMgr::DOCKED, this);

}

bool FSFloaterScriptContainer::postBuild()
{
    mNewMessageConnection = LLIMModel::instance().mNewMsgSignal.connect(boost::bind(&FSFloaterScriptContainer::onNewMessageReceived, this, _1));
    // Do not call base postBuild to not connect to mCloseSignal to not close all floaters via Close button
    // mTabContainer will be initialized in LLMultiFloater::addChild()

    mTabContainer->setAllowRearrange(true);
    mTabContainer->setRearrangeCallback(boost::bind(&FSFloaterScriptContainer::onScriptTabRearrange, this, _1, _2));
    mTabContainer->setDoubleClickCallback(boost::bind(&FSFloaterScriptContainer::onDoubleClick, this));

    return true;
}

void FSFloaterScriptContainer::initTabs()
{
    // If we're using multitabs, and we open up for the first time
    // Add localchat by default if it's not already on the screen somewhere else. -AO
    // But only if it hasnt been already so we can reopen it to the same tab -KC
    // Improved handling to leave most of the work to the LL tear-off code -Zi
    // This is mirrored from FSFloaterContacts::onOpen() and FSFloaterNearbyChat::onOpen()
    // respectively: If those floaters are hosted, they don't store their visibility state.
    // Instead, the visibility state of the hosting container is stored. (See Zi's changes to
    // LLFloater::storeVisibilityControl()) That means if contacts and/or nearby chat floater
    // are hosted and FSFloaterIMContainer was visible at logout, we will end up here during
    // next login and have to configure those floaters so their tear off state and icon is
    // correct. Configure contacts first and nearby chat last so nearby chat will be active
    // once FSFloaterIMContainer has opened. -AH
}

// [SL:KB] - Patch: UI-TabRearrange | Checked: 2012-05-05 (Catznip-3.3.0)
void FSFloaterScriptContainer::onScriptTabRearrange(S32 tab_index, LLPanel* tab_panel)
{
    LLFloater* pIMFloater = dynamic_cast<LLFloater*>(tab_panel);
    if (!pIMFloater)
        return;

    const LLUUID& idSession = pIMFloater->getKey().asUUID();
    if (idSession.isNull())
        return;

    LLChicletPanel* pChicletPanel = LLChicletBar::instance().getChicletPanel();
    LLChiclet* pIMChiclet = pChicletPanel->findChiclet<LLChiclet>(idSession);
    pChicletPanel->setChicletIndex(pIMChiclet, tab_index - mTabContainer->getNumLockedTabs());
}
// [/SL:KB]

void FSFloaterScriptContainer::onOpen(const LLSD& key)
{
    LLMultiFloater::onOpen(key);
    initTabs();
    LLFloater* active_floater = getActiveFloater();
    if (active_floater && !active_floater->hasFocus())
    {
        mTabContainer->setFocus(true);
    }
}

void FSFloaterScriptContainer::onClose(bool app_quitting)
{
    if (app_quitting)
    {
        for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
        {
            LLScriptFloater* floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getPanelByIndex(i));
            if (floater)
            {
                floater->onClose(app_quitting);
            }
        }
    }
}

void FSFloaterScriptContainer::onDoubleClick()
{
    LLScriptFloater* script_floater = dynamic_cast<LLScriptFloater*>(getCurrentScriptFloater());

    if (script_floater)
    {
        LLScriptFloater::onClickTearOff(script_floater);
    }
}

void FSFloaterScriptContainer::addFloater(LLFloater* floaterp,
    bool select_added_floater,
    LLTabContainer::eInsertionPoint insertion_point)
{
    if (!floaterp)
        return;

    // already here
    if (floaterp->getHost() == this)
    {
        openFloater(floaterp->getKey());
        return;
    }

    LLMultiFloater::addFloater(floaterp, select_added_floater, insertion_point);

    LLUUID session_id = floaterp->getKey();
    mSessions[session_id] = floaterp;
    floaterp->mCloseSignal.connect(boost::bind(&FSFloaterScriptContainer::onCloseFloater, this, session_id));
}

void FSFloaterScriptContainer::addNewSession(LLFloater* floaterp)
{
    // Make sure we don't do some strange re-arranging if we add a new IM floater due to a new session
    mIsAddingNewSession = true;
    addFloater(floaterp, false, LLTabContainer::END);
    mIsAddingNewSession = false;
}

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-12-11 (Catznip-3.2.0d) | Added: Catznip-3.2.0d
void FSFloaterScriptContainer::removeFloater(LLFloater* floaterp)
{
    const std::string floater_name = floaterp->getName();
    std::string setting_name = "";
    bool needs_unlock = true;
    /*
    if (floater_name == "nearby_chat")
    {
    setting_name = "ChatHistoryTornOff";
    needs_unlock = true;
    }
    else if (floater_name == "imcontacts")
    {
    setting_name = "ContactsTornOff";
    needs_unlock = true;
    }
    */
    if (needs_unlock)
    {
        // Calling lockTabs with 0 will lock ALL tabs - need to call unlockTabs instead!
        S32 num_locked_tabs = mTabContainer->getNumLockedTabs();
        if (num_locked_tabs > 1)
        {
            mTabContainer->lockTabs(num_locked_tabs - 1);
        }
        else
        {
            mTabContainer->unlockTabs();
        }
        gSavedSettings.setBOOL(setting_name, true);
        floaterp->setCanClose(true);
    }

    mFlashingTabStates.erase(floaterp);

    LLMultiFloater::removeFloater(floaterp);
}
// [/SL:KB]

bool FSFloaterScriptContainer::hasFloater(LLFloater* floaterp)
{
    for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
    {
        if (dynamic_cast<LLFloater*>(mTabContainer->getPanelByIndex(i)) == floaterp)
        {
            return true;
        }
    }
    return false;
}

void FSFloaterScriptContainer::onCloseFloater(LLUUID& id)
{
    mSessions.erase(id);
    if (isShown())
    {
        setFocus(true);
    }
    else if (isMinimized())
    {
        setMinimized(true); // Make sure console output that needs to be shown is still doing so
    }
}

void FSFloaterScriptContainer::onNewMessageReceived(const LLSD& msg)
{
    LLUUID session_id = msg["session_id"].asUUID();
    LLFloater* floaterp = get_ptr_in_map(mSessions, session_id);
    LLFloater* current_floater = LLMultiFloater::getActiveFloater();

    // KC: Don't flash tab on friend status changes per setting
    if (floaterp && current_floater && floaterp != current_floater)
    {
        startFlashingTab(floaterp, msg["message"].asString());
    }
}

FSFloaterScriptContainer* FSFloaterScriptContainer::findInstance()
{
    return LLFloaterReg::findTypedInstance<FSFloaterScriptContainer>("fs_script_container");
}

FSFloaterScriptContainer* FSFloaterScriptContainer::getInstance()
{
    return LLFloaterReg::getTypedInstance<FSFloaterScriptContainer>("fs_script_container");
}

void FSFloaterScriptContainer::setVisible(bool b)
{
    LLMultiFloater::setVisible(b);

    if (b)
    {
        mFlashingSessions.clear();
    }
}

void FSFloaterScriptContainer::setMinimized(bool b)
{
    if (mTabContainer)
    {
        if (LLScriptFloater* script_floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getCurrentPanel()); script_floater)
        {
            //script_floater->handleMinimized(b);
        }
    }

    LLMultiFloater::setMinimized(b);
}

void FSFloaterScriptContainer::reloadEmptyFloaters()
{
    LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
    for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
        iter != inst_list.end(); ++iter)
    {
        LLScriptFloater* floater = dynamic_cast<LLScriptFloater*>(*iter);
        //if (floater && floater->getLastChatMessageIndex() == -1)
        {
            //floater->reloadMessages(true);
        }
    }

}

// virtual
void FSFloaterScriptContainer::draw()
{
    LLMultiFloater::draw();
}

LLFloater* FSFloaterScriptContainer::getCurrentScriptFloater()
{
    LLScriptFloater* script_floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getPanelByIndex(mTabContainer->getCurrentPanelIndex()));
    if (script_floater)
    {
        return script_floater;
    }

    return nullptr;
}

void FSFloaterScriptContainer::addFlashingSession(const LLUUID& session_id)
{
    uuid_vec_t::iterator found = std::find(mFlashingSessions.begin(), mFlashingSessions.end(), session_id);
    if (found == mFlashingSessions.end())
    {
        mFlashingSessions.emplace_back(session_id);
    }

    checkFlashing();
}

void FSFloaterScriptContainer::checkFlashing()
{
    gToolBarView->flashCommand(LLCommandId("chat"), !mFlashingSessions.empty(), isMinimized());
}

/*void FSFloaterScriptContainer::sessionIDUpdated(const LLUUID& old_session_id, const LLUUID& new_session_id)
{
if (avatarID_panel_map_t::iterator found = mSessions.find(old_session_id); found != mSessions.end())
{
LLFloater* floaterp = found->second;
mSessions.erase(found);
mSessions[new_session_id] = floaterp;
}
}*/

void FSFloaterScriptContainer::tabOpen(LLFloater* opened_floater, bool from_click)
{
    mFlashingTabStates.erase(opened_floater);
}

void FSFloaterScriptContainer::startFlashingTab(LLFloater* floater, const std::string& message)
{
    if (LLMultiFloater::isFloaterFlashing(floater))
    {
        LLMultiFloater::setFloaterFlashing(floater, false);
    }
    LLMultiFloater::setFloaterFlashing(floater, true, false);
}
