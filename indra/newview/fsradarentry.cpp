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

#include "llavatarnamecache.h"
#include "rlvhandler.h"

FSRadarEntry::FSRadarEntry(const LLUUID& avid)
	: mID(avid),
	mName(LLStringUtil::null),
	mUserName(LLStringUtil::null),
	mDisplayName(LLStringUtil::null),
	mRange(0.f),
	mFirstSeen(time(NULL)),
	mGlobalPos(LLVector3d(0.0f,0.0f,0.0f)),
	mRegion(LLUUID::null),
	mStatus(0),
	mZOffset(0.f),
	mLastZOffsetTime(time(NULL))
{
	// NOTE: typically we request these once on creation to avoid excess traffic/processing. 
	//This means updates to these properties won't typically be seen while target is in nearby range.
	LLAvatarPropertiesProcessor* processor = LLAvatarPropertiesProcessor::getInstance();
	processor->addObserver(mID, this);
	processor->sendAvatarPropertiesRequest(mID);

	updateName();
}

FSRadarEntry::~FSRadarEntry()
{
	if (mID.notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(mID, this); // may try to remove null observer
	}
}

void FSRadarEntry::updateName()
{
	LLAvatarNameCache::get(mID, boost::bind(&FSRadarEntry::onAvatarNameCache, this, _2));
}

void FSRadarEntry::onAvatarNameCache(const LLAvatarName& av_name)
{
	if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		mUserName = av_name.mUsername;
		mDisplayName = av_name.mDisplayName;
	}
	else
	{
		std::string name = RlvStrings::getAnonym(av_name);
		mUserName = name;
		mDisplayName = name;
	}
}

void FSRadarEntry::processProperties(void* data, EAvatarProcessorType type)
{
	if (data && type == APT_PROPERTIES)
	{
		LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
		mAge = ((LLDate::now().secondsSinceEpoch() - (avatar_data->born_on).secondsSinceEpoch()) / 86400);
		mStatus = avatar_data->flags;		
	}
}