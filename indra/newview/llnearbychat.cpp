/** 
 * @file LLNearbyChat.cpp
 * @brief Nearby chat history scrolling panel implementation
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

#include "llnearbychat.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llrootview.h"
//#include "llchatitemscontainerctrl.h"
#include "lliconctrl.h"
#include "llsidetray.h"
#include "llfocusmgr.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llmenugl.h"
#include "llviewermenu.h"//for gMenuHolder

#include "llnearbychathandler.h"
#include "llchannelmanager.h"

#include "llagent.h" 			// gAgent
#include "llchathistory.h"
#include "llstylemap.h"

#include "llavatarnamecache.h"

#include "lldraghandle.h"

#include "llbottomtray.h"
#include "llnearbychatbar.h"
#include "llfloaterreg.h"
#include "lltrans.h"

// IM
#include "llbutton.h"
#include "lllayoutstack.h"

#include "llimfloatercontainer.h"
#include "llimfloater.h"
#include "lllineeditor.h"

static const S32 RESIZE_BAR_THICKNESS = 3;

LLNearbyChat::LLNearbyChat(const LLSD& key) 
	: LLDockableFloater(NULL, false, false, key)
	,mChatHistory(NULL)
{
	
}

LLNearbyChat::~LLNearbyChat()
{
}


BOOL LLNearbyChat::postBuild()
{
	//menu
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	enable_registrar.add("NearbyChat.Check", boost::bind(&LLNearbyChat::onNearbyChatCheckContextMenuItem, this, _2));
	registrar.add("NearbyChat.Action", boost::bind(&LLNearbyChat::onNearbyChatContextMenuItemClicked, this, _2));

	
	LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_nearby_chat.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if(menu)
		mPopupMenuHandle = menu->getHandle();

	gSavedSettings.declareS32("nearbychat_showicons_and_names",2,"NearByChat header settings",true);

	// extra icon controls -AO
	LLButton* transl = getChild<LLButton>("translate_btn");
	transl->setVisible(true);
	
	mChatHistory = getChild<LLChatHistory>("chat_history");
	
	// <vertical tab docking> -AO
	if(isChatMultiTab())
	{
			
		llinfos << "nearbychat postBuild multitab" << llendl;
		LLButton* slide_left = getChild<LLButton>("slide_left_btn");
		slide_left->setVisible(false);
		LLButton* slide_right = getChild<LLButton>("slide_right_btn");
		slide_right->setVisible(false);
		
		
		if (getDockControl() == NULL)
		{
			llinfos << "adding to multitab" << llendl;
			LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
			LLTabContainer::eInsertionPoint i_pt = LLTabContainer::START;
			if (floater_container)
			{
				floater_container->addFloater(this, TRUE, i_pt);
			}
		}
		
		
		return LLDockableFloater::postBuild();
	}
	
	if(!LLDockableFloater::postBuild())
		return false;

	if (getDockControl() == NULL)
	{
		setDockControl(new LLDockControl(
			LLBottomTray::getInstance()->getNearbyChatBar(), this,
			getDockTongue(), LLDockControl::TOP, boost::bind(&LLNearbyChat::getAllowedRect, this, _1)));
	}

        //fix for EXT-4621 
        //chrome="true" prevents floater from stilling capture
        setIsChrome(true);
	//chrome="true" hides floater caption 
	if (mDragHandle)
		mDragHandle->setTitleVisible(TRUE);

	return true;
	
}
	mInputEditor = getChild<LLLineEditor>("chat_box");
	mInputEditor->setMaxTextLength(1023);
	// enable line history support for instant message bar
	mInputEditor->setEnableLineHistory(TRUE);
	
	
	mInputEditor->setFocusReceivedCallback( boost::bind(onInputEditorFocusReceived, _1, this) );
	mInputEditor->setFocusLostCallback( boost::bind(onInputEditorFocusLost, _1, this) );
	mInputEditor->setKeystrokeCallback( onInputEditorKeystroke, this );
	mInputEditor->setCommitOnFocusLost( FALSE );
	mInputEditor->setRevertOnEsc( FALSE );
	mInputEditor->setReplaceNewlinesWithSpaces( FALSE );
	mInputEditor->setPassDelete( TRUE );

	childSetCommitCallback("chat_box", onSendMsg, this);
	
	mTypingStart = LLTrans::getString("IM_typing_start_string");

void    LLNearbyChat::applySavedVariables()
{
	if (mRectControl.size() > 1)
	{
		const LLRect& rect = LLFloater::getControlGroup()->getRect(mRectControl);
		if(!rect.isEmpty() && rect.isValid())
		{
			reshape(rect.getWidth(), rect.getHeight());
			setRect(rect);
		}
	}


	if(!LLFloater::getControlGroup()->controlExists(mDocStateControl))
	{
		setDocked(true);
	}
	else
	{
		if (mDocStateControl.size() > 1)
		{
			bool dockState = LLFloater::getControlGroup()->getBOOL(mDocStateControl);
			setDocked(dockState);
		}
	}
}



std::string appendTime()
{
	time_t utc_time;
	utc_time = time_corrected();
	std::string timeStr ="["+ LLTrans::getString("TimeHour")+"]:["
		+LLTrans::getString("TimeMin")+"]";

	LLSD substitution;

	substitution["datetime"] = (S32) utc_time;
	LLStringUtil::format (timeStr, substitution);

	return timeStr;
}


void	LLNearbyChat::addMessage(const LLChat& chat,bool archive,const LLSD &args)
{
	LLChat& tmp_chat = const_cast<LLChat&>(chat);

	if(tmp_chat.mTimeStr.empty())
		tmp_chat.mTimeStr = appendTime();

	bool use_plain_text_chat_history = gSavedSettings.getBOOL("PlainTextChatHistory");
	
	if (!chat.mMuted)
	{
		tmp_chat.mFromName = chat.mFromName;
		LLSD chat_args = args;
		chat_args["use_plain_text_chat_history"] = use_plain_text_chat_history;
		mChatHistory->appendMessage(chat, chat_args);
	}

	if(archive)
	{
		mMessageArchive.push_back(chat);
		if(mMessageArchive.size()>200)
			mMessageArchive.erase(mMessageArchive.begin());
	}

	if (args["do_not_log"].asBoolean()) 
	{
		return;
	}
	
	// IF tab mode active, flash our tab
	if(isChatMultiTab())
	{
		LLSD notification;
		notification["session_id"] = getKey();
		LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		floater_container->onNewMessageReceived(notification);
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
		}

		LLLogChat::saveHistory("chat", from_name, chat.mFromID, chat.mText);
	}
}

void LLNearbyChat::onNearbySpeakers()
{
	LLSD param;
	param["people_panel_tab_name"] = "nearby_panel";
	LLSideTray::getInstance()->showPanel("panel_people",param);
}

void	LLNearbyChat::onNearbyChatContextMenuItemClicked(const LLSD& userdata)
{
}

bool	LLNearbyChat::onNearbyChatCheckContextMenuItem(const LLSD& userdata)
{
	std::string str = userdata.asString();
	if(str == "nearby_people")
		onNearbySpeakers();	
	return false;
}

void	LLNearbyChat::openFloater(const LLSD& key)
{
	// We override this to put nearbychat in the IM floater. -AO
	if(isChatMultiTab())
	{
		LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		if (floater_container)
		{
			floater_container->showFloater(this, LLTabContainer::START);
		}
		setVisible(TRUE);
		LLFloater::openFloater(key);
	}
}

void	LLNearbyChat::setVisible(BOOL visible)
{
	if(visible)
	{
		LLNotificationsUI::LLScreenChannelBase* chat_channel = LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
		if(chat_channel)
		{
			chat_channel->removeToastsFromChannel();
		}
	}
	LLDockableFloater::setVisible(visible);
}

void	LLNearbyChat::onOpen(const LLSD& key )
{
	llinfos << "onOPEN called..." << llendl;
	// We override this to put nearbychat in the IM floater. -AO
	if(isChatMultiTab() && ! isVisible(this))
	{
		LLIMFloaterContainer* floater_container = LLIMFloaterContainer::getInstance();
		if (floater_container)
		{
			floater_container->showFloater(this, LLTabContainer::START);
		}
		setVisible(TRUE);
	}
	
	LLDockableFloater::onOpen(key);
}



void LLNearbyChat::setRect	(const LLRect &rect)
{
	LLDockableFloater::setRect(rect);
}

void LLNearbyChat::getAllowedRect(LLRect& rect)
{
	rect = gViewerWindow->getWorldViewRectScaled();
}

void LLNearbyChat::updateChatHistoryStyle()
{
	mChatHistory->clear();
	for(std::vector<LLChat>::iterator it = mMessageArchive.begin();it!=mMessageArchive.end();++it)
	{
		addMessage(*it,false);
	}
}

//static 
void LLNearbyChat::processChatHistoryStyleUpdate(const LLSD& newvalue)
{
	LLNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
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
void LLNearbyChat::onInputEditorFocusReceived( LLFocusableElement* caller, void* userdata )
{
	 LLNearbyChat* self= (LLNearbyChat*) userdata;
}
void LLNearbyChat::onInputEditorFocusLost(LLFocusableElement* caller, void* userdata)
{
	LLNearbyChat* self = (LLNearbyChat*) userdata;
	self->setTyping(false);
}
void LLNearbyChat::onInputEditorKeystroke(LLLineEditor* caller, void* userdata)
{
	LLNearbyChat* self = (LLNearbyChat*)userdata;
	std::string text = self->mInputEditor->getText();
	if (!text.empty())
	{
		self->setTyping(true);
	}
	else
	{
		// Deleting all text counts as stopping typing.
		self->setTyping(false);
	}
}
void LLNearbyChat::onSendMsg( LLUICtrl* ctrl, void* userdata )
{
	LLNearbyChat* self = (LLNearbyChat*) userdata;
	llinfos << "DEBUG: Sending message" << llendl;
	self->setTyping(false);
}
void LLNearbyChat::loadHistory()
{
	LLSD do_not_log;
	do_not_log["do_not_log"] = true;

	std::list<LLSD> history;
	LLLogChat::loadAllHistory("chat", history);

	std::list<LLSD>::const_iterator it = history.begin();
	while (it != history.end())
	{
		const LLSD& msg = *it;

		std::string from = msg[IM_FROM];
		LLUUID from_id;
		if (msg[IM_FROM_ID].isDefined())
		{
			from_id = msg[IM_FROM_ID].asUUID();
		}
		else
 		{
			std::string legacy_name = gCacheName->buildLegacyName(from);
 			gCacheName->getUUID(legacy_name, from_id);
 		}

		LLChat chat;
		chat.mFromName = from;
		chat.mFromID = from_id;
		chat.mText = msg[IM_TEXT].asString();
		chat.mTimeStr = msg[IM_TIME].asString();
		chat.mChatStyle = CHAT_STYLE_HISTORY;

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
LLNearbyChat* LLNearbyChat::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
}

bool LLNearbyChat::isChatMultiTab()
{
	// Restart is required in order to change chat window type.
	static bool is_single_window = gSavedSettings.getS32("ChatWindow") == 1;
	return is_single_window;
}

void LLNearbyChat::setDocked(bool docked, bool pop_on_undock)
{
	if(!isChatMultiTab())
	{
		LLDockableFloater::setDocked(docked, pop_on_undock);
	}
}

BOOL LLNearbyChat::getVisible()
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

////////////////////////////////////////////////////////////////////////////////
//
void LLNearbyChat::onFocusReceived()
{
	setBackgroundOpaque(true);
	LLPanel::onFocusReceived();
}

////////////////////////////////////////////////////////////////////////////////
//
void LLNearbyChat::onFocusLost()
{
	setBackgroundOpaque(false);
	LLPanel::onFocusLost();
}

BOOL	LLNearbyChat::handleMouseDown(S32 x, S32 y, MASK mask)
{
	//fix for EXT-6625
	//highlight NearbyChat history whenever mouseclick happen in NearbyChat
	//setting focus to eidtor will force onFocusLost() call that in its turn will change 
	//background opaque. This all happenn since NearByChat is "chrome" and didn't process focus change.
	
	if(mChatHistory)
		mChatHistory->setFocus(TRUE);
	return LLDockableFloater::handleMouseDown(x, y, mask);
}
