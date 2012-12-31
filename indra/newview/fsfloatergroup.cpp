/**
 * @file fsfloatergroup.cpp
 * @brief Class for the standalone group details floater in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloatergroup.h"

#include "llfloaterreg.h"

FSFloaterGroup::FSFloaterGroup(const LLSD& seed)
	: LLFloater(seed),
	mGroupPanel(NULL),
	mIsCreateGroup(false)
{
}

FSFloaterGroup::~FSFloaterGroup()
{
}

BOOL FSFloaterGroup::postBuild()
{
	mGroupPanel = findChild<LLPanelGroup>("panel_group_info_sidetray");
	if (!mGroupPanel)
	{
		return FALSE;
	}

	return TRUE;
}

void FSFloaterGroup::onOpen(const LLSD& key)
{
	// openFloater() sets the key again - we only want the group id as key, so set it again!
	setKey(LLSD().with("group_id", key.get("group_id").asUUID()));
	mIsCreateGroup = (key.has("action") && key.get("action").asString() == "create");
	mGroupPanel->onOpen(key);

	if (mIsCreateGroup)
	{
		mGroupPanel->getChildView("header_container")->setVisible(TRUE);
		mGroupPanel->getChildView("back")->setVisible(FALSE);
	}
	else
	{
		mGroupPanel->getChildView("header_container")->setVisible(FALSE);
	}
}

void FSFloaterGroup::setGroup(const LLUUID& group_id)
{
	mGroupPanel->setGroupID(group_id);
}

void FSFloaterGroup::setGroupName(const std::string& group_name)
{
	if (mIsCreateGroup)
	{
		setTitle(getString("title_create_group"));
	}
	else
	{
		if (group_name.empty())
		{
			setTitle(getString("title_loading"));
		}
		else
		{
			LLStringUtil::format_map_t args;
			args["[NAME]"] = group_name.c_str();
			setTitle(getString("title", args));
		}
	}
}

// static
FSFloaterGroup* FSFloaterGroup::openGroupFloater(const LLUUID& group_id)
{
	return FSFloaterGroup::openGroupFloater(LLSD().with("group_id", group_id));
}

// static
FSFloaterGroup* FSFloaterGroup::openGroupFloater(const LLSD& params)
{
	if (!params.has("group_id"))
	{
		return NULL;
	}

	FSFloaterGroup* floater = LLFloaterReg::getTypedInstance<FSFloaterGroup>("fs_group", LLSD().with("group_id", params.get("group_id").asUUID()));
	if (floater)
	{
		floater->openFloater(params);
		return floater;
	}

	return NULL;
}

// static
void FSFloaterGroup::closeGroupFloater(const LLUUID& group_id)
{
	LLFloaterReg::hideInstance("fs_group", LLSD().with("group_id", group_id));
}

// static
bool FSFloaterGroup::isFloaterVisible(const LLUUID& group_id)
{
	LLFloater* inst = LLFloaterReg::findInstance("fs_group", LLSD().with("group_id", group_id));
	if (inst)
	{
		return inst->getVisible();
	}
	
	return false;
}

// static
FSFloaterGroup* FSFloaterGroup::getInstance(const LLUUID& group_id)
{
	return LLFloaterReg::getTypedInstance<FSFloaterGroup>("fs_group", LLSD().with("group_id", group_id));
}

// static
FSFloaterGroup* FSFloaterGroup::findInstance(const LLUUID& group_id)
{
	return LLFloaterReg::findTypedInstance<FSFloaterGroup>("fs_group", LLSD().with("group_id", group_id));
}