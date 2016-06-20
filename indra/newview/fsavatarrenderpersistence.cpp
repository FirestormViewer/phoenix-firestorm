/**
 * @file fsavatarrenderpersistence.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsavatarrenderpersistence.h"
#include "llsdserialize.h"

FSAvatarRenderPersistence::FSAvatarRenderPersistence()
{
}

FSAvatarRenderPersistence::~FSAvatarRenderPersistence()
{
	saveAvatarRenderSettings();
}

void FSAvatarRenderPersistence::init()
{
	LL_INFOS() << "Initializing avatar render persistence manager..." << LL_ENDL;
	loadAvatarRenderSettings();
}

LLVOAvatar::VisualMuteSettings FSAvatarRenderPersistence::getAvatarRenderSettings(const LLUUID& avatar_id)
{
	avatar_render_setting_t::iterator found = mAvatarRenderMap.find(avatar_id);
	if (found != mAvatarRenderMap.end())
	{
		return found->second;
	}

	return LLVOAvatar::AV_RENDER_NORMALLY;
}

void FSAvatarRenderPersistence::setAvatarRenderSettings(const LLUUID& avatar_id, LLVOAvatar::VisualMuteSettings render_settings)
{
	if (render_settings == LLVOAvatar::AV_RENDER_NORMALLY)
	{
		avatar_render_setting_t::iterator found = mAvatarRenderMap.find(avatar_id);
		if (found != mAvatarRenderMap.end())
		{
			mAvatarRenderMap.erase(found);
		}
	}
	else
	{
		mAvatarRenderMap[avatar_id] = render_settings;
	}
}

void FSAvatarRenderPersistence::loadAvatarRenderSettings()
{
	const std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "avatar_render_settings.xml");

	if (LLFile::isfile(filename))
	{
		llifstream file(filename.c_str());
		if (!file.is_open())
		{
			LL_WARNS() << "Failed to open avatar render settings file." << LL_ENDL;
			return;
		}
		LLSD data;
		LLSDSerialize::fromXMLDocument(data, file);
		file.close();

		mAvatarRenderMap.clear();
		for (LLSD::map_const_iterator it = data.beginMap(); it != data.endMap(); ++it)
		{
			LLUUID avatar_id(it->first);
			LLVOAvatar::VisualMuteSettings render_settings = (LLVOAvatar::VisualMuteSettings)it->second.asInteger();

			mAvatarRenderMap[avatar_id] = render_settings;
		}
	}
}

void FSAvatarRenderPersistence::saveAvatarRenderSettings()
{
	LL_INFOS() << "Saving avatar render settings..." << LL_ENDL;

	const std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "avatar_render_settings.xml");

	LLSD data(LLSD::emptyMap());

	for (avatar_render_setting_t::iterator it = mAvatarRenderMap.begin(); it != mAvatarRenderMap.end(); ++it)
	{
		data[it->first.asString()] = (S32)it->second;
	}

	llofstream file(filename.c_str());
	if (!file.is_open())
	{
		LL_WARNS() << "Unable to save avatar render settings!" << LL_ENDL;
		return;
	}
	LLSDSerialize::toPrettyXML(data, file);
	file.close();
}
