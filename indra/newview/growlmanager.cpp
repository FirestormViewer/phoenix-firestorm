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

#include "llagentdata.h"
#include "llappviewer.h"
#include "lldir.h"
#include "llfile.h"
#include "llfocusmgr.h"
#include "llimview.h"
#include "llnotifications.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llwindow.h"

#include "growlmanager.h"
#include "growlnotifier.h"
#include "llurlregistry.h" // for SLURL parsing

// Platform-specific includes
#ifdef LL_DARWIN
#include "growlnotifiermacosx.h"
#elif LL_WINDOWS
#include "growlnotifierwin.h"
#elif LL_LINUX
#include "desktopnotifierlinux.h"
#endif


GrowlManager *gGrowlManager = NULL;

GrowlManager::GrowlManager() : LLEventTimer(GROWL_THROTTLE_CLEANUP_PERIOD)
{
	// Create a notifier appropriate to the platform.
#ifdef LL_DARWIN
	this->mNotifier = new GrowlNotifierMacOSX();
	LL_INFOS("GrowlManagerInit") << "Created GrowlNotifierMacOSX." << LL_ENDL;
#elif LL_WINDOWS
	this->mNotifier = new GrowlNotifierWin();
	LL_INFOS("GrowlManagerInit") << "Created GrowlNotifierWin." << LL_ENDL;
#elif LL_LINUX
	this->mNotifier = new DesktopNotifierLinux();
	LL_INFOS("GrowlManagerInit") << "Created DesktopNotifierLinux." << LL_ENDL;
#else
	this->mNotifier = new GrowlNotifier();
	LL_INFOS("GrowlManagerInit") << "Created generic GrowlNotifier." << LL_ENDL;
#endif
	
	// Don't do anything more if Growl isn't usable.
	if( !mNotifier || !mNotifier->isUsable())
	{
		LL_WARNS("GrowlManagerInit") << "Growl is unusable; bailing out." << LL_ENDL;
		return;
	}

	// Hook into LLNotifications...
	// We hook into all of them, even though (at the time of writing) nothing uses "alert", so more notifications can be added easily.
	LLNotificationChannel::buildChannel("GrowlNotifications", "Visible", &filterOldNotifications);
	LLNotifications::instance().getChannel("GrowlNotifications")->connectChanged(&onLLNotification);

	// Also hook into IM notifications.
	LLIMModel::instance().mNewMsgSignal.connect(&GrowlManager::onInstantMessage);
	this->loadConfig();
}

void GrowlManager::loadConfig()
{
	std::string config_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "growl_notifications.xml");
	if(config_file == "")
	{
		LL_WARNS("GrowlConfig") << "Couldn't find growl_notifications.xml" << LL_ENDL;
		return;
	}
	LL_INFOS("GrowlConfig") << "Loading growl notification config from " << config_file << LL_ENDL;
	llifstream configs(config_file);
	LLSD notificationLLSD;
	std::set<std::string> notificationTypes;
	notificationTypes.insert("Keyword Alert");
	notificationTypes.insert(GROWL_IM_MESSAGE_TYPE);
	if(configs.is_open())
	{
		LLSDSerialize::fromXML(notificationLLSD, configs);
		for(LLSD::map_iterator itr = notificationLLSD.beginMap(); itr != notificationLLSD.endMap(); ++itr)
		{
			GrowlNotification ntype;
			ntype.growlName = itr->second.get("GrowlName").asString();
			notificationTypes.insert(ntype.growlName);
			
			if(itr->second.has("GrowlTitle"))
				ntype.growlTitle = itr->second.get("GrowlTitle").asString();			
			if(itr->second.has("GrowlBody"))
				ntype.growlBody = itr->second.get("GrowlBody").asString();
			if(itr->second.has("UseDefaultTextForTitle"))
				ntype.useDefaultTextForTitle = itr->second.get("UseDefaultTextForTitle").asBoolean();
			else
				ntype.useDefaultTextForTitle = false;
			if(itr->second.has("UseDefaultTextForBody"))
				ntype.useDefaultTextForBody = itr->second.get("UseDefaultTextForBody").asBoolean();
			else
				ntype.useDefaultTextForBody = false;
			if(ntype.useDefaultTextForBody == false && ntype.useDefaultTextForTitle == false && 
			   ntype.growlBody == "" && ntype.growlTitle == "")
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

void GrowlManager::notify(const std::string& notification_title, const std::string& notification_message, const std::string& notification_type)
{
	static LLCachedControl<bool> enabled(gSavedSettings, "FSEnableGrowl");
	if(!enabled)
		return;
	
	if(!shouldNotify())
		return;
	
	if(this->mNotifier->needsThrottle())
	{
		U64 now = LLTimer::getTotalTime();
		if(mTitleTimers.find(notification_title) != mTitleTimers.end())
		{
			if(mTitleTimers[notification_title] > now - GROWL_THROTTLE_TIME)
			{
				LL_WARNS("GrowlNotify") << "Discarded notification with title '" << notification_title << "' - spam ._." << LL_ENDL;
				mTitleTimers[notification_title] = now;
				return;
			}
		}
		mTitleTimers[notification_title] = now;
	}
	this->mNotifier->showNotification(notification_title, notification_message.substr(0, GROWL_MAX_BODY_LENGTH), notification_type);
}

BOOL GrowlManager::tick()
{
	mTitleTimers.clear();
	return false;
}

bool GrowlManager::onLLNotification(const LLSD& notice)
{
	if(notice["sigtype"].asString() != "add")
		return false;
	LLNotificationPtr notification = LLNotifications::instance().find(notice["id"].asUUID());
	std::string name = notification->getName();
	LLSD substitutions = notification->getSubstitutions();
	if(gGrowlManager->mNotifications.find(name) != gGrowlManager->mNotifications.end())
	{
		GrowlNotification* growl_notification = &gGrowlManager->mNotifications[name];
		std::string body = "";
		std::string title = "";
		if(growl_notification->useDefaultTextForTitle)
			title = notification->getMessage();
		else if(growl_notification->growlTitle != "")
		{
			title = growl_notification->growlTitle;
			LLStringUtil::format(title, substitutions);
		}
		if(growl_notification->useDefaultTextForBody)
			body = notification->getMessage();
		else if(growl_notification->growlBody != "")
		{
			body = growl_notification->growlBody;
			LLStringUtil::format(body, substitutions);
		}
		//TM:FS no need to log whats sent to growl
		//LL_INFOS("GrowlLLNotification") << "Notice: " << title << ": " << body << LL_ENDL;
		if(name == "ObjectGiveItem" || name == "ObjectGiveItemUnknownUser" || name == "UserGiveItem" || name == "SystemMessageTip")
		{
			LLUrlMatch urlMatch;
			LLWString newLine = utf8str_to_wstring(body);
			LLWString workLine = utf8str_to_wstring(body);
			while (LLUrlRegistry::instance().findUrl(workLine, urlMatch) && !urlMatch.getUrl().empty())
			{
				LLWStringUtil::replaceString(newLine, utf8str_to_wstring(urlMatch.getUrl()), utf8str_to_wstring(urlMatch.getLabel()));

				// Remove the URL from the work line so we don't end in a loop in case of regular URLs!
				// findUrl will always return the very first URL in a string
				workLine = workLine.erase(0, urlMatch.getEnd() + 1);
			}
			body = wstring_to_utf8str(newLine);
		}
		gGrowlManager->notify(title, body, growl_notification->growlName);
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
	LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(im["session_id"]);
	if(session->isP2PSessionType())
	{
		// Don't show messages from ourselves or the system.
		LLUUID from_id = im["from_id"];
		if(from_id == LLUUID::null || from_id == gAgentID)
			return;
		std::string message = im["message"];
		std::string prefix = message.substr(0, 4);
		if(prefix == "/me " || prefix == "/me'")
		{
			message = message.substr(3);
		}
		gGrowlManager->notify(im["from"], message, GROWL_IM_MESSAGE_TYPE);
	}
}


bool GrowlManager::shouldNotify()
{
	// This magic stolen from llappviewer.cpp. LLViewerWindow::getActive lies.
	static LLCachedControl<bool> activated(gSavedSettings, "FSGrowlWhenActive");
	if(LLStartUp::getStartupState() < STATE_STARTED)
	{
		return false;
	}
	return (activated || (!gViewerWindow->getWindow()->getVisible()  || !gFocusMgr.getAppHasFocus()));
}

void GrowlManager::InitiateManager()
{
	gGrowlManager = new GrowlManager();
}

