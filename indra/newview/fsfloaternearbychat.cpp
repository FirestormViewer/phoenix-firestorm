/** 
 * @file fsfloaternearbychat.cpp
 * @brief Nearby chat history scrolling panel implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Zi Ree @ Second Life
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

// Original file: LLFloaterNearbyChat.cpp

#include "llviewerprecompiledheaders.h"

#include "fsfloaternearbychat.h"

#include "chatbar_as_cmdline.h"
#include "fschathistory.h"
#include "fscommon.h"
#include "fsfloaterim.h"
#include "fsfloaterimcontainer.h"
#include "llagent.h" 			// gAgent
#include "llanimationstates.h"	// ANIM_AGENT_WHISPER, ANIM_AGENT_TALK, ANIM_AGENT_SHOUT
#include "llautoreplace.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llchannelmanager.h"
#include "llchatentry.h"
#include "llcommandhandler.h"
#include "llconsole.h"
#include "lldraghandle.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llfocusmgr.h"
#include "llgesturemgr.h"
#include "lliconctrl.h"
#include "llkeyboard.h"
#include "lllayoutstack.h"
#include "lllogchat.h"
#include "llmenugl.h"
#include "llmultigesture.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llrootview.h"
#include "llspinctrl.h"
#include "llstylemap.h"
#include "lltrans.h"
#include "lltranslate.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"//for gMenuHolder
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "rlvhandler.h"

#define NAME_PREDICTION_MINIMUM_LENGTH 3
S32 FSFloaterNearbyChat::sLastSpecialChatChannel = 0;

// [RLVa:KB] - Checked: 2010-02-27 (RLVa-0.2.2)
void send_chat_from_nearby_floater(std::string utf8_out_text, EChatType type, S32 channel);
// [/RLVa:KB]
void really_send_chat_from_nearby_floater(std::string utf8_out_text, EChatType type, S32 channel);

struct LLChatTypeTrigger {
	std::string name;
	EChatType type;
};

static LLChatTypeTrigger sChatTypeTriggers[] = {
	{ "/whisper"	, CHAT_TYPE_WHISPER},
	{ "/shout"	, CHAT_TYPE_SHOUT}
};

FSFloaterNearbyChat::FSFloaterNearbyChat(const LLSD& key) 
	: LLDockableFloater(NULL, false, false, key)
	,mChatHistory(NULL)
	,mInputEditor(NULL)
	// <FS:Ansariel> Optional muted chat history
	,mChatHistoryMuted(NULL)
	,mChatLayoutPanel(NULL)
	,mInputPanels(NULL)
	,mChatLayoutPanelHeight(0)
{
}

FSFloaterNearbyChat::~FSFloaterNearbyChat()
{
}

void FSFloaterNearbyChat::updateFSUseNearbyChatConsole(const LLSD &data)
{
	FSUseNearbyChatConsole = data.asBoolean();

	if (FSUseNearbyChatConsole)
	{
		LLNotificationsUI::LLScreenChannelBase* chat_channel = LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
		if(chat_channel)
		{
			chat_channel->removeToastsFromChannel();
		}
		gConsole->setVisible(!getVisible());
	}
	else
	{
		gConsole->setVisible(FALSE);		
	}
}


BOOL FSFloaterNearbyChat::postBuild()
{
	setIsSingleInstance(TRUE);
	
	//menu
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	enable_registrar.add("NearbyChat.Check", boost::bind(&FSFloaterNearbyChat::onNearbyChatCheckContextMenuItem, this, _2));
	registrar.add("NearbyChat.Action", boost::bind(&FSFloaterNearbyChat::onNearbyChatContextMenuItemClicked, this, _2));
	
	LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_nearby_chat.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if(menu)
		mPopupMenuHandle = menu->getHandle();

	gSavedSettings.declareS32("nearbychat_showicons_and_names",2,"NearByChat header settings",true);

	mInputEditor = getChild<LLChatEntry>("chat_box");
	if (mInputEditor)
	{
		mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
		mInputEditor->setCommitCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxCommit, this));
		mInputEditor->setKeystrokeCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxKeystroke, this));
		mInputEditor->setFocusLostCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxFocusLost, this));
		mInputEditor->setFocusReceivedCallback(boost::bind(&FSFloaterNearbyChat::onChatBoxFocusReceived, this));
		mInputEditor->setTextExpandedCallback(boost::bind(&FSFloaterNearbyChat::reshapeChatLayoutPanel, this));
		mInputEditor->setPassDelete(TRUE);
		mInputEditor->setFont(LLViewerChat::getChatFont());
	}
	mChatLayoutPanel = getChild<LLLayoutPanel>("chat_layout_panel");
	mInputPanels = getChild<LLLayoutStack>("input_panels");
	mChatLayoutPanelHeight = mChatLayoutPanel->getRect().getHeight();
	mInputEditorPad = mChatLayoutPanelHeight - mInputEditor->getRect().getHeight();
	
	gSavedSettings.getControl("FSNearbyChatbar")->getCommitSignal()->connect(boost::bind(&FSFloaterNearbyChat::onChatBarVisibilityChanged, this));
	gSavedSettings.getControl("FSShowChatChannel")->getCommitSignal()->connect(boost::bind(&FSFloaterNearbyChat::onChatChannelVisibilityChanged, this));

	onChatBarVisibilityChanged();
	onChatChannelVisibilityChanged();

	enableTranslationButton(LLTranslate::isTranslationConfigured());

	childSetCommitCallback("chat_history_btn",onHistoryButtonClicked,this);

	mChatHistory = getChild<FSChatHistory>("chat_history");

	// <FS:Ansariel> Optional muted chat history
	mChatHistoryMuted = getChild<FSChatHistory>("chat_history_muted");
	
	// <vertical tab docking> -AO
	if(isChatMultiTab())
	{
		LLButton* slide_left = getChild<LLButton>("slide_left_btn");
		slide_left->setVisible(false);
		LLButton* slide_right = getChild<LLButton>("slide_right_btn");
		slide_right->setVisible(false);

		FSUseNearbyChatConsole = gSavedSettings.getBOOL("FSUseNearbyChatConsole");
		gSavedSettings.getControl("FSUseNearbyChatConsole")->getSignal()->connect(boost::bind(&FSFloaterNearbyChat::updateFSUseNearbyChatConsole, this, _2));
	}
	
	return LLDockableFloater::postBuild();
}

std::string appendTime()
{
	time_t utc_time;
	utc_time = time_corrected();
	std::string timeStr ="["+ LLTrans::getString("TimeHour")+"]:["
		+LLTrans::getString("TimeMin")+"]";
	if (gSavedSettings.getBOOL("FSSecondsinChatTimestamps"))
	{
		timeStr += ":["
			+LLTrans::getString("TimeSec")+"]";
	}

	LLSD substitution;

	substitution["datetime"] = (S32) utc_time;
	LLStringUtil::format (timeStr, substitution);

	return timeStr;
}

void FSFloaterNearbyChat::addMessage(const LLChat& chat,bool archive,const LLSD &args)
{
	LLChat& tmp_chat = const_cast<LLChat&>(chat);
	bool use_plain_text_chat_history = gSavedSettings.getBOOL("PlainTextChatHistory");
	bool hide_timestamps_nearby_chat = gSavedSettings.getBOOL("FSHideTimestampsNearbyChat");
	// [FIRE-1641 : SJ]: Option to hide timestamps in nearby chat - only add Timestamp when hide_timestamps_nearby_chat is not TRUE
	if (!hide_timestamps_nearby_chat)
	{
		if (tmp_chat.mTimeStr.empty())
		{
			tmp_chat.mTimeStr = appendTime();
		}
	}

	// <FS:Ansariel> Optional muted chat history
	tmp_chat.mFromName = chat.mFromName;
	LLSD chat_args = args;
	chat_args["use_plain_text_chat_history"] = use_plain_text_chat_history;
	chat_args["hide_timestamps_nearby_chat"] = hide_timestamps_nearby_chat;
	chat_args["show_time"] = gSavedSettings.getBOOL("IMShowTime");
	mChatHistoryMuted->appendMessage(chat, chat_args);
	// </FS:Ansariel> Optional muted chat history
	if (!chat.mMuted)
	{
		// <FS:Ansariel> Optional muted chat history
		//tmp_chat.mFromName = chat.mFromName;
		//LLSD chat_args = args;
		//chat_args["use_plain_text_chat_history"] = use_plain_text_chat_history;
		//chat_args["hide_timestamps_nearby_chat"] = hide_timestamps_nearby_chat;
		// <(FS:Ansariel> Optional muted chat history
		mChatHistory->appendMessage(chat, chat_args);
	}

	if (archive)
	{
		mMessageArchive.push_back(chat);
		if (mMessageArchive.size() > 200)
		{
			mMessageArchive.erase(mMessageArchive.begin());
		}
	}

	// <FS:Ansariel> Optional muted chat history
	//if (args["do_not_log"].asBoolean()) 
	if (args["do_not_log"].asBoolean() || chat.mMuted) 
	// </FS:Ansariel> Optional muted chat history
	{
		return;
	}
	
	// AO: IF tab mode active, flash our tab
	if (isChatMultiTab())
	{
		LLMultiFloater* hostp = getHost();
        // KC: Don't flash tab on system messages
		if (!isInVisibleChain() && hostp
        && (chat.mSourceType == CHAT_SOURCE_AGENT || chat.mSourceType == CHAT_SOURCE_OBJECT))
		{
			hostp->setFloaterFlashing(this, TRUE);
		}
	}

	if (gSavedPerAccountSettings.getBOOL("LogNearbyChat"))
	{
		std::string from_name = chat.mFromName;

		if (chat.mSourceType == CHAT_SOURCE_AGENT)
		{
			// if the chat is coming from an agent, log the complete name
			LLAvatarName av_name;
			LLAvatarNameCache::get(chat.mFromID, &av_name);

			if (!av_name.isDisplayNameDefault())
			{
				from_name = av_name.getCompleteName();
			}

			// Ansariel: Handle IMs in nearby chat
			// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
			//if (gSavedSettings.getBOOL("FSShowIMInChatHistory") && chat.mChatType == CHAT_TYPE_IM)
			if (gSavedSettings.getBOOL("FSShowIMInChatHistory") && (chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP))
			{
				if (gSavedSettings.getBOOL("FSLogIMInChatHistory"))
				{
					if (chat.mChatType == CHAT_TYPE_IM_GROUP && chat.mFromNameGroup != "")
					{
						from_name = LLTrans::getString("IMPrefix") + chat.mFromNameGroup + from_name;
					}
					else
					{
						from_name = LLTrans::getString("IMPrefix") + from_name;
					}
					// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
				}
				else return;
			}
		}

		LLLogChat::saveHistory("chat", from_name, chat.mFromID, chat.mText);
	}
}

// virtual
BOOL FSFloaterNearbyChat::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash)
{
	onTabInto();
	if (focus_flash)
	{
		gFocusMgr.triggerFocusFlash();
	}
	return TRUE;
}

void FSFloaterNearbyChat::onNearbySpeakers()
{
	LLSD param;
	param["people_panel_tab_name"] = "nearby_panel";
	LLFloaterSidePanelContainer::showPanel("people", "panel_people", param);
}

// static
void FSFloaterNearbyChat::onHistoryButtonClicked(LLUICtrl* ctrl, void* userdata)
{
	if (gSavedSettings.getBOOL("FSUseBuiltInHistory"))
	{
		LLFloaterReg::showInstance("preview_conversation", LLSD(LLUUID::null), true);
	}
	else
	{
		gViewerWindow->getWindow()->openFile(LLLogChat::makeLogFileName("chat"));
	}
}

void FSFloaterNearbyChat::onNearbyChatContextMenuItemClicked(const LLSD& userdata)
{
}

bool FSFloaterNearbyChat::onNearbyChatCheckContextMenuItem(const LLSD& userdata)
{
	std::string str = userdata.asString();
	if (str == "nearby_people")
	{
		onNearbySpeakers();	
	}
	return false;
}

void FSFloaterNearbyChat::onChatBarVisibilityChanged()
{
	getChild<LLLayoutPanel>("chat_bar_visibility_panel")->setVisible(gSavedSettings.getBOOL("FSNearbyChatbar"));
}

void FSFloaterNearbyChat::onChatChannelVisibilityChanged()
{
	getChild<LLLayoutPanel>("channel_spinner_visibility_panel")->setVisible(gSavedSettings.getBOOL("FSShowChatChannel"));
}

void FSFloaterNearbyChat::openFloater(const LLSD& key)
{
	// We override this to put nearbychat in the IM floater. -AO
	if (isChatMultiTab())
	{
		// <FS:Ansariel> [FS communication UI]
		//LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();
		// </FS:Ansariel> [FS communication UI]
		// only show the floater container if we are actually attached -Zi
		if (floater_container && !gSavedSettings.getBOOL("ChatHistoryTornOff"))
		{
			floater_container->showFloater(this, LLTabContainer::START);
		}
		setVisible(TRUE);
		LLFloater::openFloater(key);
	}
}

void FSFloaterNearbyChat::removeScreenChat()
{
	LLNotificationsUI::LLScreenChannelBase* chat_channel = LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
	if (chat_channel)
	{
		chat_channel->removeToastsFromChannel();
	}
}

void FSFloaterNearbyChat::setVisible(BOOL visible)
{
	if (visible)
	{
		removeScreenChat();
	}
	LLDockableFloater::setVisible(visible);
	
	// <Ansariel> Support for chat console
	static LLCachedControl<bool> chatHistoryTornOff(gSavedSettings, "ChatHistoryTornOff");
	if (FSUseNearbyChatConsole)
	{
		// <FS:Ansariel> [FS communication UI]
		//LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();
		// </FS:Ansariel> [FS communication UI]
		if (floater_container && !chatHistoryTornOff && !floater_container->getVisible())
		{
			// In case the nearby chat is docked into the IM floater and the
			// IM floater is invisible, always show the console.
			gConsole->setVisible(TRUE);
		}
		else
		{
			// In case the nearby chat is undocked OR docked and the IM floater
			// is visible, show console only if nearby chat is not visible.
			gConsole->setVisible(!getVisible());
		}
	}
	// </Ansariel> Support for chat console
}

void FSFloaterNearbyChat::onOpen(const LLSD& key )
{
	// <FS:Ansariel> [FS communication UI]
	//LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
	FSFloaterIMContainer* floater_container = FSFloaterIMContainer::getInstance();
	// </FS:Ansariel> [FS communication UI]
	if (floater_container)
	{
		if (gSavedSettings.getBOOL("ChatHistoryTornOff"))
		{
			// first set the tear-off host to this floater
			setHost(floater_container);
			// clear the tear-off host right after, the "last host used" will still stick
			setHost(NULL);
			// reparent the floater to the main view
			gFloaterView->addChild(this);
		}
		else
		{
			floater_container->addFloater(this, FALSE);
		}
	}

	// We override this to put nearbychat in the IM floater. -AO
	if(isChatMultiTab() && ! isVisible(this))
	{
		// only show the floater container if we are actually attached -Zi
		if (floater_container && !gSavedSettings.getBOOL("ChatHistoryTornOff"))
		{
			// make sure to show our parent floater, too
			floater_container->setVisible(TRUE);
			floater_container->showFloater(this, LLTabContainer::START);
		}
		setVisible(TRUE);
	}

	LLDockableFloater::onOpen(key);
}

void FSFloaterNearbyChat::setRect(const LLRect &rect)
{
	LLDockableFloater::setRect(rect);
}

void FSFloaterNearbyChat::getAllowedRect(LLRect& rect)
{
	rect = gViewerWindow->getWorldViewRectScaled();
}

// exported here for "clrchat" command line -Zi
void FSFloaterNearbyChat::clearChatHistory()
{
	mChatHistory->clear();
	// <FS:Ansariel> Optional muted chat history
	mChatHistoryMuted->clear();
}

void FSFloaterNearbyChat::updateChatHistoryStyle()
{
	clearChatHistory();

	LLSD do_not_log;
	do_not_log["do_not_log"] = true;
	for(std::vector<LLChat>::iterator it = mMessageArchive.begin();it!=mMessageArchive.end();++it)
	{
		// Update the messages without re-writing them to a log file.
		addMessage(*it,false, do_not_log);
	}
}

//static 
void FSFloaterNearbyChat::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
	FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
	if (nearby_chat)
	{
		nearby_chat->updateChatHistoryStyle();
	}
}

//static
bool FSFloaterNearbyChat::isWordsName(const std::string& name)
{
	// checking to see if it's display name plus username in parentheses 
	S32 open_paren = name.find(" (", 0);
	S32 close_paren = name.find(')', 0);

	if (open_paren != std::string::npos &&
		close_paren == name.length() - 1)
	{
		return true;
	}
	else
	{
		//checking for a single space
		S32 pos = name.find(' ', 0);
		return std::string::npos != pos && name.rfind(' ', name.length()) == pos && 0 != pos && name.length()-1 != pos;
	}
}

void FSFloaterNearbyChat::reloadMessages(bool clean_messages/* = false*/)
{
	if (clean_messages)
	{
		mMessageArchive.clear();
		loadHistory();
	}
	
	mChatHistory->clear();
	
	LLSD do_not_log;
	do_not_log["do_not_log"] = true;
	for(std::vector<LLChat>::iterator it = mMessageArchive.begin();it!=mMessageArchive.end();++it)
	{
		// Update the messages without re-writing them to a log file.
		addMessage(*it,false, do_not_log);
	}
}

void FSFloaterNearbyChat::loadHistory()
{
	LLSD do_not_log;
	do_not_log["do_not_log"] = true;

	std::list<LLSD> history;
	LLLogChat::loadChatHistory("chat", history);

	std::list<LLSD>::const_iterator it = history.begin();
	// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
	typedef enum e_im_type
	{
		IM_TYPE_NONE = 0,
		IM_TYPE_NORMAL,
		IM_TYPE_GROUP
	} EIMType;
	// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
	while (it != history.end())
	{
		EIMType im_type = IM_TYPE_NONE; // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
		const LLSD& msg = *it;

		std::string from = msg[LL_IM_FROM];
		std::string fromGroup = ""; // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
		LLUUID from_id;
		if (msg[LL_IM_FROM_ID].isDefined())
		{
			from_id = msg[LL_IM_FROM_ID].asUUID();
		}
		else
 		{
			// Ansariel: Strip IM prefix so we can properly
			//           retrieve the UUID in case we got a
			//           saved IM in nearby chat history.
			std::string im_prefix = LLTrans::getString("IMPrefix");
			size_t im_prefix_found = from.find(im_prefix);
			if (im_prefix_found != std::string::npos)
			{
				from = from.substr(im_prefix.length());
				// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
				//im_type = true;
				size_t group_im_prefix_start = from.find("[");
				size_t group_im_prefix_end = from.find("] ");
				if((group_im_prefix_start != std::string::npos) && (group_im_prefix_end != std::string::npos))
				{
					fromGroup = from.substr(group_im_prefix_start,group_im_prefix_end+2);
					from = from.substr(fromGroup.length());
					im_type = IM_TYPE_GROUP;
				}
				else
				{
					im_type = IM_TYPE_NORMAL;
				}
				// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
			}

			std::string legacy_name = gCacheName->buildLegacyName(from);
 			gCacheName->getUUID(legacy_name, from_id);
 		}

		LLChat chat;
		chat.mFromName = from;
		chat.mFromID = from_id;
		chat.mText = msg[LL_IM_TEXT].asString();
		chat.mTimeStr = msg[LL_IM_TIME].asString();
		chat.mChatStyle = CHAT_STYLE_HISTORY;

		// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
		//if (im_type) chat.mChatType = CHAT_TYPE_IM;
		if (im_type == IM_TYPE_NORMAL)
		{
			chat.mChatType = CHAT_TYPE_IM;
		}
		else if(im_type == IM_TYPE_GROUP)
		{
			chat.mChatType = CHAT_TYPE_IM_GROUP;
			chat.mFromNameGroup = fromGroup;
		}
		// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name

		chat.mSourceType = CHAT_SOURCE_AGENT;
		if (from_id.isNull() && SYSTEM_FROM == from)
		{	
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			
		}
		else if (from_id.isNull())
		{
			chat.mSourceType = isWordsName(from) ? CHAT_SOURCE_UNKNOWN : CHAT_SOURCE_OBJECT;
		}

		addMessage(chat, true, do_not_log);

		it++;
	}
}

//static
FSFloaterNearbyChat* FSFloaterNearbyChat::getInstance()
{
	return LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
}

bool FSFloaterNearbyChat::isChatMultiTab()
{
	// Restart is required in order to change chat window type.
	static bool is_single_window = gSavedSettings.getS32("ChatWindow") == 1;
	return is_single_window;
}

BOOL FSFloaterNearbyChat::getVisible()
{
	if (isChatMultiTab())
	{
		// <FS:Ansariel> [FS communication UI]
		//LLIMFloaterContainer* im_container = LLIMFloaterContainer::getInstance();
		FSFloaterIMContainer* im_container = FSFloaterIMContainer::getInstance();
		// </FS:Ansariel> [FS communication UI]
		
		// Treat inactive floater as invisible.
		bool is_active = im_container->getActiveFloater() == this;
		
		//torn off floater is always inactive
		if (!is_active && getHost() != im_container)
		{
			return LLDockableFloater::getVisible();
		}
		
		// getVisible() returns TRUE when Tabbed IM window is minimized.
		return is_active && !im_container->isMinimized() && im_container->getVisible();
	}
	else
	{
		return LLDockableFloater::getVisible();
	}
}

void FSFloaterNearbyChat::enableTranslationButton(bool enabled)
{
	getChildView("translate_btn")->setEnabled(enabled);
}

// virtual
BOOL FSFloaterNearbyChat::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;
	
	if( KEY_RETURN == key && mask == MASK_CONTROL)
	{
		// shout
		sendChat(CHAT_TYPE_SHOUT);
		handled = TRUE;
	}
	else if (KEY_RETURN == key && mask == MASK_SHIFT)
	{
		// whisper
		sendChat(CHAT_TYPE_WHISPER);
		handled = TRUE;
	}
	
	return handled;
}

BOOL FSFloaterNearbyChat::matchChatTypeTrigger(const std::string& in_str, std::string* out_str)
{
	U32 in_len = in_str.length();
	S32 cnt = sizeof(sChatTypeTriggers) / sizeof(*sChatTypeTriggers);
	
	bool string_was_found = false;
	
	for (S32 n = 0; n < cnt && !string_was_found; n++)
	{
		if (in_len <= sChatTypeTriggers[n].name.length())
		{
			std::string trigger_trunc = sChatTypeTriggers[n].name;
			LLStringUtil::truncate(trigger_trunc, in_len);
			
			if (!LLStringUtil::compareInsensitive(in_str, trigger_trunc))
			{
				*out_str = sChatTypeTriggers[n].name;
				string_was_found = true;
			}
		}
	}
	
	return string_was_found;
}

void FSFloaterNearbyChat::onChatBoxKeystroke()
{
	LLWString raw_text = mInputEditor->getWText();
	
	// Can't trim the end, because that will cause autocompletion
	// to eat trailing spaces that might be part of a gesture.
	LLWStringUtil::trimHead(raw_text);
	
	S32 length = raw_text.length();
	
	S32 channel=0;
	if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
		gSavedSettings.getBOOL("FSShowChatChannel"))
	{
		// <FS:Ansariel> [FS communication UI]
		//channel = (S32)(LLFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
		channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
		// </FS:Ansariel> [FS communication UI]
	}
	// -Zi
	
	//	if( (length > 0) && (raw_text[0] != '/') )  // forward slash is used for escape (eg. emote) sequences
	// [RLVa:KB] - Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.0d
	if ( (length > 0) && (raw_text[0] != '/') && (!(gSavedSettings.getBOOL("AllowMUpose") && raw_text[0] == ':')) && (!gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT)) )
		// [/RLVa:KB]
	{
		// only start typing animation if we are chatting without / on channel 0 -Zi
		if(channel==0)
			gAgent.startTyping();
	}
	else
	{
		gAgent.stopTyping();
	}
	
	KEY key = gKeyboard->currentKey();
	MASK mask = gKeyboard->currentMask(FALSE);
	
	// Ignore "special" keys, like backspace, arrows, etc.
	if (length > 1
		&& raw_text[0] == '/'
		&& key < KEY_SPECIAL
		&& gSavedSettings.getBOOL("FSChatbarGestureAutoCompleteEnable"))
	{
		// we're starting a gesture, attempt to autocomplete
		
		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);
		
		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			if (!rest_of_match.empty())
			{
				mInputEditor->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part
				
				// Select to end of line, starting from the character
				// after the last one the user typed.
				mInputEditor->selectNext(rest_of_match, false);
			}
		}
		else if (matchChatTypeTrigger(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			mInputEditor->setText(utf8_trigger + rest_of_match + " "); // keep original capitalization for user-entered part
			mInputEditor->endOfDoc();
		}
	}
	
	// <FS:CR> FIRE-3192 - Predictive name completion, based on code by Satomi Ahn
	static LLCachedControl<bool> sNameAutocomplete(gSavedSettings, "FSChatbarNamePrediction");
	if (length > NAME_PREDICTION_MINIMUM_LENGTH && sNameAutocomplete && key < KEY_SPECIAL && mask != MASK_CONTROL)
	{
		S32 cur_pos = mInputEditor->getCursorPos();
		if (cur_pos && (raw_text[cur_pos - 1] != ' '))
		{
			// Get a list of avatars within range
			std::vector<LLUUID> avatar_ids;
			std::vector<LLVector3d> positions;
			LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgent.getPositionGlobal(), gSavedSettings.getF32("NearMeRange"));
			
			if (avatar_ids.empty()) return; // Nobody's in range!
			
			// Parse text for a pattern to search
			std::string prefix = wstring_to_utf8str(raw_text.substr(0, cur_pos)); // Text before search string
			std::string suffix = "";
			if (cur_pos <= raw_text.length()) // Is there anything after the cursor?
			{
				suffix = wstring_to_utf8str(raw_text.substr(cur_pos)); // Text after search string
			}
			U32 last_space = prefix.rfind(" ");
			std::string pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1); // Search pattern
			
			prefix = prefix.substr(0, last_space + 1);
			std::string match_pattern = "";
			
			if (pattern.size() < NAME_PREDICTION_MINIMUM_LENGTH) return;
			
			match_pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1);
			prefix = prefix.substr(0, last_space + 1);
			std::string match = pattern;
			LLStringUtil::toLower(pattern);
			
			std::string name;
			bool found = false;
			bool full_name = false;
			std::vector<LLUUID>::iterator iter = avatar_ids.begin();
			
			if (last_space != std::string::npos && !prefix.empty())
			{
				last_space = prefix.substr(0, prefix.length() - 2).rfind(" ");
				match_pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1);
				prefix = prefix.substr(0, last_space + 1);
				
				// prepare search pattern
				std::string full_pattern(match_pattern + pattern);
				LLStringUtil::toLower(full_pattern);
				
				// Look for a match
				while (iter != avatar_ids.end() && !found)
				{
					if ((bool)gCacheName->getFullName(*iter++, name))
					{
						if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
						{
							name = RlvStrings::getAnonym(name);
						}
						LLStringUtil::toLower(name);
						found = (name.find(full_pattern) == 0);
					}
				}
			}
			
			if (found)
			{
				full_name = true; // ignore OnlyFirstName in case we want to disambiguate
				prefix += match_pattern;
			}
			else if (!pattern.empty()) // if first search did not work, try matching with last word before cursor only
			{
				prefix += match_pattern; // first part of the pattern wasn't a pattern, so keep it in prefix
				LLStringUtil::toLower(pattern);
				iter = avatar_ids.begin();
				
				// Look for a match
				while (iter != avatar_ids.end() && !found)
				{
					if ((bool)gCacheName->getFullName(*iter++, name))
					{
						if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
						{
							name = RlvStrings::getAnonym(name);
						}
						LLStringUtil::toLower(name);
						found = (name.find(pattern) == 0);
					}
				}
			}
			
			// if we found something by either method, replace the pattern by the avatar name
			if (found)
			{
				std::string first_name, last_name;
				gCacheName->getFirstLastName(*(iter - 1), first_name, last_name);
				std::string rest_of_match;
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				{
					prefix += RlvStrings::getAnonym(first_name + " " + last_name) + " ";
				}
				else
				{
					if (full_name)
					{
						rest_of_match = /*first_name + " " +*/ last_name.substr(pattern.size());
					}
					else
					{
						rest_of_match = first_name.substr(pattern.size());
					}
					prefix += match + rest_of_match + " ";
				}
				mInputEditor->setText(prefix + suffix);
				mInputEditor->selectNext(rest_of_match, false);
			}
		}
	}
	// </FS:CR>
}

// static
void FSFloaterNearbyChat::onChatBoxFocusLost()
{
	// stop typing animation
	gAgent.stopTyping();
}

void FSFloaterNearbyChat::onChatBoxFocusReceived()
{
	mInputEditor->setEnabled(!gDisconnected);
}

void FSFloaterNearbyChat::reshapeChatLayoutPanel()
{
	mChatLayoutPanel->reshape(mChatLayoutPanel->getRect().getWidth(), mInputEditor->getRect().getHeight() + mInputEditorPad, FALSE);
}

EChatType FSFloaterNearbyChat::processChatTypeTriggers(EChatType type, std::string &str)
{
	U32 length = str.length();
	S32 cnt = sizeof(sChatTypeTriggers) / sizeof(*sChatTypeTriggers);
	
	for (S32 n = 0; n < cnt; n++)
	{
		if (length >= sChatTypeTriggers[n].name.length())
		{
			std::string trigger = str.substr(0, sChatTypeTriggers[n].name.length());
			
			if (!LLStringUtil::compareInsensitive(trigger, sChatTypeTriggers[n].name))
			{
				U32 trigger_length = sChatTypeTriggers[n].name.length();
				
				// It's to remove space after trigger name
				if (length > trigger_length && str[trigger_length] == ' ')
					trigger_length++;
				
				str = str.substr(trigger_length, length);
				
				if (CHAT_TYPE_NORMAL == type)
					return sChatTypeTriggers[n].type;
				else
					break;
			}
		}
	}
	
	return type;
}

void FSFloaterNearbyChat::sendChat( EChatType type )
{
	if (mInputEditor)
	{
		LLWString text = mInputEditor->getWText();
		LLWStringUtil::trim(text);
		LLWStringUtil::replaceChar(text,182,'\n'); // Convert paragraph symbols back into newlines.
		if (!text.empty())
		{
			if(type == CHAT_TYPE_OOC)
			{
				std::string tempText = wstring_to_utf8str( text );
				tempText = gSavedSettings.getString("FSOOCPrefix") + " " + tempText + " " + gSavedSettings.getString("FSOOCPostfix");
				text = utf8str_to_wstring(tempText);
			}
			
			// Check if this is destined for another channel
			S32 channel = 0;
			stripChannelNumber(text, &channel);
			// If "/<number>" is not specified, see if a channel has been set in
			//  the spinner.
			if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
				gSavedSettings.getBOOL("FSShowChatChannel") &&
				(channel == 0))
			{
				channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
			}
			
			std::string utf8text = wstring_to_utf8str(text);
			// Try to trigger a gesture, if not chat to a script.
			std::string utf8_revised_text;
			if (0 == channel)
			{
				// Convert OOC and MU* style poses
				utf8text = applyAutoCloseOoc(utf8text);
				utf8text = applyMuPose(utf8text);
				
				// discard returned "found" boolean
				LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text);
			}
			else
			{
				utf8_revised_text = utf8text;
			}
			
			utf8_revised_text = utf8str_trim(utf8_revised_text);
			
			EChatType nType;
			if(type == CHAT_TYPE_OOC)
				nType = CHAT_TYPE_NORMAL;
			else
				nType = type;
			
			type = processChatTypeTriggers(nType, utf8_revised_text);
			
			if (!utf8_revised_text.empty() && cmd_line_chat(utf8_revised_text, type))
			{
				// Chat with animation
				sendChatFromViewer(utf8_revised_text, type, gSavedSettings.getBOOL("FSPlayChatAnimation"));
			}
		}
		
		mInputEditor->setText(LLStringExplicit(""));
	}
	
	gAgent.stopTyping();
	
	// If the user wants to stop chatting on hitting return, lose focus
	// and go out of chat mode.
	if (gSavedSettings.getBOOL("CloseChatOnReturn"))
	{
		stopChat();
	}
}

void FSFloaterNearbyChat::onChatBoxCommit()
{
	if (mInputEditor->getText().length() > 0)
	{
		sendChat(CHAT_TYPE_NORMAL);
	}
	
	gAgent.stopTyping();
}

void FSFloaterNearbyChat::sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate)
{
	sendChatFromViewer(utf8str_to_wstring(utf8text), type, animate);
}

void FSFloaterNearbyChat::sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate)
{
	S32 channel = 0;
	LLWString out_text = stripChannelNumber(wtext, &channel);
	// If "/<number>" is not specified, see if a channel has been set in
	//  the spinner.
	if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
		gSavedSettings.getBOOL("FSShowChatChannel") &&
		(channel == 0))
	{
		// <FS:Ansariel> [FS communication UI]
		//channel = (S32)(LLFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
		channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
		// </FS:Ansariel> [FS communication UI]
	}
	std::string utf8_out_text = wstring_to_utf8str(out_text);
	std::string utf8_text = wstring_to_utf8str(wtext);
	
	utf8_text = utf8str_trim(utf8_text);
	if (!utf8_text.empty())
	{
		utf8_text = utf8str_truncate(utf8_text, MAX_STRING - 1);
	}
	
	// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0b
	if ( (0 == channel) && (rlv_handler_t::isEnabled()) )
	{
		// Adjust the (public) chat "volume" on chat and gestures (also takes care of playing the proper animation)
		if ( ((CHAT_TYPE_SHOUT == type) || (CHAT_TYPE_NORMAL == type)) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATNORMAL)) )
			type = CHAT_TYPE_WHISPER;
		else if ( (CHAT_TYPE_SHOUT == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATSHOUT)) )
			type = CHAT_TYPE_NORMAL;
		else if ( (CHAT_TYPE_WHISPER == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATWHISPER)) )
			type = CHAT_TYPE_NORMAL;
		
		animate &= !gRlvHandler.hasBehaviour( (!RlvUtil::isEmote(utf8_text)) ? RLV_BHVR_REDIRCHAT : RLV_BHVR_REDIREMOTE );
	}
	// [/RLVa:KB]
	
	// Don't animate for chats people can't hear (chat to scripts)
	if (animate && (channel == 0))
	{
		if (type == CHAT_TYPE_WHISPER)
		{
			lldebugs << "You whisper " << utf8_text << llendl;
			gAgent.sendAnimationRequest(ANIM_AGENT_WHISPER, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_NORMAL)
		{
			lldebugs << "You say " << utf8_text << llendl;
			gAgent.sendAnimationRequest(ANIM_AGENT_TALK, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_SHOUT)
		{
			lldebugs << "You shout " << utf8_text << llendl;
			gAgent.sendAnimationRequest(ANIM_AGENT_SHOUT, ANIM_REQUEST_START);
		}
		else
		{
			llinfos << "send_chat_from_viewer() - invalid volume" << llendl;
			return;
		}
	}
	else
	{
		if (type != CHAT_TYPE_START && type != CHAT_TYPE_STOP)
		{
			lldebugs << "Channel chat: " << utf8_text << llendl;
		}
	}
	
	send_chat_from_nearby_floater(utf8_out_text, type, channel);
}

// Exit "chat mode" and do the appropriate focus changes
// static
void FSFloaterNearbyChat::stopChat()
{
	FSFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("nearby_chat");
	if (nearby_chat)
	{
		nearby_chat->mInputEditor->setFocus(FALSE);
	    gAgent.stopTyping();
	}
}

// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
// Otherwise returns input and channel 0.
LLWString FSFloaterNearbyChat::stripChannelNumber(const LLWString &mesg, S32* channel)
{
	if (mesg[0] == '/'
		&& mesg[1] == '/')
	{
		// This is a "repeat channel send"
		*channel = sLastSpecialChatChannel;
		return mesg.substr(2, mesg.length() - 2);
	}
	else if (mesg[0] == '/'
			 && mesg[1]
			 && LLStringOps::isDigit(mesg[1]))
	{
		// This a special "/20" speak on a channel
		S32 pos = 0;
		
		// Copy the channel number into a string
		LLWString channel_string;
		llwchar c;
		do
		{
			c = mesg[pos+1];
			channel_string.push_back(c);
			pos++;
		}
		while(c && pos < 64 && LLStringOps::isDigit(c));
		
		// Move the pointer forward to the first non-whitespace char
		// Check isspace before looping, so we can handle "/33foo"
		// as well as "/33 foo"
		while(c && iswspace(c))
		{
			c = mesg[pos+1];
			pos++;
		}
		
		sLastSpecialChatChannel = strtol(wstring_to_utf8str(channel_string).c_str(), NULL, 10);
		*channel = sLastSpecialChatChannel;
		return mesg.substr(pos, mesg.length() - pos);
	}
	else
	{
		// This is normal chat.
		*channel = 0;
		return mesg;
	}
}

//void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel)
// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-0.2.2a
void send_chat_from_nearby_floater(std::string utf8_out_text, EChatType type, S32 channel)
// [/RLVa:KB]
{
	// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0a
	// Only process chat messages (ie not CHAT_TYPE_START, CHAT_TYPE_STOP, etc)
	if ( (rlv_handler_t::isEnabled()) && ( (CHAT_TYPE_WHISPER == type) || (CHAT_TYPE_NORMAL == type) || (CHAT_TYPE_SHOUT == type) ) )
	{
		if (0 == channel)
		{
			// (We already did this before, but LLChatHandler::handle() calls this directly)
			if ( ((CHAT_TYPE_SHOUT == type) || (CHAT_TYPE_NORMAL == type)) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATNORMAL)) )
				type = CHAT_TYPE_WHISPER;
			else if ( (CHAT_TYPE_SHOUT == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATSHOUT)) )
				type = CHAT_TYPE_NORMAL;
			else if ( (CHAT_TYPE_WHISPER == type) && (gRlvHandler.hasBehaviour(RLV_BHVR_CHATWHISPER)) )
				type = CHAT_TYPE_NORMAL;
			
			// Redirect chat if needed
			if ( ( (gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT) || (gRlvHandler.hasBehaviour(RLV_BHVR_REDIREMOTE)) ) &&
				  (gRlvHandler.redirectChatOrEmote(utf8_out_text)) ) )
			{
				return;
			}
			
			// Filter public chat if sendchat restricted
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHAT))
				gRlvHandler.filterChat(utf8_out_text, true);
		}
		else
		{
			// Don't allow chat on a non-public channel if sendchannel restricted (unless the channel is an exception)
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHANNEL)) && (!gRlvHandler.isException(RLV_BHVR_SENDCHANNEL, channel)) )
				return;
			
			// Don't allow chat on debug channel if @sendchat, @redirchat or @rediremote restricted (shows as public chat on viewers)
			if (CHAT_CHANNEL_DEBUG == channel)
			{
				bool fIsEmote = RlvUtil::isEmote(utf8_out_text);
				if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHAT)) ||
					((!fIsEmote) && (gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT))) ||
					((fIsEmote) && (gRlvHandler.hasBehaviour(RLV_BHVR_REDIREMOTE))) )
				{
					return;
				}
			}
		}
	}
	// [/RLVa:KB]
	
	//<FS:TS> FIRE-787: break up too long chat lines into multiple messages
	U32 split = MAX_MSG_BUF_SIZE - 1;
	U32 pos = 0;
	U32 total = utf8_out_text.length();
	
	// Don't break null messages
	if(total == 0)
	{
		really_send_chat_from_nearby_floater(utf8_out_text, type, channel);
	}
	
	while(pos < total)
	{
		U32 next_split = split;
		
		if (pos + next_split > total)
		{
			// just send the rest of the message
			next_split = total - pos;
		}
		else
		{
			// first, try to split at a space
			while((U8(utf8_out_text[pos + next_split]) != ' ')
				  && (next_split > 0))
			{
				--next_split;
			}
			
			if (next_split == 0)
			{
				next_split = split;
				// no space found, split somewhere not in the middle of UTF-8
				while((U8(utf8_out_text[pos + next_split]) >= 0x80)
					  && (U8(utf8_out_text[pos + next_split]) < 0xC0)
					  && (next_split > 0))
				{
					--next_split;
				}
			}
			
			if(next_split == 0)
			{
				next_split = split;
				LL_WARNS("Splitting") << "utf-8 couldn't be split correctly" << LL_ENDL;
			}
		}
		
		std::string send = utf8_out_text.substr(pos, next_split);
		pos += next_split;
		
		// *FIXME: Queue messages and wait for server
		really_send_chat_from_nearby_floater(send, type, channel);
	}
	
	// moved here so we don't bump the count for every message segment
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_CHAT_COUNT);
	//</FS:TS> FIRE-787
}

//<FS:TS> FIRE-787: break up too long chat lines into multiple messages
// This function just sends the message, with no other processing. Moved out
//	of send_chat_from_viewer.
void really_send_chat_from_nearby_floater(std::string utf8_out_text, EChatType type, S32 channel)
{
	LLMessageSystem* msg = gMessageSystem;
	
	// <FS:ND> gMessageSystem can be 0, not sure how it is exactly to reproduce, maybe during viewer shutdown?
	if( !msg )
		return;
	// </FS:ND>
	
	if(channel >= 0)
	{
		msg->newMessageFast(_PREHASH_ChatFromViewer);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ChatData);
		msg->addStringFast(_PREHASH_Message, utf8_out_text);
		msg->addU8Fast(_PREHASH_Type, type);
		msg->addS32("Channel", channel);
	}
	else
	{
		msg->newMessage("ScriptDialogReply");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("ObjectID", gAgent.getID());
		msg->addS32("ChatChannel", channel);
		msg->addS32("ButtonIndex", 0);
		msg->addString("ButtonLabel", utf8_out_text);
	}
	
	gAgent.sendReliableMessage();
}
//</FS:TS> FIRE-787

class LLChatCommandHandler : public LLCommandHandler
{
public:
	// not allowed from outside the app
	LLChatCommandHandler() : LLCommandHandler("chat", UNTRUSTED_BLOCK) { }
	
    // Your code here
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		bool retval = false;
		// Need at least 2 tokens to have a valid message.
		if (tokens.size() < 2)
		{
			retval = false;
		}
		else
		{
			S32 channel = tokens[0].asInteger();
			// VWR-19499 Restrict function to chat channels greater than 0.
			if ((channel > 0) && (channel < CHAT_CHANNEL_DEBUG))
			{
				retval = true;
				// Send unescaped message, see EXT-6353.
				std::string unescaped_mesg (LLURI::unescape(tokens[1].asString()));
				send_chat_from_nearby_floater(unescaped_mesg, CHAT_TYPE_NORMAL, channel);
			}
			else
			{
				retval = false;
				// Tell us this is an unsupported SLurl.
			}
		}
		return retval;
	}
};
