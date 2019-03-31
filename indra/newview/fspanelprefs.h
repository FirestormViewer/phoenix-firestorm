/** 
 * @file fspanelprefs.h 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_PANELPREFS_H
#define FS_PANELPREFS_H

#include "llfloaterpreference.h"

class FSEmbeddedItemDropTarget;

class FSPanelPrefs : public LLPanelPreference
{
public:
	FSPanelPrefs();
	virtual ~FSPanelPrefs() {}

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

	void refreshBeamLists();

private:
	void onBeamColorNew();
	void onBeamNew();
	void onBeamColorDelete();
	void onBeamDelete();

	void onCommitTexture(const LLSD& data);
	void onCommitCopy();
	void onCommitTrans();

	void onDADEmbeddedItem(const LLUUID& item_id);

	void onResetDefaultFolders();

	FSEmbeddedItemDropTarget*	mInvDropTarget;
	std::string					mEmbeddedItem;
};

#endif // FS_PANELPREFS_H
