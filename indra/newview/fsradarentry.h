/**
 * @file fsradarentry.h
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

#ifndef FS_RADARENTRY_H
#define FS_RADARENTRY_H

#include "llavatarpropertiesprocessor.h"

class LLAvatarName;

class FSRadarEntry : public LLAvatarPropertiesObserver
{
friend class FSRadar;

public:
	FSRadarEntry(const LLUUID& avid);
	~FSRadarEntry();

	LLUUID		getId() const { return mID; }
	std::string	getName() const { return mName; }
	std::string	getUserName() const { return mUserName; }
	std::string	getDisplayName() const { return mDisplayName; }
	F32			getRange() const { return mRange; }
	LLVector3d	getGlobalPos() const { return mGlobalPos; }
	LLUUID		getRegion() const { return mRegion; }
	time_t		getFirstSeen() const { return mFirstSeen; }
	S32			getStatus() const { return mStatus; }
	S32			getAge() const { return mAge; }
	F32			getZOffset() const { return mZOffset; }
	time_t		getLastZOffsetTime() const { return mLastZOffsetTime; }

	void		setZOffset(F32 offset) { mZOffset = offset; }

private:
	void updateName();
	void onAvatarNameCache(const LLAvatarName& av_name);
	void processProperties(void* data, EAvatarProcessorType type);

	LLUUID		mID;
	std::string mName;
	std::string mUserName;
	std::string mDisplayName;
	F32			mRange;
	LLVector3d	mGlobalPos;
	LLUUID		mRegion;
	time_t		mFirstSeen;
	S32			mStatus;
	S32			mAge;
	F32			mZOffset;
	time_t		mLastZOffsetTime;

};

#endif // FS_RADARENTRY_H