/** 
 * @file fspanelblocklist.h
 * @brief Container for blocked Residents & Objects list
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (c) 2014 Ansariel Hiller
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// Original file: llpanelblockedlist.cpp

#ifndef FS_PANELBLOCKLIST_H
#define FS_PANELBLOCKLIST_H

#include "llpanel.h"
#include "llmutelist.h"
#include "llfloater.h"

class LLAvatarName;
class FSScrollListCtrl;

class FSPanelBlockList
	:	public LLPanel, public LLMuteListObserver
{
	friend class FSBlockListMenu;

public:
	FSPanelBlockList();
	~FSPanelBlockList();

	virtual BOOL postBuild();
	virtual void onOpen(const LLSD& key);
	/*virtual*/ BOOL handleKeyHere(KEY key, MASK mask);
	/*virtual*/ bool hasAccelerators() const { return true; }
	
	void selectBlocked(const LLUUID& id);

	/**
	 *	Shows current Panel in side tray and select passed blocked item.
	 * 
	 *	@param idToSelect - LLUUID of blocked Resident or Object to be selected. 
	 *			If it is LLUUID::null, nothing will be selected.
	 */
	static void showPanelAndSelect(const LLUUID& idToSelect = LLUUID::null);

	// LLMuteListObserver callback interface implementation.
	/* virtual */ void onChange() {	refreshBlockedList();}
	
private:
	typedef enum e_sort_oder{
		E_SORT_BY_NAME_ASC = 0,
		E_SORT_BY_TYPE_ASC = 1,
		E_SORT_BY_NAME_DESC = 2,
		E_SORT_BY_TYPE_DESC = 3
	} ESortOrder;

	void refreshBlockedList();
	void updateButtons();
	void removePicker();
	void onSortChanged();

	// UI callbacks
	void removeMutes();
	void blockResidentByName();
	void blockObjectByName();
	void showProfile();

	void onSelectionChanged();
	void onFilterEdit(std::string search_string);

	// List commnads
	void onCustomAction(const LLSD& userdata);
	bool isActionChecked(const LLSD& userdata);
	bool isActionEnabled(const LLSD& userdata);

	void callbackBlockPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);
	void callbackBlockByName(const std::string& text);

private:
	FSScrollListCtrl* mBlockedList;
	LLHandle<LLFloater> mAvatarPicker;
	LLHandle<LLFloater> mObjectPicker;
	std::string mFilterSubString;
	std::string mFilterSubStringOrig;
};

#endif // FS_PANELBLOCKLIST_H
