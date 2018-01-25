/* Copyright (c) 2010 Katharine Berry All rights reserved.
 *
 * @file growlmanager.cpp
 * @Implementation of desktop notification system (aka growl).
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Katharine Berry nor the names of any contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KATHARINE BERRY AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KATHARINE BERRY OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "growlmanager.h"

#include "fscommon.h"
#include "growlnotifier.h"
#include "llagent.h"
#include "llagentdata.h"
#include "llappviewer.h"
#include "llfloaterimnearbychathandler.h"
#include "llimview.h"
#include "llnotificationmanager.h"
#include "llscriptfloater.h"
#include "llsdserialize.h"
#include "llstartup.h"
#include "llurlregistry.h" // for SLURL parsing
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llwindow.h"

// Platform-specific includes
#ifndef LL_LINUX
#include "growlnotifierwin.h"
#elif LL_LINUX
#include "desktopnotifierlinux.h"
#endif


GrowlManager *gGrowlManager = NULL;

GrowlManager::GrowlManager()
	: LLEventTimer(GROWL_THROTTLE_CLEANUP_PERIOD),
	mNotifier(NULL),
	mNotificationConnection(),
	mInstantMessageConnection(),
	mScriptDialogConnection(),
	mChatMessageConnection()
{
	// Create a notifier appropriate to the platform.
#ifndef LL_LINUX
	mNotifier = new GrowlNotifierWin();
	LL_INFOS("GrowlManagerInit") << "Created GrowlNotifierWin." << LL_ENDL;
#elif LL_LINUX
	mNotifier = new DesktopNotifierLinux();
	LL_INFOS("GrowlManagerInit") << "Created DesktopNotifierLinux." << LL_ENDL;
#else
	mNotifier = new GrowlNotifier();
	LL_INFOS("GrowlManagerInit") << "Created generic GrowlNotifier." << LL_ENDL;
#endif
	
#ifndef LL_LINUX
	if (mNotifier)
	{
		// Need to call loadConfig for Windows first before we know if
		// Growl is usable -Ansariel
		loadConfig();
		if (!mNotifier->isUsable())
		{
			LL_INFOS("GrowlManagerInit") << "Growl not available" << LL_ENDL;
			return;
		}
	}
	else
	{
		LL_INFOS("GrowlManagerInit") << "Growl not available" << LL_ENDL;
		return;
	}
#else
	// Don't do anything more if Growl isn't usable.
	if( !mNotifier || !mNotifier->isUsable())
	{
		LL_INFOS("GrowlManagerInit") << "Growl not available" << LL_ENDL;
		return;
	}

	loadConfig();
#endif

	// Hook into LLNotifications...
	// We hook into all of them, even though (at the time of writing) nothing uses "alert", so more notifications can be added easily.
	mGrowlNotificationsChannel = new LLNotificationChannel("GrowlNotifications", "Visible", &filterOldNotifications);
	mNotificationConnection = LLNotifications::instance().getChannel("GrowlNotifications")->connectChanged(&onLLNotification);

	// Also hook into IM notifications.
	mInstantMessageConnection = LLIMModel::instance().addNewMsgCallback(&GrowlManager::onInstantMessage);
	
	// Hook into script dialogs
	mScriptDialogConnection = LLScriptFloaterManager::instance().addNewObjectCallback(&GrowlManager::onScriptDialog);

	// Hook into new chat messages (needed for object IMs that will go into nearby chat)
	mChatMessageConnection = LLNotificationsUI::LLNotificationManager::instance().getChatHandler()->addNewChatCallback(&GrowlManager::onNearbyChatMessage);
}

GrowlManager::~GrowlManager()
{
	if (mNotificationConnection.connected())
	{
		mNotificationConnection.disconnect();
	}
	if (mInstantMessageConnection.connected())
	{
		mInstantMessageConnection.disconnect();
	}
	if (mScriptDialogConnection.connected())
	{
		mScriptDialogConnection.disconnect();
	}
	if (mChatMessageConnection.connected())
	{
		mChatMessageConnection.disconnect();
	}

	if (mNotifier)
	{
		delete mNotifier;
	}
}

void GrowlManager::loadConfig()
{
	std::string config_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "growl_notifications.xml");
	if (config_file.empty())
	{
		LL_WARNS("GrowlConfig") << "Couldn't find growl_notifications.xml" << LL_ENDL;
		return;
	}

	LL_INFOS("GrowlConfig") << "Loading growl notification config from " << config_file << LL_ENDL;
	llifstream configs(config_file.c_str());
	LLSD notificationLLSD;
	std::set<std::string> notificationTypes;
	notificationTypes.insert(GROWL_KEYWORD_ALERT_TYPE);
	notificationTypes.insert(GROWL_IM_MESSAGE_TYPE);
	if (configs.is_open())
	{
		LLSDSerialize::fromXML(notificationLLSD, configs);
		for (LLSD::map_iterator itr = notificationLLSD.beginMap(); itr != notificationLLSD.endMap(); ++itr)
		{
			GrowlNotification ntype;
			ntype.growlName = itr->second.get("GrowlName").asString();
			notificationTypes.insert(ntype.growlName);
			
			if (itr->second.has("GrowlTitle"))
			{
				ntype.growlTitle = itr->second.get("GrowlTitle").asString();
			}

			if (itr->second.has("GrowlBody"))
			{
				ntype.growlBody = itr->second.get("GrowlBody").asString();
			}

			if (itr->second.has("UseDefaultTextForTitle"))
			{
				ntype.useDefaultTextForTitle = itr->second.get("UseDefaultTextForTitle").asBoolean();
			}
			else
			{
				ntype.useDefaultTextForTitle = false;
			}

			if (itr->second.has("UseDefaultTextForBody"))
			{
				ntype.useDefaultTextForBody = itr->second.get("UseDefaultTextForBody").asBoolean();
			}
			else
			{
				ntype.useDefaultTextForBody = false;
			}

			if (!ntype.useDefaultTextForBody && !ntype.useDefaultTextForTitle && ntype.growlBody.empty() && ntype.growlTitle.empty())
			{
				ntype.useDefaultTextForBody = true;
			}

			this->mNotifications[itr->first] = ntype;
		}
		configs.close();

		this->mNotifier->registerApplication("Firestorm Viewer", notificationTypes);
	}
	else
	{
		LL_WARNS("GrowlConfig") << "Couldn't open growl config file." << LL_ENDL;
	}

}

void GrowlManager::performNotification(const std::string& title, const std::string& message, const std::string& type)
{
	if (LLAppViewer::instance()->isExiting())
	{
		return;
	}

	static LLCachedControl<bool> enabled(gSavedSettings, "FSEnableGrowl");
	if (!enabled)
	{
		return;
	}
	
	if (!shouldNotify())
	{
		return;
	}
	
	if (mNotifier->needsThrottle())
	{
		const U64 now = LLTimer::getTotalTime();
		if (mTitleTimers.find(title) != mTitleTimers.end())
		{
			if (mTitleTimers[title] > now - GROWL_THROTTLE_TIME)
			{
				LL_WARNS("GrowlNotify") << "Discarded notification with title '" << title << "' due to throttle" << LL_ENDL;
				mTitleTimers[title] = now;
				return;
			}
		}
		mTitleTimers[title] = now;
	}
	mNotifier->showNotification(title, message.substr(0, GROWL_MAX_BODY_LENGTH), type);
}

BOOL GrowlManager::tick()
{
	mTitleTimers.clear();
	return FALSE;
}

bool GrowlManager::onLLNotification(const LLSD& notice)
{
	if (notice["sigtype"].asString() != "add")
	{
		return false;
	}

	LLNotificationPtr notification = LLNotifications::instance().find(notice["id"].asUUID());
	const std::string name = notification->getName();
	LLSD substitutions = notification->getSubstitutions();
	if (gGrowlManager->mNotifications.find(name) != gGrowlManager->mNotifications.end())
	{
		GrowlNotification* growl_notification = &gGrowlManager->mNotifications[name];
		std::string body = "";
		std::string title = "";
		if (growl_notification->useDefaultTextForTitle)
		{
			title = notification->getMessage();
		}
		else if (!growl_notification->growlTitle.empty())
		{
			title = growl_notification->growlTitle;
			LLStringUtil::format(title, substitutions);
		}
		if (growl_notification->useDefaultTextForBody)
		{
			body = notification->getMessage();
		}
		else if (!growl_notification->growlBody.empty())
		{
			body = growl_notification->growlBody;
			LLStringUtil::format(body, substitutions);
		}

		if (name == "ObjectGiveItem" || name == "OwnObjectGiveItem" || name == "ObjectGiveItemUnknownUser" || name == "UserGiveItem" || name == "SystemMessageTip")
		{
			LLUrlMatch urlMatch;
			LLWString newLine = utf8str_to_wstring(body);
			LLWString workLine = utf8str_to_wstring(body);
			while (LLUrlRegistry::instance().findUrl(workLine, urlMatch) && !urlMatch.getUrl().empty())
			{
				LLWStringUtil::replaceString(newLine, utf8str_to_wstring(urlMatch.getMatchedText()), utf8str_to_wstring(urlMatch.getLabel()));

				// Remove the URL from the work line so we don't end in a loop in case of regular URLs!
				// findUrl will always return the very first URL in a string
				workLine = workLine.erase(0, urlMatch.getEnd() + 1);
			}
			body = wstring_to_utf8str(newLine);
		}
		gGrowlManager->performNotification(title, body, growl_notification->growlName);
	}
	return false;
}

bool GrowlManager::filterOldNotifications(LLNotificationPtr pNotification)
{
	// *HACK: I don't see any better way to avoid getting old, persisted messages...
	return (pNotification->getDate().secondsSinceEpoch() >= LLDate::now().secondsSinceEpoch() - 10);
}

void GrowlManager::onInstantMessage(const LLSD& im)
{
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(im["session_id"].asUUID());
	if (session->isP2PSessionType() && // Must be P2P
		!im["is_announcement"].asBoolean() && // Not an announcement (incoming IM, autoresponse sent info...)
		(!im["keyword_alert_performed"].asBoolean() || !gSavedSettings.getBOOL("FSFilterGrowlKeywordDuplicateIMs"))) // Not keyword or show duplicate IMs due to keywords
	{
		// Don't show messages from ourselves or the system.
		const LLUUID from_id = im["from_id"].asUUID();
		if (from_id.isNull() || from_id == gAgentID)
		{
			return;
		}

		std::string message = im["message"].asString();
		if (is_irc_me_prefix(message))
		{
			message = message.substr(3);
		}
		LLAvatarNameCache::get(from_id, boost::bind(&GrowlManager::onAvatarNameCache, _2, message, GROWL_IM_MESSAGE_TYPE));
	}
}

//static
void GrowlManager::onScriptDialog(const LLSD& data)
{
	LLNotificationPtr notification = LLNotifications::instance().find(data["notification_id"].asUUID());
	const std::string name = notification->getName();
	LLSD substitutions = notification->getSubstitutions();

	if (gGrowlManager->mNotifications.find(name) != gGrowlManager->mNotifications.end())
	{
		GrowlNotification* growl_notification = &gGrowlManager->mNotifications[name];

		std::string body = "";
		std::string title = "";
		if (growl_notification->useDefaultTextForTitle)
		{
			title = notification->getMessage();
		}
		else if (!growl_notification->growlTitle.empty())
		{
			title = growl_notification->growlTitle;
			LLStringUtil::format(title, substitutions);
		}

		if (growl_notification->useDefaultTextForBody)
		{
			body = notification->getMessage();
		}
		else if (!growl_notification->growlBody.empty())
		{
			body = growl_notification->growlBody;
			LLStringUtil::format(body, substitutions);
		}

		gGrowlManager->performNotification(title, body, growl_notification->growlName);
	}
}

void GrowlManager::onNearbyChatMessage(const LLSD& chat)
{
	if ((EChatType)chat["chat_type"].asInteger() == CHAT_TYPE_IM)
	{
		std::string message = chat["message"].asString();
		if (is_irc_me_prefix(message))
		{
			message = message.substr(3);
		}

		if ((EChatSourceType)chat["source"].asInteger() == CHAT_SOURCE_AGENT)
		{
			LLAvatarNameCache::get(chat["from_id"].asUUID(), boost::bind(&GrowlManager::onAvatarNameCache, _2, message, GROWL_IM_MESSAGE_TYPE));
		}
		else
		{
			gGrowlManager->performNotification(chat["from"].asString(), message, GROWL_IM_MESSAGE_TYPE);
		}
	}
}

//static
void GrowlManager::onAvatarNameCache(const LLAvatarName& av_name, const std::string& message, const std::string& type)
{
	const std::string sender = FSCommon::getAvatarNameByDisplaySettings(av_name);
	notify(sender, message, type);
}

bool GrowlManager::shouldNotify()
{
	// This magic stolen from llappviewer.cpp. LLViewerWindow::getActive lies.
	static LLCachedControl<bool> activated(gSavedSettings, "FSGrowlWhenActive");
	if (LLStartUp::getStartupState() < STATE_STARTED)
	{
		return false;
	}
	if (gAgent.isDoNotDisturb())
	{
		return false;
	}
	return (activated || (!gViewerWindow->getWindow()->getVisible() || !gFocusMgr.getAppHasFocus()));
}

// static
void GrowlManager::initiateManager()
{
	gGrowlManager = new GrowlManager();
}

// static
void GrowlManager::destroyManager()
{
	if (gGrowlManager)
	{
		delete gGrowlManager;
		gGrowlManager = NULL;
	}
}

// static
bool GrowlManager::isUsable()
{
	return (gGrowlManager && gGrowlManager->mNotifier && gGrowlManager->mNotifier->isUsable());
}

// static
void GrowlManager::notify(const std::string& title, const std::string& message, const std::string& type)
{
	if (isUsable())
	{
		gGrowlManager->performNotification(title, message, type);
	}
}
