/** 
 * @file llimhandler.cpp
 * @brief Notification Handler Class for IM notifications
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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


#include "llviewerprecompiledheaders.h" // must be first include

#include "llnotificationhandler.h"

#include "llagentdata.h"
#include "llnotifications.h"
#include "lltoastimpanel.h"
#include "llviewerwindow.h"

#include "llavatarnamecache.h"
#include "llconsole.h"
#include "llfloaterreg.h"
#include "llnearbychat.h"
#include "rlvhandler.h"
#include "lggcontactsets.h"

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLIMHandler::LLIMHandler(e_notification_type type, const LLSD& id)
{
	mType = type;

	// Getting a Channel for our notifications
	mChannel = LLChannelManager::getInstance()->createNotificationChannel();

	PhoenixLogImToChatConsole = gSavedSettings.getBOOL("PhoenixLogImToChatConsole");
	gSavedSettings.getControl("PhoenixLogImToChatConsole")->getSignal()->connect(boost::bind(&LLIMHandler::updatePhoenixLogImToChatConsole, this, _2));
}

//--------------------------------------------------------------------------
LLIMHandler::~LLIMHandler()
{
}

//--------------------------------------------------------------------------
void LLIMHandler::initChannel()
{
	S32 channel_right_bound = gViewerWindow->getWorldViewRectScaled().mRight - gSavedSettings.getS32("NotificationChannelRightMargin"); 
	S32 channel_width = gSavedSettings.getS32("NotifyBoxWidth");
	mChannel->init(channel_right_bound - channel_width, channel_right_bound);
}

void LLIMHandler::updatePhoenixLogImToChatConsole(const LLSD &data)
{
	PhoenixLogImToChatConsole = data.asBoolean();
}

//<Ansariel> better DN lookups
//void LLIMHandler::lookupDisplayNames(const LLUUID& agent_id)
//{
//	LLAvatarNameCache::get(agent_id, boost::bind(&LLIMHandler::onAvatarNameCache, this, _1, _2));
//}
//
//void LLIMHandler::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
//{
//	mAvatarName = av_name;
//}
//
//bool LLIMHandler::checkDisplayName()
//{
//	for (int i = 0; i <=20; i++)
//	{
//		if (mAvatarName.mDisplayName.empty())
//			ms_sleep(50);
//		else
//			return true;
//	}
//	return false;
//}



//--------------------------------------------------------------------------
bool LLIMHandler::processNotification(const LLSD& notify)
{
	if(!mChannel)
	{
		return false;
	}

	LLNotificationPtr notification = LLNotifications::instance().find(notify["id"].asUUID());

	if(!notification)
		return false;

	if (PhoenixLogImToChatConsole && gSavedSettings.getBOOL("PhoenixUseNearbyChatConsole"))
	{
		if(notify["sigtype"].asString() == "add" || notify["sigtype"].asString() == "change")
		{
			LLSD substitutions = notification->getSubstitutions();

			// Filter notifications with empty ID and empty message
			if (substitutions["FROM_ID"].asString() == "" && substitutions["MESSAGE"].asString() == "") return false;

			// Ansarial, replace long lock of local DN handling with the following call
			LLAvatarNameCache::get(LLUUID(substitutions["FROM_ID"].asString()), boost::bind(&LLIMHandler::onAvatarNameLookup, this, _1, _2, substitutions["MESSAGE"].asString()));
		}
	}
	else
	{
		// arrange a channel on a screen
		if(!mChannel->getVisible())
		{
			initChannel();
		}

		if(notify["sigtype"].asString() == "add" || notify["sigtype"].asString() == "change")
		{
			LLSD substitutions = notification->getSubstitutions();

			// According to comments in LLIMMgr::addMessage(), if we get message
			// from ourselves, the sender id is set to null. This fixes EXT-875.
			LLUUID avatar_id = substitutions["FROM_ID"].asUUID();
			if (avatar_id.isNull())
				avatar_id = gAgentID;

			LLToastIMPanel::Params im_p;
			im_p.notification = notification;
			im_p.avatar_id = avatar_id;
			im_p.from = substitutions["FROM"].asString();
			im_p.time = substitutions["TIME"].asString();
			im_p.message = substitutions["MESSAGE"].asString();
			im_p.session_id = substitutions["SESSION_ID"].asUUID();

			LLToastIMPanel* im_box = new LLToastIMPanel(im_p);

			LLToast::Params p;
			p.notif_id = notification->getID();
			p.session_id = im_p.session_id;
			p.notification = notification;
			p.panel = im_box;
			p.can_be_stored = false;
			p.on_delete_toast = boost::bind(&LLIMHandler::onDeleteToast, this, _1);
			LLScreenChannel* channel = dynamic_cast<LLScreenChannel*>(mChannel);
			if(channel)
				channel->addToast(p);

			// send a signal to the counter manager;
			mNewNotificationSignal();
		}
		else if (notify["sigtype"].asString() == "delete")
		{
			mChannel->killToastByNotificationID(notification->getID());
		}
	}
	return false;
}

//--------------------------------------------------------------------------
void LLIMHandler::onDeleteToast(LLToast* toast)
{
	// send a signal to the counter manager
	mDelNotificationSignal();
}

//--------------------------------------------------------------------------

void LLIMHandler::onAvatarNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& message_str)
//Ansariel Hiller
{
			std::string senderName;
			std::string message(message_str);
			std::string delimiter = ": ";
			std::string prefix = message.substr(0, 4);
			LLStringUtil::toLower(prefix);

			// irc styled messages
			if (prefix == "/me " || prefix == "/me'")
			{
				delimiter = LLStringUtil::null;
				message = message.substr(3);
			}

			if ((gSavedSettings.getBOOL("NameTagShowUsernames")) && (gSavedSettings.getBOOL("UseDisplayNames")))
			{
				senderName = av_name.getCompleteName();
			}
			else if (gSavedSettings.getBOOL("UseDisplayNames"))
			{
				senderName = av_name.mDisplayName;
			}
			else
			{
				senderName = av_name.getLegacyName();
			}

			if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
			{
				senderName = RlvStrings::getAnonym(senderName);
			}
			LLColor4 textColor = LLUIColorTable::instance().getColor("AgentChatColor");
			//color based on contact sets prefs
			static LLCachedControl<bool> contactSetsColorize(gSavedSettings,"PhoenixContactSetsColorizeMiniMap");
			if(contactSetsColorize &&
				(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)))
			{
				LLColor4 fgColor = LGGContactSets::getInstance()->getFriendColor(agent_id);
				if(fgColor!=LGGContactSets::getInstance()->getDefaultColor())
					textColor=fgColor;
			}
			gConsole->addConsoleLine("IM: " + senderName + delimiter + message, textColor);

			LLNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLNearbyChat>("nearby_chat", LLSD());
			gConsole->setVisible(!nearby_chat->getVisible());
}
