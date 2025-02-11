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
#include "llinventorypanel.h"
#include "lldndbutton.h"

#include "llfolderview.h"

class LLComboBox;
class LLFolderViewItem;
class LLInventoryPanel;
class LLInventoryGallery;
class LLSaveFolderState;
class LLFilterEditor;
class LLTabContainer;
class LLFloaterInventoryFinder;
class LLMenuButton;
class LLMenuGL;
class LLSidepanelInventory;
class LLToggleableMenu;
class LLFloater;
class LLFloaterSidePanelContainer;
class LLSidepanelInventory;
class LLPanelMarketplaceInbox;
class LLComboBox;   // <FS:Zi> Filter dropdown

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

    bool postBuild();

    enum EViewModeType
    {
        MODE_LIST,
        MODE_GALLERY,
        MODE_COMBINATION
    };

    virtual bool handleKeyHere(KEY key, MASK mask);

    // Inherited functionality
    /*virtual*/ bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                                       EDragAndDropType cargo_type,
                                       void* cargo_data,
                                       EAcceptance* accept,
                                       std::string& tooltip_msg);
    /*virtual*/ void changed(U32);
    /*virtual*/ void draw();
    /*virtual*/ void    onVisibilityChange ( bool new_visibility );
    // <FS:Ansariel> CTRL-F focusses local search editor
    /*virtual*/ bool hasAccelerators() const { return true; }

    LLInventoryPanel* getPanel() { return mActivePanel; }
    LLInventoryPanel* getActivePanel() { return mActivePanel; }
    const LLInventoryPanel* getActivePanel() const { return mActivePanel; }
    LLInventoryPanel* getAllItemsPanel();
    void selectAllItemsPanel();
    // <FS:Ansariel> FIRE-19493: "Show Original" should open main inventory panel
    void showAllItemsPanel();
    // </FS:Ansariel>
    void setActivePanel();

    bool isRecentItemsPanelSelected();

    const std::string& getFilterText() const { return mFilterText; }

    void setSelectCallback(const LLFolderView::signal_t::slot_type& cb);

    void onFilterEdit(const std::string& search_string );

    void setFocusOnFilterEditor();

    static LLFloaterSidePanelContainer* newWindow();
    static void newFolderWindow(LLUUID folder_id = LLUUID(), LLUUID item_to_select = LLUUID());

    void toggleFindOptions();

    void resetFilters();
    void resetAllItemsFilters();

    void findLinks(const LLUUID& item_id, const std::string& item_name);

    void onViewModeClick();
    void toggleViewMode();
    void initSingleFolderRoot(const LLUUID& start_folder_id = LLUUID::null);
    void initInventoryViews();
    void onUpFolderClicked();
    void onBackFolderClicked();
    void onForwardFolderClicked();
    void setSingleFolderViewRoot(const LLUUID& folder_id, bool clear_nav_history = true);
    void setGallerySelection(const LLUUID& item_id, bool new_window = false);
    LLUUID getSingleFolderViewRoot();
    bool isSingleFolderMode() { return mSingleFolderMode; }

    void scrollToGallerySelection();
    void scrollToInvPanelSelection();

    void setViewMode(EViewModeType mode);
    bool isListViewMode() { return (mViewMode == MODE_LIST); }
    bool isGalleryViewMode() { return (mViewMode == MODE_GALLERY); }
    bool isCombinationViewMode() { return (mViewMode == MODE_COMBINATION); }
    LLUUID getCurrentSFVRoot();
    std::string getLocalizedRootName();

    LLInventoryFilter& getCurrentFilter();

    void setParentSidepanel(LLSidepanelInventory* parent_sidepanel) { mParentSidepanel = parent_sidepanel; }
    void setInboxPanel(LLPanelMarketplaceInbox* inbox_panel) { mInboxPanel = inbox_panel; }

    // <FS:Zi> Filter dropdown
    void onFilterTypeSelected(const std::string& filter_type_name);
    void updateFilterDropdown(const LLInventoryFilter* filter);
    // </FS:Zi> Filter dropdown

    void doCustomAction(const LLSD& userdata) { onCustomAction(userdata); } // <FS:Ansariel> Prevent warning "No callback found for: 'Inventory.CustomAction' in control: Find Links"

    // <FS:Ansariel> FIRE-12808: Don't save filters during settings restore
    static bool sSaveFilters;

protected:
    //
    // Misc functions
    //
    void setFilterTextFromFilter();
    void startSearch();

    void onSelectionChange(LLInventoryPanel *panel, const std::deque<LLFolderViewItem*>& items, bool user_action);

    static bool filtersVisible(void* user_data);
    void onClearSearch();
    static void onFoldersByName(void *user_data);
    static bool checkFoldersByName(void *user_data);

    static bool incrementalFind(LLFolderViewItem* first_item, const char *find_text, bool backward);
    void onFilterSelected();

    const std::string getFilterSubString();
    void setFilterSubString(const std::string& string);

    // menu callbacks
    void doToSelected(const LLSD& userdata);
    void closeAllFolders();
    void doCreate(const LLSD& userdata);

    // <FS:Zi> Sort By menu handlers
    void setSortBy(const LLSD& userdata);
    bool isSortByChecked(const LLSD& userdata);
    // </FS:Zi> Sort By menu handlers

    // <FS:minerjr> [FIRE-35042] Inventory - Only Coalesced Filter - More accessible
    // Callback method for the new Show Filter button on the bottom button panel of the main inventory.
    // Stays highlighted if any filter is enabled.
    bool isAnyFilterChecked(const LLSD& userdata);
    // </FS:minerjr> [FIRE-35042]

    void saveTexture(const LLSD& userdata);
    bool isSaveTextureEnabled(const LLSD& userdata);
    void updateItemcountText();

    void updatePanelVisibility();
    void updateCombinationVisibility();

    // <FS:Zi> Inventory Collapse and Expand Buttons
    void onCollapseButtonClicked();
    void onExpandButtonClicked();
    // </FS:Zi> Inventory Collapse and Expand Buttons
    void onFocusReceived();
    void onSelectSearchType();
    void updateSearchTypeCombo();
    void setSearchType(LLInventoryFilter::ESearchType type);

    LLSidepanelInventory* getParentSidepanelInventory();

private:
    enum class EFetchState
    {
        Unknown,
        Fetching,
        Complete
    };

    LLFloaterInventoryFinder* getFinder();

    LLFilterEditor*             mFilterEditor;
    LLTabContainer*             mFilterTabs;
    LLUICtrl*                   mCounterCtrl;
    LLHandle<LLFloater>         mFinderHandle;
    LLInventoryPanel*           mActivePanel;
    LLInventoryPanel*           mAllItemsPanel = nullptr;
    LLInventoryPanel*           mRecentPanel = nullptr;
    LLInventoryPanel*           mWornItemsPanel = nullptr;
    bool                        mResortActivePanel;
    LLSaveFolderState*          mSavedFolderState;
    std::string                 mFilterText;
    std::string                 mFilterSubString;
    S32                         mItemCount = 0;
    std::string                 mLastFilterText;
    std::string                 mItemCountString;
    S32                         mCategoryCount = 0;
    std::string                 mCategoryCountString;
    LLComboBox*                 mSearchTypeCombo;
    EFetchState                 mLastFetchState{ EFetchState::Unknown };

    LLButton* mBackBtn;
    LLButton* mForwardBtn;
    LLButton* mUpBtn;
    LLButton* mViewModeBtn;
    LLLayoutPanel* mNavigationBtnsPanel;

    LLPanel* mDefaultViewPanel;
    LLPanel* mCombinationViewPanel;

    bool mSingleFolderMode;
    EViewModeType mViewMode;

    LLInventorySingleFolderPanel* mCombinationInventoryPanel;
    LLInventoryGallery* mCombinationGalleryPanel;
    LLPanel* mCombinationGalleryLayoutPanel;
    LLLayoutPanel* mCombinationListLayoutPanel;
    LLLayoutStack* mCombinationLayoutStack;

    // <FS:Zi> Filter dropdown
    LLComboBox*                 mFilterComboBox;
    std::map<std::string,U64>   mFilterMap;         // contains name-to-number mapping for dropdown filter types
    U64                         mFilterMask;        // contains the cumulated bit filter for all dropdown filter types
    // </FS:Zi> Filter dropdown

    boost::signals2::connection mListViewRootUpdatedConnection;
    boost::signals2::connection mGalleryRootUpdatedConnection;

    //////////////////////////////////////////////////////////////////////////////////
    // List Commands                                                                //
protected:
    void initListCommandsHandlers();
    void updateListCommands();
    void onAddButtonClick();
    void showActionMenu(LLMenuGL* menu, std::string spawning_view_name);
    void onTrashButtonClick(); // <FS:Ansariel> Keep better inventory layout
    void onClipboardAction(const LLSD& userdata);
    bool isActionEnabled(const LLSD& command_name);
    bool isActionChecked(const LLSD& userdata);
    void onCustomAction(const LLSD& command_name);
    bool isActionVisible(const LLSD& userdata);

    // <FS:Zi> FIRE-31369: Add inventory filter for coalesced objects
    void onCoalescedObjectsToggled(const LLSD& userdata);
    bool isCoalescedObjectsChecked(const LLSD& userdata);
    // </FS:Zi>

    // <FS:Zi> Filter Links Menu
    bool isFilterLinksChecked(const LLSD& userdata);
    void onFilterLinksChecked(const LLSD& userdata);
    // </FS:Zi> Filter Links Menu

    // <FS:Zi> FIRE-1175 - Filter Permissions Menu
    bool isFilterPermissionsChecked(const LLSD &userdata);
    void onFilterPermissionsChecked(const LLSD &userdata);
    // </FS:Zi>

    // <FS:Zi> Extended Inventory Search
    bool isSearchTypeChecked(const LLSD& userdata);
    void onSearchTypeChecked(const LLSD& userdata);
    // </FS:Zi> Extended Inventory Search

    // <FS:Ansariel> Keep better inventory layout
    bool handleDragAndDropToTrash(bool drop, EDragAndDropType cargo_type, EAcceptance* accept);
    static bool hasSettingsInventory();
    static bool hasMaterialsInventory();
    void updateTitle();
    void updateNavButtons();

    void onCombinationRootChanged(bool gallery_clicked);
    void onCombinationGallerySelectionChanged(const LLUUID& category_id);
    void onCombinationInventorySelectionChanged(const std::deque<LLFolderViewItem*>& items, bool user_action);
    /**
     * Set upload cost in "Upload" sub menu.
     */
    void setUploadCostIfNeeded();
    void disableAddIfNeeded();
private:
    LLSidepanelInventory*       mParentSidepanel = nullptr;
    LLPanelMarketplaceInbox*    mInboxPanel = nullptr;

    LLDragAndDropButton*        mTrashButton; // <FS:Ansariel> Keep better inventory layout
    LLToggleableMenu*           mMenuGearDefault;
    LLToggleableMenu*           mMenuViewDefault;
    LLToggleableMenu*           mMenuVisibility;
    LLMenuButton*               mGearMenuButton;
    LLMenuButton*               mViewMenuButton;
    LLMenuButton*               mVisibilityMenuButton;
    LLHandle<LLView>            mMenuAddHandle;

    // <FS:Zi> Inventory Collapse and Expand Buttons
    LLButton*                   mCollapseBtn;
    LLButton*                   mExpandBtn;
    // </FS:Zi> Inventory Collapse and Expand Buttons

    bool                        mNeedUploadCost;

    bool                        mForceShowInvLayout;
    bool                        mReshapeInvLayout;
    LLUUID                      mCombInvUUIDNeedsRename;
    // List Commands                                                              //
    ////////////////////////////////////////////////////////////////////////////////
};

#endif // LL_LLPANELMAININVENTORY_H



