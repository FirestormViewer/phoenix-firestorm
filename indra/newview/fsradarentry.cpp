/**
 * @file fsradarentry.cpp
 * @brief Firestorm radar entry implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsradarentry.h"

#include "fscommon.h"
#include "fsradar.h"
#include "llagent.h"
#include "rlvhandler.h"

using namespace boost;

FSRadarEntry::FSRadarEntry(const LLUUID& avid)
	: mID(avid),
	mName(avid.asString()),
	mUserName(LLStringUtil::null),
	mDisplayName(LLStringUtil::null),
	mRange(0.f),
	mFirstSeen(time(NULL)),
	mGlobalPos(LLVector3d(0.0, 0.0, 0.0)),
	mRegion(LLUUID::null),
	mStatus(0),
	mZOffset(0.f),
	mLastZOffsetTime(time(NULL)),
	mAge(-1),
	mIsLinden(false),
	mIgnore(false),
	mHasNotes(false),
	mAlertAge(false),
	mAgeAlertPerformed(false),
	mAvatarNameCallbackConnection()
{
	if (mID.notNull())
	{
		// NOTE: typically we request these once on creation to avoid excess traffic/processing. 
		//This means updates to these properties won't typically be seen while target is in nearby range.
		LLAvatarPropertiesProcessor* processor = LLAvatarPropertiesProcessor::getInstance();

		processor->addObserver(mID, this);
		processor->sendAvatarPropertiesRequest(mID);
		processor->sendAvatarNotesRequest(mID);
	}

	updateName();
}

FSRadarEntry::~FSRadarEntry()
{
	if (mID.notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(mID, this); // may try to remove null observer
	}
	if (mAvatarNameCallbackConnection.connected())
	{
		mAvatarNameCallbackConnection.disconnect();
	}
}

void FSRadarEntry::updateName()
{
	if (mAvatarNameCallbackConnection.connected())
	{
		mAvatarNameCallbackConnection.disconnect();
	}
	mAvatarNameCallbackConnection = LLAvatarNameCache::get(mID, boost::bind(&FSRadarEntry::onAvatarNameCache, this, _1, _2));
}

void FSRadarEntry::onAvatarNameCache(const LLUUID& av_id, const LLAvatarName& av_name)
{
	if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		mUserName = av_name.getUserNameForDisplay();
		mDisplayName = av_name.getDisplayName();
		mName = getRadarName(av_name);
		mIsLinden = FSCommon::isLinden(av_id);
	}
	else
	{
		std::string name = getRadarName(av_name);
		mUserName = name;
		mDisplayName = name;
		mName = name;
		mIsLinden = false;
	}
}

void FSRadarEntry::processProperties(void* data, EAvatarProcessorType type)
{
	if (data)
	{
		if (type == APT_PROPERTIES)
		{
			LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
			if (avatar_data && avatar_data->agent_id == gAgentID && avatar_data->avatar_id == mID)
			{
				mAge = ((LLDate::now().secondsSinceEpoch() - (avatar_data->born_on).secondsSinceEpoch()) / 86400);
				mStatus = avatar_data->flags;
				checkAge();
			}
		}
		else if (type == APT_NOTES)
		{
			LLAvatarNotes* avatar_notes = static_cast<LLAvatarNotes*>(data);
			if (avatar_notes && avatar_notes->agent_id == gAgentID && avatar_notes->target_id == mID)
			{
				mHasNotes = !avatar_notes->notes.empty();
			}
		}
	}
}

// static
std::string FSRadarEntry::getRadarName(const LLAvatarName& av_name)
{
// [RLVa:KB-FS] - Checked: 2011-06-11 (RLVa-1.3.1) | Added: RLVa-1.3.1
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		return RlvStrings::getAnonym(av_name);
	}
// [/RLVa:KB-FS]

	U32 fmt = gSavedSettings.getU32("RadarNameFormat");
	// if display names are enabled, allow a variety of formatting options, depending on menu selection
	if (gSavedSettings.getBOOL("UseDisplayNames"))
	{	
		if (fmt == FSRADAR_NAMEFORMAT_DISPLAYNAME)
		{
			return av_name.getDisplayName();
		}
		else if (fmt == FSRADAR_NAMEFORMAT_USERNAME)
		{
			return av_name.getUserNameForDisplay();
		}
		else if (fmt == FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME)
		{
			if (av_name.isDisplayNameDefault())
			{
				return av_name.getUserNameForDisplay();
			}
			else
			{
				return llformat("%s (%s)", av_name.getDisplayName().c_str(), av_name.getUserNameForDisplay().c_str());
			}
		}
		else if (fmt == FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME)
		{
			if (av_name.isDisplayNameDefault())
			{
				return av_name.getUserNameForDisplay();
			}
			else
			{
				return llformat("%s (%s)", av_name.getUserNameForDisplay().c_str(), av_name.getDisplayName().c_str());
			}
		}
	}
	
	// else use legacy name lookups
	return av_name.getUserNameForDisplay(); // will be mapped to legacyname automatically by the name cache
}

void FSRadarEntry::checkAge()
{
	mAlertAge = (mAge > -1 && mAge <= gSavedSettings.getS32("RadarAvatarAgeAlertValue"));
	if (!mAlertAge || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		mAgeAlertPerformed = true;
	}
}
