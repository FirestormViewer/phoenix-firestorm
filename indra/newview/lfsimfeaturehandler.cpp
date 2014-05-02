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
#include "llenvmanager.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"

LFSimFeatureHandler::LFSimFeatureHandler()
: mSupportsExport(false)
, mSayRange(20)
, mShoutRange(100)
, mWhisperRange(10)
{
	if (!LLGridManager::getInstance()->isInSecondLife()) // Remove this line if we ever handle SecondLife sim features
		gAgent.addRegionChangedCallback(boost::bind(&LFSimFeatureHandler::handleRegionChange, this));
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
		if (info.has("OpenSimExtras")) // OpenSim specific sim features
		{
			// For definition of OpenSimExtras please see
			// http://opensimulator.org/wiki/SimulatorFeatures_Extras
			const LLSD& extras(info["OpenSimExtras"]);
			mSupportsExport = extras.has("ExportSupported") ? extras["ExportSupported"].asBoolean() : false;
			mMapServerURL = extras.has("map-server-url") ? extras["map-server-url"].asString() : "";
			mSearchURL = extras.has("search-server-url") ? extras["search-server-url"].asString() : "";
			mSayRange = extras.has("say-range") ? extras["say-range"].asInteger() : 20;
			mShoutRange = extras.has("shout-range") ? extras["shout-range"].asInteger() : 100;
			mWhisperRange = extras.has("whisper-range") ? extras["whisper-range"].asInteger() : 10;
		}
		else // OpenSim specifics are unsupported reset all to default
		{
			mSupportsExport = false;
			mMapServerURL = "";
			mSearchURL = "";
			mSayRange = 20;
			mShoutRange = 100;
			mWhisperRange = 10;
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
