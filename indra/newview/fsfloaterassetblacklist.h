/**
 * @file fsfloaterassetblacklist.h
 * @brief Floater for Asset Blacklist and Derender
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Wolfspirit Magic
 * Copyright (C) 2016, Ansariel Hiller
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

#ifndef FS_FLOATERASSETBLACKLIST_H
#define FS_FLOATERASSETBLACKLIST_H

#include "llfloater.h"
#include "lllistcontextmenu.h"

class FSScrollListCtrl;

class FSFloaterAssetBlacklist : public LLFloater
{
	LOG_CLASS(FSFloaterAssetBlacklist);

public:
	FSFloaterAssetBlacklist(const LLSD& key);
	virtual ~FSFloaterAssetBlacklist();

	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/ BOOL postBuild();

	void addElementToList(const LLUUID& id, const LLSD& data);
	void removeElements();

protected:
	void onRemoveBtn();
	void onCloseBtn();
	void onFilterEdit(const std::string& search_string);

	void buildBlacklist();
	std::string getTypeString(S32 type);

private:
	FSScrollListCtrl*	mResultList;

	std::string			mFilterSubString;
	std::string			mFilterSubStringOrig;
};

namespace FSFloaterAssetBlacklistMenu
{

	class FSAssetBlacklistMenu : public LLListContextMenu
	{
	public:
		/*virtual*/ LLContextMenu* createMenu();
	private:
		void onContextMenuItemClick(const LLSD& param);
	};

	extern FSAssetBlacklistMenu gFSAssetBlacklistMenu;

} // namespace FSFloaterAssetBlacklistMenu

#endif // FS_FLOATERWSASSETBLACKLIST_H
