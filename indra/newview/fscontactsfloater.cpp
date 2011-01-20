/** 
 * @file 
 * @brief 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"

#include "fscontactsfloater.h"
#include "llfloaterreg.h"
#include "llfloater.h"

//
// LLFloaterContacts
//

LLFloaterContacts::LLFloaterContacts(const LLSD& seed)
	: LLFloater(seed)
{
	//mFactoryMap["friends_panel"] = LLCallbackMap(LLFloaterContacts::createFriendsPanel, NULL);
	//mFactoryMap["groups_panel"] = LLCallbackMap(LLFloaterContacts::createGroupsPanel, NULL);
}

LLFloaterContacts::~LLFloaterContacts()
{
}

BOOL LLFloaterContacts::postBuild()
{
	return TRUE;
}

void LLFloaterContacts::onOpen(const LLSD& key)
{
	if (key.asString() == "friends")
	{
		childShowTab("friends_and_groups", "friends_panel");
	}
	else if (key.asString() == "groups")
	{
		childShowTab("friends_and_groups", "groups_panel");
	}
}

//static
void* LLFloaterContacts::createFriendsPanel(void* data)
{
	//return new LLPanelPeople();
	return NULL;
}

//static
void* LLFloaterContacts::createGroupsPanel(void* data)
{
	//return new LLPanelGroups();
	return NULL;
}

//static
LLFloaterContacts* LLFloaterContacts::findInstance()
{
	return LLFloaterReg::findTypedInstance<LLFloaterContacts>("imcontacts");
}

LLFloaterContacts* LLFloaterContacts::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLFloaterContacts>("imcontacts");
}

// EOF
