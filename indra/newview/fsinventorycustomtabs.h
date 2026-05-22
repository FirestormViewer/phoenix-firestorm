/**
 * @file fsinventorycustomtabs.h
 * @brief User-defined inventory filter tabs (FIRE-35598) - feature idea: Catznip
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_INVENTORY_CUSTOM_TABS_H
#define FS_INVENTORY_CUSTOM_TABS_H

#include "llhandle.h"
#include "llnotificationptr.h"
#include "llsd.h"

#include <boost/signals2/connection.hpp>
#include <set>
#include <string>
#include <unordered_set>

class LLButton;
class LLFilterEditor;
class LLInventoryPanel;
class LLPanel;
class LLPanelMainInventory;
class LLTabContainer;
class LLView;

class FSInventoryCustomTabs : public LLHandleProvider<FSInventoryCustomTabs>
{
    LOG_CLASS(FSInventoryCustomTabs);

public:
    explicit FSInventoryCustomTabs(LLPanelMainInventory* parent);
    ~FSInventoryCustomTabs();

    bool handleRightMouseDown(S32 x, S32 y);
    bool handleMouseDown(S32 x, S32 y);

    void noteActivePanel(LLInventoryPanel* panel);
    void onActiveFilterChanged(LLPanel* active_panel);
    void notifyActiveFilterStateChanged();

    static void registerCommitCallbacks(LLPanelMainInventory* parent);
    void load();
    void attachToFilterEditor(LLFilterEditor* filter_editor);

    bool isCustomTab(const LLPanel* panel) const;
    bool isAddTab(const LLPanel* panel) const;
    void onAddTabSelected();
    void onAddClicked();

    void onRenameClicked();
    void onCloneClicked();
    void onCloseClicked();

    static void install(LLPanelMainInventory* parent, LLFilterEditor* filter_editor);
    static void notifyFilterStateChanged(LLPanelMainInventory* parent);
    static bool activePanelIsCustom(const LLPanelMainInventory* parent);
    static bool maybeHandleAddTabSelected(LLPanelMainInventory* parent);
    static void noteActivePanel(LLPanelMainInventory* parent, LLInventoryPanel* panel);
    static void onActiveFilterChanged(LLPanelMainInventory* parent, LLPanel* active);
    static S32  captureFilterGeneration(const LLPanelMainInventory* parent);
    static void notifyIfFilterChanged(LLPanelMainInventory* parent, S32 prev_generation);
    static bool handleRightMouseDown(LLPanelMainInventory* parent, S32 x, S32 y);
    static bool handleMouseDown(LLPanelMainInventory* parent, S32 x, S32 y);
    static void onParentDraw(LLPanelMainInventory* parent);

private:
    static FSInventoryCustomTabs* instanceFor(LLPanelMainInventory* parent);
    static const FSInventoryCustomTabs* instanceFor(const LLPanelMainInventory* parent);
    void ensureRootOpenForActivePanel();

    LLInventoryPanel* addTab(const std::string& explicit_name, const std::string& filter, bool select);
    void save();
    bool isUnchangedSetting(const LLSD& array) const;
    void clearCustomTabs();
    void installAddTab();
    void onSettingChangedExternally();
    void onRenameConfirmed(const LLSD& notification, const LLSD& response);
    void onFilterFocusLost();
    void onClosePressed(LLInventoryPanel* panel);
    void doLoad();
    void subscribeToSetting();

    LLButton* findTabButton(const LLPanel* panel) const;
    void decorateCustomTabButton(LLInventoryPanel* panel);
    void applyTabLabel(LLInventoryPanel* panel, const std::string& label);
    void updateAutoLabel(LLInventoryPanel* panel);
    std::string computeDisplayLabel(LLInventoryPanel* panel) const;
    LLInventoryPanel* hitTestCustomTab(S32 x, S32 y, S32& tab_local_x, S32& tab_local_y) const;

    static std::string getDefaultTabName();
    static std::string sanitizeTabName(std::string name);

    LLPanelMainInventory* mParent{ nullptr };
    LLTabContainer* mTabs{ nullptr };
    LLHandle<LLView> mMenuHandle;
    std::set<LLInventoryPanel*> mPanels;
    std::unordered_set<LLInventoryPanel*> mExplicitlyNamed;
    std::unordered_set<LLInventoryPanel*> mPendingRootOpen;
    LLInventoryPanel* mContextPanel{ nullptr };
    LLInventoryPanel* mAddTabPanel{ nullptr };
    LLInventoryPanel* mLastActivePanel{ nullptr };
    LLNotificationPtr mRenameNotification;
    boost::signals2::connection mSettingConnection;
    boost::signals2::connection mAddClickConnection;
    bool mSaving{ false };
    bool mLoading{ false };
    bool mLoadedOk{ false };
};

#endif // FS_INVENTORY_CUSTOM_TABS_H
