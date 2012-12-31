/**
 * @file fsfloatergroup.h
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

#ifndef FS_FLOATERGROUP_H
#define FS_FLOATERGROUP_H

#include "llfloater.h"
#include "llpanelgroup.h"

class LLPanelGroup;

class FSFloaterGroup : public LLFloater
{
public:
	FSFloaterGroup(const LLSD& seed);
	/*virtual*/ ~FSFloaterGroup();
	/*virtual*/ void onOpen(const LLSD& key);

	BOOL postBuild();

	void setGroup(const LLUUID& group_id);

	void setGroupName(const std::string& group_name);
	LLPanelGroup* getGroupPanel() const { return mGroupPanel; };

	static FSFloaterGroup* openGroupFloater(const LLUUID& group_id);
	static FSFloaterGroup* openGroupFloater(const LLSD& params);
	static void closeGroupFloater(const LLUUID& group_id);
	static bool isFloaterVisible(const LLUUID& group_id);
	static FSFloaterGroup* getInstance(const LLUUID& group_id);
	static FSFloaterGroup* findInstance(const LLUUID& group_id);

private:
	LLPanelGroup*	mGroupPanel;
	bool			mIsCreateGroup;
};

#endif // FS_FLOATERGROUP_H
