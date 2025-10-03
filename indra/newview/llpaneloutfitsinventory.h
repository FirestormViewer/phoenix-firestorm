/**
 * @file llpaneloutfitsinventory.h
 * @brief Outfits inventory panel
 * class definition
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#ifndef LL_LLPANELOUTFITSINVENTORY_H
#define LL_LLPANELOUTFITSINVENTORY_H

#include "llpanel.h"

class LLButton;
class LLOutfitGallery;
class LLOutfitsList;
class LLOutfitListGearMenuBase;
class LLPanelAppearanceTab;
class LLPanelWearing;
class LLMenuButton;
class LLMenuGL;
class LLSidepanelAppearance;
class LLTabContainer;
class LLInventoryCategoriesObserver; // <FS:Ansariel> FIRE-17626: Attachment count in appearance floater

class LLPanelOutfitsInventory : public LLPanel
{
    LOG_CLASS(LLPanelOutfitsInventory);
public:
    LLPanelOutfitsInventory();
    virtual ~LLPanelOutfitsInventory();

    /*virtual*/ bool postBuild();
    /*virtual*/ void onOpen(const LLSD& key);

    void draw(); // <FS:Ansariel> FIRE-17626: Attachment count in appearance floater

    void onSearchEdit(const std::string& string);
    void onSave();
    void saveOutfit(bool as_new = false);

    bool onSaveCommit(const LLSD& notification, const LLSD& response);

    static LLSidepanelAppearance* getAppearanceSP();

// [RLVa:KB] - Checked: 2010-08-24 (RLVa-1.4.0a) | Added: RLVa-1.2.1a
    LLTabContainer* getAppearanceTabs()     { return mAppearanceTabs; }
    LLOutfitsList*  getMyOutfitsPanel()     { return mMyOutfitsPanel; }
    LLPanelWearing* getCurrentOutfitPanel() { return mCurrentOutfitPanel; }
// [/RLVa:KB]

    static LLPanelOutfitsInventory* findInstance();

    void openApearanceTab(const std::string& tab_name);

    bool isCOFPanelActive() const;

    void setMenuButtons(
        LLMenuButton* gear_menu,
        LLMenuButton* sort_menu,
        LLButton* trash_btn,
        LLPanel* sort_menu_panel,
        LLPanel* trash_menu_panel);

    // <FS:Ansariel> Show avatar complexity in appearance floater
    void updateAvatarComplexity(U32 complexity, const std::map<LLUUID, U32>& item_complexity, const std::map<LLUUID, U32>& temp_item_complexity, U32 body_parts_complexity);

protected:
    void updateVerbs();

private:
    LLTabContainer*         mAppearanceTabs;

    // <FS:Ansariel> FIRE-17626: Attachment count in appearance floater
    LLInventoryCategoriesObserver* mCategoriesObserver;
    void onCOFChanged();

    U32 mCurrentTempAttachmentCount{ 0 };

    LLFrameTimer mTempAttachmentUpdateTimer;
    // </FS:Ansariel>

    //////////////////////////////////////////////////////////////////////////////////
    // tab panels                                                                   //
protected:
    void                    initTabPanels();
    void                    onTabChange();
    bool                    isOutfitsListPanelActive() const;
    bool                    isOutfitsGalleryPanelActive() const;

private:
    LLPanelAppearanceTab*   mActivePanel;
    LLOutfitsList*          mMyOutfitsPanel;
    LLOutfitGallery*        mOutfitGalleryPanel;
    LLPanelWearing*         mCurrentOutfitPanel;

    // tab panels                                                                   //
    //////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////
    // List Commands                                                                //
protected:
    void initListCommandsHandlers();
    void updateListCommands();
    void onWearButtonClick();
    void onTrashButtonClick();
    void onGearMouseDown();
    bool isActionEnabled(const LLSD& userdata);
    void setWearablesLoading(bool val);
    void onWearablesLoaded();
    void onWearablesLoading();
private:
    LLPanel*                    mListCommands;
    LLButton*                   mWearBtn = nullptr;
    // List Commands                                                                //
    //////////////////////////////////////////////////////////////////////////////////

    bool mInitialized;

    // not owned items
    LLMenuButton* mGearMenu;
    LLMenuButton* mSortMenu;
    LLButton* mTrashBtn;
    LLPanel* mSortMenuPanel;
    LLPanel* mTrashMenuPanel;
    boost::signals2::connection mGearMenuConnection;
    boost::signals2::connection mSortMenuConnection;
    boost::signals2::connection mTrashMenuConnection;
};

#endif //LL_LLPANELOUTFITSINVENTORY_H
