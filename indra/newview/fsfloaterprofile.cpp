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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "fsfloaterprofile.h"

// Newview
#include "fspanelprofile.h"

static const std::string PANEL_PROFILE = "panel_profile";

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
    
    FSPanelProfile* panel_profile = findChild<FSPanelProfile>(PANEL_PROFILE);
    panel_profile->onOpen(getAvatarId());

    //process tab open cmd here
}

BOOL FSFloaterProfile::postBuild()
{

	return TRUE;
}

// eof
