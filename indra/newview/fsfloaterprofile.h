/**
 * @file fsfloaterprofile.h
 * @brief Legacy Profile Floater
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Kadah Coba
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

#ifndef FS_FLOATERPROFILE_H
#define FS_FLOATERPROFILE_H

#include "llavatarnamecache.h"
#include "llfloater.h"

class FSPanelProfile;

class FSFloaterProfile : public LLFloater
{
	LOG_CLASS(FSFloaterProfile);
public:
	FSFloaterProfile(const LLSD& key);
	virtual ~FSFloaterProfile();

	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/ BOOL postBuild();

	void showPick(const LLUUID& pick_id = LLUUID::null);
	bool isPickTabSelected();

	void showClassified(const LLUUID& classified_id = LLUUID::null, bool edit = false);

protected:
	void onOKBtn();
	void onCancelBtn();

private:
	LLAvatarNameCache::callback_connection_t mNameCallbackConnection;
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);

	FSPanelProfile* mPanelProfile;

	LLUUID mAvatarId;
};

#endif // FS_FLOATERPROFILE_H
