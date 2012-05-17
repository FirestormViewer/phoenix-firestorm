/**
 * @file llnotificationofferhandler.cpp
 * @brief Notification Handler Class for Simple Notifications and Notification Tips
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
#include "lltoastnotifypanel.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llnotificationmanager.h"
#include "llnotifications.h"
#include "llscriptfloater.h"
#include "llimview.h"
#include "llnotificationsutil.h"

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLOfferHandler::LLOfferHandler(e_notification_type type, const LLSD& id)
{
	mType = type;

	// Getting a Channel for our notifications
	LLScreenChannel* channel = LLChannelManager::getInstance()->createNotificationChannel();
	if(channel)
	{
		channel->setControlHovering(true);
		channel->setOnRejectToastCallback(boost::bind(&LLOfferHandler::onRejectToast, this, _1));
		mChannel = channel->getHandle();
	}
}

//--------------------------------------------------------------------------
LLOfferHandler::~LLOfferHandler()
{
}

//--------------------------------------------------------------------------
void LLOfferHandler::initChannel()
{
	S32 channel_right_bound = gViewerWindow->getWorldViewRectScaled().mRight - gSavedSettings.getS32("NotificationChannelRightMargin");
	S32 channel_width = gSavedSettings.getS32("NotifyBoxWidth");
	mChannel.get()->init(channel_right_bound - channel_width, channel_right_bound);
}

//--------------------------------------------------------------------------
bool LLOfferHandler::processNotification(const LLSD& notify)
{
	if(mChannel.isDead())
	{
		return false;
	}

	LLNotificationPtr notification = LLNotifications::instance().find(notify["id"].asUUID());

	if(!notification)
		return false;

	// arrange a channel on a screen
	if(!mChannel.get()->getVisible())
	{
		initChannel();
	}

	if(notify["sigtype"].asString() == "add" || notify["sigtype"].asString() == "change")
	{


		if( notification->getPayload().has("give_inventory_notification")
			&& !notification->getPayload()["give_inventory_notification"] )
		{
			// This is an original inventory offer, so add a script floater
			LLScriptFloaterManager::instance().onAddNotification(notification->getID());
		}
		else
		{
			notification->setReusable(LLHandlerUtil::isNotificationReusable(notification));

			LLUUID session_id;
//			if (LLHandlerUtil::canSpawnIMSession(notification))
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Modified: RLVa-1.3.0h
			// Don't spawn a new IM session for inventory offers if this notification was subject to @shownames=n
			bool spawn_session = (LLHandlerUtil::canSpawnIMSession(notification)) && (!notification->getPayload().has("rlv_shownames"));
// [/RLVa:KB]
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Modified: Catznip-2.5.0a
//			bool spawn_session = LLHandlerUtil::canSpawnIMSession(notification);
			if (spawn_session)
// [/SL:KB]
			{
				const std::string name = LLHandlerUtil::getSubstitutionName(notification);

				LLUUID from_id = notification->getPayload()["from_id"];

				session_id = LLHandlerUtil::spawnIMSession(name, from_id);
			}

			bool show_toast = LLHandlerUtil::canSpawnToast(notification);
//			bool add_notid_to_im = LLHandlerUtil::canAddNotifPanelToIM(notification);
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Modified: Catznip-2.5.0a
			// NOTE: add_notid_to_im needs to be FALSE if we suppressed spawning an IM because in that case the notification needs to
			//       be routed to the "syswell" or the inventory offer floater will dissapear and the user won't be able to accept it
			bool add_notid_to_im = (spawn_session) && (LLHandlerUtil::canAddNotifPanelToIM(notification));
// [/SL:KB]
			if (add_notid_to_im)
			{
				LLHandlerUtil::addNotifPanelToIM(notification);
			}

			if (notification->getPayload().has("SUPPRESS_TOAST")
						&& notification->getPayload()["SUPPRESS_TOAST"])
			{
				LLNotificationsUtil::cancel(notification);
			}
			else if(show_toast)
			{
				LLToastNotifyPanel* notify_box = new LLToastNotifyPanel(notification);
				// don't close notification on panel destroy since it will be used by IM floater
				notify_box->setCloseNotificationOnDestroy(!add_notid_to_im);
				LLToast::Params p;
				p.notif_id = notification->getID();
				p.notification = notification;
				p.panel = notify_box;
				p.on_delete_toast = boost::bind(&LLOfferHandler::onDeleteToast, this, _1);
				// we not save offer notifications to the syswell floater that should be added to the IM floater
				p.can_be_stored = !add_notid_to_im;

				LLScreenChannel* channel = dynamic_cast<LLScreenChannel*>(mChannel.get());
				if(channel)
					channel->addToast(p);

				// if we not add notification to IM - add it to notification well
				if (!add_notid_to_im)
				{
					// send a signal to the counter manager
					mNewNotificationSignal();
				}
			}

			if (LLHandlerUtil::canLogToIM(notification))
			{
				// log only to file if notif panel can be embedded to IM and IM is opened
// [RLVa:KB] - Checked: 2010-04-20 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
				if (notification->getPayload().has("rlv_shownames"))
				{
					// Log to chat history if this notification was subject to @shownames=n
					LLHandlerUtil::logToNearbyChat(notification, CHAT_SOURCE_SYSTEM);
				}
				else if (add_notid_to_im && LLHandlerUtil::isIMFloaterOpened(notification))
// [/RLVa:KB]
//				if (add_notid_to_im && LLHandlerUtil::isIMFloaterOpened(notification))
				{
					LLHandlerUtil::logToIMP2P(notification, true);
				}
				else
				{
					LLHandlerUtil::logToIMP2P(notification);
				}
			}
		}
	}
	else if (notify["sigtype"].asString() == "delete")
	{
		if( notification->getPayload().has("give_inventory_notification")
			&& !notification->getPayload()["give_inventory_notification"] )
		{
			// Remove original inventory offer script floater
			LLScriptFloaterManager::instance().onRemoveNotification(notification->getID());
		}
		else
		{
//			if (LLHandlerUtil::canAddNotifPanelToIM(notification)
//					&& !LLHandlerUtil::isIMFloaterOpened(notification))
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Modified: Catznip-2.5.0a
			// LLHandlerUtil::canAddNotifPanelToIM() won't necessarily tell us whether the notification went into an IM or to the syswell
			//   -> the one and only time we need to decrease the unread IM count is when we've clicked any of the buttons on the *toast*
			//   -> since LLIMFloater::updateMessages() hides the toast when we open the IM (which resets the unread count to 0) we should 
			//      *only* decrease the unread IM count if there's a visible toast since the unread count will be at 0 otherwise anyway
			LLScreenChannel* pChannel = dynamic_cast<LLScreenChannel*>(mChannel.get());
			LLToast* pToast = (pChannel) ? pChannel->getToastByNotificationID(notification->getID()) : NULL;
			if ( (pToast) && (!pToast->getCanBeStored()) )
// [/SL:KB]
			{
				LLHandlerUtil::decIMMesageCounter(notification);
			}
			mChannel.get()->killToastByNotificationID(notification->getID());
		}
	}

	return false;
}

//--------------------------------------------------------------------------

void LLOfferHandler::onDeleteToast(LLToast* toast)
{
//	if (!LLHandlerUtil::canAddNotifPanelToIM(toast->getNotification()))
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Modified: Catznip-2.5.0a
	if (toast->getCanBeStored())
// [/SL:KB]
	{
		// send a signal to the counter manager
		mDelNotificationSignal();
	}

	// send a signal to a listener to let him perform some action
	// in this case listener is a SysWellWindow and it will remove a corresponding item from its list
	mNotificationIDSignal(toast->getNotificationID());
}

//--------------------------------------------------------------------------
void LLOfferHandler::onRejectToast(LLUUID& id)
{
	LLNotificationPtr notification = LLNotifications::instance().find(id);

//	if (notification
//			&& LLNotificationManager::getInstance()->getHandlerForNotification(
//					notification->getType()) == this
//					// don't delete notification since it may be used by IM floater
//					&& !LLHandlerUtil::canAddNotifPanelToIM(notification))
//	{
//		LLNotifications::instance().cancel(notification);
//	}
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Modified: Catznip-2.5.0a
	// NOTE: this will be fired from LLScreenChannel::killToastByNotificationID() which treats visible and stored toasts differently
	if ( (notification) && (!notification->isCancelled()) && 
		 (LLNotificationManager::getInstance()->getHandlerForNotification(notification->getType()) == this)  )
	{
		LLScreenChannel* pChannel = dynamic_cast<LLScreenChannel*>(mChannel.get());
		LLToast* pToast = (pChannel) ? pChannel->getToastByNotificationID(notification->getID()) : NULL;
		if ( (!pToast) || (pToast->getCanBeStored()) )
		{
			LLNotifications::instance().cancel(notification);
		}
	}
// [/SL:KB]
}
