/** 
 * @file fscontactsfloater.h
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
 
 
#ifndef LL_FSCONTACTSFLOATER_H
#define LL_FSCONTACTSFLOATER_H

#include <map>
#include <vector>

#include "llfloater.h"
#include "llpanelpeople.h"

class LLAvatarList;
class LLPanel;
class LLPanel;

class FSFloaterContacts : public LLFloater
{
public:
	friend class LLPanelPeople;
	FSFloaterContacts(const LLSD& seed);
	virtual ~FSFloaterContacts();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

	static FSFloaterContacts* getInstance();
	static FSFloaterContacts* findInstance();
	
	static void* createFriendsPanel(void* data);
	static void* createGroupsPanel(void* data);

private:
	LLAvatarList*			mFriendList;
	LLPanel*				mFriendsTab;
};


#endif // LL_FSCONTACTSFLOATER_H
