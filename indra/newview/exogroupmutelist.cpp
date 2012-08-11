/**
 * @file exogroupmutelist.cpp
 * @brief Persistently stores groups to ignore.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (C) 2012 Katharine Berry
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldir.h"
#include "llfile.h"
#include "llgroupactions.h"
#include "llsdserialize.h"

#include "exogroupmutelist.h"

exoGroupMuteList::exoGroupMuteList()
: mMuted()
{
	loadMuteList();
}

bool exoGroupMuteList::isMuted(const LLUUID& group) const
{
	return mMuted.count(group);
}

void exoGroupMuteList::add(const LLUUID& group)
{
	LLGroupActions::endIM(group); // Actually kill ongoing conversation
	if(mMuted.insert(group).second)
	{
		saveMuteList();
	}
}

void exoGroupMuteList::remove(const LLUUID& group)
{
	if(mMuted.erase(group))
	{
		saveMuteList();
	}
}

bool exoGroupMuteList::loadMuteList()
{
	std::string path = getFilePath();
	if(!LLFile::isfile(path))
	{
		// We consider the absence of a mute file to be a successful load
		// because it won't exist if the user's never muted a group.
		LL_INFOS("GroupMute") << "Mute file doesn't exist; skipping load." << LL_ENDL;
		return true;
	}
	llifstream file(path);
	if(!file.is_open())
	{
		LL_WARNS("GroupMute") << "Failed to open group muting list." << LL_ENDL;
		return false;
	}
	LLSD data;
	LLSDSerialize::fromXMLDocument(data, file);
	file.close();

	std::copy(data.beginArray(), data.endArray(), inserter(mMuted, mMuted.begin()));
	return true;
}

bool exoGroupMuteList::saveMuteList()
{
	LLSD data;
	// LLSD doesn't seem to expose insertion using iterators.
	for(std::set<LLUUID>::iterator it = mMuted.begin(); it != mMuted.end(); ++it)
	{
		data.append(*it);
	}

	llofstream file(getFilePath());
	if(!file.is_open())
	{
		LL_WARNS("GroupMute") << "Unable to save group muting list!" << LL_ENDL;
		return false;
	}
	LLSDSerialize::toPrettyXML(data, file);
	file.close();
	return true;
}

std::string exoGroupMuteList::getFilePath() const
{
	return gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "muted_groups.xml");
}

