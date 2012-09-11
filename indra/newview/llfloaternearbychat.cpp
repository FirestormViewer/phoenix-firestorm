/** 
 * @file llfloaternearbychat.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaternearbychat.h"

#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llrootview.h"
//#include "llchatitemscontainerctrl.h"
#include "lliconctrl.h"
#include "llspinctrl.h"
#include "llfloatersidepanelcontainer.h"
#include "llfocusmgr.h"
#include "lllogchat.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llmenugl.h"
#include "llviewermenu.h"//for gMenuHolder

#include "llnearbychathandler.h"
// #include "llnearbychatbar.h"	// <FS:Zi> Remove floating chat bar
#include "llnearbychathub.h"
#include "llchannelmanager.h"

#include "llagent.h" 			// gAgent
#include "llchathistory.h"
#include "llstylemap.h"

#include "llavatarnamecache.h"

#include "lldraghandle.h"

// #include "llnearbychatbar.h"	// <FS:Zi> Remove floating chat bar
#include "llfloaterreg.h"
#include "lltrans.h"

// IM
#include "llbutton.h"
#include "lllayoutstack.h"

#include "llimfloatercontainer.h"
#include "llimfloater.h"
#include "lllineeditor.h"

//AO - includes for textentry
#include "rlvhandler.h"
#include "llcommandhandler.h"
#include "llkeyboard.h"
#include "llgesturemgr.h"
#include "llmultigesture.h"

#include "llconsole.h"

// <FS:Zi> Moved nearby chat functionality here for now
#include "chatbar_as_cmdline.h"
#include "llanimationstates.h"	// ANIM_AGENT_WHISPER, ANIM_AGENT_TALK, ANIM_AGENT_SHOUT
#include "llviewerstats.h"
// </FS:Zi>

static const S32 RESIZE_BAR_THICKNESS = 3;

// ## Zi // static LLRegisterPanelClassWrapper<LLNearbyChat> t_panel_nearby_chat("panel_nearby_chat");

LLFloaterNearbyChat::LLFloaterNearbyChat(const LLSD& key) 
	: LLDockableFloater(NULL, false, false, key)
	,mChatHistory(NULL)
	,mInputEditor(NULL)
	// <FS:Ansariel> Optional muted chat history
	,mChatHistoryMuted(NULL)
{
}

LLFloaterNearbyChat::~LLFloaterNearbyChat()
{
}

void LLFloaterNearbyChat::updateFSUseNearbyChatConsole(const LLSD &data)
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


BOOL LLFloaterNearbyChat::postBuild()
{
	//menu
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	enable_registrar.add("NearbyChat.Check", boost::bind(&LLFloaterNearbyChat::onNearbyChatCheckContextMenuItem, this, _2));
	registrar.add("NearbyChat.Action", boost::bind(&LLFloaterNearbyChat::onNearbyChatContextMenuItemClicked, this, _2));

	
	LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_nearby_chat.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if(menu)
		mPopupMenuHandle = menu->getHandle();

	gSavedSettings.declareS32("nearbychat_showicons_and_names",2,"NearByChat header settings",true);

	mInputEditor = getChild<LLLineEditor>("chat_box");

	mInputEditor->setEnabled(TRUE);
	
	gSavedSettings.getControl("FSNearbyChatbar")->getCommitSignal()->connect(boost::bind(&LLFloaterNearbyChat::onChatBarVisibilityChanged, this));
	gSavedSettings.getControl("FSShowChatChannel")->getCommitSignal()->connect(boost::bind(&LLFloaterNearbyChat::onChatChannelVisibilityChanged, this));

	onChatBarVisibilityChanged();
	onChatChannelVisibilityChanged();

	// extra icon controls -AO
	LLButton* transl = getChild<LLButton>("translate_btn");
	transl->setVisible(true);

	childSetCommitCallback("chat_history_btn",onHistoryButtonClicked,this);

	mChatHistory = getChild<LLChatHistory>("chat_history");

	// <FS:Ansariel> Optional muted chat history
	mChatHistoryMuted = getChild<LLChatHistory>("chat_history_muted");
	
	// <vertical tab docking> -AO
	if(isChatMultiTab())
	{
		LLButton* slide_left = getChild<LLButton>("slide_left_btn");
		slide_left->setVisible(false);
		LLButton* slide_right = getChild<LLButton>("slide_right_btn");
		slide_right->setVisible(false);

		FSUseNearbyChatConsole = gSavedSettings.getBOOL("FSUseNearbyChatConsole");
		gSavedSettings.getControl("FSUseNearbyChatConsole")->getSignal()->connect(boost::bind(&LLFloaterNearbyChat::updateFSUseNearbyChatConsole, this, _2));
		
		return LLDockableFloater::postBuild();
	}
	
	if(!LLDockableFloater::postBuild())
		return false;

	return true;
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


void	LLFloaterNearbyChat::addMessage(const LLChat& chat,bool archive,const LLSD &args)
{
	LLChat& tmp_chat = const_cast<LLChat&>(chat);
	bool use_plain_text_chat_history = gSavedSettings.getBOOL("PlainTextChatHistory");
	bool hide_timestamps_nearby_chat = gSavedSettings.getBOOL("FSHideTimestampsNearbyChat");
	// [FIRE-1641 : SJ]: Option to hide timestamps in nearby chat - only add Timestamp when hide_timestamps_nearby_chat is not TRUE
	if (!hide_timestamps_nearby_chat)
	{
		if(tmp_chat.mTimeStr.empty())
			tmp_chat.mTimeStr = appendTime();
	}

	
	// <FS:Ansariel> Optional muted chat history
	tmp_chat.mFromName = chat.mFromName;
	LLSD chat_args = args;
	chat_args["use_plain_text_chat_history"] = use_plain_text_chat_history;
	chat_args["hide_timestamps_nearby_chat"] = hide_timestamps_nearby_chat;
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

	if(archive)
	{
		mMessageArchive.push_back(chat);
		if(mMessageArchive.size()>200)
			mMessageArchive.erase(mMessageArchive.begin());
	}

	// <FS:Ansariel> Optional muted chat history
	//if (args["do_not_log"].asBoolean()) 
	if (args["do_not_log"].asBoolean() || chat.mMuted) 
	// </FS:Ansariel> Optional muted chat history
	{
		return;
	}
	
	// AO: IF tab mode active, flash our tab
	if(isChatMultiTab())
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

			if (!av_name.mIsDisplayNameDefault)
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
					//from_name = "IM: " + from_name;
					if(chat.mChatType == CHAT_TYPE_IM_GROUP && chat.mFromNameGroup != "")
					{
						from_name = "IM: " + chat.mFromNameGroup + from_name;
					}
					else
					{
						from_name = "IM: " + from_name;
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
BOOL LLFloaterNearbyChat::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash)
{
	mInputEditor->setFocus(TRUE);
	onTabInto();
	if(focus_flash)
	{
		gFocusMgr.triggerFocusFlash();
	}
	return TRUE;
}

void LLFloaterNearbyChat::onNearbySpeakers()
{
	LLSD param;
	param["people_panel_tab_name"] = "nearby_panel";
	LLFloaterSidePanelContainer::showPanel("people", "panel_people", param);
}

// static
void LLFloaterNearbyChat::onHistoryButtonClicked(LLUICtrl* ctrl, void* userdata)
{
	gViewerWindow->getWindow()->openFile(LLLogChat::makeLogFileName("chat"));
}

void	LLFloaterNearbyChat::onNearbyChatContextMenuItemClicked(const LLSD& userdata)
{
}

bool	LLFloaterNearbyChat::onNearbyChatCheckContextMenuItem(const LLSD& userdata)
{
	std::string str = userdata.asString();
	if(str == "nearby_people")
		onNearbySpeakers();	
	return false;
}

void LLFloaterNearbyChat::onChatBarVisibilityChanged()
{
	getChild<LLLayoutPanel>("chat_bar_visibility_panel")->setVisible(gSavedSettings.getBOOL("FSNearbyChatbar"));
}

void LLFloaterNearbyChat::onChatChannelVisibilityChanged()
{
	getChild<LLLayoutPanel>("channel_spinner_visibility_panel")->setVisible(gSavedSettings.getBOOL("FSShowChatChannel"));
}

void	LLFloaterNearbyChat::openFloater(const LLSD& key)
{
	// We override this to put nearbychat in the IM floater. -AO
	if(isChatMultiTab())
	{
		LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		// only show the floater container if we are actually attached -Zi
		if (floater_container && !gSavedSettings.getBOOL("ChatHistoryTornOff"))
		{
			floater_container->showFloater(this, LLTabContainer::START);
		}
		setVisible(TRUE);
		LLFloater::openFloater(key);
	}
}

void LLFloaterNearbyChat::removeScreenChat()
{
	LLNotificationsUI::LLScreenChannelBase* chat_channel = LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
	if(chat_channel)
	{
		chat_channel->removeToastsFromChannel();
	}
}

void	LLFloaterNearbyChat::setVisible(BOOL visible)
{
	if(visible)
	{
		removeScreenChat();
	}
	LLDockableFloater::setVisible(visible);
	
	// <Ansariel> Support for chat console
	static LLCachedControl<bool> chatHistoryTornOff(gSavedSettings, "ChatHistoryTornOff");
	if (FSUseNearbyChatConsole)
	{
		LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
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

void LLFloaterNearbyChat::onFocusReceived()
{
	// <FS:Ansariel> Give focus to input textbox
	mInputEditor->setFocus(TRUE);
}

void	LLFloaterNearbyChat::onOpen(const LLSD& key )
{
	LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
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

void LLFloaterNearbyChat::setRect	(const LLRect &rect)
{
	LLDockableFloater::setRect(rect);
}

void LLFloaterNearbyChat::getAllowedRect(LLRect& rect)
{
	rect = gViewerWindow->getWorldViewRectScaled();
}

// exported here for "clrchat" command line -Zi
void LLFloaterNearbyChat::clearChatHistory()
{
	mChatHistory->clear();
	// <FS:Ansariel> Optional muted chat history
	mChatHistoryMuted->clear();
}

void LLFloaterNearbyChat::updateChatHistoryStyle()
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
void LLFloaterNearbyChat::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
	LLFloaterNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterNearbyChat>("nearby_chat", LLSD());
	if(nearby_chat)
		nearby_chat->updateChatHistoryStyle();
}

bool isWordsName(const std::string& name)
{
	// checking to see if it's display name plus username in parentheses 
	S32 open_paren = name.find(" (", 0);
	S32 close_paren = name.find(')', 0);

	if (open_paren != std::string::npos &&
		close_paren == name.length()-1)
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

void LLFloaterNearbyChat::loadHistory()
{
	LLSD do_not_log;
	do_not_log["do_not_log"] = true;

	std::list<LLSD> history;
	LLLogChat::loadAllHistory("chat", history);

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

		std::string from = msg[IM_FROM];
		std::string fromGroup = ""; // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
		LLUUID from_id;
		if (msg[IM_FROM_ID].isDefined())
		{
			from_id = msg[IM_FROM_ID].asUUID();
		}
		else
 		{
			// Ansariel: Strip IM prefix so we can properly
			//           retrieve the UUID in case we got a
			//           saved IM in nearby chat history.
			std::string im_prefix = "IM: ";
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
		chat.mText = msg[IM_TEXT].asString();
		chat.mTimeStr = msg[IM_TIME].asString();
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
LLFloaterNearbyChat* LLFloaterNearbyChat::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLFloaterNearbyChat>("nearby_chat", LLSD());
}

bool LLFloaterNearbyChat::isChatMultiTab()
{
	// Restart is required in order to change chat window type.
	static bool is_single_window = gSavedSettings.getS32("ChatWindow") == 1;
	return is_single_window;
}

BOOL LLFloaterNearbyChat::getVisible()
{
	if(isChatMultiTab())
	{
		LLIMFloaterContainer* im_container = LLIMFloaterContainer::getInstance();
		
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
