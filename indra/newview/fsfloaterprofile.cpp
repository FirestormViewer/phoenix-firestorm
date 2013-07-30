/**
 * @file fsfloaterprofile.cpp
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

#include "llviewerprecompiledheaders.h"
#include "fsfloaterprofile.h"

// Newview
#include "fspanelprofile.h"
#include "llagent.h" //gAgent
#include "llavatarnamecache.h"
#include "fspanelprofileclassifieds.h"

static const std::string PANEL_PROFILE_VIEW = "panel_profile_view";

FSFloaterProfile::FSFloaterProfile(const LLSD& key)
 : LLFloater(key)
 , mAvatarId(LLUUID::null)
{
}

FSFloaterProfile::~FSFloaterProfile()
{
}

void FSFloaterProfile::onOpen(const LLSD& key)
{
	LLUUID id;
	if(key.has("id"))
	{
		id = key["id"];
	}
	if(!id.notNull()) return;

	setAvatarId(id);

	FSPanelProfile* panel_profile		= findChild<FSPanelProfile>(PANEL_PROFILE_VIEW);
	panel_profile->onOpen(getAvatarId());

	if (getAvatarId() == gAgent.getID())
	{
		getChild<LLUICtrl>("ok_btn")->setVisible( true );
		getChild<LLUICtrl>("cancel_btn")->setVisible( true );
	}

	// Update the avatar name.
	LLAvatarNameCache::get(getAvatarId(), boost::bind(&FSFloaterProfile::onAvatarNameCache, this, _1, _2));
}

BOOL FSFloaterProfile::postBuild()
{
	childSetAction("ok_btn", boost::bind(&FSFloaterProfile::onOKBtn, this));
	childSetAction("cancel_btn", boost::bind(&FSFloaterProfile::onCancelBtn, this));

	return TRUE;
}

void FSFloaterProfile::onOKBtn()
{
	if (getAvatarId() == gAgent.getID())
	{
		FSPanelProfile* panel_profile		= findChild<FSPanelProfile>(PANEL_PROFILE_VIEW);
		panel_profile->apply();
	}

	closeFloater();
}

void FSFloaterProfile::onCancelBtn()
{
	closeFloater();
}

void FSFloaterProfile::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	setTitle(av_name.getCompleteName());
}

// eof
