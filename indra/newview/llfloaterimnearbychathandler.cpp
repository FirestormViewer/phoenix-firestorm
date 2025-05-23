/**
 * @file LLFloaterIMNearbyChatHandler.cpp
 * @brief Nearby chat chat managment
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

#include "llagent.h"
#include "llagentdata.h" // for gAgentID
#include "llfloaterimnearbychathandler.h"

#include "llchatitemscontainerctrl.h"
#include "llfirstuse.h"
#include "llfloaterscriptdebug.h"
#include "llhints.h"
#include "llfloaterimnearbychat.h"
#include "llrecentpeople.h"

#include "llviewercontrol.h"

#include "llfloaterreg.h"//for LLFloaterReg::getTypedInstance
#include "llviewerwindow.h"//for screen channel position
#include "llfloaterimnearbychat.h"
#include "llfloaterimcontainer.h"
#include "llrootview.h"
#include "lllayoutstack.h"

// [RLVa:KB] - Checked: RLVa-2.0.0
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]

#include "fsconsoleutils.h"
#include "fsfloaternearbychat.h"
#include "llviewernetwork.h"

//add LLFloaterIMNearbyChatHandler to LLNotificationsUI namespace

using namespace LLNotificationsUI;

static LLFloaterIMNearbyChatToastPanel* createToastPanel()
{
    LLFloaterIMNearbyChatToastPanel* item = LLFloaterIMNearbyChatToastPanel::createInstance();
    return item;
}


//-----------------------------------------------------------------------------------------------
//LLFloaterIMNearbyChatScreenChannel
//-----------------------------------------------------------------------------------------------

class LLFloaterIMNearbyChatScreenChannel: public LLScreenChannelBase
{
    LOG_CLASS(LLFloaterIMNearbyChatScreenChannel);
public:
    typedef std::vector<LLHandle<LLToast> > toast_vec_t;
    typedef std::list<LLHandle<LLToast> > toast_list_t;

    LLFloaterIMNearbyChatScreenChannel(const Params& p)
        : LLScreenChannelBase(p)
    {
        // <FS:Ansariel> Zi Ree's customizable nearby chat toast width
        mWorldViewRectConnection = gViewerWindow->setOnWorldViewRectUpdated(boost::bind(&LLFloaterIMNearbyChatScreenChannel::updateSize, this, _1, _2));

        mStopProcessing = false;

        LLControlVariable* ctrl = gSavedSettings.getControl("NearbyToastLifeTime").get();
        if (ctrl)
        {
            ctrl->getSignal()->connect(boost::bind(&LLFloaterIMNearbyChatScreenChannel::updateToastsLifetime, this));
        }

        ctrl = gSavedSettings.getControl("NearbyToastFadingTime").get();
        if (ctrl)
        {
            ctrl->getSignal()->connect(boost::bind(&LLFloaterIMNearbyChatScreenChannel::updateToastFadingTime, this));
        }
        // <FS:darl> [FIRE-35039 > FIRE-35294] Add flag to show/hide the on-screen console
        ctrl = gSavedSettings.getControl("FSShowOnscreenConsole").get();
        if (ctrl)
        {
            ctrl->getSignal()->connect(boost::bind(&LLFloaterIMNearbyChatScreenChannel::removeToastsFromChannel, this));
        }
        // </FS:darl> [FIRE-35039 > FIRE-35294] Add flag to show/hide the on-screen console
    }

    void addChat    (LLSD& chat);
    void arrangeToasts      ();

    typedef boost::function<LLFloaterIMNearbyChatToastPanel* (void )> create_toast_panel_callback_t;
    void setCreatePanelCallback(create_toast_panel_callback_t value) { m_create_toast_panel_callback_t = value;}

    void onToastDestroyed   (LLToast* toast, bool app_quitting);
    void onToastFade        (LLToast* toast);

    void redrawToasts()
    {
        arrangeToasts();
    }

    // hide all toasts from screen, but not remove them from a channel
    // removes all toasts from a channel
    virtual void        removeToastsFromChannel()
    {
        for(toast_vec_t::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
        {
            addToToastPool(it->get());
        }
        m_active_toasts.clear();
    };

    virtual void deleteAllChildren()
    {
        LL_DEBUGS("NearbyChat") << "Clearing toast pool" << LL_ENDL;
        m_toast_pool.clear();
        m_active_toasts.clear();
        LLScreenChannelBase::deleteAllChildren();
    }

protected:
    void    deactivateToast(LLToast* toast);
    void    addToToastPool(LLToast* toast)
    {
        if (!toast) return;
        LL_DEBUGS("NearbyChat") << "Pooling toast" << LL_ENDL;
        toast->setVisible(false);
        toast->stopTimer();
        toast->setIsHidden(true);

        // Nearby chat toasts that are hidden, not destroyed. They are collected to the toast pool, so that
        // they can be used next time, this is done for performance. But if the toast lifetime was changed
        // (from preferences floater (STORY-36)) while it was shown (at this moment toast isn't in the pool yet)
        // changes don't take affect.
        // So toast's lifetime should be updated each time it's added to the pool. Otherwise viewer would have
        // to be restarted so that changes take effect.
        toast->setLifetime(gSavedSettings.getS32("NearbyToastLifeTime"));
        toast->setFadingTime(gSavedSettings.getS32("NearbyToastFadingTime"));
        m_toast_pool.push_back(toast->getHandle());
    }

    void    createOverflowToast(S32 bottom, F32 timer);

    void    updateToastsLifetime();

    void    updateToastFadingTime();

    create_toast_panel_callback_t m_create_toast_panel_callback_t;

    bool    createPoolToast();

    toast_vec_t m_active_toasts;
    toast_list_t m_toast_pool;

    bool    mStopProcessing;

    // <FS:Ansariel> Zi Ree's customizable nearby chat toast width
    void            reshapePanel(LLFloaterIMNearbyChatToastPanel* panel);
    virtual void    updateSize(LLRect old_world_rect, LLRect new_world_rect);
    boost::signals2::connection mWorldViewRectConnection;
    // </FS:Ansariel> Zi Ree's customizable nearby chat toast width
};



//-----------------------------------------------------------------------------------------------
// LLFloaterIMNearbyChatToast
//-----------------------------------------------------------------------------------------------

// We're deriving from LLToast to be able to override onClose()
// in order to handle closing nearby chat toasts properly.
class LLFloaterIMNearbyChatToast : public LLToast
{
    LOG_CLASS(LLFloaterIMNearbyChatToast);
public:
    LLFloaterIMNearbyChatToast(const LLToast::Params& p, LLFloaterIMNearbyChatScreenChannel* nc_channelp)
    :   LLToast(p),
        mNearbyChatScreenChannelp(nc_channelp)
    {
    }

    /*virtual*/ void onClose(bool app_quitting);

private:
    LLFloaterIMNearbyChatScreenChannel* mNearbyChatScreenChannelp;
};

//-----------------------------------------------------------------------------------------------
// LLFloaterIMNearbyChatScreenChannel
//-----------------------------------------------------------------------------------------------

// <FS:Ansariel> Zi Ree's customizable nearby chat toast width
void LLFloaterIMNearbyChatScreenChannel::reshapePanel(LLFloaterIMNearbyChatToastPanel* panel)
{
    S32 percentage = gSavedSettings.getS32("NearbyToastWidth");
    panel->reshape(gViewerWindow->getWindowWidthScaled() * percentage / 100, panel->getRect().getHeight(), true);
}

void LLFloaterIMNearbyChatScreenChannel::updateSize(LLRect old_world_rect, LLRect new_world_rect)
{
    for(toast_vec_t::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
    {
        LLToast* toast = it->get();

        if (toast)
        {
            LLFloaterIMNearbyChatToastPanel* panel = dynamic_cast<LLFloaterIMNearbyChatToastPanel*>(toast->getPanel());

            if(panel)
            {
                reshapePanel(panel);
                toast->reshapeToPanel();
            }
        }
    }
    arrangeToasts();
}
// </FS:Ansariel> Zi Ree's customizable nearby chat toast width

void LLFloaterIMNearbyChatScreenChannel::deactivateToast(LLToast* toast)
{
    toast_vec_t::iterator pos = std::find(m_active_toasts.begin(), m_active_toasts.end(), toast->getHandle());

    if (pos != m_active_toasts.end())
    {
        LL_DEBUGS("NearbyChat") << "Deactivating toast" << LL_ENDL;
        m_active_toasts.erase(pos);
    }
}

void    LLFloaterIMNearbyChatScreenChannel::createOverflowToast(S32 bottom, F32 timer)
{
    //we don't need overflow toast in nearby chat
}

void LLFloaterIMNearbyChatScreenChannel::onToastDestroyed(LLToast* toast, bool app_quitting)
{
    LL_DEBUGS("NearbyChat") << "Toast destroyed (app_quitting=" << app_quitting << ")" << LL_ENDL;

    if (app_quitting)
    {
        // Viewer is quitting.
        // Immediately stop processing chat messages (EXT-1419).
        mStopProcessing = true;
    }
    else
    {
        // The toast is being closed by user (STORM-192).
        // Remove it from the list of active toasts to prevent
        // further references to the invalid pointer.
        deactivateToast(toast);
    }
}

void LLFloaterIMNearbyChatScreenChannel::onToastFade(LLToast* toast)
{
    LL_DEBUGS("NearbyChat") << "Toast fading" << LL_ENDL;

    //fade mean we put toast to toast pool
    if(!toast)
        return;

    deactivateToast(toast);

    addToToastPool(toast);

    arrangeToasts();
}

void LLFloaterIMNearbyChatScreenChannel::updateToastsLifetime()
{
    S32 seconds = gSavedSettings.getS32("NearbyToastLifeTime");
    toast_list_t::iterator it;

    for(it = m_toast_pool.begin(); it != m_toast_pool.end(); ++it)
    {
        (*it).get()->setLifetime(seconds);
    }
}

void LLFloaterIMNearbyChatScreenChannel::updateToastFadingTime()
{
    S32 seconds = gSavedSettings.getS32("NearbyToastFadingTime");
    toast_list_t::iterator it;

    for(it = m_toast_pool.begin(); it != m_toast_pool.end(); ++it)
    {
        (*it).get()->setFadingTime(seconds);
    }
}

bool    LLFloaterIMNearbyChatScreenChannel::createPoolToast()
{
    LLFloaterIMNearbyChatToastPanel* panel= m_create_toast_panel_callback_t();
    if(!panel)
        return false;

    LLToast::Params p;
    p.panel = panel;
    p.lifetime_secs = (F32)gSavedSettings.getS32("NearbyToastLifeTime");
    p.fading_time_secs = (F32)gSavedSettings.getS32("NearbyToastFadingTime");

    LLToast* toast = new LLFloaterIMNearbyChatToast(p, this);

    // <FS:Ansariel> Zi Ree's customizable nearby chat toast width
    reshapePanel(panel);

    toast->setOnFadeCallback(boost::bind(&LLFloaterIMNearbyChatScreenChannel::onToastFade, this, _1));

    // If the toast gets somehow prematurely destroyed, deactivate it to prevent crash (STORM-1352).
    toast->setOnToastDestroyedCallback(boost::bind(&LLFloaterIMNearbyChatScreenChannel::onToastDestroyed, this, _1, false));

    LL_DEBUGS("NearbyChat") << "Creating and pooling toast" << LL_ENDL;
    m_toast_pool.push_back(toast->getHandle());
    return true;
}

void LLFloaterIMNearbyChatScreenChannel::addChat(LLSD& chat)
{
    //look in pool. if there is any message
    if (mStopProcessing)
        return;

    if (mFloaterSnapRegion == NULL)
    {
        mFloaterSnapRegion = gViewerWindow->getFloaterSnapRegion();
    }
    LLRect channel_rect;
    mFloaterSnapRegion->localRectToOtherView(mFloaterSnapRegion->getLocalRect(), &channel_rect, gFloaterView);
    chat["available_height"] = channel_rect.getHeight() - channel_rect.mBottom - gSavedSettings.getS32("ToastGap") - 110;;

    /*
    find last toast and check ID
    */

    if (m_active_toasts.size())
    {
        LLUUID fromID = chat["from_id"].asUUID();       // agent id or object id
        std::string from = chat["from"].asString();
        LLToast* toast = m_active_toasts[0].get();
        if (toast)
        {
            LLFloaterIMNearbyChatToastPanel* panel = dynamic_cast<LLFloaterIMNearbyChatToastPanel*>(toast->getPanel());

            if (panel && panel->messageID() == fromID && panel->getFromName() == from && panel->canAddText())
            {
                panel->addMessage(chat);
                // <FS:Ansariel> Zi Ree's customizable nearby chat toast width
                reshapePanel(panel);
                // </FS:Ansariel> Zi Ree's customizable nearby chat toast width
                toast->reshapeToPanel();
                toast->startTimer();

                arrangeToasts();
                return;
            }
        }
    }



    if (m_toast_pool.empty())
    {
        //"pool" is empty - create one more panel
        LL_DEBUGS("NearbyChat") << "Empty pool" << LL_ENDL;
        if(!createPoolToast())//created toast will go to pool. so next call will find it
            return;
        addChat(chat);
        return;
    }

    int chat_type = chat["chat_type"].asInteger();

    if (chat_type == CHAT_TYPE_DEBUG_MSG)
    {
        if (!gSavedSettings.getBOOL("ShowScriptErrors"))
            return;
        if (gSavedSettings.getS32("ShowScriptErrorsLocation") == 1)
            return;
    }

    //take 1st element from pool, (re)initialize it, put it in active toasts

    LL_DEBUGS("NearbyChat") << "Getting toast from pool" << LL_ENDL;
    LLToast* toast = m_toast_pool.back().get();

    m_toast_pool.pop_back();


    LLFloaterIMNearbyChatToastPanel* panel = dynamic_cast<LLFloaterIMNearbyChatToastPanel*>(toast->getPanel());
    if (!panel)
        return;
    panel->init(chat);

    // <FS:Ansariel> Zi Ree's customizable nearby chat toast width
    reshapePanel(panel);

    toast->reshapeToPanel();
    toast->startTimer();

    m_active_toasts.push_back(toast->getHandle());

    arrangeToasts();
}

static bool sort_toasts_predicate(LLHandle<LLToast> first, LLHandle<LLToast> second)
{
    if (!first.get() || !second.get()) return false; // STORM-1352

    F32 v1 = first.get()->getTimeLeftToLive();
    F32 v2 = second.get()->getTimeLeftToLive();
    return v1 > v2;
}

void LLFloaterIMNearbyChatScreenChannel::arrangeToasts()
{
    if(mStopProcessing || isHovering())
        return;

    if (mFloaterSnapRegion == NULL)
    {
        mFloaterSnapRegion = gViewerWindow->getFloaterSnapRegion();
    }

    if (!getParent())
    {
        // connect to floater snap region just to get resize events, we don't care about being a proper widget
        mFloaterSnapRegion->addChild(this);
        setFollows(FOLLOWS_ALL);
    }

    LLRect  toast_rect;
    updateRect();

    LLRect channel_rect;
    mFloaterSnapRegion->localRectToOtherView(mFloaterSnapRegion->getLocalRect(), &channel_rect, gFloaterView);
    channel_rect.mLeft += 10;
    channel_rect.mRight = channel_rect.mLeft + 300;

    S32 channel_bottom = channel_rect.mBottom;

    // <FS:Ansariel> Configurable nearby chat toasts offset
    //S32       bottom = channel_bottom + 80;
    S32     bottom = channel_bottom + gSavedSettings.getS32("FSNearbyChatToastsOffset");
    S32     margin = gSavedSettings.getS32("ToastGap");

    //sort active toasts
    std::sort(m_active_toasts.begin(),m_active_toasts.end(),sort_toasts_predicate);

    //calc max visible item and hide other toasts.

    for(toast_vec_t::iterator it = m_active_toasts.begin(); it != m_active_toasts.end(); ++it)
    {
        LLToast* toast = it->get();
        if (!toast)
        {
            LL_WARNS() << "NULL found in the active chat toasts list!" << LL_ENDL;
            continue;
        }

        S32 toast_top = bottom + toast->getRect().getHeight() + margin;

        if(toast_top > channel_rect.getHeight())
        {
            while(it!=m_active_toasts.end())
            {
                addToToastPool(it->get());
                it=m_active_toasts.erase(it);
            }
            break;
        }

        toast_rect = toast->getRect();
        toast_rect.setLeftTopAndSize(channel_rect.mLeft , bottom + toast_rect.getHeight(), toast_rect.getWidth() ,toast_rect.getHeight());

        toast->setRect(toast_rect);
        bottom += toast_rect.getHeight() - toast->getTopPad() + margin;
    }

    // use reverse order to provide correct z-order and avoid toast blinking

    for(toast_vec_t::reverse_iterator it = m_active_toasts.rbegin(); it != m_active_toasts.rend(); ++it)
    {
        LLToast* toast = it->get();
        if (toast)
        {
        toast->setIsHidden(false);
        toast->setVisible(true);
        }
    }

}



//-----------------------------------------------------------------------------------------------
//LLFloaterIMNearbyChatHandler
//-----------------------------------------------------------------------------------------------
std::unique_ptr<LLEventPump> LLFloaterIMNearbyChatHandler::sChatWatcher(new LLEventStream("LLChat"));

LLFloaterIMNearbyChatHandler::LLFloaterIMNearbyChatHandler()
{
    // Getting a Channel for our notifications
    LLFloaterIMNearbyChatScreenChannel::Params p;
    p.id = NEARBY_CHAT_CHANNEL_UUID;
    LLFloaterIMNearbyChatScreenChannel* channel = new LLFloaterIMNearbyChatScreenChannel(p);

    LLFloaterIMNearbyChatScreenChannel::create_toast_panel_callback_t callback = createToastPanel;

    channel->setCreatePanelCallback(callback);

    LLChannelManager::getInstance()->addChannel(channel);

    mChannel = channel->getHandle();
}

LLFloaterIMNearbyChatHandler::~LLFloaterIMNearbyChatHandler()
{
}


void LLFloaterIMNearbyChatHandler::initChannel()
{
    //LLRect snap_rect = gFloaterView->getSnapRect();
    //mChannel->init(snap_rect.mLeft, snap_rect.mLeft + 200);
}



void LLFloaterIMNearbyChatHandler::processChat(const LLChat& chat_msg,
                                      const LLSD &args)
{
    if (chat_msg.mMuted)
    // <FS:Ansariel> Optional muted chat history
        //return;
    {
        FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
        nearby_chat->addMessage(chat_msg, true, args);
        return;
    }
    // </FS:Ansariel> Optional muted chat history

    if (chat_msg.mText.empty())
        return; // don't process empty messages

    LLChat& tmp_chat = const_cast<LLChat&>(chat_msg);
// [RLVa:KB] - Checked: 2010-04-20 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
    if (RlvActions::isRlvEnabled())
    {
        // NOTE-RLVa: we can only filter the *message* here since most everything else will already be part of "args" as well
        LLChat& tmp_chat = const_cast<LLChat&>(chat_msg);
        if ( (!RlvActions::canShowLocation()) && (!tmp_chat.mRlvLocFiltered) && (CHAT_SOURCE_AGENT != tmp_chat.mSourceType) )
        {
            RlvUtil::filterLocation(tmp_chat.mText);
            tmp_chat.mRlvLocFiltered = true;
        }
        if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT)) && (!tmp_chat.mRlvNamesFiltered) && (CHAT_SOURCE_AGENT != tmp_chat.mSourceType) && (!args.has("ONLINE_STATUS") || !args["ONLINE_STATUS"].asBoolean()) )
        {
            RlvUtil::filterNames(tmp_chat.mText);
            tmp_chat.mRlvNamesFiltered = true;
        }
    }
// [/RLVa:KB]

    // <FS:Ansariel> [FS communication UI]
    //LLFloaterReg::getInstance("im_container");
    //LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
    FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
    // </FS:Ansariel> [FS communication UI]

    // Build notification data
    LLSD chat;
    chat["message"] = chat_msg.mText;
    chat["from"] = chat_msg.mFromName;
    chat["from_id"] = chat_msg.mFromID;
    chat["time"] = chat_msg.mTime;
    chat["source"] = (S32)chat_msg.mSourceType;
    chat["chat_type"] = (S32)chat_msg.mChatType;
    chat["chat_style"] = (S32)chat_msg.mChatStyle;
    // Pass sender info so that it can be rendered properly (STORM-1021).
//  chat["sender_slurl"] = LLViewerChat::getSenderSLURL(chat_msg, args);
// [RLVa:KB] - Checked: 2011-12-13 (RLVa-1.4.6) | Added: RLVa-1.4.6
    if ((CHAT_SOURCE_AGENT != chat_msg.mSourceType) || (!chat_msg.mRlvNamesFiltered))
        chat["sender_slurl"] = LLViewerChat::getSenderSLURL(chat_msg, args);
// [/RLVa:KB]

    if (chat_msg.mChatType == CHAT_TYPE_DIRECT &&
        chat_msg.mText.length() > 0 &&
        chat_msg.mText[0] == '@')
    {
        // Send event on to LLEventStream and exit
        sChatWatcher->post(chat);
        return;
    }

    // <FS:Ansariel> Add notification callback for new chat
    mNewChatSignal(chat);

    // don't show toast and add message to chat history on receive debug message
    // with disabled setting showing script errors or enabled setting to show script
    // errors in separate window.
    // <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
    // if (chat_msg.mChatType == CHAT_TYPE_DEBUG_MSG)
    static LLCachedControl<bool> FSllOwnerSayToScriptDebugWindow(gSavedPerAccountSettings, "FSllOwnerSayToScriptDebugWindow");
    if (chat_msg.mChatType == CHAT_TYPE_DEBUG_MSG || (chat_msg.mChatType == CHAT_TYPE_OWNER && FSllOwnerSayToScriptDebugWindow))
    {
        if (LLFloater::isQuitRequested())
            return;

        // <FS:Kadah> [FSllOwnerSayToScriptDebugWindow] Show llOwnerSays in the script debug window instead of local chat
        // if (!gSavedSettings.getBOOL("ShowScriptErrors"))
        if (!gSavedSettings.getBOOL("ShowScriptErrors") && chat_msg.mChatType == CHAT_TYPE_DEBUG_MSG)
            return;

        // don't process debug messages from not owned objects, see EXT-7762
        // <FS:Ansariel> FIRE-15014: [OpenSim] osMessageObject(target, message) fails silently
        //if (gAgentID != chat_msg.mOwnerID)
#ifdef OPENSIM
        if (LLGridManager::getInstance()->isInSecondLife() && gAgentID != chat_msg.mOwnerID)
#else
        if (gAgentID != chat_msg.mOwnerID)
#endif
        // </FS:Ansariel>
        {
            return;
        }

        // <FS:Ansariel> Script debug icon
        //if (gSavedSettings.getS32("ShowScriptErrorsLocation") == 1)// show error in window //("ScriptErrorsAsChat"))
        {
            // <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
            //LLUIColor txt_color;
            //F32 alpha = 1.f;
            //LLViewerChat::getChatColor(chat_msg, txt_color, alpha);

            // LLFloaterScriptDebug::addScriptLine(chat_msg.mText,
                                                // chat_msg.mFromName,
                                                // txt_color % alpha,
                                                // chat_msg.mFromID);
            LLFloaterScriptDebug::addScriptLine(chat_msg);
            // <FS:Ansariel> Script debug icon
            //return;
            // <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
            // if (gSavedSettings.getS32("ShowScriptErrorsLocation") == 1)
            if (gSavedSettings.getS32("ShowScriptErrorsLocation") == 1 || chat_msg.mChatType == CHAT_TYPE_OWNER)
            {
                return;
            }
            // </FS:Ansariel> Script debug icon

        }
    }
    // <FS:Beq> Hide Primfeed OAuth message from chat to prevent accidental leak of secret.
    const std::string primfeed_oauth = "#PRIMFEED_OAUTH: ";
    if( chat_msg.mText.compare(0, primfeed_oauth.length(), primfeed_oauth) == 0 && chat_msg.mChatType == CHAT_TYPE_IM && chat_msg.mSourceType == CHAT_SOURCE_OBJECT )
    {
        // Don't show the message in chat.
        return;
    }
    // </FS:Beq>
    nearby_chat->addMessage(chat_msg, true, args);

    if (chat_msg.mSourceType == CHAT_SOURCE_AGENT
        && chat_msg.mFromID.notNull()
        && chat_msg.mFromID != gAgentID)
    {
        LLFirstUse::otherAvatarChatFirst();

        // Add sender to the recent people list.
// [RLVa:KB] - Checked: RLVa-2.0.0
        if ( (!RlvActions::isRlvEnabled()) || (RlvActions::canShowName(RlvActions::SNC_DEFAULT, chat_msg.mFromID)) )
            LLRecentPeople::instance().add(chat_msg.mFromID);
// [/RLVa:KB]
//      LLRecentPeople::instance().add(chat_msg.mFromID);
    }

    // Send event on to LLEventStream
    sChatWatcher->post(chat);

    // <FS:Ansariel> [FS communication UI]
    //LLFloaterIMContainer* im_box = LLFloaterReg::getTypedInstance<LLFloaterIMContainer>("im_container");
    // </FS:Ansariel> [FS communication UI]

    // <FS:Ansariel> Nearby chat in console
    if (FSConsoleUtils::ProcessChatMessage(chat_msg, args))
    {
        return;
    }
    // </FS:Ansariel>

    // <FS:darl> [FIRE-35039 > FIRE-35294] Add flag to show/hide the on-screen console
    static LLUICachedControl<bool> showOnscreenConsole("FSShowOnscreenConsole");
    if (!showOnscreenConsole)
    {
        return;
    }
    // </FS:darl> [FIRE-35039 > FIRE-35294]

    static LLCachedControl<bool> useChatBubbles(gSavedSettings, "UseChatBubbles");
    static LLCachedControl<bool> fsBubblesHideConsoleAndToasts(gSavedSettings, "FSBubblesHideConsoleAndToasts");
    // <FS:Ansariel> [FS communication UI]
    //if((  ( chat_msg.mSourceType == CHAT_SOURCE_AGENT
    //      && useChatBubbles )
    if(  ( chat_msg.mSourceType == CHAT_SOURCE_AGENT
            && useChatBubbles && fsBubblesHideConsoleAndToasts )
    // </FS:Ansariel> [FS communication UI]
        || mChannel.isDead()
        || !mChannel.get()->getShowToasts() )
    // <FS:Ansariel> [FS communication UI]
    //  && nearby_chat->isMessagePaneExpanded())
    // </FS:Ansariel> [FS communication UI]
        // to prevent toasts in Do Not Disturb mode
        return;//no need in toast if chat is visible or if bubble chat is enabled

    // arrange a channel on a screen
    if(!mChannel.get()->getVisible())
    {
        initChannel();
    }

    /*
    //comment all this due to EXT-4432
    ..may clean up after some time...

    //only messages from AGENTS
    if(CHAT_SOURCE_OBJECT == chat_msg.mSourceType)
    {
        if(chat_msg.mChatType == CHAT_TYPE_DEBUG_MSG)
            return;//ok for now we don't skip messeges from object, so skip only debug messages
    }
    */

    LLFloaterIMNearbyChatScreenChannel* channel = dynamic_cast<LLFloaterIMNearbyChatScreenChannel*>(mChannel.get());

    if(channel)
    {
        // Handle IRC styled messages.
        std::string toast_msg;
        if (tmp_chat.mChatStyle == CHAT_STYLE_IRC)
        {
            if (!tmp_chat.mFromName.empty())
            {
                toast_msg += tmp_chat.mFromName;
            }
            toast_msg += tmp_chat.mText.substr(3);
        }
        else
        {
            toast_msg = tmp_chat.mText;
        }

        // <FS:Ansariel> [FS communication UI]
        //bool chat_overlaps = false;
        //if(nearby_chat->getChatHistory())
        //{
        //  LLRect chat_rect = nearby_chat->getChatHistory()->calcScreenRect();
        //  for (std::list<LLView*>::const_iterator child_iter = gFloaterView->getChildList()->begin();
        //       child_iter != gFloaterView->getChildList()->end(); ++child_iter)
        //  {
        //      LLView *view = *child_iter;
        //      const LLRect& rect = view->getRect();
        //      if(view->isInVisibleChain() && (rect.overlaps(chat_rect)))
        //      {
        //          if(!nearby_chat->getChatHistory()->hasAncestor(view))
        //          {
        //              chat_overlaps = true;
        //          }
        //          break;
        //      }
        //  }
        //}
        ////Don't show nearby toast, if conversation is visible and selected
        //if ((nearby_chat->hasFocus()) ||
        //  (LLFloater::isVisible(nearby_chat) && nearby_chat->isTornOff() && !nearby_chat->isMinimized()) ||
        //    ((im_box->getSelectedSession().isNull() && !chat_overlaps &&
        //      ((LLFloater::isVisible(im_box) && !nearby_chat->isTornOff() && !im_box->isMinimized())
        //              || (LLFloater::isVisible(nearby_chat) && nearby_chat->isTornOff() && !nearby_chat->isMinimized())))))
        //{
        //  if(nearby_chat->isMessagePaneExpanded())
        //  {
        //      return;
        //  }
        //}
        if (nearby_chat->getVisible())
        {
            return;
        }
        // </FS:Ansariel> [FS communication UI]

        // <FS:Ansariel> [FS communication UI] [CHUI Merge] Maybe need this later...
        //std::string user_preferences;
        //if (chat_msg.mSourceType == CHAT_SOURCE_OBJECT)
        //{
        //    user_preferences = gSavedSettings.getString("NotificationObjectIMOptions");
        //}
        //else
        //{
        //    user_preferences = gSavedSettings.getString("NotificationNearbyChatOptions");
        //}

        ////Will show toast when chat preference is set
        //if((user_preferences == "toast") || !nearby_chat->isMessagePaneExpanded())
        if (gSavedSettings.getS32("NearbyToastLifeTime") > 0 || gSavedSettings.getS32("NearbyToastFadingTime")) // Ansa: only create toast if it should be visible at all
        // </FS:Ansariel> [FS communication UI]
        {
            // Add a nearby chat toast.
            LLUUID id;
            id.generate();
            chat["id"] = id;
// [RLVa:KB] - Checked: RLVa-1.2.0
            if (RlvActions::isRlvEnabled())
                chat["show_icon_tooltip"] = !chat_msg.mRlvNamesFiltered;
// [/RLVa:KB]
            std::string r_color_name = "White";
            F32 r_color_alpha = 1.0f;
            LLViewerChat::getChatColor( chat_msg, r_color_name, r_color_alpha);

            chat["text_color"] = r_color_name;
            chat["color_alpha"] = r_color_alpha;
            chat["font_size"] = (S32)LLViewerChat::getChatFontSize() ;
            chat["message"] = toast_msg;
            channel->addChat(chat);
        }

    }
}


//-----------------------------------------------------------------------------------------------
// LLFloaterIMNearbyChatToast
//-----------------------------------------------------------------------------------------------

// virtual
void LLFloaterIMNearbyChatToast::onClose(bool app_quitting)
{
    mNearbyChatScreenChannelp->onToastDestroyed(this, app_quitting);
}

// EOF
