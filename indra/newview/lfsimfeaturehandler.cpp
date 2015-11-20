/* Copyright (C) 2013 Liru Færs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#include "llviewerprecompiledheaders.h"

#include "lfsimfeaturehandler.h"

#include "llagent.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "lllogininstance.h"

LFSimFeatureHandler::LFSimFeatureHandler()
{
	LL_INFOS() << "Initializing Sim Feature Handler" << LL_ENDL;

	gAgent.addRegionChangedCallback(boost::bind(&LFSimFeatureHandler::handleRegionChange, this));

	// Call setSupportedFeatures -> This will work because we construct
	// our singleton instance during STATE_SEED_CAP_GRANTED startup state
	// after we received the initial region caps
	setSupportedFeatures();
}

ExportSupport LFSimFeatureHandler::exportPolicy() const
{
	return LLGridManager::getInstance()->isInSecondLife() ? EXPORT_UNDEFINED : (mSupportsExport ? EXPORT_ALLOWED : EXPORT_DENIED);
}

void LFSimFeatureHandler::handleRegionChange()
{
	if (LLViewerRegion* region = gAgent.getRegion())
	{
		if (region->capabilitiesReceived())
		{
			setSupportedFeatures();
		}
		else
		{
			region->setCapabilitiesReceivedCallback(boost::bind(&LFSimFeatureHandler::setSupportedFeatures, this));
		}
	}
}

void LFSimFeatureHandler::setSupportedFeatures()
{
	if (LLViewerRegion* region = gAgent.getRegion())
	{
		LLSD info;
		region->getSimulatorFeatures(info);
		if (!LLGridManager::getInstance()->isInSecondLife() && info.has("OpenSimExtras")) // OpenSim specific sim features
		{
			LL_INFOS() << "Setting OpenSimExtras..." << LL_ENDL;

			// For definition of OpenSimExtras please see
			// http://opensimulator.org/wiki/SimulatorFeatures_Extras
			const LLSD& extras(info["OpenSimExtras"]);
			LL_DEBUGS() << "OpenSimExtras received: " << extras << LL_ENDL;

			mSupportsExport = extras.has("ExportSupported") ? extras["ExportSupported"].asBoolean() : false;
			mMapServerURL = extras.has("map-server-url") ? extras["map-server-url"].asString() : gSavedSettings.getString("CurrentMapServerURL");
			mSayRange = extras.has("say-range") ? extras["say-range"].asInteger() : 20;
			mShoutRange = extras.has("shout-range") ? extras["shout-range"].asInteger() : 100;
			mWhisperRange = extras.has("whisper-range") ? extras["whisper-range"].asInteger() : 10;

			if (extras.has("search-server-url"))
			{
				mSearchURL = extras["search-server-url"].asString();
			}
			else if (LLLoginInstance::getInstance()->hasResponse("search"))
			{
				mSearchURL = LLLoginInstance::getInstance()->getResponse("search").asString();
			}
			else
			{
				mSearchURL = LLGridManager::getInstance()->isInSecondLife() ? gSavedSettings.getString("SearchURL") : gSavedSettings.getString("SearchURLOpenSim");
			}

			if (extras.has("destination-guide-url"))
			{
				mDestinationGuideURL = extras["destination-guide-url"].asString();
			}
			else if (LLLoginInstance::getInstance()->hasResponse("destination_guide_url"))
			{
				mDestinationGuideURL = LLLoginInstance::getInstance()->getResponse("destination_guide_url").asString();
			}
			else if (LLGridManager::instance().isInSecondLife())
			{
				mDestinationGuideURL = gSavedSettings.getString("DestinationGuideURL");
			}
			else
			{
				mDestinationGuideURL = LLStringUtil::null;
			}

			if (extras.has("avatar-picker-url"))
			{
				mAvatarPickerURL = extras["avatar-picker-url"].asString();
			}
			else if (LLLoginInstance::getInstance()->hasResponse("avatar-picker-url"))
			{
				mAvatarPickerURL = LLLoginInstance::getInstance()->getResponse("avatar-picker-url").asString();
			}
			else if (LLGridManager::instance().isInSecondLife())
			{
				mAvatarPickerURL = gSavedSettings.getString("AvatarPickerURL");
			}
			else
			{
				mAvatarPickerURL = LLStringUtil::null;
			}
		}
		else // OpenSim specifics are unsupported reset all to default
		{
			LL_INFOS() << "Setting defaults..." << LL_ENDL;
			mSupportsExport = false;
			mMapServerURL = gSavedSettings.getString("CurrentMapServerURL");
			mSayRange = 20;
			mShoutRange = 100;
			mWhisperRange = 10;

			if (LLLoginInstance::getInstance()->hasResponse("search"))
			{
				mSearchURL = LLLoginInstance::getInstance()->getResponse("search").asString();
			}
			else
			{
				mSearchURL = LLGridManager::getInstance()->isInSecondLife() ? gSavedSettings.getString("SearchURL") : gSavedSettings.getString("SearchURLOpenSim");
			}

			if (LLLoginInstance::getInstance()->hasResponse("destination_guide_url"))
			{
				mDestinationGuideURL = LLLoginInstance::getInstance()->getResponse("destination_guide_url").asString();
			}
			else if (LLGridManager::instance().isInSecondLife())
			{
				mDestinationGuideURL = gSavedSettings.getString("DestinationGuideURL");
			}
			else
			{
				mDestinationGuideURL = LLStringUtil::null;
			}

			if (LLLoginInstance::getInstance()->hasResponse("avatar-picker-url"))
			{
				mAvatarPickerURL = LLLoginInstance::getInstance()->getResponse("avatar-picker-url").asString();
			}
			else if (LLGridManager::instance().isInSecondLife())
			{
				mAvatarPickerURL = gSavedSettings.getString("AvatarPickerURL");
			}
			else
			{
				mAvatarPickerURL = LLStringUtil::null;
			}
		}
	}
}

boost::signals2::connection LFSimFeatureHandler::setSupportsExportCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mSupportsExport.connect(slot);
}

boost::signals2::connection LFSimFeatureHandler::setSearchURLCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mSearchURL.connect(slot);
}

boost::signals2::connection LFSimFeatureHandler::setSayRangeCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mSayRange.connect(slot);
}

boost::signals2::connection LFSimFeatureHandler::setShoutRangeCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mShoutRange.connect(slot);
}

boost::signals2::connection LFSimFeatureHandler::setWhisperRangeCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mWhisperRange.connect(slot);
}

boost::signals2::connection LFSimFeatureHandler::setAvatarPickerCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mAvatarPickerURL.connect(slot);
}

boost::signals2::connection LFSimFeatureHandler::setDestinationGuideCallback(const boost::signals2::signal<void()>::slot_type& slot)
{
	return mDestinationGuideURL.connect(slot);
}
