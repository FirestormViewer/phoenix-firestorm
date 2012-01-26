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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#ifndef FS_FSFLOATERPROFILE_H
#define FS_FSFLOATERPROFILE_H

#include "llviewerprecompiledheaders.h"
#include "llfloater.h"

class LLAvatarName;
class LLTextBox;
class AvatarStatusObserver;

class FSFloaterProfile : public LLFloater
{
	LOG_CLASS(FSFloaterProfile);
	friend class AvatarStatusObserver;
public:
	FSFloaterProfile(const LLSD& key);
	virtual ~FSFloaterProfile();

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ BOOL postBuild();

    const LLUUID& getAvatarId() const { return mAvatarId; }

protected:
    void setAvatarId(const LLUUID& avatar_id) { mAvatarId = avatar_id; }

	bool isGrantedToSeeOnlineStatus();

	/**
	 * Displays avatar's online status if possible.
	 *
	 * Requirements from EXT-3880:
	 * For friends:
	 * - Online when online and privacy settings allow to show
	 * - Offline when offline and privacy settings allow to show
	 * - Else: nothing
	 * For other avatars:
	 *  - Online when online and was not set in Preferences/"Only Friends & Groups can see when I am online"
	 *  - Else: Offline
	 */
	void updateOnlineStatus();
	void processOnlineStatus(bool online);

private:
    void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);


    LLUUID mAvatarId;
	AvatarStatusObserver* mAvatarStatusObserver;


    LLTextBox* mStatusText;

};

#endif // FS_FSFLOATERPROFILE_H
