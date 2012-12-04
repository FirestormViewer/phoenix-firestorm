/**
 * @file floatermedialists.h
 * @brief Floater to edit media white/blacklists - headers
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, Sione Lomu
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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
 * $/LicenseInfo$
 */
#ifndef LL_LLFLOATERMEDIALISTS_H
#define LL_LLFLOATERMEDIALISTS_H

#include "llfloater.h"


class LLScrollListCtrl;
class LLButton;

class FloaterMediaLists : public LLFloater
{
public:
    FloaterMediaLists(const LLSD &);
	BOOL postBuild();
    virtual ~FloaterMediaLists();

	virtual void draw();
	void refresh();

	static void onWhitelistAdd(void*);
	static void onWhitelistRemove(void*);
	static void onBlacklistAdd(void*);
	static void onBlacklistRemove(void*);
	static void onCommitDomain(void*);

private:
	static bool sIsWhitelist;
	LLScrollListCtrl*	mWhitelistSLC;
	LLScrollListCtrl*	mBlacklistSLC;
};
#endif