/**
 * @file fsavatarrenderpersistence.h
 * @brief Firestorm avatar render settings persistence
 *
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Copyright (c) 2016 Ansariel Hiller @ Second Life
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

#ifndef FS_AVATARRENDERPERSISTENCE_H
#define FS_AVATARRENDERPERSISTENCE_H

#include "llsingleton.h"
#include "llvoavatar.h"

class FSAvatarRenderPersistence
	: public LLSingleton<FSAvatarRenderPersistence>
{
	LOG_CLASS(FSAvatarRenderPersistence);

friend class LLSingleton<FSAvatarRenderPersistence>;
friend class FSPanelPreferenceBackup;

public:
	void init();
	LLVOAvatar::VisualMuteSettings getAvatarRenderSettings(const LLUUID& avatar_id);
	void setAvatarRenderSettings(const LLUUID& avatar_id, LLVOAvatar::VisualMuteSettings render_settings);

private:
	FSAvatarRenderPersistence();
	virtual ~FSAvatarRenderPersistence();

	void loadAvatarRenderSettings();
	void saveAvatarRenderSettings();

	typedef std::map<LLUUID, LLVOAvatar::VisualMuteSettings> avatar_render_setting_t;
	avatar_render_setting_t mAvatarRenderMap;
};

#endif // FS_AVATARRENDERPERSISTENCE_H
