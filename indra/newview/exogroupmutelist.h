/**
 * @file exogroupmutelist.h
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

#ifndef EXO_GROUPMUTELIST_H
#define EXO_GROUPMUTELIST_H

#include "lluuid.h"

class exoGroupMuteList : public LLSingleton<exoGroupMuteList>
{
public:
	exoGroupMuteList();
	bool isMuted(const LLUUID &group) const;
	void add(const LLUUID &group);
	void remove(const LLUUID &group);

private:
	bool loadMuteList();
	bool saveMuteList();
	std::string getFilePath() const;
	
	std::set<LLUUID> mMuted;
};

#endif
