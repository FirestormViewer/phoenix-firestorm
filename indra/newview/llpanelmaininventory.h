/** 
 * @file llpanelmaininventory.h
 * @brief llpanelmaininventory.h
 * class definition
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_LLPANELMAININVENTORY_H
#define LL_LLPANELMAININVENTORY_H

#include "llpanel.h"
#include "llinventoryfilter.h"
#include "llinventoryobserver.h"
#include "lldndbutton.h"

#include "llfolderview.h"

class LLFolderViewItem;
class LLInventoryPanel;
class LLSaveFolderState;
class LLFilterEditor;
class LLTabContainer;
class LLFloaterInventoryFinder;
class LLMenuButton;
class LLMenuGL;
class LLToggleableMenu;
class LLFloater;
class LLComboBox;	// ## Zi: Filter dropdown

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPanelMainInventory
//
// This is a panel used to view and control an agent's inventory,
// including all the fixin's (e.g. AllItems/RecentItems tabs, filter floaters).
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLPanelMainInventory : public LLPanel, LLInventoryObserver
{
public:
	friend class LLFloaterInventoryFinder;

	LLPanelMainInventory(const LLPanel::Params& p = getDefaultParams());
	~LLPanelMainInventory();

	BOOL postBuild();

	virtual BOOL handleKeyHere(KEY key, MASK mask);

	// Inherited functionality
	/*virtual*/ BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									   EDragAndDropType cargo_type,
									   void* cargo_data,
									   EAcceptance* accept,
									   std::string& tooltip_msg);
	/*virtual*/ void changed(U32);
	/*virtual*/ void draw();

	LLInventoryPanel* getPanel() { return mActivePanel; }
	LLInventoryPanel* getActivePanel() { return mActivePanel; }
	const LLInventoryPanel* getActivePanel() const { return mActivePanel; }

	const std::string& getFilterText() const { return mFilterText; }
	
	void setSelectCallback(const LLFolderView::signal_t::slot_type& cb);

	void onFilterEdit(const std::string& search_string );

	// ## Zi: Filter dropdown
	void onFilterTypeSelected(const std::string& filter_type_name);
	void updateFilterDropdown(const LLInventoryFilter* filter);
	// ## Zi: Filter dropdown

protected:
	//
	// Misc functions
	//
	void setFilterTextFromFilter();
	void startSearch();
	
	void toggleFindOptions();
	void onSelectionChange(LLInventoryPanel *panel, const std::deque<LLFolderViewItem*>& items, BOOL user_action);

	static BOOL filtersVisible(void* user_data);
	void onClearSearch();
	static void onFoldersByName(void *user_data);
	static BOOL checkFoldersByName(void *user_data);
	
	static BOOL incrementalFind(LLFolderViewItem* first_item, const char *find_text, BOOL backward);
	void onFilterSelected();

	const std::string getFilterSubString();
	void setFilterSubString(const std::string& string);
	
	// menu callbacks
	void doToSelected(const LLSD& userdata);
	void closeAllFolders();
	void newWindow();
	void doCreate(const LLSD& userdata);
	void resetFilters();

	// ## Zi: Sort By menu handlers
	void setSortBy(const LLSD& userdata);
	BOOL isSortByChecked(const LLSD& userdata);
	// ## Zi: Sort By menu handlers

	void saveTexture(const LLSD& userdata);
	bool isSaveTextureEnabled(const LLSD& userdata);
	void updateItemcountText();

	// ## Zi: Inventory Collapse and Expand Buttons
	void onCollapseButtonClicked();
	void onExpandButtonClicked();
	// ## Zi: Inventory Collapse and Expand Buttons
	void onFocusReceived();

private:
	LLFloaterInventoryFinder* getFinder();

	LLFilterEditor*				mFilterEditor;
	LLTabContainer*				mFilterTabs;
	LLHandle<LLFloater>			mFinderHandle;
	LLInventoryPanel*			mActivePanel;
	bool						mResortActivePanel;
	LLSaveFolderState*			mSavedFolderState;
	std::string					mFilterText;
	std::string					mFilterSubString;
	LLTextBox*					mItemcountText;

	// ## Zi: Filter dropdown
	LLComboBox*					mFilterComboBox;
	std::map<std::string,U64>	mFilterMap;			// contains name-to-number mapping for dropdown filter types
	U64							mFilterMask;		// contains the cumulated bit filter for all dropdown filter types
	// ## Zi: Filter dropdown

	//////////////////////////////////////////////////////////////////////////////////
	// List Commands                                                                //
protected:
	void initListCommandsHandlers();
	void updateListCommands();
	void onAddButtonClick();
	void showActionMenu(LLMenuGL* menu, std::string spawning_view_name);
	void onTrashButtonClick();
	void onClipboardAction(const LLSD& userdata);
	BOOL isActionEnabled(const LLSD& command_name);
	BOOL isActionChecked(const LLSD& userdata);
	void onCustomAction(const LLSD& command_name);

	// ## Zi: Filter Links Menu
	BOOL isFilterLinksChecked(const LLSD& userdata);
	void onFilterLinksChecked(const LLSD& userdata);
	// ## Zi: Filter Links Menu

	// ## Zi: Extended Inventory Search
	BOOL isSearchTargetChecked(const LLSD& userdata);
	void onSearchTargetChecked(const LLSD& userdata);
	LLInventoryFilter::EFilterSubstringTarget getSearchTarget() const;
	// ## Zi: Extended Inventory Search

	bool handleDragAndDropToTrash(BOOL drop, EDragAndDropType cargo_type, EAcceptance* accept);
	/**
	 * Set upload cost in "Upload" sub menu.
	 */
	void setUploadCostIfNeeded();
private:
	LLDragAndDropButton*		mTrashButton;
	LLToggleableMenu*			mMenuGearDefault;
	LLMenuGL*					mMenuAdd;
	LLMenuButton*				mGearMenuButton;

	// ## Zi: Inventory Collapse and Expand Buttons
	LLButton*					mCollapseBtn;
	LLButton*					mExpandBtn;
	// ## Zi: Inventory Collapse and Expand Buttons

	bool						mNeedUploadCost;
	// List Commands                                                              //
	////////////////////////////////////////////////////////////////////////////////
};

#endif // LL_LLPANELMAININVENTORY_H



