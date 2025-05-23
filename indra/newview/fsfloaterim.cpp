/**
 * @file fsfloaterim.cpp
 * @brief FSFloaterIM class definition
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

// Original file: llimfloater.cpp

#include "llviewerprecompiledheaders.h"

#include "fsfloaterim.h"

#include "fschathistory.h"
#include "fschatoptionsmenu.h"
#include "fscommon.h"
#include "fsdata.h"
#include "fsfloaterimcontainer.h" // to replace separate IM Floaters with multifloater container
#include "fsfloaternearbychat.h"
#include "fsnearbychathub.h"    // <FS:Zi> FIRE-24133 - Redirect chat channel messages
#include "fspanelimcontrolpanel.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llautoreplace.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llchannelmanager.h"
#include "llchatentry.h"
#include "llcheckboxctrl.h"
#include "llchiclet.h"
#include "llchicletbar.h"
#include "llconsole.h"
#include "llfloaterabout.h"     // for sysinfo button -Zi
#include "llfloateravatarpicker.h"
#include "llfloateremojipicker.h"
#include "llfloaterreg.h"
#include "llfloatersearchreplace.h"
#include "llgroupactions.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "lllayoutstack.h"
#include "lllogchat.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llnotificationtemplate.h"     // <FS:Zi> Viewer version popup
#include "llpanelemojicomplete.h"
#include "llrootview.h"
#include "llscreenchannel.h"
#include "llspeakers.h"
#include "llsyswellwindow.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lltransientfloatermgr.h"
#include "llversioninfo.h"
#include "llviewerchat.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoicechannel.h"
#include "rlvactions.h"
#include "rlvhandler.h"

#include <boost/regex.hpp>  // <FS:Zi> FIRE-24133 - Redirect chat channel messages

constexpr F32 ME_TYPING_TIMEOUT = 4.0f;
constexpr F32 OTHER_TYPING_TIMEOUT = 9.0f;
constexpr F32 NAME_REFRESH_TIMEOUT = 300.0f;

floater_showed_signal_t FSFloaterIM::sIMFloaterShowedSignal;

FSFloaterIMTimer::FSFloaterIMTimer(FSFloaterIMTimer::callback_t callback) :
    LLEventTimer(0.1f),
    mCallback(callback)
{ }

bool FSFloaterIMTimer::tick()
{
    if (!mCallback.empty())
    {
        mCallback();
    }

    return false;
}


FSFloaterIM::FSFloaterIM(const LLUUID& session_id)
  : LLTransientDockableFloater(NULL, true, session_id),
  LLEventTimer(0.1f),
    mControlPanel(NULL),
    mSessionID(session_id),
    mLastMessageIndex(-1),
    mPendingMessages(0),
    mDialog(IM_NOTHING_SPECIAL),
    mChatHistory(NULL),
    mInputEditor(NULL),
    mSavedTitle(),
    mTypingStart(),
    mShouldSendTypingState(false),
    mMeTyping(false),
    mOtherTyping(false),
    mTypingTimer(),
    mTypingTimeoutTimer(),
    mSessionInitialized(false),
    mChatLayoutPanel(NULL),
    mInputPanels(NULL),
    mChatLayoutPanelHeight(0),
    mAvatarNameCacheConnection(),
    mVoiceChannel(NULL),
    mMeTypingTimer(),
    mOtherTypingTimer(),
    mRefreshNameTimer(),
    mUnreadMessagesNotificationPanel(NULL),
    mUnreadMessagesNotificationTextBox(NULL),
    mApplyRect(true),
    mIMFloaterTimer(NULL)
{
    initIMSession(session_id);

    switch (mDialog)
    {
        case IM_NOTHING_SPECIAL:
        case IM_SESSION_P2P_INVITE:
            mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelIMControl, this);
            break;
        case IM_SESSION_CONFERENCE_START:
            mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelAdHocControl, this);
            break;
        case IM_SESSION_GROUP_START:
            setCanSnooze(true);
            mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelGroupControl, this);
            break;
        case IM_SESSION_INVITE:
            if (gAgent.isInGroup(mSessionID))
            {
                setCanSnooze(true);
                mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelGroupControl, this);
            }
            else
            {
                mFactoryMap["panel_im_control_panel"] = LLCallbackMap(createPanelAdHocControl, this);
            }
            break;
        default: break;
    }

    mCommitCallbackRegistrar.add("IMSession.Menu.Action", boost::bind(&FSFloaterIM::doToSelected, this, _2));
    mEnableCallbackRegistrar.add("IMSession.Menu.Enable", boost::bind(&FSFloaterIM::checkEnabled, this, _2));

    mEnableCallbackRegistrar.add("ChatOptions.Check", boost::bind(&FSFloaterIM::onChatOptionsCheckContextMenuItem, this, _2));
    mCommitCallbackRegistrar.add("ChatOptions.Action", boost::bind(&FSFloaterIM::onChatOptionsContextMenuItemClicked, this, _2));
    mEnableCallbackRegistrar.add("ChatOptions.Visible", boost::bind(&FSFloaterIM::onChatOptionsVisibleContextMenuItem, this, _2));
    mEnableCallbackRegistrar.add("ChatOptions.Enable", boost::bind(&FSFloaterIM::onChatOptionsEnableContextMenuItem, this, _2));

    setOverlapsScreenChannel(true);

    LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::IM, this);

    // only dock when chiclets are visible, or the floater will get stuck in the top left
    // FIRE-9984 -Zi
    bool disable_chiclets = gSavedSettings.getBOOL("FSDisableIMChiclets");
    setDocked(!disable_chiclets);
    // make sure to save position and size with chiclets disabled (torn off floater does that)
    setTornOff(disable_chiclets);

    mRefreshNameTimer.setTimerExpirySec(NAME_REFRESH_TIMEOUT);

    mIMFloaterTimer = new FSFloaterIMTimer(boost::bind(&FSFloaterIM::timedUpdate, this));
}

// virtual
bool FSFloaterIM::focusFirstItem(bool prefer_text_fields, bool focus_flash)
{
    mInputEditor->setFocus(true);
    onTabInto();
    if (focus_flash)
    {
        gFocusMgr.triggerFocusFlash();
    }
    return true;
}

void FSFloaterIM::onFocusLost()
{
    LLIMModel::getInstance()->resetActiveSessionID();

    LLChicletBar::getInstance()->getChicletPanel()->setChicletToggleState(mSessionID, false);
}

void FSFloaterIM::onFocusReceived()
{
    LLIMModel::getInstance()->setActiveSessionID(mSessionID);

    LLChicletBar::getInstance()->getChicletPanel()->setChicletToggleState(mSessionID, true);

    if (getVisible())
    {
        LLIMModel::instance().sendNoUnreadMessages(mSessionID);
    }
}

// virtual
void FSFloaterIM::onClose(bool app_quitting)
{
    mEventTimer.stop();

    setTyping(false);

    // The source of much argument and design thrashing
    // Should the window hide or the session close when the X is clicked?
    //
    // Last change:
    // EXT-3516 X Button should end IM session, _ button should hide


    // AO: Make sure observers are removed on close
    mVoiceChannelStateChangeConnection.disconnect();
    LLVoiceClient::removeObserver((LLVoiceClientStatusObserver*)this);

    //<FS:ND> FIRE-6077 et al: Always clean up observers when the floater dies
    LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
    //</FS:ND> FIRE-6077 et al

    gIMMgr->leaveSession(mSessionID);
}

void FSFloaterIM::onSnooze()
{
    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);

    if (!session)
    {
        LL_WARNS("FSFloaterIM") << "Empty session." << LL_ENDL;
        return;
    }

    bool is_call_with_chat = session->isGroupSessionType()
            || session->isAdHocSessionType() || session->isP2PSessionType();

    LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

    if (is_call_with_chat && voice_channel && voice_channel->isActive())
    {
        LLSD payload;
        payload["session_id"] = mSessionID;
        payload["snooze"] = true;
        LLNotificationsUtil::add("ConfirmLeaveCall", LLSD(), payload, confirmLeaveCallCallback);
        return;
    }

    confirmSnooze();
}

void FSFloaterIM::confirmSnooze()
{
    if (gSavedSettings.getBOOL("FSEnablePerGroupSnoozeDuration"))
    {
        LLSD args;
        args["DURATION"] = gSavedSettings.getS32("GroupSnoozeTime");

        LLNotificationsUtil::add("SnoozeDuration", args, LLSD(), boost::bind(&FSFloaterIM::snoozeDurationCallback, this, _1, _2));
        return;
    }

    snooze();
}

void FSFloaterIM::snoozeDurationCallback(const LLSD& notification, const LLSD& response)
{
    if (S32 option = LLNotificationsUtil::getSelectedOption(notification, response); 0 == option)
    {
        std::istringstream duration_str(response["duration"].asString());
        S32 duration(-1);
        if (duration_str >> duration && duration >= 0)
        {
            snooze(duration);
        }
        else
        {
            LLNotificationsUtil::add("SnoozeDurationInvalidInput");
        }
    }
}

void FSFloaterIM::snooze(S32 duration /*= -1*/)
{
    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);

    if (!session)
    {
        LL_WARNS("FSFloaterIM") << "Empty session." << LL_ENDL;
        return;
    }

    session->mSnoozeTime = duration;
    session->mCloseAction = LLIMModel::LLIMSession::CLOSE_SNOOZE;

    LLFloater::onClickCloseBtn();
}

/* static */
void FSFloaterIM::newIMCallback(const LLSD& data){

    if (data["num_unread"].asInteger() > 0 || data["from_id"].asUUID().isNull())
    {
        LLUUID session_id = data["session_id"].asUUID();

        FSFloaterIM* floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
        if (!floater)
            return;

        // update if visible or max pending messages exceeded, otherwise will be updated when opened
        static LLCachedControl<S32> fsMaxPendingIMMessages(gSavedSettings, "FSMaxPendingIMMessages");
        floater->mPendingMessages++;
        if (floater->getVisible() || floater->mPendingMessages > fsMaxPendingIMMessages)
        {
            floater->updateMessages();
        }
    }
}

void FSFloaterIM::onVisibilityChange(bool new_visibility)
{
    LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

    if (new_visibility && voice_channel &&
        voice_channel->getState() == LLVoiceChannel::STATE_CONNECTED)
    {
        LLFloaterReg::showInstance("voice_call", mSessionID);
    }
    else
    {
        LLFloaterReg::hideInstance("voice_call", mSessionID);
    }
}

void FSFloaterIM::sendMsgFromInputEditor(EChatType type)
{
    if (gAgent.isGodlike()
        || (mDialog != IM_NOTHING_SPECIAL)
        || !mOtherParticipantUUID.isNull())
    {
        // <FS:Techwolf Lupindo> fsdata support
        if (mDialog == IM_NOTHING_SPECIAL && FSData::instance().isSupport(mOtherParticipantUUID) && FSData::instance().isAgentFlag(gAgentID, FSData::NO_SUPPORT))
        {
            return;
        }
        // </FS:Techwolf Lupindo>

        if (mInputEditor)
        {
            LLWString text = mInputEditor->getWText();
            LLWStringUtil::trim(text);
            LLWStringUtil::replaceChar(text,182,'\n'); // Convert paragraph symbols back into newlines.
            if (!text.empty())
            {
                FSCommon::updateUsedEmojis(text);

                if (type == CHAT_TYPE_OOC)
                {
                    std::string tempText = wstring_to_utf8str( text );
                    tempText = gSavedSettings.getString("FSOOCPrefix") + " " + tempText + " " + gSavedSettings.getString("FSOOCPostfix");
                    text = utf8str_to_wstring(tempText);
                }

                // Truncate and convert to UTF8 for transport
                std::string utf8_text = wstring_to_utf8str(text);

                // <FS:Zi> FIRE-24133 - Redirect chat channel messages
                if (boost::regex_match(utf8_text.c_str(), boost::regex("/-{0,1}[0-9].*")))
                {
                    // message starts with a / and a valid channel number, so redirect it to a chat channel
                    FSNearbyChat::instance().sendChatFromViewer(text, CHAT_TYPE_NORMAL, false);

                    // clean out the text box and typing indicator, which we wouldn't reach otherwise
                    mInputEditor->setText(LLStringUtil::null);
                    setTyping(false);

                    return;
                }

                // Convert OOC and MU* style poses
                utf8_text = FSCommon::applyAutoCloseOoc(utf8_text);
                utf8_text = FSCommon::applyMuPose(utf8_text);

                // <FS:Techwolf Lupindo> Support group chat prefix
                static LLCachedControl<bool> chat_prefix_support(gSavedSettings, "FSSupportGroupChatPrefix3");
                static LLCachedControl<bool> chat_prefix_testing(gSavedSettings, "FSSupportGroupChatPrefixTesting");
                if ((chat_prefix_support || chat_prefix_testing) && FSData::getInstance()->isFirestormGroup(mSessionID))
                {
                    // <FS:PP> FIRE-7075: Skin indicator
                    static LLCachedControl<std::string> FSInternalSkinCurrent(gSavedSettings, "FSInternalSkinCurrent");
                    std::string skin_indicator(FSInternalSkinCurrent);
                    LLStringUtil::toLower(skin_indicator);
                    if (skin_indicator == "starlight cui")
                    {
                        skin_indicator = "sc"; // Separate "s" (StarLight) from "sc" (StarLight CUI)
                    }
                    else
                    {
                        skin_indicator = skin_indicator.substr(0, 1); // "FS 4.4.1f os", "FS 4.4.1v", "FS 4.4.1a", "FS 4.4.1s os", "FS 4.4.1m os" etc.
                    }
                    // </FS:PP>

                    //Address size check
#if ADDRESS_SIZE == 32
                    std::string str_address_size_tag = "32";
#else
                    std::string str_address_size_tag = "";
#endif
#ifdef USE_AVX2_OPTIMIZATION
                    std::string str_optimization_tag = "+";
#else
                    std::string str_optimization_tag = "-";
#endif
                    //OpenSim check
                    std::string str_opensim_tag;
#ifdef OPENSIM
                    str_opensim_tag = " os";
#endif

                    //Operating System check
#if LL_WINDOWS
                    std::string str_operating_system_tag = "W";
#elif LL_LINUX
                    std::string str_operating_system_tag = "L";
#elif LL_DARWIN
                    std::string str_operating_system_tag = "M";
#endif

                    //RLV check
                    std::string str_rlv_enabled = "";
                    if (RlvHandler::isEnabled())
                        str_rlv_enabled = "*";

                    // Text mode check
                    std::string str_viewer_mode = "";

                    // Unfortunately, we have to cheat a little here. Ideally we'd have
                    // a method defined to check if the viewer is running in Text Mode.
                    // For now, we will use the same method as used in llappviewer.cpp(LLAppViewer::getViewerInfo())
                    static LLCachedControl<std::string> FSViewerMode(gSavedSettings, "SessionSettingsFile");
                    std::string viewer_mode(FSViewerMode);
                    LLStringUtil::toLower(viewer_mode);
                    if (viewer_mode == "settings_text.xml")
                        str_viewer_mode = "T";

                    //Build it up
                    size_t insert_pos = FSCommon::is_irc_me_prefix(utf8_text) ? 4 : 0;

                    //For testing/beta groups, we display the build version since it doesn't speed by and this might change often
                    if (FSData::getInstance()->isTestingGroup(mSessionID))
                    {
                        if(chat_prefix_testing)
                        {
                            auto viewer_maturity = LLVersionInfo::getInstance()->getFSViewerMaturity();
                            std::string str_version_tag;
                            if( viewer_maturity == LLVersionInfo::FSViewerMaturity::RELEASE_VIEWER )
                            {
                                str_version_tag = "Release";
                            }
                            else if( viewer_maturity == LLVersionInfo::FSViewerMaturity::STREAMING_VIEWER )
                            {
                                str_version_tag = "Streaming";
                            }
                            else if( viewer_maturity == LLVersionInfo::FSViewerMaturity::UNOFFICIAL_VIEWER )
                            {
                                str_version_tag = "Unofficial";
                            }
                            else// In testing groups we'll allow all non-release recognised channels.
                            {
                                str_version_tag = LLVersionInfo::getInstance()->getBuildVersion();
                            }
                            utf8_text.insert(insert_pos, ("(" + str_address_size_tag + str_operating_system_tag + str_optimization_tag + " " + str_version_tag + " " + skin_indicator + str_viewer_mode + str_rlv_enabled + str_opensim_tag + ") "));
                        }

                    }
                    //For release support groups, only display the short version(Major.Minor.Patch) since chat can speed by. This makes it easier on Support's eyes.
                    else if(FSData::getInstance()->isSupportGroup(mSessionID))
                    {
                        if(chat_prefix_support)
                        {
                            std::string str_version_tag;
                            auto viewer_maturity = LLVersionInfo::getInstance()->getFSViewerMaturity();
                            if( viewer_maturity == LLVersionInfo::FSViewerMaturity::UNOFFICIAL_VIEWER )
                            {
                                str_version_tag = "Unofficial";
                            }
                            if( viewer_maturity == LLVersionInfo::FSViewerMaturity::STREAMING_VIEWER )
                            {
                                str_version_tag = "Streaming";
                            }
                            else if( viewer_maturity != LLVersionInfo::FSViewerMaturity::RELEASE_VIEWER )
                            {
                                str_version_tag = "pre-Release";
                            }
                            else
                            {
                                str_version_tag = LLVersionInfo::getInstance()->getShortVersion();
                            }
                            utf8_text.insert(insert_pos, ("(" + str_address_size_tag + str_operating_system_tag + str_optimization_tag + " " + str_version_tag + " " + skin_indicator + str_viewer_mode + str_rlv_enabled + str_opensim_tag + ") "));
                        }
                    }
                }

                // <FS:Techwolf Lupindo> Allow user to send system info.
                if (mDialog == IM_NOTHING_SPECIAL && utf8_text.find("/sysinfo") == 0)
                {
                    LLSD system_info = FSData::getSystemInfo();
                    utf8_text = system_info["Part1"].asString() + system_info["Part2"].asString();
                }
                // </FS:Techwolf Lupindo>

                sendMsg(utf8_text);

                mInputEditor->setText(LLStringUtil::null);
            }
        }
    }
    else
    {
        LL_INFOS("FSFloaterIM") << "Cannot send IM to everyone unless you're a god." << LL_ENDL;
    }

    setTyping(false);
}

void FSFloaterIM::onChatSearchButtonClicked()
{
    LLFloaterSearchReplace::show(mChatHistory);
}

void FSFloaterIM::sendMsg(const std::string& msg)
{
    //  const std::string utf8_text = utf8str_truncate(msg, MAX_MSG_BUF_SIZE - 1);
    // [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
    // <FS:CR> Don't truncate our messages, they're broken up as part of FIRE-787
    //std::string utf8_text = utf8str_truncate(msg, MAX_MSG_BUF_SIZE - 1);
    std::string utf8_text = msg;
    // </FS:CR>

    if ( (RlvActions::hasBehaviour(RLV_BHVR_SENDIM)) || (RlvActions::hasBehaviour(RLV_BHVR_SENDIMTO)) )
    {
        const LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
        RLV_ASSERT(pIMSession);

        bool fRlvFilter = !pIMSession;
        if (pIMSession)
        {
            switch (pIMSession->mSessionType)
            {
                case LLIMModel::LLIMSession::P2P_SESSION:   // One-on-one IM
                    fRlvFilter = !RlvActions::canSendIM(mOtherParticipantUUID);
                    break;
                case LLIMModel::LLIMSession::GROUP_SESSION: // Group chat
                    fRlvFilter = !RlvActions::canSendIM(mSessionID);
                    break;
                case LLIMModel::LLIMSession::ADHOC_SESSION: // Conference chat: allow if all participants can be sent an IM
                {
                    if (!pIMSession->mSpeakers)
                    {
                        fRlvFilter = true;
                        break;
                    }

                    LLSpeakerMgr::speaker_list_t speakers;
                    pIMSession->mSpeakers->getSpeakerList(&speakers, true);
                    for (const auto& pSpeaker : speakers)
                    {
                        if ( (gAgent.getID() != pSpeaker->mID) && (!RlvActions::canSendIM(pSpeaker->mID)) )
                        {
                            fRlvFilter = true;
                            break;
                        }
                    }
                }
                    break;
                default:
                    fRlvFilter = true;
                    break;
            }
        }

        if (fRlvFilter)
        {
            utf8_text = RlvStrings::getString(RlvStringKeys::Blocked::SendIm);
        }
    }
    // [/RLVa:KB]

    if (mSessionInitialized)
    {
        LLIMModel::sendMessage(utf8_text, mSessionID, mOtherParticipantUUID, mDialog);
    }
    else
    {
        //queue up the message to send once the session is initialized
        mQueuedMsgsForInit.append(utf8_text);
    }

    updateMessages();
}

FSFloaterIM::~FSFloaterIM()
{
    delete mIMFloaterTimer;

    mEventTimer.stop();

    LLTransientFloaterMgr::getInstance()->removeControlView(LLTransientFloaterMgr::IM, (LLView*)this);
    mVoiceChannelStateChangeConnection.disconnect();
    LLVoiceClient::removeObserver((LLVoiceClientStatusObserver*)this);

    LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
    if ((pIMSession) && (pIMSession->mSessionType == LLIMModel::LLIMSession::P2P_SESSION))
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
    }

    // Clean up any stray name cache connections
    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }

    if (mRecentEmojisUpdatedCallbackConnection.connected())
    {
        mRecentEmojisUpdatedCallbackConnection.disconnect();
    }
}

void FSFloaterIM::onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state)
{
    LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::onVoiceChannelStateChanged" << LL_ENDL;
    updateButtons(new_state >= LLVoiceChannel::STATE_CALL_STARTED);
}

void FSFloaterIM::doToSelected(const LLSD& userdata)
{
    const std::string command = userdata.asString();

    if (command == "offer_tp")
    {
        LLAvatarActions::offerTeleport(mOtherParticipantUUID);
    }
    else if (command == "request_tp")
    {
        LLAvatarActions::teleportRequest(mOtherParticipantUUID);
    }
    else if (command == "share")
    {
        LLAvatarActions::share(mOtherParticipantUUID);
    }
    else if (command == "add_participant")
    {
        onAddButtonClicked();
    }
    else if (command == "pay")
    {
        LLAvatarActions::pay(mOtherParticipantUUID);
    }
    else if (command == "show_profile")
    {
        LLAvatarActions::showProfile(mOtherParticipantUUID);
    }
    else if (command == "group_info")
    {
        LLGroupActions::show(mSessionID);
    }
    else if (command == "call")
    {
        gIMMgr->startCall(mSessionID);
    }
    else if (command == "end_call")
    {
        gIMMgr->endCall(mSessionID);
    }
    else if (command == "volume")
    {
        LLFloaterReg::showInstance("fs_voice_controls");
    }
    else if (command == "add_friend")
    {
        LLAvatarActions::requestFriendshipDialog(mOtherParticipantUUID);
    }
    else if (command == "history")
    {
        if (gSavedSettings.getBOOL("FSUseBuiltInHistory"))
        {
            LLFloaterReg::showInstance("preview_conversation", mSessionID, true);
        }
        else
        {
            gViewerWindow->getWindow()->openFile(LLLogChat::makeLogFileName(LLIMModel::instance().getHistoryFileName(mSessionID)));
        }
    }
    else
    {
        LL_WARNS("FSFloaterIM") << "Unhandled command '" << command << "'. Ignoring." << LL_ENDL;
    }
}

bool FSFloaterIM::checkEnabled(const LLSD& userdata)
{
    const std::string command = userdata.asString();

    if (command == "enable_offer_tp")
    {
        return LLAvatarActions::canOfferTeleport(mOtherParticipantUUID);
    }
    return false;
}

void FSFloaterIM::onChatOptionsContextMenuItemClicked(const LLSD& userdata)
{
    FSChatOptionsMenu::onMenuItemClick(userdata, this);
}

bool FSFloaterIM::onChatOptionsCheckContextMenuItem(const LLSD& userdata)
{
    return FSChatOptionsMenu::onMenuItemCheck(userdata, this);
}

bool FSFloaterIM::onChatOptionsVisibleContextMenuItem(const LLSD& userdata)
{
    return FSChatOptionsMenu::onMenuItemVisible(userdata, this);
}

bool FSFloaterIM::onChatOptionsEnableContextMenuItem(const LLSD& userdata)
{
    return FSChatOptionsMenu::onMenuItemEnable(userdata, this);
}

// support sysinfo button -Zi
void FSFloaterIM::onSysinfoButtonClicked()
{
    LLSD system_info = FSData::getSystemInfo();
    LLSD args;
    args["SYSINFO"] = system_info["Part1"].asString() + system_info["Part2"].asString();
    args["Part1"] = system_info["Part1"];
    args["Part2"] = system_info["Part2"];
    LLNotificationsUtil::add("SendSysinfoToIM",args,LLSD(),boost::bind(&FSFloaterIM::onSendSysinfo,this,_1,_2));
}

bool FSFloaterIM::onSendSysinfo(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification,response);

    if (option == 0)
    {
        std::string part1 = notification["substitutions"]["Part1"];
        std::string part2 = notification["substitutions"]["Part2"];
        if (mSessionInitialized)
        {
            LLIMModel::sendMessage(part1, mSessionID, mOtherParticipantUUID, mDialog);
            LLIMModel::sendMessage(part2, mSessionID, mOtherParticipantUUID, mDialog);
        }
        else
        {
            //queue up the message to send once the session is initialized
            mQueuedMsgsForInit.append(part1);
            mQueuedMsgsForInit.append(part2);
        }
        return true;
    }
    return false;
}

void FSFloaterIM::onSysinfoButtonVisibilityChanged(const LLSD& yes)
{
    getChild<LLUICtrl>("send_sysinfo_btn_panel")->setVisible(yes.asBoolean() /* && mIsSupportIM */);
}
// support sysinfo button -Zi

void FSFloaterIM::onChange(EStatusType status, const LLSD& channelInfo, bool proximal)
{
    if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
    {
        return;
    }

    updateCallButton();
}

void FSFloaterIM::updateCallButton()
{
    // hide/show call button
    bool voice_enabled = LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();
    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);

    if (!session)
    {
        getChild<LLButton>("call_btn")->setEnabled(false);
        return;
    }

    bool session_initialized = session->mSessionInitialized;
    bool callback_enabled = session->mCallBackEnabled;

    bool enable_connect = session_initialized
    && voice_enabled
    && callback_enabled;

    getChild<LLButton>("call_btn")->setEnabled(enable_connect);
}

void FSFloaterIM::updateButtons(bool is_call_started)
{
    LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::updateButtons" << LL_ENDL;
    getChildView("end_call_btn_panel")->setVisible( is_call_started);
    getChildView("voice_ctrls_btn_panel")->setVisible( is_call_started);
    getChildView("call_btn_panel")->setVisible( ! is_call_started);
    updateCallButton();
}

void FSFloaterIM::changed(U32 mask)
{
    LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::changed(U32 mask)" << LL_ENDL;

    if(LLAvatarActions::isFriend(mOtherParticipantUUID))
    {
        bool is_online = LLAvatarTracker::instance().isBuddyOnline(mOtherParticipantUUID);
        getChild<LLButton>("teleport_btn")->setEnabled(is_online);
        getChild<LLButton>("call_btn")->setEnabled(is_online);
        getChild<LLButton>("add_friend_btn")->setEnabled(false);
    }
    else
    {
        // If friendship dissolved, enable buttons by default because we don't
        // know about their online status anymore
        getChild<LLButton>("teleport_btn")->setEnabled(true);
        getChild<LLButton>("call_btn")->setEnabled(true);
        getChild<LLButton>("add_friend_btn")->setEnabled(true);
    }
}

// </AO> Callbacks for llimcontrol panel, merged into this floater

//virtual
bool FSFloaterIM::postBuild()
{
    const LLUUID& other_party_id = LLIMModel::getInstance()->getOtherParticipantID(mSessionID);
    if (other_party_id.notNull())
    {
        mOtherParticipantUUID = other_party_id;
    }

    mControlPanel->setSessionId(mSessionID);

    // AO: always hide the control panel to start.
    LL_DEBUGS("FSFloaterIM") << "mControlPanel->getParent()" << mControlPanel->getParent() << LL_ENDL;
    mControlPanel->getParent()->setVisible(false);

    LL_DEBUGS("FSFloaterIM") << "buttons setup in IM start" << LL_ENDL;

    LLButton* button = getChild<LLButton>("slide_left_btn");
    button->setVisible(mControlPanel->getParent()->getVisible());
    button->setClickedCallback(boost::bind(&FSFloaterIM::onSlide, this));

    button = getChild<LLButton>("slide_right_btn");
    button->setVisible(!mControlPanel->getParent()->getVisible());
    button->setClickedCallback(boost::bind(&FSFloaterIM::onSlide, this));

    // support sysinfo button -Zi
    mSysinfoButton = getChild<LLButton>("send_sysinfo_btn");
    onSysinfoButtonVisibilityChanged(false);

    // type-specfic controls
    LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
    if (pIMSession)
    {
        switch (pIMSession->mSessionType)
        {
            case LLIMModel::LLIMSession::P2P_SESSION:   // One-on-one IM
            {
                LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::P2P_SESSION" << LL_ENDL;
                getChild<LLLayoutPanel>("slide_panel")->setVisible(false);
                getChild<LLLayoutPanel>("gprofile_panel")->setVisible(false);
                getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
                getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);

                LL_DEBUGS("FSFloaterIM") << "adding FSFloaterIM removing/adding particularfriendobserver" << LL_ENDL;
                LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);
                LLAvatarTracker::instance().addParticularFriendObserver(mOtherParticipantUUID, this);

                // Disable "Add friend" button for friends.
                LL_DEBUGS("FSFloaterIM") << "add_friend_btn check start" << LL_ENDL;
                getChild<LLButton>("add_friend_btn")->setEnabled(!LLAvatarActions::isFriend(mOtherParticipantUUID));

                // Disable "Teleport" button if friend is offline
                if(LLAvatarActions::isFriend(mOtherParticipantUUID))
                {
                    LL_DEBUGS("FSFloaterIM") << "LLAvatarActions::isFriend - tp button" << LL_ENDL;
                    getChild<LLButton>("teleport_btn")->setEnabled(LLAvatarTracker::instance().isBuddyOnline(mOtherParticipantUUID));
                }

                // support sysinfo button -Zi
                mSysinfoButton->setClickedCallback(boost::bind(&FSFloaterIM::onSysinfoButtonClicked, this));
                // this needs to be extended to fsdata awareness, once we have it. -Zi
                // mIsSupportIM=fsdata(partnerUUID).isSupport(); // pseudocode something like this
                onSysinfoButtonVisibilityChanged(gSavedSettings.getBOOL("SysinfoButtonInIM"));
                gSavedSettings.getControl("SysinfoButtonInIM")->getCommitSignal()->connect(boost::bind(&FSFloaterIM::onSysinfoButtonVisibilityChanged, this, _2));
                // support sysinfo button -Zi

                break;
            }
            case LLIMModel::LLIMSession::GROUP_SESSION: // Group chat
            {
                LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::GROUP_SESSION start" << LL_ENDL;
                getChild<LLLayoutPanel>("profile_panel")->setVisible(false);
                getChild<LLLayoutPanel>("friend_panel")->setVisible(false);
                getChild<LLLayoutPanel>("tp_panel")->setVisible(false);
                getChild<LLLayoutPanel>("share_panel")->setVisible(false);
                getChild<LLLayoutPanel>("pay_panel")->setVisible(false);
                getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
                getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
                getChild<LLLayoutPanel>("add_participant_panel")->setVisible(false);

                LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::GROUP_SESSION end" << LL_ENDL;
                break;
            }
            case LLIMModel::LLIMSession::ADHOC_SESSION: // Conference chat
            {
                LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::ADHOC_SESSION  start" << LL_ENDL;
                getChild<LLLayoutPanel>("profile_panel")->setVisible(false);
                getChild<LLLayoutPanel>("gprofile_panel")->setVisible(false);
                getChild<LLLayoutPanel>("friend_panel")->setVisible(false);
                getChild<LLLayoutPanel>("tp_panel")->setVisible(false);
                getChild<LLLayoutPanel>("share_panel")->setVisible(false);
                getChild<LLLayoutPanel>("pay_panel")->setVisible(false);
                getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
                getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
                LL_DEBUGS("FSFloaterIM") << "LLIMModel::LLIMSession::ADHOC_SESSION end" << LL_ENDL;
                break;
            }
            default:
                LL_DEBUGS("FSFloaterIM") << "default buttons start" << LL_ENDL;
                getChild<LLLayoutPanel>("end_call_btn_panel")->setVisible(false);
                getChild<LLLayoutPanel>("voice_ctrls_btn_panel")->setVisible(false);
                LL_DEBUGS("FSFloaterIM") << "default buttons end" << LL_ENDL;
                break;
        }
    }
    mVoiceChannel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);
    if(mVoiceChannel)
    {
        LL_DEBUGS("FSFloaterIM") << "voice_channel start" << LL_ENDL;
        mVoiceChannelStateChangeConnection = mVoiceChannel->setStateChangedCallback(boost::bind(&FSFloaterIM::onVoiceChannelStateChanged, this, _1, _2));

        //call (either p2p, group or ad-hoc) can be already in started state
        updateButtons(mVoiceChannel->getState() >= LLVoiceChannel::STATE_CALL_STARTED);
        LL_DEBUGS("FSFloaterIM") << "voice_channel end" << LL_ENDL;
    }
    LLVoiceClient::addObserver((LLVoiceClientStatusObserver*)this);

    // </AO>

    mInputEditor = getChild<LLChatEntry>("chat_editor");
    mChatHistory = getChild<FSChatHistory>("chat_history");
    mChatLayoutPanel = getChild<LLLayoutPanel>("chat_layout_panel");
    mInputPanels = getChild<LLLayoutStack>("input_panels");
    mChatLayoutPanelHeight = mChatLayoutPanel->getRect().getHeight();
    mInputEditorPad = mChatLayoutPanelHeight - mInputEditor->getRect().getHeight();

    mUnreadMessagesNotificationPanel = getChild<LLLayoutPanel>("unread_messages_holder");
    mUnreadMessagesNotificationTextBox = getChild<LLTextBox>("unread_messages_text");
    mChatHistory->setUnreadMessagesUpdateCallback(boost::bind(&FSFloaterIM::updateUnreadMessageNotification, this, _1));

    mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
    mInputEditor->setFocusReceivedCallback(boost::bind(&FSFloaterIM::onInputEditorFocusReceived, this));
    mInputEditor->setFocusLostCallback(boost::bind(&FSFloaterIM::onInputEditorFocusLost, this));
    mInputEditor->setKeystrokeCallback(boost::bind(&FSFloaterIM::onInputEditorKeystroke, this));
    mInputEditor->setTextExpandedCallback(boost::bind(&FSFloaterIM::reshapeChatLayoutPanel, this));
    mInputEditor->setCommitOnFocusLost(false);
    mInputEditor->setPassDelete(true);
    mInputEditor->setFont(LLViewerChat::getChatFont());
    mInputEditor->enableSingleLineMode(gSavedSettings.getBOOL("FSUseSingleLineChatEntry"));
    mInputEditor->setCommitCallback(boost::bind(&FSFloaterIM::sendMsgFromInputEditor, this, CHAT_TYPE_NORMAL));

    mEmojiRecentPanelToggleBtn = getChild<LLButton>("emoji_recent_panel_toggle_btn");
    mEmojiRecentPanelToggleBtn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onEmojiRecentPanelToggleBtnClicked(); });

    mEmojiRecentPanel = getChild<LLLayoutPanel>("emoji_recent_layout_panel");
    mEmojiRecentPanel->setVisible(false);

    mEmojiRecentEmptyText = getChild<LLTextBox>("emoji_recent_empty_text");
    mEmojiRecentEmptyText->setToolTip(mEmojiRecentEmptyText->getText());
    mEmojiRecentEmptyText->setVisible(false);

    mEmojiRecentIconsCtrl = getChild<LLPanelEmojiComplete>("emoji_recent_icons_ctrl");
    mEmojiRecentIconsCtrl->setCommitCallback([this](LLUICtrl*, const LLSD& value) { onRecentEmojiPicked(value); });
    mEmojiRecentIconsCtrl->setVisible(false);

    static bool usePrettyEmojiButton = gSavedSettings.getBOOL( "FSUsePrettyEmojiButton" );
    static bool useBWEmojis = gSavedSettings.getBOOL( "FSUseBWEmojis" );
    mEmojiPickerToggleBtn = getChild<LLButton>("emoji_picker_toggle_btn");
    if (usePrettyEmojiButton)
    {
        static auto emoji_btn_char = gSavedSettings.getU32("FSPrettyEmojiButtonCode");
        mEmojiPickerToggleBtn->setImageOverlay(LLUUID::null);
        mEmojiPickerToggleBtn->setFont(LLFontGL::getFontEmojiLarge(useBWEmojis));
        mEmojiPickerToggleBtn->setLabel(LLUIString(LLWString(1, emoji_btn_char)));
    }
    else
    {
        mEmojiPickerToggleBtn->setLabel(LLUIString(""));
        mEmojiPickerToggleBtn->setImageOverlay("Emoji_Picker_Icon");
    }
    mEmojiPickerToggleBtn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onEmojiPickerToggleBtnClicked(); });

    mRecentEmojisUpdatedCallbackConnection = LLFloaterEmojiPicker::setRecentEmojisUpdatedCallback([this](const std::list<llwchar>& recent_emojis_list) { initEmojiRecentPanel(); });

    getChild<LLButton>("send_chat")->setCommitCallback(boost::bind(&FSFloaterIM::sendMsgFromInputEditor, this, CHAT_TYPE_NORMAL));
    getChild<LLButton>("chat_search_btn")->setCommitCallback(boost::bind(&FSFloaterIM::onChatSearchButtonClicked, this));

    bool isFSSupportGroup = FSData::getInstance()->isFirestormGroup(mSessionID);
    bool isFSTestingGroup = FSData::getInstance()->isTestingGroup(mSessionID);

    //We can show the testing group button simply by checking testing group
    childSetVisible("testing_panel", isFSTestingGroup);
    //But we cannot with the support group button, because testing groups are also support groups
    childSetVisible("support_panel", isFSSupportGroup && !isFSTestingGroup);

    // <FS:Zi> Viewer version popup
    if (isFSSupportGroup || isFSTestingGroup)
    {
        // check if the dialog was set to ignore
        LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate("FirstJoinSupportGroup2");
        if (!templatep.get()->mForm->getIgnored())
        {
            // if not, give the user a choice, whether to enable the version prefix or not
            LLSD args;
            LLNotificationsUtil::add("FirstJoinSupportGroup2", args, LLSD(), boost::bind(&FSFloaterIM::enableViewerVersionCallback, this, _1, _2));
        }
    }
    // </FS:Zi> Viewer version popup

    // only dock when chiclets are visible, or the floater will get stuck in the top left
    // FIRE-9984 -Zi
    bool disable_chiclets = gSavedSettings.getBOOL("FSDisableIMChiclets");
    setDocked(!disable_chiclets);

    mTypingStart = LLTrans::getString("IM_typing_start_string");

    // Disable input editor if session cannot accept text
    LLIMModel::LLIMSession* im_session =
        LLIMModel::instance().findIMSession(mSessionID);
    if (im_session && !im_session->mTextIMPossible)
    {
        mInputEditor->setEnabled(false);
        mInputEditor->setLabel(LLTrans::getString("IM_unavailable_text_label"));
    }

    if (im_session && im_session->isP2PSessionType())
    {
        mTypingStart.setArg("[NAME]", im_session->mName);
        updateSessionName(im_session->mName, im_session->mName);
        fetchAvatarName(im_session->mOtherParticipantID);
    }
    else
    {
        std::string session_name(LLIMModel::instance().getName(mSessionID));
        updateSessionName(session_name, session_name);
    }

    //*TODO if session is not initialized yet, add some sort of a warning message like "starting session...blablabla"
    //see LLFloaterIMPanel for how it is done (IB)

    // don't call dockable floater functions when chiclets are disabled, it will dock the floater
    // FIRE-9984 -Zi
    if (isChatMultiTab() || disable_chiclets)
    {
        return LLFloater::postBuild();
    }
    else
    {
        return LLDockableFloater::postBuild();
    }
}

void FSFloaterIM::updateSessionName(const std::string& ui_title,
                                    const std::string& ui_label)
{
    // <FS:Ansariel> FIRE-7874: Name is missing on tab if announcing incoming IMs is enabled and sender's name is not in name cache
    mSavedTitle = ui_title;

    mInputEditor->setLabel(LLTrans::getString("IM_to_label") + " " + ui_label);
    setTitle(ui_title);
}

void FSFloaterIM::fetchAvatarName(LLUUID& agent_id)
{
    if (agent_id.notNull())
    {
        if (mAvatarNameCacheConnection.connected())
        {
            mAvatarNameCacheConnection.disconnect();
        }
        mAvatarNameCacheConnection = LLAvatarNameCache::get(agent_id,
                                                            boost::bind(&FSFloaterIM::onAvatarNameCache, this, _1, _2));
    }
}

void FSFloaterIM::onAvatarNameCache(const LLUUID& agent_id,
                                    const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();

    std::string name = av_name.getCompleteName();
    if (LLAvatarName::useDisplayNames())
    {
        switch (gSavedSettings.getS32("FSIMTabNameFormat"))
        {
            // Display name
            case 0:
                name = av_name.getDisplayName();
                break;
            // Username
            case 1:
                name = av_name.getUserNameForDisplay();
                break;
            // Display name (username)
            case 2:
                // Do nothing - we already set the complete name as default
                break;
            // Username (display name)
            case 3:
                if (av_name.isDisplayNameDefault())
                {
                    name = av_name.getUserNameForDisplay();
                }
                else
                {
                    name = av_name.getUserNameForDisplay() + " (" + av_name.getDisplayName() + ")";
                }
                break;
            default:
                // Do nothing - we already set the complete name as default
                break;
        }
    }

    updateSessionName(name, name);
    mTypingStart.setArg("[NAME]", name);
    if (mOtherTyping)
    {
        setTitle((gSavedSettings.getBOOL("FSTypingChevronPrefix") ? "> " : "") + mTypingStart.getString());
    }
    LL_DEBUGS("FSFloaterIM") << "Setting IM tab name to '" << name << "'" << LL_ENDL;
}

void FSFloaterIM::timedUpdate()
{
    if (mMeTyping)
    {
        // Send an additional Start Typing packet every ME_TYPING_TIMEOUT seconds
        if (mMeTypingTimer.getElapsedTimeF32() > ME_TYPING_TIMEOUT && false == mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL)
        {
            LL_DEBUGS("TypingMsgs") << "Send additional Start Typing packet" << LL_ENDL;
            LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, true);
            mMeTypingTimer.reset();
        }

        // Time out if user hasn't typed for a while.
        if (mTypingTimeoutTimer.getElapsedTimeF32() > LLAgent::TYPING_TIMEOUT_SECS)
        {
            setTyping(false);
            LL_DEBUGS("TypingMsgs") << "Send stop typing due to timeout" << LL_ENDL;
        }
    }

    // Clear <name is typing> message if no data received for OTHER_TYPING_TIMEOUT seconds
    if (mOtherTyping && mOtherTypingTimer.getElapsedTimeF32() > OTHER_TYPING_TIMEOUT)
    {
        LL_DEBUGS("TypingMsgs") << "Received: is typing cleared due to timeout" << LL_ENDL;
        removeTypingIndicator();
        mOtherTyping = false;
    }

    if (mRefreshNameTimer.checkExpirationAndReset(NAME_REFRESH_TIMEOUT))
    {
        LLIMModel::LLIMSession* im_session = LLIMModel::instance().findIMSession(mSessionID);
        if (im_session && im_session->isP2PSessionType())
        {
            fetchAvatarName(im_session->mOtherParticipantID);
        }
    }
}

// static
void* FSFloaterIM::createPanelIMControl(void* userdata)
{
    FSFloaterIM *self = (FSFloaterIM*)userdata;
    self->mControlPanel = new FSPanelIMControlPanel();
    self->mControlPanel->setXMLFilename("panel_fs_im_control_panel.xml");
    return self->mControlPanel;
}


// static
void* FSFloaterIM::createPanelGroupControl(void* userdata)
{
    FSFloaterIM *self = (FSFloaterIM*)userdata;
    self->mControlPanel = new FSPanelGroupControlPanel(self->mSessionID);
    self->mControlPanel->setXMLFilename("panel_fs_group_control_panel.xml");
    return self->mControlPanel;
}

// static
void* FSFloaterIM::createPanelAdHocControl(void* userdata)
{
    FSFloaterIM *self = (FSFloaterIM*)userdata;
    self->mControlPanel = new FSPanelAdHocControlPanel(self->mSessionID);
    self->mControlPanel->setXMLFilename("panel_fs_adhoc_control_panel.xml");
    return self->mControlPanel;
}

void FSFloaterIM::onSlide()
{
    mControlPanel->getParent()->setVisible(!mControlPanel->getParent()->getVisible());

    gSavedSettings.setBOOL("IMShowControlPanel", mControlPanel->getParent()->getVisible());

    getChild<LLButton>("slide_left_btn")->setVisible(mControlPanel->getParent()->getVisible());
    getChild<LLButton>("slide_right_btn")->setVisible(!mControlPanel->getParent()->getVisible());
}

//static
FSFloaterIM* FSFloaterIM::show(const LLUUID& session_id)
{
    closeHiddenIMToasts();

    if (!gIMMgr->hasSession(session_id))
        return nullptr;

    // <AS:chanayane> [FIRE-34494] fixes unable to open an IM with someone who started a group chat
    // Prevent showing non-IM sessions in FSFloaterIM::show()
    LLIMModel::LLIMSession* session = LLIMModel::getInstance()->findIMSession(session_id);
    if (!session || ( 
           IM_NOTHING_SPECIAL          != session->mType
        && IM_SESSION_P2P_INVITE       != session->mType
        && IM_SESSION_INVITE           != session->mType
        && IM_SESSION_CONFERENCE_START != session->mType
        && IM_SESSION_GROUP_START      != session->mType))
    {
        LL_WARNS("IMVIEW") << "Attempted to show FSFloaterIM for non-IM session: "
                        << (session ? std::to_string(session->mType) : "null") << LL_ENDL;
        return nullptr;
    }
    // </AS:chanayane>

    if (!isChatMultiTab())
    {
        //hide all
        LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
        for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
             iter != inst_list.end(); ++iter)
        {
            FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
            if (floater && floater->isDocked())
            {
                floater->setVisible(false);
            }
        }
    }

    bool exist = findInstance(session_id);

    FSFloaterIM* floater = getInstance(session_id);
    if (!floater)
    {
        return nullptr;
    }

    if (isChatMultiTab())
    {
        FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();

        // do not add existed floaters to avoid adding torn off instances
        if (!exist)
        {
            //      LLTabContainer::eInsertionPoint i_pt = user_initiated ? LLTabContainer::RIGHT_OF_CURRENT : LLTabContainer::END;
            // TODO: mantipov: use LLTabContainer::RIGHT_OF_CURRENT if it exists
            LLTabContainer::eInsertionPoint i_pt = LLTabContainer::END;

            if (floater_container)
            {
                floater_container->addFloater(floater, true, i_pt);
            }
        }

        floater->mApplyRect = false;
        floater->openFloater(floater->getKey());
        floater->mApplyRect = true;
        floater->setFocus(true);
    }
    else
    {
        // Docking may move chat window, hide it before moving, or user will see how window "jumps"
        floater->setVisible(false);

        // only dock when chiclets are visible, or the floater will get stuck in the top left
        // FIRE-9984 -Zi
        if (floater->getDockControl() == NULL && !gSavedSettings.getBOOL("FSDisableIMChiclets"))
        {
            LLChiclet* chiclet =
                    LLChicletBar::getInstance()->getChicletPanel()->findChiclet<LLChiclet>(
                            session_id);
            if (chiclet == NULL)
            {
                LL_ERRS() << "Dock chiclet for FSFloaterIM doesn't exists" << LL_ENDL;
            }
            else
            {
                LLChicletBar::getInstance()->getChicletPanel()->scrollToChiclet(chiclet);
            }

            // <FS:Ansariel> Group notices, IMs and chiclets position
            //floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
            //      LLDockControl::BOTTOM));
            if (gSavedSettings.getBOOL("InternalShowGroupNoticesTopRight"))
            {
                floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
                        LLDockControl::BOTTOM));
            }
            else
            {
                floater->setDockControl(new LLDockControl(chiclet, floater, floater->getDockTongue(),
                        LLDockControl::TOP));
            }
            // </FS:Ansariel> Group notices, IMs and chiclets position
        }

        // window is positioned, now we can show it.
        floater->setVisible(true);
    }

    return floater;
}

void FSFloaterIM::setDocked(bool docked, bool pop_on_undock)
{
    // update notification channel state
    LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
        (LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLNotificationsUI::NOTIFICATION_CHANNEL_UUID));

    if(!isChatMultiTab())
    {
        LLTransientDockableFloater::setDocked(docked, pop_on_undock);
    }

    // update notification channel state
    if(channel)
    {
        channel->updateShowToastsState();
        channel->redrawToasts();
    }
}

void FSFloaterIM::setVisible(bool visible)
{
    LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
        (LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLNotificationsUI::NOTIFICATION_CHANNEL_UUID));
    LLTransientDockableFloater::setVisible(visible);

    // update notification channel state
    if (channel)
    {
        channel->updateShowToastsState();
        channel->redrawToasts();
    }

    bool is_minimized = visible && isChatMultiTab()
        ? FSFloaterIMContainer::getInstance()->isMinimized()
        : !visible;

    if (!is_minimized && mChatHistory && mInputEditor)
    {
        //only if floater was construced and initialized from xml
        updateMessages();
        FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();

        //prevent stealing focus when opening a background IM tab (EXT-5387, checking focus for EXT-6781)
        // If this is docked, is the selected tab, and the im container has focus, put focus in the input ctrl -KC
        bool is_active = im_container->getActiveFloater() == this && im_container->hasFocus();
        if (!isChatMultiTab() || is_active || hasFocus())
        {
            mInputEditor->setFocus(true);
        }
    }

    if (!visible)
    {
        if (LLIMChiclet* chiclet = LLChicletBar::getInstance()->getChicletPanel()->findChiclet<LLIMChiclet>(mSessionID); chiclet)
        {
            chiclet->setToggleState(false);
        }
    }

    if (visible && isInVisibleChain())
    {
        sIMFloaterShowedSignal(mSessionID);
        gConsole->addSession(mSessionID);
    }
    else
    {
        gConsole->removeSession(mSessionID);
    }
}

void FSFloaterIM::setMinimized(bool b)
{
    handleMinimized(b);

    LLTransientDockableFloater::setMinimized(b);
}

bool FSFloaterIM::getVisible()
{
    if (isChatMultiTab())
    {
        FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();

        // Treat inactive floater as invisible.
        bool is_active = im_container->getActiveFloater() == this;

        //torn off floater is always inactive
        if (!is_active && getHost() != im_container)
        {
            return LLTransientDockableFloater::getVisible();
        }

        // getVisible() returns true when Tabbed IM window is minimized.
        return is_active && !im_container->isMinimized() && im_container->getVisible();
    }
    else
    {
        return LLTransientDockableFloater::getVisible();
    }
}

//static
bool FSFloaterIM::toggle(const LLUUID& session_id)
{
    if (!isChatMultiTab())
    {
        FSFloaterIM* floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
        if (floater && floater->getVisible() && floater->hasFocus())
        {
            // clicking on chiclet to close floater just hides it to maintain existing
            // scroll/text entry state
            floater->setVisible(false);
            return false;
        }
        else if (floater && (!floater->isDocked() || (floater->getVisible() && !floater->hasFocus())))
        {
            floater->setVisible(true);
            floater->setFocus(true);
            return true;
        }
    }

    // ensure the list of messages is updated when floater is made visible
    show(session_id);
    return true;
}

//static
FSFloaterIM* FSFloaterIM::findInstance(const LLUUID& session_id)
{
    return LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
}

FSFloaterIM* FSFloaterIM::getInstance(const LLUUID& session_id)
{
    return LLFloaterReg::getTypedInstance<FSFloaterIM>("fs_impanel", session_id);
}

void FSFloaterIM::sessionInitReplyReceived(const LLUUID& im_session_id)
{
    mSessionInitialized = true;

    //will be different only for an ad-hoc im session
    if (mSessionID != im_session_id)
    {
        mSessionID = im_session_id;
        setKey(im_session_id);
        mControlPanel->setSessionId(im_session_id);
    }

    // updating "Call" button from group/ad-hoc control panel here to enable it without placing into draw() (EXT-4796)
    if (LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(im_session_id); session)
    {
        if ((session->isGroupSessionType() && gAgent.isInGroup(im_session_id)) || session->isAdHocSessionType())
        {
            updateCallButton();
        }
    }

    //*TODO here we should remove "starting session..." warning message if we added it in postBuild() (IB)

    //need to send delayed messaged collected while waiting for session initialization
    if (mQueuedMsgsForInit.size())
    {
        LLSD::array_iterator iter;
        for ( iter = mQueuedMsgsForInit.beginArray();
                    iter != mQueuedMsgsForInit.endArray(); ++iter)
        {
            LLIMModel::sendMessage(iter->asString(), mSessionID,
                mOtherParticipantUUID, mDialog);
        }

        mQueuedMsgsForInit.clear();
    }
}

void FSFloaterIM::updateMessages()
{
    //<FS:HG> FS-1734 seperate name and text styles for moderator
    bool highlight_mods_chat = gSavedSettings.getBOOL("FSHighlightGroupMods");

    std::list<LLSD> messages;

    // we shouldn't reset unread message counters if IM floater doesn't have focus
    LLIMModel::instance().getMessages(mSessionID, messages, mLastMessageIndex + 1, hasFocus());

    if (messages.size())
    {
        LLSD chat_args;
        chat_args["use_plain_text_chat_history"] = gSavedSettings.getBOOL("PlainTextChatHistory");
        chat_args["show_names_for_p2p_conv"] = gSavedSettings.getBOOL("IMShowNamesForP2PConv");
        chat_args["show_time"] = gSavedSettings.getBOOL("FSShowTimestampsIM");

        LLIMModel::LLIMSession* pIMSession = LLIMModel::instance().findIMSession(mSessionID);
        RLV_ASSERT(pIMSession);

        chat_args["is_p2p"] = pIMSession->isP2PSessionType();

        std::ostringstream message;
        std::list<LLSD>::const_reverse_iterator iter = messages.rbegin();
        std::list<LLSD>::const_reverse_iterator iter_end = messages.rend();
        for (; iter != iter_end; ++iter)
        {
            LLSD msg = *iter;

            std::string time = msg["time"].asString();
            LLUUID from_id = msg["from_id"].asUUID();
            std::string from = msg["from"].asString();
            std::string message = msg["message"].asString();
            S32 is_history = msg["is_history"].asInteger();
            bool is_region_msg = msg["is_region_msg"].asBoolean();

            LLChat chat;
            chat.mFromID = from_id;
            chat.mSessionID = mSessionID;
            chat.mFromName = from;
            chat.mTimeStr = time;
            if (is_history)
            {
                chat.mChatStyle = (EChatStyle) is_history;
            }
            if (is_region_msg)
            {
                chat.mSourceType = CHAT_SOURCE_REGION;
            }

            // Bold group moderators' chat -KC
            //<FS:HG> FS-1734 seperate name and text styles for moderator
            //if (!is_history && bold_mods_chat && pIMSession && pIMSession->mSpeakers)
            if (!is_history && highlight_mods_chat && pIMSession && pIMSession->mSpeakers)
            {
                LLPointer<LLSpeaker> speakerp = pIMSession->mSpeakers->findSpeaker(from_id);
                if (speakerp && speakerp->mIsModerator)
                {
                    chat.mChatStyle = CHAT_STYLE_MODERATOR;
                }
            }

            // process offer notification
            if (msg.has("notification_id"))
            {
                chat.mNotifId = msg["notification_id"].asUUID();
                // if notification exists - embed it
                if (LLNotificationsUtil::find(chat.mNotifId) != NULL)
                {
                    // remove embedded notification from channel
                    LLNotificationsUI::LLScreenChannel* channel = static_cast<LLNotificationsUI::LLScreenChannel*>
                            (LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLNotificationsUI::NOTIFICATION_CHANNEL_UUID));
                    if (channel && getVisible())
                    {
                        // toast will be automatically closed since it is not storable toast
                        channel->hideToast(chat.mNotifId);
                    }
                }
                // if notification doesn't exist - try to use next message which should be log entry
                else
                {
                    continue;
                }
            }
            //process text message
            else
            {
                chat.mText = message;
            }

            static const LLStyle::Params input_append_params = LLStyle::Params();
            mChatHistory->appendMessage(chat, chat_args, input_append_params);
            mLastMessageIndex = msg["index"].asInteger();

            // if it is a notification - next message is a notification history log, so skip it
            if (chat.mNotifId.notNull() && LLNotificationsUtil::find(chat.mNotifId) != NULL)
            {
                if (++iter == iter_end)
                {
                    break;
                }
                else
                {
                    mLastMessageIndex++;
                }
            }
        }
    }

    mPendingMessages = 0;
}

void FSFloaterIM::reloadMessages(bool clean_messages/* = false*/)
{
    if (clean_messages)
    {
        LLIMModel::LLIMSession * sessionp = LLIMModel::instance().findIMSession(mSessionID);

        if (NULL != sessionp)
        {
            sessionp->loadHistory();
        }
    }

    mChatHistory->clear();
    mLastMessageIndex = -1;
    updateMessages();
}

void FSFloaterIM::onInputEditorFocusReceived()
{
    // Allow enabling the FSFloaterIM input editor only if session can accept text
    LLIMModel::LLIMSession* im_session =
        LLIMModel::instance().findIMSession(mSessionID);
    //TODO: While disabled lllineeditor can receive focus we need to check if it is enabled (EK)
    if (im_session && im_session->mTextIMPossible && mInputEditor->getEnabled())
    {
        //in disconnected state IM input editor should be disabled
        mInputEditor->setEnabled(!gDisconnected);
    }
}

void FSFloaterIM::onInputEditorFocusLost()
{
    setTyping(false);
}

void FSFloaterIM::onInputEditorKeystroke()
{
    if (!mInputEditor->getText().empty())
    {
        setTyping(true);
    }
    else
    {
        // Deleting all text counts as stopping typing.
        setTyping(false);
    }
}

void FSFloaterIM::setTyping(bool typing)
{
    if (typing)
    {
        // Started or proceeded typing, reset the typing timeout timer
        mTypingTimeoutTimer.reset();
    }

    if (mMeTyping != typing)
    {
        // Typing state is changed
        mMeTyping = typing;
        // So, should send current state
        mShouldSendTypingState = true;
        // In case typing is started, send state after some delay
        mTypingTimer.reset();
    }

    // Don't want to send typing indicators to multiple people, potentially too
    // much network traffic. Only send in person-to-person IMs.
    // <FS:Techwolf Lupindo> fsdata support
    //if ( mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL )
    if ( mShouldSendTypingState && mDialog == IM_NOTHING_SPECIAL && !(FSData::instance().isSupport(mOtherParticipantUUID) && FSData::instance().isAgentFlag(gAgentID, FSData::NO_SUPPORT)))
    // </FS:Techwolf Lupindo>
    {
        if (mMeTyping)
        {
            if ( mTypingTimer.getElapsedTimeF32() > 1.f )
            {
                // Still typing, send 'start typing' notification
                LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, true);
                mShouldSendTypingState = false;
                mMeTypingTimer.reset();
            }
        }
        else
        {
            // Send 'stop typing' notification immediately
            LLIMModel::instance().sendTypingState(mSessionID, mOtherParticipantUUID, false);
            mShouldSendTypingState = false;
        }
    }

    LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
    if (speaker_mgr)
        speaker_mgr->setSpeakerTyping(gAgent.getID(), false);
}

void FSFloaterIM::processIMTyping(const LLUUID& from_id, bool typing)
{
    if (typing)
    {
        // other user started typing
        addTypingIndicator(from_id);
        mOtherTypingTimer.reset();
    }
    else
    {
        // other user stopped typing
        removeTypingIndicator(from_id);
    }
}

void FSFloaterIM::processAgentListUpdates(const LLSD& body)
{
    loadInitialInvitedIDs();

    uuid_vec_t joined_uuids;

    if (body.isMap() && body.has("agent_updates") && body["agent_updates"].isMap())
    {
        LLSD::map_const_iterator update_it;
        for (update_it = body["agent_updates"].beginMap();
            update_it != body["agent_updates"].endMap();
            ++update_it)
        {
            LLUUID agent_id(update_it->first);
            LLSD agent_data = update_it->second;

            if (agent_data.isMap())
            {
                // store the new participants in joined_uuids
                if (agent_data.has("transition") && agent_data["transition"].asString() == "ENTER")
                {
                    joined_uuids.push_back(agent_id);
                }

                // process the moderator mutes
                if (agent_id == gAgentID && agent_data.has("info") && agent_data["info"].has("mutes"))
                {
                    bool moderator_muted_text = agent_data["info"]["mutes"]["text"].asBoolean();
                    mInputEditor->setEnabled(!moderator_muted_text);
                    std::string label;
                    if (moderator_muted_text)
                        label = LLTrans::getString("IM_muted_text_label");
                    else
                        label = LLTrans::getString("IM_to_label") + " " + LLIMModel::instance().getName(mSessionID);
                    mInputEditor->setLabel(label);

                    if (moderator_muted_text)
                        LLNotificationsUtil::add("TextChatIsMutedByModerator");
                }
            }
        }
    }

    // the vectors need to be sorted for computing the intersection and difference
    std::sort(mInvitedParticipants.begin(), mInvitedParticipants.end());
    std::sort(joined_uuids.begin(), joined_uuids.end());

    uuid_vec_t intersection; // uuids of invited residents who have joined the conversation
    std::set_intersection(mInvitedParticipants.begin(), mInvitedParticipants.end(),
                          joined_uuids.begin(), joined_uuids.end(),
                          std::back_inserter(intersection));

    if (!intersection.empty())
    {
        sendParticipantsAddedNotification(intersection);
    }

    // Remove all joined participants from invited array.
    // The difference between the two vectors (the elements in mInvitedParticipants which are not in joined_uuids)
    // is placed at the beginning of mInvitedParticipants, then all other elements are erased.
    mInvitedParticipants.erase(std::set_difference(mInvitedParticipants.begin(), mInvitedParticipants.end(),
                                                   joined_uuids.begin(), joined_uuids.end(),
                                                   mInvitedParticipants.begin()),
                               mInvitedParticipants.end());
}

void FSFloaterIM::sendParticipantsAddedNotification(const uuid_vec_t& uuids)
{
    std::string names_string;
    LLAvatarActions::buildResidentsString(uuids, names_string);
    LLStringUtil::format_map_t args;
    args["[NAME]"] = names_string;

    sendMsg(getString(uuids.size() > 1 ? "multiple_participants_added" : "participant_added", args));
}

void FSFloaterIM::updateChatHistoryStyle()
{
    mChatHistory->clear();
    mLastMessageIndex = -1;
    updateMessages();
}

void FSFloaterIM::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
    LLFontGL* font = LLViewerChat::getChatFont();
    LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
    for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
         iter != inst_list.end(); ++iter)
    {
        FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
        if (floater)
        {
            floater->updateChatHistoryStyle();
            floater->mInputEditor->setFont(font);

            // Re-set the current text to make style update instant
            std::string text = floater->mInputEditor->getText();
            floater->mInputEditor->clear();
            floater->mInputEditor->setText(text);
        }
    }
}

bool FSFloaterIM::handleDragAndDrop(S32 x, S32 y, MASK mask,
                           bool drop, EDragAndDropType cargo_type,
                           void *cargo_data, EAcceptance *accept,
                           std::string& tooltip_msg)
{
    if (cargo_type == DAD_PERSON)
    {
        if (dropPerson(static_cast<LLUUID*>(cargo_data), drop))
        {
            *accept = ACCEPT_YES_MULTI;
        }
        else
        {
            *accept = ACCEPT_NO;
        }
    }

    else if (mDialog == IM_NOTHING_SPECIAL)
    {
        LLToolDragAndDrop::handleGiveDragAndDrop(mOtherParticipantUUID, mSessionID, drop,
                                                 cargo_type, cargo_data, accept);
    }

    // handle case for dropping calling cards (and folders of calling cards) onto invitation panel for invites
    else if (isInviteAllowed() && !mIsP2PChat)
    {
        *accept = ACCEPT_NO;

        if (cargo_type == DAD_CALLINGCARD)
        {
            if (dropCallingCard((LLInventoryItem*)cargo_data, drop))
            {
                *accept = ACCEPT_YES_MULTI;
            }
        }
        else if (cargo_type == DAD_CATEGORY)
        {
            if (dropCategory((LLInventoryCategory*)cargo_data, drop))
            {
                *accept = ACCEPT_YES_MULTI;
            }
        }
    }
    return true;
}

bool FSFloaterIM::dropCallingCard(LLInventoryItem* item, bool drop)
{
    bool rv = true;
    if (item && item->getCreatorUUID().notNull())
    {
        uuid_vec_t ids;
        ids.push_back(item->getCreatorUUID());
        if (canAddSelectedToChat(ids))
        {
            if (drop)
            {
                mPendingParticipants.push_back(item->getCreatorUUID());
                mEventTimer.start();
            }
        }
        else
        {
            rv = false;
        }
    }
    else
    {
        // set to false if creator uuid is null.
        rv = false;
    }
    return rv;
}

bool FSFloaterIM::dropCategory(LLInventoryCategory* category, bool drop)
{
    bool rv = true;
    if (category)
    {
        LLInventoryModel::cat_array_t cats;
        LLInventoryModel::item_array_t items;
        LLUniqueBuddyCollector buddies;
        gInventory.collectDescendentsIf(category->getUUID(),
                                        cats,
                                        items,
                                        LLInventoryModel::EXCLUDE_TRASH,
                                        buddies);
        auto count = items.size();
        if (count == 0)
        {
            rv = false;
        }
        else
        {
            uuid_vec_t ids;
            ids.reserve(count);
            for(S32 i = 0; i < count; ++i)
            {
                ids.push_back(items.at(i)->getCreatorUUID());
            }

            if (canAddSelectedToChat(ids))
            {
                if (drop)
                {
                    mPendingParticipants.insert(mPendingParticipants.end(), ids.begin(), ids.end());
                    mEventTimer.start();
                }
            }
            else
            {
                rv = false;
            }
        }
    }
    return rv;
}

bool FSFloaterIM::dropPerson(LLUUID* person_id, bool drop)
{
    bool res = person_id && person_id->notNull();
    if (res)
    {
        uuid_vec_t ids;
        ids.push_back(*person_id);

        res = canAddSelectedToChat(ids);
        if (res && drop)
        {
            // these people will be added during the next draw() call
            // (so they can be added all at once)
            mPendingParticipants.push_back(*person_id);
            mEventTimer.start();
        }
    }

    return res;
}

//virtual
bool FSFloaterIM::tick()
{
    // add people who were added via dropPerson()
    if (!mPendingParticipants.empty())
    {
        addSessionParticipants(mPendingParticipants);
        mPendingParticipants.clear();
    }

    mEventTimer.stop();

    return false;
}

// virtual
bool FSFloaterIM::handleKeyHere( KEY key, MASK mask )
{
    bool handled = false;

    if (key == KEY_RETURN)
    {
        if (mask == MASK_ALT)
        {
            mInputEditor->updateHistory();
            sendMsgFromInputEditor(CHAT_TYPE_OOC);
            handled = true;
        }
        else if (mask == (MASK_SHIFT | MASK_CONTROL))
        {
            if (!gSavedSettings.getBOOL("FSUseSingleLineChatEntry"))
            {
                if ((wstring_utf8_length(mInputEditor->getWText()) + wchar_utf8_length('\n')) > mInputEditor->getMaxTextLength())
                {
                    LLUI::getInstance()->reportBadKeystroke();
                }
                else
                {
                    mInputEditor->insertLinefeed();
                }
            }
            else
            {
                if ((wstring_utf8_length(mInputEditor->getWText()) + wchar_utf8_length(llwchar(182))) > mInputEditor->getMaxTextLength())
                {
                    LLUI::getInstance()->reportBadKeystroke();
                }
                else
                {
                    LLWString line_break(1, llwchar(182));
                    mInputEditor->insertText(line_break);
                }
            }
            handled = true;
        }
    }

    return handled;
}

bool FSFloaterIM::isInviteAllowed() const
{
    return ( (IM_SESSION_CONFERENCE_START == mDialog)
             || (IM_SESSION_INVITE == mDialog && !gAgent.isInGroup(mSessionID))
             || mIsP2PChat);
}

bool FSFloaterIM::inviteToSession(const uuid_vec_t& ids)
{
    LLViewerRegion* region = gAgent.getRegion();
    bool is_region_exist = region != NULL;

    if (is_region_exist)
    {
        size_t count = ids.size();

        if (isInviteAllowed() && (count > 0))
        {
            LL_DEBUGS("FSFloaterIM") << "FSFloaterIM::inviteToSession() - inviting participants" << LL_ENDL;

            std::string url = region->getCapability("ChatSessionRequest");

            LLSD data;
            data["params"] = LLSD::emptyArray();
            for (size_t i = 0; i < count; ++i)
            {
                data["params"].append(ids[i]);
            }
            data["method"] = "invite";
            data["session-id"] = mSessionID;

            LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost( url, data );
        }
        else
        {
            LL_DEBUGS("FSFloaterIM") << "LLFloaterIMSession::inviteToSession -"
                                     << " no need to invite agents for "
                                     << mDialog << LL_ENDL;
            // successful add, because everyone that needed to get added
            // was added.
        }
    }

    return is_region_exist;
}

void FSFloaterIM::addTypingIndicator(const LLUUID& from_id)
{
    // We may have lost a "stop-typing" packet, don't add it twice
    if (from_id.notNull() && !mOtherTyping)
    {
        mOtherTyping = true;

        // Save and set new title
        mSavedTitle = getTitle();
        setTitle((gSavedSettings.getBOOL("FSTypingChevronPrefix") ? "> " : "") + mTypingStart.getString());

        // Update speaker
        LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
        if ( speaker_mgr )
        {
            speaker_mgr->setSpeakerTyping(from_id, true);
        }
    }
}

void FSFloaterIM::removeTypingIndicator(const LLUUID& from_id)
{
    if (mOtherTyping)
    {
        mOtherTyping = false;

        // Revert the title to saved one
        setTitle(mSavedTitle);

        if (from_id.notNull())
        {
            // Update speaker
            LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
            if ( speaker_mgr )
            {
                speaker_mgr->setSpeakerTyping(from_id, false);
            }
        }
        // Ansariel: Transplant of STORM-1975; Typing notifications are only sent in P2P sessions,
        //           so we can use mOtherParticipantUUID here and don't need LLIMInfo (Kitty said
        //           it's dangerous to use the stored LLIMInfo in LL's version!)
        else if (mDialog == IM_NOTHING_SPECIAL && mOtherParticipantUUID.notNull())
        {
            // Update speaker
            LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
            if ( speaker_mgr )
            {
                speaker_mgr->setSpeakerTyping(mOtherParticipantUUID, false);
            }
        }
    }
}

// static
void FSFloaterIM::closeHiddenIMToasts()
{
    class IMToastMatcher: public LLNotificationsUI::LLScreenChannel::Matcher
    {
    public:
        bool matches(const LLNotificationPtr notification) const
        {
            // "notifytoast" type of notifications is reserved for IM notifications
            return "notifytoast" == notification->getType();
        }
    };

    LLNotificationsUI::LLScreenChannel* channel = LLNotificationsUI::LLChannelManager::getNotificationScreenChannel();
    if (channel)
    {
        channel->closeHiddenToasts(IMToastMatcher());
    }
}
// static
void FSFloaterIM::confirmLeaveCallCallback(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    const LLSD& payload = notification["payload"];
    LLUUID session_id = payload["session_id"];
    bool snooze = payload["snooze"].asBoolean();

    FSFloaterIM* im_floater = LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
    if (option == 0 && im_floater)
    {
        LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(session_id);
        if (session)
        {
            if (snooze)
            {
                im_floater->confirmSnooze();
                return;
            }

            session->mCloseAction = LLIMModel::LLIMSession::CLOSE_DEFAULT;
        }
        im_floater->closeFloater();
    }

    return;
}

// static
bool FSFloaterIM::isChatMultiTab()
{
    // Restart is required in order to change chat window type.
    static bool is_single_window = gSavedSettings.getS32("FSChatWindow") == 1;
    return is_single_window;
}

// static
void FSFloaterIM::initIMFloater()
{
    // This is called on viewer start up
    // init chat window type before user changed it in preferences
    isChatMultiTab();
}

//static
void FSFloaterIM::sRemoveTypingIndicator(const LLSD& data)
{
    LLUUID session_id = data["session_id"];
    if (session_id.isNull())
        return;

    LLUUID from_id = data["from_id"];
    if (gAgentID == from_id || LLUUID::null == from_id)
        return;

    FSFloaterIM* floater = FSFloaterIM::findInstance(session_id);
    if (!floater)
        return;

    if (IM_NOTHING_SPECIAL != floater->mDialog)
        return;

    floater->removeTypingIndicator();
}

void FSFloaterIM::onNewIMReceived(const LLUUID& session_id)
{
    if (isChatMultiTab())
    {
        if (FSFloaterIM::findInstance(session_id))
        {
            return;
        }

        FSFloaterIM* new_tab = FSFloaterIM::getInstance(session_id);
        FSFloaterIMContainer::getInstance()->addNewSession(new_tab);
    }
}

void FSFloaterIM::onClickCloseBtn(bool app_quitting)
{
    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
                mSessionID);

    if (!session)
    {
        LL_WARNS("FSFloaterIM") << "Empty session." << LL_ENDL;
        return;
    }

    bool is_call_with_chat = session->isGroupSessionType()
            || session->isAdHocSessionType() || session->isP2PSessionType();

    LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

    if (is_call_with_chat && voice_channel != NULL && voice_channel->isActive())
    {
        LLSD payload;
        payload["session_id"] = mSessionID;
        payload["snooze"] = false;
        LLNotificationsUtil::add("ConfirmLeaveCall", LLSD(), payload, confirmLeaveCallCallback);
        return;
    }

    session->mCloseAction = LLIMModel::LLIMSession::CLOSE_DEFAULT;

    LLFloater::onClickCloseBtn();
}

// <FS:Zi> Viewer version popup
bool FSFloaterIM::enableViewerVersionCallback(const LLSD& notification,const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification,response);

    bool result = false;
    if (option == 0)        // "yes"
    {
        result = true;
    }

    gSavedSettings.setBOOL("FSSupportGroupChatPrefix3", result);
    gSavedSettings.setBOOL("FSSupportGroupChatPrefixTesting", result);
    return result;
}
// </FS:Zi>

// <FS:CR> FIRE-11734
//static
void FSFloaterIM::clearAllOpenHistories()
{
    LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("fs_impanel");
    for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
         iter != inst_list.end(); ++iter)
    {
        FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(*iter);
        if (floater)
        {
            floater->reloadMessages(true);
        }
    }

    FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
    if (nearby_chat)
    {
        nearby_chat->reloadMessages(true);
    }
}

void FSFloaterIM::initIMSession(const LLUUID& session_id)
{
    // Change the floater key to bind it to a new session.
    setKey(session_id);

    mSessionID = session_id;
    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);

    if (session)
    {
        mIsP2PChat = session->isP2PSessionType();
        mSessionInitialized = session->mSessionInitialized;
        mDialog = session->mType;
    }
}

void FSFloaterIM::reshapeChatLayoutPanel()
{
    mChatLayoutPanel->reshape(mChatLayoutPanel->getRect().getWidth(), mInputEditor->getRect().getHeight() + mInputEditorPad, false);
}

boost::signals2::connection FSFloaterIM::setIMFloaterShowedCallback(const floater_showed_signal_t::slot_type& cb)
{
    return FSFloaterIM::sIMFloaterShowedSignal.connect(cb);
}

void FSFloaterIM::updateUnreadMessageNotification(S32 unread_messages)
{
    if (unread_messages == 0 || !gSavedSettings.getBOOL("FSNotifyUnreadIMMessages"))
    {
        mUnreadMessagesNotificationPanel->setVisible(false);
    }
    else
    {
        mUnreadMessagesNotificationTextBox->setTextArg("[NUM]", llformat("%d", unread_messages));
        mUnreadMessagesNotificationPanel->setVisible(true);
    }
}

void FSFloaterIM::onAddButtonClicked()
{
    LLView* button = findChild<LLButton>("add_participant_btn");
    LLFloater* root_floater = this;
    LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&FSFloaterIM::addSessionParticipants, this, _1), true, true, false, root_floater->getName(), button);
    if (!picker)
    {
        return;
    }

    // Need to disable 'ok' button when selected users are already in conversation.
    picker->setOkBtnEnableCb(boost::bind(&FSFloaterIM::canAddSelectedToChat, this, _1));

    if (root_floater)
    {
        root_floater->addDependentFloater(picker);
    }

    LLTransientFloaterMgr::getInstance()->addControlView(LLTransientFloaterMgr::IM, picker);
}

bool FSFloaterIM::canAddSelectedToChat(const uuid_vec_t& uuids)
{
    if (!LLIMModel::instance().findIMSession(mSessionID)
        || mDialog == IM_SESSION_GROUP_START
        || (mDialog == IM_SESSION_INVITE && gAgent.isInGroup(mSessionID)))
    {
        return false;
    }

    if (mIsP2PChat)
    {
        // For a P2P session just check if we are not adding the other participant.

        for (const auto& uuid : uuids)
        {
            if (uuid == mOtherParticipantUUID)
            {
                return false;
            }
        }
    }

    return true;
}

void FSFloaterIM::addSessionParticipants(const uuid_vec_t& uuids)
{
    if (mIsP2PChat)
    {
        LLSD payload;
        LLSD args;

        LLNotificationsUtil::add("ConfirmAddingChatParticipants", args, payload,
                boost::bind(&FSFloaterIM::addP2PSessionParticipants, this, _1, _2, uuids));
    }
    else
    {
        if (findInstance(mSessionID))
        {
            // remember whom we have invited, to notify others later, when the invited ones actually join
            mInvitedParticipants.insert(mInvitedParticipants.end(), uuids.begin(), uuids.end());
        }

        inviteToSession(uuids);
    }
}

void FSFloaterIM::addP2PSessionParticipants(const LLSD& notification, const LLSD& response, const uuid_vec_t& uuids)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option != 0)
    {
        return;
    }

    LLVoiceChannel* voice_channel = LLIMModel::getInstance()->getVoiceChannel(mSessionID);

    // first check whether this is a voice session
    bool is_voice_call = voice_channel && voice_channel->isActive();

    uuid_vec_t temp_ids;
    uuid_vec_t invited_ids;

    // Add the initial participant of a P2P session
    temp_ids.push_back(mOtherParticipantUUID);
    temp_ids.insert(temp_ids.end(), uuids.begin(), uuids.end());

    LLUUID session_id = mSessionID;

    // then we can close the current session
    if (findInstance(mSessionID))
    {
        // remember whom we have invited, to notify others later, when the invited ones actually join
        std::copy(uuids.begin(), uuids.end(), std::back_inserter(mInvitedParticipants));
        std::copy(mInvitedParticipants.begin(), mInvitedParticipants.end(), std::back_inserter(invited_ids));

        // Ansariel: This will result in the floater actually being closed as opposed in CHUI!
        onClose(false);
    }

    // Start a new ad hoc voice call if we invite new participants to a P2P call,
    // or start a text chat otherwise.
    if (is_voice_call)
    {
        session_id = LLAvatarActions::startAdhocCall(temp_ids, session_id);
    }
    else
    {
        session_id = LLAvatarActions::startConference(temp_ids, session_id);
    }

    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(session_id);
    if (session)
    {
        session->mInitialInvitedIDs = invited_ids;
    }
}

void FSFloaterIM::loadInitialInvitedIDs()
{
    LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);
    if (session && !session->mInitialInvitedIDs.empty())
    {
        mInvitedParticipants = session->mInitialInvitedIDs;
        session->mInitialInvitedIDs.clear();
    }
}

void FSFloaterIM::handleMinimized(bool minimized)
{
    if (minimized)
    {
        gConsole->removeSession(mSessionID);
    }
    else
    {
        gConsole->addSession(mSessionID);
        if (mChatHistory)
        {
            updateMessages();
        }
    }
}

bool FSFloaterIM::applyRectControl()
{
    bool res = true;

    // We need to do some sort of hack here to prevent torn-off floaters from
    // jumping around if clicking on the IM chiclets: Clicking on a chiclet to
    // switch to a particular IM floater will call FSFloaterIM::show() and in
    // the process LLFloater::applyControlsAndPosition(). Depending on the result
    // of the call to applyRectControl(), applyPositioning() positioning is
    // called that will ultimately set the position of the floater depending
    // of the position of the last floater in the group. However, this doesn't
    // work properly and will cause floaters to jump. To prevent this, we only
    // apply the rect if not called by FSFloaterIM::show(). To prevent an
    // additionally misplaced floater caused by cascading a group of IM floaters,
    // we force LLFloater::applyRectControl() into the path where no cascaded
    // position is going to be applied by temporarily clear the instance name
    // of the floater. -AH
    if (mApplyRect)
    {
        std::string name = mInstanceName;
        mInstanceName.clear();
        res = LLFloater::applyRectControl();
        mInstanceName = name;
    }

    return res;
}

void FSFloaterIM::onEmojiRecentPanelToggleBtnClicked()
{
    bool show = mEmojiRecentPanel->getVisible() ? false : true;
    if (show)
    {
        initEmojiRecentPanel();
    }

    mEmojiRecentPanel->setVisible(show);
    mInputEditor->setFocus(true);
}

void FSFloaterIM::initEmojiRecentPanel()
{
    std::list<llwchar>& recentlyUsed = LLFloaterEmojiPicker::getRecentlyUsed();
    if (recentlyUsed.empty())
    {
        mEmojiRecentEmptyText->setVisible(true);
        mEmojiRecentIconsCtrl->setVisible(false);
    }
    else
    {
        LLWString emojis;
        for (llwchar emoji : recentlyUsed)
        {
            emojis += emoji;
        }
        mEmojiRecentIconsCtrl->setEmojis(emojis);
        mEmojiRecentEmptyText->setVisible(false);
        mEmojiRecentIconsCtrl->setVisible(true);
    }
}

void FSFloaterIM::onRecentEmojiPicked(const LLSD& value)
{
    LLSD::String str = value.asString();
    if (str.size())
    {
        LLWString wstr = utf8string_to_wstring(str);
        if (wstr.size())
        {
            llwchar emoji = wstr[0];
            mInputEditor->insertEmoji(emoji);
        }
    }
}

void FSFloaterIM::onEmojiPickerToggleBtnClicked()
{
    mInputEditor->setFocus(true);
    mInputEditor->showEmojiHelper();
}
