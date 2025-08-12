/**
 * @file llscriptfloater.cpp
 * @brief LLScriptFloater class definition
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
#include "llscriptfloater.h"
#include "llagentcamera.h"

#include "llchannelmanager.h"
#include "llchiclet.h"
#include "llchicletbar.h"
#include "llfloaterreg.h"
#include "lllslconstants.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llscreenchannel.h"
#include "llsyswellwindow.h"
#include "lltoastnotifypanel.h"
#include "lltoastscripttextbox.h"
#include "lltrans.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llfloaterimsession.h"

#include "llwindow.h"   // <FS:CR> flashIcon()
#include "lltoolbarview.h"      // <FS:Zi> script dialogs position
// <FS:Zi> Dialog Stacking browser
#include "dialogstack.h"
#include "llbutton.h"
#include "llnavigationbar.h"
// </FS:Zi>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLUUID notification_id_to_object_id(const LLUUID& notification_id)
{
    LLNotificationPtr notification = LLNotifications::getInstance()->find(notification_id);
    if(notification)
    {
        return notification->getPayload()["object_id"].asUUID();
    }
    return LLUUID::null;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


LLScriptFloater::LLScriptFloater(const LLSD& key)
// <FS:Ansariel> FIRE-21546: Don't close docked floater if IMs arrive, but only if IMs shown in tabs - still hide when using docked windows!
//: LLDockableFloater(NULL, true, key)
: LLDockableFloater(NULL, gSavedSettings.getS32("FSChatWindow") != 1, key)
// </FS:Ansariel>
, mScriptForm(NULL)
, mSaveFloaterPosition(false)
{
    setMouseDownCallback(boost::bind(&LLScriptFloater::onMouseDown, this));
    setOverlapsScreenChannel(true);
    // <FS:Ansariel> FIRE-13459: Script dialogs ignore opening position in mouselook
    //mIsDockedStateForcedCallback = boost::bind(&LLAgentCamera::cameraMouselook, &gAgentCamera);
    mNoTransparency = gSavedSettings.getBOOL("FSScriptDialogNoTransparency");
}

bool LLScriptFloater::toggle(const LLUUID& notification_id)
{
    LLScriptFloater* floater = LLFloaterReg::findTypedInstance<LLScriptFloater>("script_floater", notification_id);

    // show existing floater
    if(floater)
    {
        if(floater->getVisible())
        {
            floater->setVisible(false);
            return false;
        }
        else
        {
            floater->setVisible(true);
            floater->setFocus(false);
        }
    }
    // create and show new floater
    else
    {
        show(notification_id);
    }

    LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
    if (NULL != chiclet_panelp)
    {
        chiclet_panelp->setChicletToggleState(notification_id, true);
    }

    return true;
}

/*
// <FS:Zi> script dialogs position
// Reimplemented the show() method at the end of this file

LLScriptFloater* LLScriptFloater::show(const LLUUID& notification_id)
{
    LLScriptFloater* floater = LLFloaterReg::getTypedInstance<LLScriptFloater>("script_floater", notification_id);
    floater->setNotificationId(notification_id);
    floater->createForm(notification_id);

    //LLDialog(LLGiveInventory and LLLoadURL) should no longer steal focus (see EXT-5445)
    floater->setAutoFocus(false);

    if(LLScriptFloaterManager::OBJ_SCRIPT == LLScriptFloaterManager::getObjectType(notification_id))
    {
        floater->setSavePosition(true);
        floater->restorePosition();
    }
    else
    {
        floater->dockToChiclet(true);
    }

    //LLDialog(LLGiveInventory and LLLoadURL) should no longer steal focus (see EXT-5445)
    LLFloaterReg::showTypedInstance<LLScriptFloater>("script_floater", notification_id, false);

    return floater;
}
// </FS:Zi>
*/

void LLScriptFloater::setNotificationId(const LLUUID& id)
{
    mNotificationId = id;
    // Lets save object id now while notification exists
    mObjectId = notification_id_to_object_id(id);
}

void LLScriptFloater::createForm(const LLUUID& notification_id)
{
    // delete old form
    if(mScriptForm)
    {
        removeChild(mScriptForm);
        mScriptForm->die();
    }

    LLNotificationPtr notification = LLNotifications::getInstance()->find(notification_id);
    if(NULL == notification)
    {
        return;
    }

    // create new form
    LLRect toast_rect = getRect();
    if (isScriptTextbox(notification))
    {
        mScriptForm = new LLToastScriptTextbox(notification);
    }
    else
    {
        // LLToastNotifyPanel will fit own content in vertical direction,
        // but it needs an initial rect to properly calculate  its width
        // Use an initial rect of the script floater to make the floater
        // window more configurable.
        mScriptForm = new LLToastNotifyPanel(notification, toast_rect);
    }
    addChild(mScriptForm);

    // position form on floater
    mScriptForm->setOrigin(0, 0);

    // make floater size fit form size
    LLRect panel_rect = mScriptForm->getRect();
    // <FS:Zi> Animated dialogs
    // toast_rect.setLeftTopAndSize(toast_rect.mLeft, toast_rect.mTop, panel_rect.getWidth(), panel_rect.getHeight() + getHeaderHeight());
    eDialogPosition dialog_position = (eDialogPosition)gSavedSettings.getS32("ScriptDialogsPosition");
    mDesiredHeight = panel_rect.getHeight() + getHeaderHeight();
    // <FS:minerjr>
    static LLCachedControl<bool> script_dialog_container(gSavedSettings,"FSScriptDialogContainer", false);

    //if (gSavedSettings.getBOOL("FSAnimatedScriptDialogs") && (dialog_position == POS_TOP_LEFT || dialog_position == POS_TOP_RIGHT))
    // Only animate the script dialog if it is not using the container or the position is set to the TOP.
    if (!script_dialog_container && gSavedSettings.getBOOL("FSAnimatedScriptDialogs") && (dialog_position == POS_TOP_LEFT || dialog_position == POS_TOP_RIGHT))
    // </FS:minerjr>
    {
        mCurrentHeight = 0;
        mStartTime = LLFrameTimer::getElapsedSeconds();
    }
    else
    {
        mCurrentHeight = mDesiredHeight;
    }    
    toast_rect.setLeftTopAndSize(toast_rect.mLeft, toast_rect.mTop, panel_rect.getWidth(), mCurrentHeight);
    // </FS:Zi>
    setShape(toast_rect);

    // <FS:Zi> Dialog Stacking browser
    mScriptForm->getChild<LLButton>("DialogStackButton")->setCommitCallback(boost::bind(&LLScriptFloater::onStackClicked, this));

    if (dialog_position != POS_DOCKED)
    {
        DialogStack::instance().push(notification_id);
    }
    // </FS:Zi>
}

// <FS:Zi> Dialog Stacking browser
void LLScriptFloater::onStackClicked()
{
    LLFloater* floater = LLFloaterReg::getTypedInstance<LLScriptFloater>("script_floater", getNotificationId());
    if (floater->isFrontmost())
    {
        const LLUUID& next_notification = DialogStack::instance().flip(getNotificationId());
        floater = LLFloaterReg::getTypedInstance<LLScriptFloater>("script_floater", next_notification);
    }
    gFloaterView->bringToFront(floater, true);
}
// </FS:Zi>

void LLScriptFloater::onClose(bool app_quitting)
{
    savePosition();

    if(getNotificationId().notNull())
    {
        // we shouldn't kill notification on exit since it may be used as persistent.
        if (app_quitting)
        {
            LLScriptFloaterManager::getInstance()->onRemoveNotification(getNotificationId());
        }
        else
        {
            LLScriptFloaterManager::getInstance()->removeNotification(getNotificationId());
        }
    }
}

void LLScriptFloater::setDocked(bool docked, bool pop_on_undock /* = true */)
{
    LLDockableFloater::setDocked(docked, pop_on_undock);

    savePosition();

    hideToastsIfNeeded();
}

void LLScriptFloater::setVisible(bool visible)
{
    LLDockableFloater::setVisible(visible);

    hideToastsIfNeeded();

    if(!visible)
    {
        LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
        if (NULL != chiclet_panelp)
        {
            LLIMChiclet * chicletp = chiclet_panelp->findChiclet<LLIMChiclet>(getNotificationId());
            if(NULL != chicletp)
            {
                chicletp->setToggleState(false);
            }
        }
    }
}

void LLScriptFloater::onMouseDown()
{
    if(getNotificationId().notNull())
    {
        LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
        if (NULL != chiclet_panelp)
        {
            LLIMChiclet * chicletp = chiclet_panelp->findChiclet<LLIMChiclet>(getNotificationId());
            // Remove new message icon
            if (NULL == chicletp)
            {
                LL_ERRS() << "Dock chiclet for LLScriptFloater doesn't exist" << LL_ENDL;
            }
            else
            {
                chicletp->setShowNewMessagesIcon(false);
            }
        }
    }
}

void LLScriptFloater::savePosition()
{
    if(getSavePosition() && mObjectId.notNull())
    {
        LLScriptFloaterManager::FloaterPositionInfo fpi = {getRect(), isDocked()};
        LLScriptFloaterManager::getInstance()->saveFloaterPosition(mObjectId, fpi);
    }
}

void LLScriptFloater::restorePosition()
{
    LLScriptFloaterManager::FloaterPositionInfo fpi;
    if(LLScriptFloaterManager::getInstance()->getFloaterPosition(mObjectId, fpi))
    {
        dockToChiclet(fpi.mDockState);
        if(!fpi.mDockState)
        {
            // Un-docked floater is opened in 0,0, now move it to saved position
            translate(fpi.mRect.mLeft - getRect().mLeft, fpi.mRect.mTop - getRect().mTop);
        }
    }
    else
    {
        dockToChiclet(true);
    }
}

// <FS:Ansariel> Override base method so we have the option to ignore
// the global transparency settings and show the script dialog
// always on opaque background.
F32 LLScriptFloater::getCurrentTransparency()
{
    if (mNoTransparency)
    {
        return 1.0f;
    }
    else
    {
        return LLUICtrl::getCurrentTransparency();
    }
}
// </FS:Ansariel>

void LLScriptFloater::onFocusLost()
{
    if(getNotificationId().notNull())
    {
        LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
        if (NULL != chiclet_panelp)
        {
            chiclet_panelp->setChicletToggleState(getNotificationId(), false);
        }
    }
}

void LLScriptFloater::onFocusReceived()
{
    // first focus will be received before setObjectId() call - don't toggle chiclet
    if(getNotificationId().notNull())
    {
        LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
        if (NULL != chiclet_panelp)
        {
            chiclet_panelp->setChicletToggleState(getNotificationId(), true);
        }
    }
}

// <FS:Ansariel> FIRE-12929: Fix script floater docking issues
//void LLScriptFloater::dockToChiclet(bool dock)
void LLScriptFloater::dockToChiclet(bool dock, bool scroll_to_chiclet /* = true */)
{
    if (getDockControl() == NULL)
    {
        LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
        if (NULL != chiclet_panelp)
        {
            LLChiclet * chicletp = chiclet_panelp->findChiclet<LLChiclet>(getNotificationId());
            if (NULL == chicletp)
            {
                LL_WARNS() << "Dock chiclet for LLScriptFloater doesn't exist" << LL_ENDL;
                return;
            }

            // <FS:Ansariel> FIRE-12929: Fix script floater docking issues
            //chiclet_panelp->scrollToChiclet(chicletp);
            if (scroll_to_chiclet)
            {
                chiclet_panelp->scrollToChiclet(chicletp);
            }
            // </FS:Ansariel>

            // Stop saving position while we dock floater
            bool save = getSavePosition();
            setSavePosition(false);

            // <FS:Ansariel> Group notices, IMs and chiclets position
            //setDockControl(new LLDockControl(chicletp, this, getDockTongue(),
            //  LLDockControl::BOTTOM));
            if (gSavedSettings.getBOOL("InternalShowGroupNoticesTopRight"))
            {
                setDockControl(new LLDockControl(chicletp, this, getDockTongue(),
                    LLDockControl::BOTTOM));
            }
            else
            {
                setDockControl(new LLDockControl(chicletp, this, getDockTongue(),
                    LLDockControl::TOP));
            }
            // </FS:Ansariel> Group notices, IMs and chiclets position
            setDocked(dock);

            // Restore saving
            setSavePosition(save);
        }
    }
}

void LLScriptFloater::hideToastsIfNeeded()
{
    using namespace LLNotificationsUI;

    // find channel
    LLScreenChannel* channel = dynamic_cast<LLScreenChannel*>(LLChannelManager::getInstance()->findChannelByID(
        LLNotificationsUI::NOTIFICATION_CHANNEL_UUID));
    // update notification channel state
    if(channel)
    {
        channel->updateShowToastsState();
        channel->redrawToasts();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
    bool needs_unlock = false;
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


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLScriptFloaterManager::LLScriptFloaterManager()
        : mDialogLimitationsSlot()
// <FS:Zi> script dialogs position
,   mNavigationPanelPad(-1),    // The height of the favorite and navigation panels might not be known yet
    mFavoritesPanelPad(-1)      // so don't fill the values in here yet, but remember to do it at first use
// </FS:Zi>
{
}

void LLScriptFloaterManager::onAddNotification(const LLUUID& notification_id)
{
    if(notification_id.isNull())
    {
        LL_WARNS() << "Invalid notification ID" << LL_ENDL;
        return;
    }

    if (!mDialogLimitationsSlot.connected())
    {
        LLPointer<LLControlVariable> cntrl_ptr = gSavedSettings.getControl("ScriptDialogLimitations");
        if (cntrl_ptr.notNull())
        {
            mDialogLimitationsSlot = cntrl_ptr->getCommitSignal()->connect(boost::bind(&clearScriptNotifications));
        }
        else
        {
            LL_WARNS() << "Unable to set signal on setting 'ScriptDialogLimitations'" << LL_ENDL;
        }
    }

    // get scripted Object's ID
    LLUUID object_id = notification_id_to_object_id(notification_id);

    // Need to indicate of "new message" for object chiclets according to requirements
    // specified in the Message Bar design specification. See EXT-3142.
    bool set_new_message = false;
    EObjectType obj_type = getObjectType(notification_id);

    // LLDialog can spawn only one instance, LLLoadURL and LLGiveInventory can spawn unlimited number of instances
    if(OBJ_SCRIPT == obj_type)
    {
        static LLCachedControl<U32> script_dialog_limitations(gSavedSettings, "ScriptDialogLimitations", 0);
        script_notification_map_t::const_iterator it = mNotifications.end();
        switch (script_dialog_limitations)
        {
            case SCRIPT_PER_CHANNEL:
            {
                // If an Object spawns more-than-one floater per channel, only the newest one is shown.
                // The previous is automatically closed.
                LLNotificationPtr notification = LLNotifications::instance().find(notification_id);
                if (notification)
                {
                    it = findUsingObjectIdAndChannel(object_id, notification->getPayload()["chat_channel"].asInteger());
                }
                break;
            }
            case SCRIPT_ATTACHMENT_PER_CHANNEL:
            {
                LLViewerObject* objectp = gObjectList.findObject(object_id);
                if (objectp && objectp->getAttachmentItemID().notNull()) //in user inventory
                {
                    LLNotificationPtr notification = LLNotifications::instance().find(notification_id);
                    if (notification)
                    {
                        it = findUsingObjectIdAndChannel(object_id, notification->getPayload()["chat_channel"].asInteger());
                    }
                }
                else
                {
                    it = findUsingObjectId(object_id);
                }
                break;
            }
            case SCRIPT_HUD_PER_CHANNEL:
            {
                LLViewerObject* objectp = gObjectList.findObject(object_id);
                if (objectp && objectp->isHUDAttachment())
                {
                    LLNotificationPtr notification = LLNotifications::instance().find(notification_id);
                    if (notification)
                    {
                        it = findUsingObjectIdAndChannel(object_id, notification->getPayload()["chat_channel"].asInteger());
                    }
                }
                else
                {
                    it = findUsingObjectId(object_id);
                }
                break;
            }
            case SCRIPT_HUD_UNCONSTRAINED:
            {
                LLViewerObject* objectp = gObjectList.findObject(object_id);
                if (objectp && objectp->isHUDAttachment())
                {
                    // don't remove existing floaters
                    break;
                }
                else
                {
                    it = findUsingObjectId(object_id);
                }
                break;
            }
            // <FS:Ansariel> FIRE-21224: Unconstrained script dialog option
            case SCRIPT_UNCONSTRAINED:
                // Do nothing here - unlimited dialog spam!
                break;
            // </FS:Ansariel>
            case SCRIPT_PER_OBJECT:
            default:
            {
                // If an Object spawns more-than-one floater, only the newest one is shown.
                // The previous is automatically closed.
                it = findUsingObjectId(object_id);
                break;
            }
        }

        if(it != mNotifications.end())
        {
            LLUUID old_id = it->first; // copy LLUUID to prevent use after free when it is erased below
            LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
            if (NULL != chiclet_panelp)
            {
                LLIMChiclet * chicletp = chiclet_panelp->findChiclet<LLIMChiclet>(old_id);
                if (NULL != chicletp)
                {
                    // Pass the new_message icon state further.
                    set_new_message = chicletp->getShowNewMessagesIcon();
                    chicletp->hidePopupMenu();
                }
            }

            LLScriptFloater* floater = LLFloaterReg::findTypedInstance<LLScriptFloater>("script_floater", old_id);
            if (floater)
            {
                // Generate chiclet with a "new message" indicator if a docked window was opened but not in focus. See EXT-3142.
                set_new_message |= !floater->hasFocus();
            }

            removeNotification(old_id);
        }
    }

    mNotifications.insert(std::make_pair(notification_id, object_id));

    LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
    if (NULL != chiclet_panelp)
    {
        // Create inventory offer chiclet for offer type notifications
        if( OBJ_GIVE_INVENTORY == obj_type )
        {
            chiclet_panelp->createChiclet<LLInvOfferChiclet>(notification_id);
        }
        else
        {
            chiclet_panelp->createChiclet<LLScriptChiclet>(notification_id);
        }
    }

    LLIMWellWindow::getInstance()->addObjectRow(notification_id, set_new_message);

    LLSD data;
    data["notification_id"] = notification_id;
    data["new_message"] = set_new_message;
    data["unread"] = 1; // each object has got only one floater
    mNewObjectSignal(data);

    // <FS:CR> Flash or bounce the app icon on new script dialog
    LLWindow* viewer_window = gViewerWindow->getWindow();
    if (viewer_window && gSavedSettings.getBOOL("FSFlashOnScriptDialog"))
    {
        viewer_window->flashIcon(5.f);
    }
    // </FS:CR>

    toggleScriptFloater(notification_id, set_new_message);
}

void LLScriptFloaterManager::removeNotification(const LLUUID& notification_id)
{
    LLNotificationPtr notification = LLNotifications::instance().find(notification_id);
    if (notification != NULL && !notification->isCancelled())
    {
        LLNotificationsUtil::cancel(notification);
    }

    onRemoveNotification(notification_id);
}

void LLScriptFloaterManager::onRemoveNotification(const LLUUID& notification_id)
{
    if(notification_id.isNull())
    {
        LL_WARNS() << "Invalid notification ID" << LL_ENDL;
        return;
    }

    DialogStack::instance().pop(notification_id);   // <FS:Zi> Dialog Stacking browser

    // remove related chiclet
    if (LLChicletBar::instanceExists())
    {
        LLChicletPanel * chiclet_panelp = LLChicletBar::getInstance()->getChicletPanel();
        if (NULL != chiclet_panelp)
        {
            chiclet_panelp->removeChiclet(notification_id);
        }
    }

    LLIMWellWindow* im_well_window = LLIMWellWindow::findInstance();
    if (im_well_window)
    {
        im_well_window->removeObjectRow(notification_id);
    }

    mNotifications.erase(notification_id);

    // close floater
    LLScriptFloater* floater = LLFloaterReg::findTypedInstance<LLScriptFloater>("script_floater", notification_id);
    if(floater)
    {
        floater->savePosition();
        floater->setNotificationId(LLUUID::null);
        floater->closeFloater();
    }
}

void LLScriptFloaterManager::toggleScriptFloater(const LLUUID& notification_id, bool set_new_message)
{
    LLSD data;
    data["notification_id"] = notification_id;
    data["new_message"] = set_new_message;
    mToggleFloaterSignal(data);

    // toggle floater
    LLScriptFloater::toggle(notification_id);
}

LLUUID LLScriptFloaterManager::findObjectId(const LLUUID& notification_id)
{
    script_notification_map_t::const_iterator it = mNotifications.find(notification_id);
    if(mNotifications.end() != it)
    {
        return it->second;
    }
    return LLUUID::null;
}

LLUUID LLScriptFloaterManager::findNotificationId(const LLUUID& object_id)
{
    if(object_id.notNull())
    {
        script_notification_map_t::const_iterator it = findUsingObjectId(object_id);
        if(mNotifications.end() != it)
        {
            return it->first;
        }
    }
    return LLUUID::null;
}

// static
LLScriptFloaterManager::EObjectType LLScriptFloaterManager::getObjectType(const LLUUID& notification_id)
{
    if(notification_id.isNull())
    {
        LL_WARNS() << "Invalid notification ID" << LL_ENDL;
        return OBJ_UNKNOWN;
    }

    static const object_type_map TYPE_MAP = initObjectTypeMap();

    LLNotificationPtr notification = LLNotificationsUtil::find(notification_id);
    object_type_map::const_iterator it = TYPE_MAP.find(notification->getName());
    if(it != TYPE_MAP.end())
    {
        return it->second;
    }

    LL_WARNS() << "Unknown object type: " << notification->getName() << LL_ENDL;
    return OBJ_UNKNOWN;
}

// static
std::string LLScriptFloaterManager::getObjectName(const LLUUID& notification_id)
{
    using namespace LLNotificationsUI;
    LLNotificationPtr notification = LLNotifications::getInstance()->find(notification_id);
    if(!notification)
    {
        LL_WARNS() << "Invalid notification" << LL_ENDL;
        return LLStringUtil::null;
    }

    std::string text;

    switch(LLScriptFloaterManager::getObjectType(notification_id))
    {
    case LLScriptFloaterManager::OBJ_SCRIPT:
        text = notification->getSubstitutions()["TITLE"].asString();
        break;
    case LLScriptFloaterManager::OBJ_LOAD_URL:
        text = notification->getSubstitutions()["OBJECTNAME"].asString();
        break;
    case LLScriptFloaterManager::OBJ_GIVE_INVENTORY:
        text = notification->getSubstitutions()["OBJECTFROMNAME"].asString();
        break;
    default:
        text = LLTrans::getString("object");
        break;
    }

    return text;
}

//static
LLScriptFloaterManager::object_type_map LLScriptFloaterManager::initObjectTypeMap()
{
    object_type_map type_map;
    type_map["ScriptDialog"] = OBJ_SCRIPT;
    type_map["ScriptDialogGroup"] = OBJ_SCRIPT;
    type_map["LoadWebPage"] = OBJ_LOAD_URL;
    type_map["ObjectGiveItem"] = OBJ_GIVE_INVENTORY;
    type_map["OwnObjectGiveItem"] = OBJ_GIVE_INVENTORY; // <FS:Ansariel> Fix log warning about unkown script message type
    return type_map;
}

LLScriptFloaterManager::script_notification_map_t::const_iterator LLScriptFloaterManager::findUsingObjectId(const LLUUID& object_id)
{
    script_notification_map_t::const_iterator it = mNotifications.begin();
    for(; mNotifications.end() != it; ++it)
    {
        if(object_id == it->second)
        {
            return it;
        }
    }
    return mNotifications.end();
}

LLScriptFloaterManager::script_notification_map_t::const_iterator LLScriptFloaterManager::findUsingObjectIdAndChannel(const LLUUID& object_id, S32 im_channel)
{
    script_notification_map_t::const_iterator it = mNotifications.begin();
    for (; mNotifications.end() != it; ++it)
    {
        if (object_id == it->second)
        {
            LLNotificationPtr notification = LLNotifications::instance().find(it->first);
            if (notification && (im_channel == notification->getPayload()["chat_channel"].asInteger()))
            {
                return it;
            }
        }
    }
    return mNotifications.end();
}

void LLScriptFloaterManager::saveFloaterPosition(const LLUUID& object_id, const FloaterPositionInfo& fpi)
{
    if(object_id.notNull())
    {
        LLScriptFloaterManager::getInstance()->mFloaterPositions[object_id] = fpi;
    }
    else
    {
        LL_WARNS() << "Invalid object id" << LL_ENDL;
    }
}

bool LLScriptFloaterManager::getFloaterPosition(const LLUUID& object_id, FloaterPositionInfo& fpi)
{
    floater_position_map_t::const_iterator it = mFloaterPositions.find(object_id);
    if(LLScriptFloaterManager::getInstance()->mFloaterPositions.end() != it)
    {
        fpi = it->second;
        return true;
    }
    return false;
}

void LLScriptFloaterManager::setFloaterVisible(const LLUUID& notification_id, bool visible)
{
    LLScriptFloater* floater = LLFloaterReg::findTypedInstance<LLScriptFloater>(
        "script_floater", notification_id);
    if(floater)
    {
        floater->setVisible(visible);
    }
}

//static
void LLScriptFloaterManager::clearScriptNotifications()
{
    LLScriptFloaterManager* inst = LLScriptFloaterManager::getInstance();
    static const object_type_map TYPE_MAP = initObjectTypeMap();

    script_notification_map_t::const_iterator ntf_it = inst->mNotifications.begin();
    while (inst->mNotifications.end() != ntf_it)
    {
        LLUUID notification_id = ntf_it->first;
        ntf_it++; // onRemoveNotification() erases notification
        LLNotificationPtr notification = LLNotifications::instance().find(notification_id);
        if (notification)
        {
            object_type_map::const_iterator map_it = TYPE_MAP.find(notification->getName());
            if (map_it != TYPE_MAP.end() && map_it->second == OBJ_SCRIPT)
            {
                if (notification != NULL && !notification->isCancelled())
                {
                    LLNotificationsUtil::cancel(notification);
                }
                inst->onRemoveNotification(notification_id);
            }
        }
    }
}

// <FS:Zi> script dialogs position
S32 LLScriptFloaterManager::getTopPad()
{
    // initialize if needed
    if (mNavigationPanelPad == -1)
    {
        mNavigationPanelPad = LLNavigationBar::instance().getView()->getChild<LLView>("location_search_layout")->getRect().getHeight();
    }

    // initialize if needed
    if (mFavoritesPanelPad == -1)
    {
        mFavoritesPanelPad = LLNavigationBar::instance().getView()->getChild<LLView>("favorite")->getRect().getHeight();
    }

    S32 pad = 0;
    if (gSavedSettings.getBOOL("ShowNavbarNavigationPanel"))
    {
        pad = mNavigationPanelPad;
    }

    if (gSavedSettings.getBOOL("ShowNavbarFavoritesPanel"))
    {
        pad += mFavoritesPanelPad;
    }

    return pad;
}
// </FS:Zi>

//////////////////////////////////////////////////////////////////

bool LLScriptFloater::isScriptTextbox(LLNotificationPtr notification)
{
    // get a form for the notification
    LLNotificationFormPtr form(notification->getForm());

    if (form)
    {
        // get number of elements in the form
        int num_options = form->getNumElements();

        // if ANY of the buttons have the magic lltextbox string as
        // name, then treat the whole dialog as a simple text entry
        // box (i.e. mixed button and textbox forms are not supported)
        for (int i=0; i<num_options; ++i)
        {
            LLSD form_element = form->getElement(i);
            if (form_element["name"].asString() == TEXTBOX_MAGIC_TOKEN)
            {
                return true;
            }
        }
    }

    return false;
}

// <FS:Zi> Animated dialogs
void LLScriptFloater::draw()
{
    if (mCurrentHeight < mDesiredHeight)
    {
        mCurrentHeight = (S32)((LLFrameTimer::getElapsedSeconds() - mStartTime) * 2.5 * (F64)mDesiredHeight);

        if (mCurrentHeight > mDesiredHeight)
        {
            mCurrentHeight = mDesiredHeight;
        }

        LLRect toast_rect=getRect();
        toast_rect.setLeftTopAndSize(toast_rect.mLeft, toast_rect.mTop, toast_rect.getWidth(), mCurrentHeight);
        setShape(toast_rect);
    }

    LLDockableFloater::draw();
}
// </FS:Zi>

// <FS:Zi> script dialogs position
LLScriptFloater* LLScriptFloater::show(const LLUUID& notification_id)
{
    LLScriptFloater* floater = LLFloaterReg::getTypedInstance<LLScriptFloater>("script_floater", notification_id);
    floater->createForm(notification_id);

    //LLDialog(LLGiveInventory and LLLoadURL) should no longer steal focus (see EXT-5445)
    floater->setAutoFocus(false);

    eDialogPosition dialog_position = (eDialogPosition)gSavedSettings.getS32("ScriptDialogsPosition");

    bool chicletsDisabled = gSavedSettings.getBOOL("FSDisableIMChiclets");

    if (dialog_position == POS_DOCKED && chicletsDisabled)
    {
        dialog_position = POS_TOP_RIGHT;
    }

    if (dialog_position != POS_DOCKED)
    {
        // undock the dialog
        floater->setDocked(false, true);
        if (chicletsDisabled)
        {
            // Remove the dock icon in case chiclets are hidden since there is nothing to dock to
            floater->setCanDock(false);
        }
    }

    S32 topPad = LLScriptFloaterManager::instance().getTopPad();

    S32 bottomPad = 0;
    if (gToolBarView->getToolbar(LLToolBarEnums::TOOLBAR_BOTTOM)->hasButtons())
    {
        bottomPad = gToolBarView->getToolbar(LLToolBarEnums::TOOLBAR_BOTTOM)->getRect().getHeight();
    }

    S32 leftPad = 0;
    if (gToolBarView->getToolbar(LLToolBarEnums::TOOLBAR_LEFT)->hasButtons())
    {
        leftPad = gToolBarView->getToolbar(LLToolBarEnums::TOOLBAR_LEFT)->getRect().getWidth();
    }

    S32 rightPad = 0;
    if (gToolBarView->getToolbar(LLToolBarEnums::TOOLBAR_RIGHT)->hasButtons())
    {
        rightPad = gToolBarView->getToolbar(LLToolBarEnums::TOOLBAR_RIGHT)->getRect().getWidth();
    }

    LLRect pos = floater->getRect();
    S32 width = pos.getWidth();
    S32 height = pos.getHeight();

    floater->setOpenPositioning(LLFloaterEnums::POSITIONING_SPECIFIED);

    if (!gSavedSettings.getBOOL("FSDisableIMChiclets"))
    {
        // This also sets up the dock, so we always need to call this (unless chiclets are hidden)
        // or the dock button won't work at all.
        floater->dockToChiclet(dialog_position == POS_DOCKED, dialog_position == POS_DOCKED);
    }

    switch (dialog_position)
    {
        case POS_TOP_LEFT:
        {
            pos.setOriginAndSize(leftPad,
                                gViewerWindow->getWorldViewHeightScaled() - height - topPad,
                                width, height);
            break;
        }
        case POS_TOP_RIGHT:
        {
            pos.setOriginAndSize(gViewerWindow->getWorldViewWidthScaled() - width - rightPad,
                                gViewerWindow->getWorldViewHeightScaled() - height - topPad,
                                width, height);
            break;
        }
        case POS_BOTTOM_LEFT:
        {
            pos.setOriginAndSize(leftPad,
                                bottomPad,
                                width, height);
            break;
        }
        case POS_BOTTOM_RIGHT:
        {
            pos.setOriginAndSize(gViewerWindow->getWorldViewWidthScaled() - width - rightPad,
                                bottomPad,
                                width, height);
            break;
        }
        default:
        {
            LL_WARNS() << "dialog_position value " << dialog_position << " not handled in switch() statement." << LL_ENDL;
        }
    }

    //LLDialog(LLGiveInventory and LLLoadURL) should no longer steal focus (see EXT-5445)
    LLFloaterReg::showTypedInstance<LLScriptFloater>("script_floater", notification_id, false);

    if (!floater->isDocked())
    {
        // reposition the floater which might have been shifted to cascade
        floater->setRect(pos);
    }

    static LLCachedControl<bool> script_dialog_container(gSavedSettings,"FSScriptDialogContainer", false);

    if (script_dialog_container)
    {
        FSFloaterScriptContainer* container = LLFloaterReg::getTypedInstance<FSFloaterScriptContainer>("fs_script_container");
        floater->setCanDock(false);

        LLNotificationPtr notification = LLNotifications::getInstance()->find(notification_id);
        if (notification != NULL)
        {
            floater->setShortTitle(LLScriptFloaterManager::getObjectName(notification_id));
        }

        container->addFloater(floater, true);
        container->setVisible(true);
    }

    return floater;
}
// </FS:Zi>

// EOF
