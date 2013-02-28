/**
 * @file llnotificationofferhandler.cpp
 * @brief Provides set of utility methods for notifications processing.
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

#include "llavatarnamecache.h"

#include "llfloaterreg.h"
#include "llnotifications.h"
#include "llurlaction.h"

#include "llagent.h"
// <FS:Ansariel> [FS communication UI]
//#include "llimfloater.h"
#include "fsfloaterim.h"
// </FS:Ansariel> [FS communication UI]
#include "llimview.h"
// <FS:Zi> Remove floating chat bar
// #include "llnearbychat.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaternearbychat.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
// </FS:Zi>
#include "llnotificationhandler.h"
#include "llnotifications.h"
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
#include "llavatarnamecache.h"
// [/SL:KB]
#include "llfloaterreg.h"
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
#include "rlvhandler.h"
// [/RLVa:KB]
#include "llconsole.h"

using namespace LLNotificationsUI;

// static
std::list< std::set<std::string> > LLSysHandler::sExclusiveNotificationGroups;

// static
void LLSysHandler::init()
{
	std::set<std::string> online_offline_group;
	online_offline_group.insert("FriendOnline");
	online_offline_group.insert("FriendOffline");

	sExclusiveNotificationGroups.push_back(online_offline_group);
}

LLSysHandler::LLSysHandler()
{
	if(sExclusiveNotificationGroups.empty())
	{
		init();
	}
}

void LLSysHandler::removeExclusiveNotifications(const LLNotificationPtr& notif)
{
	LLScreenChannel* channel = dynamic_cast<LLScreenChannel *>(mChannel.get());
	if (channel == NULL)
	{
		return;
	}

	class ExclusiveMatcher: public LLScreenChannel::Matcher
	{
	public:
		ExclusiveMatcher(const std::set<std::string>& excl_group,
				const std::string& from_name) :
			mExclGroup(excl_group), mFromName(from_name)
		{
		}
		bool matches(const LLNotificationPtr notification) const
		{
			for (std::set<std::string>::const_iterator it = mExclGroup.begin(); it
					!= mExclGroup.end(); it++)
			{
				std::string from_name = LLHandlerUtil::getSubstitutionName(notification);
				if (notification->getName() == *it && from_name == mFromName)
				{
					return true;
				}
			}
			return false;
		}
	private:
		const std::set<std::string>& mExclGroup;
		const std::string& mFromName;
	};


	for (exclusive_notif_sets::iterator it = sExclusiveNotificationGroups.begin(); it
			!= sExclusiveNotificationGroups.end(); it++)
	{
		std::set<std::string> group = *it;
		std::set<std::string>::iterator g_it = group.find(notif->getName());
		if (g_it != group.end())
		{
			channel->killMatchedToasts(ExclusiveMatcher(group,
					LLHandlerUtil::getSubstitutionName(notif)));
		}
	}
}

const static std::string GRANTED_MODIFY_RIGHTS("GrantedModifyRights"),
		REVOKED_MODIFY_RIGHTS("RevokedModifyRights"),
		OBJECT_GIVE_ITEM("ObjectGiveItem"),
		OBJECT_GIVE_ITEM_UNKNOWN_USER("ObjectGiveItemUnknownUser"),
						PAYMENT_RECEIVED("PaymentReceived"),
						PAYMENT_SENT("PaymentSent"),
						ADD_FRIEND_WITH_MESSAGE("AddFriendWithMessage"),
						USER_GIVE_ITEM("UserGiveItem"),
						INVENTORY_ACCEPTED("InventoryAccepted"),
						INVENTORY_DECLINED("InventoryDeclined"),
						OFFER_FRIENDSHIP("OfferFriendship"),
						FRIENDSHIP_ACCEPTED("FriendshipAccepted"),
						FRIENDSHIP_OFFERED("FriendshipOffered"),
						FRIENDSHIP_ACCEPTED_BYME("FriendshipAcceptedByMe"),
						FRIENDSHIP_DECLINED_BYME("FriendshipDeclinedByMe"),
						FRIEND_ONLINE("FriendOnline"), FRIEND_OFFLINE("FriendOffline"),
						SERVER_OBJECT_MESSAGE("ServerObjectMessage"),
						TELEPORT_OFFERED("TeleportOffered"),
						TELEPORT_OFFERED_MATURITY_EXCEEDED("TeleportOffered_MaturityExceeded"),
						TELEPORT_OFFERED_MATURITY_BLOCKED("TeleportOffered_MaturityBlocked"),
						TELEPORT_OFFER_SENT("TeleportOfferSent"),
						IM_SYSTEM_MESSAGE_TIP("IMSystemMessageTip");


// static
bool LLHandlerUtil::canLogToIM(const LLNotificationPtr& notification)
{
	return GRANTED_MODIFY_RIGHTS == notification->getName()
			|| REVOKED_MODIFY_RIGHTS == notification->getName()
			|| PAYMENT_RECEIVED == notification->getName()
			|| PAYMENT_SENT == notification->getName()
			|| OFFER_FRIENDSHIP == notification->getName()
			|| FRIENDSHIP_OFFERED == notification->getName()
			|| FRIENDSHIP_ACCEPTED == notification->getName()
			|| FRIENDSHIP_ACCEPTED_BYME == notification->getName()
			|| FRIENDSHIP_DECLINED_BYME == notification->getName()
			|| SERVER_OBJECT_MESSAGE == notification->getName()
			|| INVENTORY_ACCEPTED == notification->getName()
			|| INVENTORY_DECLINED == notification->getName()
			|| USER_GIVE_ITEM == notification->getName()
			|| TELEPORT_OFFERED == notification->getName()
			|| TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()
			|| TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName()
			|| TELEPORT_OFFER_SENT == notification->getName()
			|| IM_SYSTEM_MESSAGE_TIP == notification->getName();
}

// static
bool LLHandlerUtil::canLogToNearbyChat(const LLNotificationPtr& notification)
{
	return notification->getType() == "notifytip"
			&&  FRIEND_ONLINE != notification->getName()
			&& FRIEND_OFFLINE != notification->getName()
			&& INVENTORY_ACCEPTED != notification->getName()
			&& INVENTORY_DECLINED != notification->getName()
			&& IM_SYSTEM_MESSAGE_TIP != notification->getName();
}

// static
bool LLHandlerUtil::canSpawnIMSession(const LLNotificationPtr& notification)
{
//	return OFFER_FRIENDSHIP == notification->getName()
//			|| USER_GIVE_ITEM == notification->getName()
//			|| TELEPORT_OFFERED == notification->getName()
//			|| TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()
//			|| TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName();
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
//	return
//		(canEmbedNotificationInIM(notification)) && 
//		( (OFFER_FRIENDSHIP == notification->getName()) || (USER_GIVE_ITEM == notification->getName()) || 
//		  (TELEPORT_OFFERED == notification->getName()) || 
//		  (TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()) || (TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName()) );
// [/SL:KB]
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	return
		(canEmbedNotificationInIM(notification)) && 
		( (!rlv_handler_t::isEnabled()) || (gRlvHandler.canStartIM(notification->getPayload()["from_id"].asUUID())) ) &&
		( (OFFER_FRIENDSHIP == notification->getName()) || (USER_GIVE_ITEM == notification->getName()) || 
		  (TELEPORT_OFFERED == notification->getName()) || 
		  (TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()) || (TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName()) );
// [/RLVa:KB]
}

// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
// static
bool LLHandlerUtil::canEmbedNotificationInIM(const LLNotificationPtr& notification)
{
	switch (gSavedSettings.getS32("NotificationCanEmbedInIM"))
	{
		case 1:			// Focused
			return isIMFloaterFocused(notification);
		case 2:			// Never
			return false;
		case 0:			// Always (Viewer 2 default)
		default:
			return true;
	}
}
// [/SL:KB]

// static
bool LLHandlerUtil::canAddNotifPanelToIM(const LLNotificationPtr& notification)
{
//	return OFFER_FRIENDSHIP == notification->getName()
//					|| USER_GIVE_ITEM == notification->getName()
//					|| TELEPORT_OFFERED == notification->getName()
//					|| TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()
//					|| TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName();
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
	return 
		(canEmbedNotificationInIM(notification)) && 
		( (OFFER_FRIENDSHIP == notification->getName()) || (USER_GIVE_ITEM == notification->getName()) || 
		  (TELEPORT_OFFERED == notification->getName()) || 
		  (TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()) || (TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName()) );
// [/SL:KB]
}

// static
bool LLHandlerUtil::isNotificationReusable(const LLNotificationPtr& notification)
{
//	return OFFER_FRIENDSHIP == notification->getName()
//		|| USER_GIVE_ITEM == notification->getName()
//		|| TELEPORT_OFFERED == notification->getName()
//		|| TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()
//		|| TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName();
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
	return 
		(canEmbedNotificationInIM(notification)) && 
		( (OFFER_FRIENDSHIP == notification->getName()) || (USER_GIVE_ITEM == notification->getName()) || 
		  (TELEPORT_OFFERED == notification->getName()) ||
		  (TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()) || (TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName()) );
// [/SL:KB]
}

// static
bool LLHandlerUtil::canSpawnSessionAndLogToIM(const LLNotificationPtr& notification)
{
	return canLogToIM(notification) && canSpawnIMSession(notification);
}

// static
bool LLHandlerUtil::canSpawnToast(const LLNotificationPtr& notification)
{
	if(INVENTORY_DECLINED == notification->getName() 
		|| INVENTORY_ACCEPTED == notification->getName())
	{
		// return false for inventory accepted/declined notifications if respective IM window is open (EXT-5909)
		return ! isIMFloaterOpened(notification);
	}

	if(FRIENDSHIP_ACCEPTED == notification->getName())
	{
		// don't show FRIENDSHIP_ACCEPTED if IM window is opened and focused - EXT-6441
		return ! isIMFloaterFocused(notification);
	}

	if(OFFER_FRIENDSHIP == notification->getName()
		|| USER_GIVE_ITEM == notification->getName()
		|| TELEPORT_OFFERED == notification->getName()
		|| TELEPORT_OFFERED_MATURITY_EXCEEDED == notification->getName()
		|| TELEPORT_OFFERED_MATURITY_BLOCKED == notification->getName())
	{
		// When ANY offer arrives, show toast, unless IM window is already open - EXT-5904
//		return ! isIMFloaterOpened(notification);
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
		// Force a toast if the user opted not to embed notifications panels in IMs
		return (!canEmbedNotificationInIM(notification)) || (!isIMFloaterOpened(notification));
// [/SL:KB]
	}

	return true;
}

// static
// <FS:Ansariel> [FS communication UI]
//LLIMFloater* LLHandlerUtil::findIMFloater(const LLNotificationPtr& notification)
FSFloaterIM* LLHandlerUtil::findIMFloater(const LLNotificationPtr& notification)
// </FS:Ansariel> [FS communication UI]
{
	LLUUID from_id = notification->getPayload()["from_id"];
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, from_id);
	// <FS:Ansariel> [FS communication UI]
	//return LLFloaterReg::findTypedInstance<LLIMFloater>("impanel", session_id);
	return LLFloaterReg::findTypedInstance<FSFloaterIM>("fs_impanel", session_id);
	// </FS:Ansariel> [FS communication UI]
}

// static
bool LLHandlerUtil::isIMFloaterOpened(const LLNotificationPtr& notification)
{
	bool res = false;

	// <FS:Ansariel> [FS communication UI]
	//LLIMFloater* im_floater = findIMFloater(notification);
	FSFloaterIM* im_floater = findIMFloater(notification);
	// </FS:Ansariel> [FS communication UI]
	if (im_floater != NULL)
	{
		res = im_floater->getVisible() == TRUE;
	}

	return res;
}

bool LLHandlerUtil::isIMFloaterFocused(const LLNotificationPtr& notification)
{
	bool res = false;

	// <FS:Ansariel> [FS communication UI]
	//LLIMFloater* im_floater = findIMFloater(notification);
	FSFloaterIM* im_floater = findIMFloater(notification);
	// </FS:Ansariel> [FS communication UI]
	if (im_floater != NULL)
	{
		res = im_floater->hasFocus() == TRUE;
	}

	return res;
}

// static
void LLHandlerUtil::logToIM(const EInstantMessage& session_type,
//		const std::string& session_name, const std::string& from_name,
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
		const std::string& file_name, const std::string& from_name,
// [/SL:KB]
		const std::string& message, const LLUUID& session_owner_id,
		const LLUUID& from_id)
{
	std::string from = from_name;
	if (from_name.empty())
	{
		from = SYSTEM_FROM;
	}

	LLUUID session_id = LLIMMgr::computeSessionID(session_type,
			session_owner_id);
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
			session_id);
	if (session == NULL)
	{
		// replace interactive system message marker with correct from string value
		if (INTERACTIVE_SYSTEM_FROM == from_name)
		{
			from = SYSTEM_FROM;
		}
//		LLIMModel::instance().logToFile(session_name, from, from_id, message);
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
		LLIMModel::instance().logToFile(file_name, from, from_id, message);
// [/SL:KB]
		//-TT 2.8.2 merge changes - left out for now
		//// Build a new format username or firstname_lastname for legacy names
		//// to use it for a history log filename.
		//std::string user_name = LLCacheName::buildUsername(session_name);
		//LLIMModel::instance().logToFile(user_name, from, from_id, message);
	}
	else
	{
		// store active session id
		const LLUUID & active_session_id =
				LLIMModel::instance().getActiveSessionID();

		// set searched session as active to avoid IM toast popup
		LLIMModel::instance().setActiveSessionID(session_id);

		S32 unread = session->mNumUnread;
		S32 participant_unread = session->mParticipantUnreadMessageCount;
		LLIMModel::instance().addMessageSilently(session_id, from, from_id,
				message);
		// we shouldn't increment counters when logging, so restore them
		session->mNumUnread = unread;
		session->mParticipantUnreadMessageCount = participant_unread;

		// update IM floater messages
		updateIMFLoaterMesages(session_id);

		// restore active session id
		if (active_session_id.isNull())
		{
			LLIMModel::instance().resetActiveSessionID();
		}
		else
		{
			LLIMModel::instance().setActiveSessionID(active_session_id);
		}
	}
}

// static
void LLHandlerUtil::logToIMP2P(const LLNotificationPtr& notification)
{
	logToIMP2P(notification, false);
}

//void log_name_callback(const std::string& full_name, const std::string& from_name, 
//					   const std::string& message, const LLUUID& from_id)
//
//{
//	LLHandlerUtil::logToIM(IM_NOTHING_SPECIAL, full_name, from_name, message,
//					from_id, LLUUID());
//}
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
void log_name_callback(const LLUUID& agent_id, const LLAvatarName& av_name,
					   const std::string& from_name, const std::string& message, const LLUUID& from_id)
{
	std::string strFilename;
	if (LLIMModel::buildIMP2PLogFilename(agent_id, av_name.getCompleteName(), strFilename))
	{
		LLHandlerUtil::logToIM(IM_NOTHING_SPECIAL, strFilename, from_name, message, from_id, LLUUID());
	}
}
// [/SL:KB]

// static
void LLHandlerUtil::logToIMP2P(const LLNotificationPtr& notification, bool to_file_only)
{
	// don't create IM p2p session with objects, it's necessary condition to log
	if (notification->getName() != OBJECT_GIVE_ITEM)
	{
		LLUUID from_id = notification->getPayload()["from_id"];

		if (from_id.isNull())
		{
			// Normal behavior for system generated messages, don't spam.
			// llwarns << " from_id for notification " << notification->getName() << " is null " << llendl;
			return;
		}

		if(to_file_only)
		{
//			gCacheName->get(from_id, false, boost::bind(&log_name_callback, _2, "", notification->getMessage(), LLUUID()));
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
			LLAvatarNameCache::get(from_id, boost::bind(&log_name_callback, _1, _2, "", notification->getMessage(), LLUUID()));
// [/SL:KB]
		}
		else
		{
//			gCacheName->get(from_id, false, boost::bind(&log_name_callback, _2, INTERACTIVE_SYSTEM_FROM, notification->getMessage(), from_id));
// [SL:KB] - Patch: Chat-Logs | Checked: 2010-11-18 (Catznip-2.4.0c) | Added: Catznip-2.4.0c
			LLAvatarNameCache::get(from_id, boost::bind(&log_name_callback, _1, _2, INTERACTIVE_SYSTEM_FROM, notification->getMessage(), from_id));
// [/SL:KB]
		}
	}
}

// static
void LLHandlerUtil::logGroupNoticeToIMGroup(
		const LLNotificationPtr& notification)
{

	const LLSD& payload = notification->getPayload();
	LLGroupData groupData;
	if (!gAgent.getGroupData(payload["group_id"].asUUID(), groupData))
	{
		llwarns
						<< "Group notice for unknown group: "
								<< payload["group_id"].asUUID() << llendl;
		return;
	}

	const std::string group_name = groupData.mName;
	const std::string sender_name = payload["sender_name"].asString();

	// we can't retrieve sender id from group notice system message, so try to lookup it from cache
	LLUUID sender_id;
	gCacheName->getUUID(sender_name, sender_id);

	logToIM(IM_SESSION_GROUP_START, group_name, sender_name, payload["message"],
			payload["group_id"], sender_id);
}

// static
void LLHandlerUtil::logToNearbyChat(const LLNotificationPtr& notification, EChatSourceType type)
{
	// <FS:Ansariel> [FS communication UI]
	//LLFloaterNearbyChat* nearby_chat = LLFloaterNearbyChat::getInstance();
	FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
	// </FS:Ansariel> [FS communication UI]
	if(nearby_chat)
	{
		LLChat chat_msg(notification->getMessage());
		chat_msg.mSourceType = type;
		chat_msg.mFromName = SYSTEM_FROM;
		chat_msg.mFromID = LLUUID::null;
		nearby_chat->addMessage(chat_msg);

		// Ansariel: Also log to console if enabled
		if (gSavedSettings.getBOOL("FSUseNearbyChatConsole"))
		{
			LLColor4 chatcolor;
			LLViewerChat::getChatColor(chat_msg, chatcolor);
			gConsole->addConsoleLine(chat_msg.mText, chatcolor);
			gConsole->setVisible(!nearby_chat->getVisible());
		}
	}
}

// static
LLUUID LLHandlerUtil::spawnIMSession(const std::string& name, const LLUUID& from_id)
{
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, from_id);

	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(
			session_id);
	if (session == NULL)
	{
		session_id = LLIMMgr::instance().addSession(name, IM_NOTHING_SPECIAL, from_id);
	}

	return session_id;
}

// static
std::string LLHandlerUtil::getSubstitutionName(const LLNotificationPtr& notification)
{
	std::string res = notification->getSubstitutions().has("NAME")
		? notification->getSubstitutions()["NAME"]
		: notification->getSubstitutions()["[NAME]"];
	if (res.empty())
	{
		LLUUID from_id = notification->getPayload()["FROM_ID"];

		//*TODO all keys everywhere should be made of the same case, there is a mix of keys in lower and upper cases
		if (from_id.isNull()) 
		{
			from_id = notification->getPayload()["from_id"];
		}
		if(!gCacheName->getFullName(from_id, res))
		{
			res = "";
		}
	}
	return res;
}

// static
void LLHandlerUtil::addNotifPanelToIM(const LLNotificationPtr& notification)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);
	LLUUID from_id = notification->getPayload()["from_id"];

	LLUUID session_id = spawnIMSession(name, from_id);
	// add offer to session
	LLIMModel::LLIMSession * session = LLIMModel::getInstance()->findIMSession(
			session_id);
	llassert_always(session != NULL);

	LLSD offer;
	offer["notification_id"] = notification->getID();
	offer["from"] = SYSTEM_FROM;
	offer["time"] = LLLogChat::timestamp(false);
	offer["index"] = (LLSD::Integer)session->mMsgs.size();
	session->mMsgs.push_front(offer);


	// update IM floater and counters
	LLSD arg;
	arg["session_id"] = session_id;
	arg["num_unread"] = ++(session->mNumUnread);
	arg["participant_unread"] = ++(session->mParticipantUnreadMessageCount);
	LLIMModel::getInstance()->mNewMsgSignal(arg);
}

// static
void LLHandlerUtil::updateIMFLoaterMesages(const LLUUID& session_id)
{
	// <FS:Ansariel> [FS communication UI]
	//LLIMFloater* im_floater = LLIMFloater::findInstance(session_id);
	FSFloaterIM* im_floater = FSFloaterIM::findInstance(session_id);
	// </FS:Ansariel> [FS communication UI]
	if (im_floater != NULL && im_floater->getVisible())
	{
		im_floater->updateMessages();
	}
}

// static
void LLHandlerUtil::updateVisibleIMFLoaterMesages(const LLNotificationPtr& notification)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);
	LLUUID from_id = notification->getPayload()["from_id"];
	LLUUID session_id = spawnIMSession(name, from_id);

	updateIMFLoaterMesages(session_id);
}

// static
void LLHandlerUtil::decIMMesageCounter(const LLNotificationPtr& notification)
{
	const std::string name = LLHandlerUtil::getSubstitutionName(notification);
	LLUUID from_id = notification->getPayload()["from_id"];
	LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, from_id);

	LLIMModel::LLIMSession * session = LLIMModel::getInstance()->findIMSession(
			session_id);

	if (session == NULL)
	{
		return;
	}

	LLSD arg;
	arg["session_id"] = session_id;
	session->mNumUnread--;
	arg["num_unread"] = session->mNumUnread;
	session->mParticipantUnreadMessageCount--;
	arg["participant_unread"] = session->mParticipantUnreadMessageCount;
	LLIMModel::getInstance()->mNewMsgSignal(arg);
}
