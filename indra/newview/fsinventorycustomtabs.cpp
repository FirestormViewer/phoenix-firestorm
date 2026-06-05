/**
 * @file fsinventorycustomtabs.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsinventorycustomtabs.h"

#include "llbutton.h"
#include "llfiltereditor.h"
#include "llfolderviewitem.h"
#include "llinventoryfilter.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpanelmaininventory.h"
#include "llsdparam.h"
#include "llsdutil.h"
#include "llstring.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "llui.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"

namespace
{
    constexpr char SETTING_KEY[]        = "FSInventoryCustomTabs";
    constexpr char DEFAULT_NAME_KEY[]   = "FSInventoryCustomTabDefaultName";
    constexpr char RENAME_NOTIFY[]      = "FSInventoryCustomTabRename";
    constexpr char MENU_FILE[]          = "menu_inventory_custom_tab.xml";
    constexpr char ADD_TAB_PANEL_NAME[] = "FSInventoryCustomTabAdd";
    constexpr char ADD_TAB_LABEL[]      = "+";
    constexpr char FALLBACK_LABEL[]     = "Custom";
    constexpr char CLOSE_ICON_NAME[]    = "Icon_Close_Foreground";
    constexpr char CLOSE_ICON_PRESS[]   = "Icon_Close_Press";
    constexpr char TAB_BUTTON_PREFIX[]  = "htab_";
    constexpr S32  CLOSE_ICON_SIZE      = 16;
    constexpr S32  CLOSE_ICON_MARGIN    = 4;
    constexpr S32  CLOSE_BUTTON_RESERVE = CLOSE_ICON_SIZE + CLOSE_ICON_MARGIN + 2;
    constexpr S32  ADD_TAB_MIN_WIDTH    = 18;
    constexpr size_t MAX_TABS           = 64;
    constexpr size_t MAX_NAME_LENGTH    = 128;
    constexpr size_t MAX_FILTER_LENGTH  = 400; // It's 300 in XML, but keep some wiggle room
    std::set<FSInventoryCustomTabs*>& allInstances()
    {
        static std::set<FSInventoryCustomTabs*> s;
        return s;
    }
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

FSInventoryCustomTabs::FSInventoryCustomTabs(LLPanelMainInventory* parent)
    : mParent(parent)
{
    if (!mParent)
    {
        return;
    }

    mTabs = mParent->mFilterTabs;
    if (!mTabs)
    {
        return;
    }

    if (gMenuHolder)
    {
        if (auto* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>(MENU_FILE, gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mMenuHandle = menu->getHandle();
        }
    }

    installAddTab();
    allInstances().insert(this);
}

FSInventoryCustomTabs::~FSInventoryCustomTabs()
{
    allInstances().erase(this);
    if (mSettingConnection.connected())
    {
        mSettingConnection.disconnect();
    }
    if (mRenameNotification)
    {
        LLNotifications::instance().cancel(mRenameNotification);
        mRenameNotification.reset();
    }
    if (mAddClickConnection.connected())
    {
        mAddClickConnection.disconnect();
    }
    if (mTabs && !mPanels.empty())
    {
        save();
    }
    if (auto* menu = mMenuHandle.get())
    {
        menu->die();
        mMenuHandle.markDead();
    }
}

// ---------------------------------------------------------------------------
// Public hooks called from LLPanelMainInventory
// ---------------------------------------------------------------------------

void FSInventoryCustomTabs::registerCommitCallbacks(LLPanelMainInventory* parent)
{
    if (!parent)
    {
        return;
    }
    auto& reg = parent->getCommitCallbackRegistrar();
    reg.add("FSInventoryCustomTab.Rename", [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onRenameClicked(); });
    reg.add("FSInventoryCustomTab.Clone",  [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onCloneClicked();  });
    reg.add("FSInventoryCustomTab.Close",  [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onCloseClicked();  });
}

bool FSInventoryCustomTabs::handleRightMouseDown(S32 x, S32 y)
{
    auto* menu = static_cast<LLMenuGL*>(mMenuHandle.get());
    if (!menu)
    {
        return false;
    }

    S32 local_x = 0;
    S32 local_y = 0;
    auto* inv_panel = hitTestCustomTab(x, y, local_x, local_y);
    if (!inv_panel)
    {
        return false;
    }

    mContextPanel = inv_panel;
    menu->buildDrawLabels();
    menu->updateParent(LLMenuGL::sMenuContainer);
    LLMenuGL::showPopup(mParent, menu, x, y);
    return true;
}

bool FSInventoryCustomTabs::handleMouseDown(S32 x, S32 y)
{
    S32 local_x = 0;
    S32 local_y = 0;
    auto* inv_panel = hitTestCustomTab(x, y, local_x, local_y);
    if (!inv_panel)
    {
        return false;
    }

    auto* tab_btn = findTabButton(inv_panel);
    if (!tab_btn)
    {
        return false;
    }

    const S32 btn_local_x = local_x - tab_btn->getRect().mLeft;
    if (btn_local_x < tab_btn->getRect().getWidth() - CLOSE_BUTTON_RESERVE)
    {
        return false;
    }

    onClosePressed(inv_panel);
    return true;
}

void FSInventoryCustomTabs::onFilterFocusLost()
{
    if (mParent)
    {
        auto* active = dynamic_cast<LLInventoryPanel*>(mParent->getActivePanel());
        if (active && isCustomTab(active) && !mExplicitlyNamed.count(active))
        {
            updateAutoLabel(active);
        }
    }
    save();
}

void FSInventoryCustomTabs::onAddTabSelected()
{
    if (!mTabs)
    {
        return;
    }

    const S32 add_idx = mTabs->getIndexForPanel(mAddTabPanel);
    const S32 last_idx = mLastActivePanel ? mTabs->getIndexForPanel(mLastActivePanel) : -1;
    const S32 count = mTabs->getTabCount();

    if (add_idx >= 0 && last_idx >= 0)
    {
        if (last_idx < add_idx && add_idx + 1 < count)
        {
            mTabs->selectTab(add_idx + 1);
            return;
        }
        if (last_idx > add_idx && add_idx - 1 >= 0)
        {
            mTabs->selectTab(add_idx - 1);
            return;
        }
    }

    if (mLastActivePanel && last_idx >= 0)
    {
        mTabs->selectTabPanel(mLastActivePanel);
    }
    else
    {
        mTabs->selectFirstTab();
    }
}

void FSInventoryCustomTabs::onAddClicked()
{
    if (addTab(std::string(), std::string(), true))
    {
        save();
    }
}

void FSInventoryCustomTabs::noteActivePanel(LLInventoryPanel* panel)
{
    if (mLastActivePanel && mLastActivePanel != panel && isCustomTab(mLastActivePanel))
    {
        if (!mExplicitlyNamed.count(mLastActivePanel))
        {
            updateAutoLabel(mLastActivePanel);
        }
        save();
    }
    if (panel && panel != mAddTabPanel && mTabs && mTabs->getIndexForPanel(panel) >= 0)
    {
        mLastActivePanel = panel;
    }
}

void FSInventoryCustomTabs::onClosePressed(LLInventoryPanel* panel)
{
    if (!isCustomTab(panel))
    {
        return;
    }
    mContextPanel = panel;
    onCloseClicked();
}

void FSInventoryCustomTabs::notifyActiveFilterStateChanged()
{
    if (!mParent || !isCustomTab(mParent->getActivePanel()))
    {
        return;
    }
    save();
}

FSInventoryCustomTabs* FSInventoryCustomTabs::instanceFor(LLPanelMainInventory* parent)
{
    return parent ? parent->mCustomTabs.get() : nullptr;
}

const FSInventoryCustomTabs* FSInventoryCustomTabs::instanceFor(const LLPanelMainInventory* parent)
{
    return parent ? parent->mCustomTabs.get() : nullptr;
}

void FSInventoryCustomTabs::install(LLPanelMainInventory* parent, LLFilterEditor* filter_editor)
{
    if (!parent)
    {
        return;
    }
    parent->mCustomTabs = std::make_unique<FSInventoryCustomTabs>(parent);
    parent->mCustomTabs->load();
    parent->mCustomTabs->attachToFilterEditor(filter_editor);
}

void FSInventoryCustomTabs::notifyFilterStateChanged(LLPanelMainInventory* parent)
{
    if (auto* self = instanceFor(parent))
    {
        self->notifyActiveFilterStateChanged();
    }
}

bool FSInventoryCustomTabs::activePanelIsCustom(const LLPanelMainInventory* parent)
{
    const auto* self = instanceFor(parent);
    return self && self->isCustomTab(parent->getActivePanel());
}

bool FSInventoryCustomTabs::maybeHandleAddTabSelected(LLPanelMainInventory* parent)
{
    auto* self = instanceFor(parent);
    if (!self || !parent->mFilterTabs || !self->isAddTab(parent->mFilterTabs->getCurrentPanel()))
    {
        return false;
    }
    self->onAddTabSelected();
    return true;
}

void FSInventoryCustomTabs::noteActivePanel(LLPanelMainInventory* parent, LLInventoryPanel* panel)
{
    if (auto* self = instanceFor(parent))
    {
        self->noteActivePanel(panel);
    }
}

S32 FSInventoryCustomTabs::captureFilterGeneration(const LLPanelMainInventory* parent)
{
    if (!parent)
    {
        return 0;
    }
    const auto* active = parent->getActivePanel();
    return active ? active->getFilter().getCurrentGeneration() : 0;
}

void FSInventoryCustomTabs::notifyIfFilterChanged(LLPanelMainInventory* parent, S32 prev_generation)
{
    auto* self = instanceFor(parent);
    if (!self)
    {
        return;
    }
    const auto* active = parent->getActivePanel();
    if (active && active->getFilter().getCurrentGeneration() != prev_generation)
    {
        self->notifyActiveFilterStateChanged();
    }
}

bool FSInventoryCustomTabs::handleRightMouseDown(LLPanelMainInventory* parent, S32 x, S32 y)
{
    auto* self = instanceFor(parent);
    return self && self->handleRightMouseDown(x, y);
}

bool FSInventoryCustomTabs::handleMouseDown(LLPanelMainInventory* parent, S32 x, S32 y)
{
    auto* self = instanceFor(parent);
    return self && self->handleMouseDown(x, y);
}

void FSInventoryCustomTabs::onParentDraw(LLPanelMainInventory* parent)
{
    if (auto* self = instanceFor(parent))
    {
        self->ensureRootOpenForActivePanel();
    }
}

void FSInventoryCustomTabs::ensureRootOpenForActivePanel()
{
    if (mPendingRootOpen.empty() || !mParent)
    {
        return;
    }
    auto* active = dynamic_cast<LLInventoryPanel*>(mParent->getActivePanel());
    if (!active)
    {
        return;
    }
    const auto it = mPendingRootOpen.find(active);
    if (it == mPendingRootOpen.end())
    {
        return;
    }
    if (auto* folder = dynamic_cast<LLFolderViewFolder*>(active->getItemByID(gInventory.getRootFolderID())))
    {
        folder->setOpen(true);
        active->getFilter().setModified(LLInventoryFilter::FILTER_RESTART);
        mPendingRootOpen.erase(it);
    }
}

// ---------------------------------------------------------------------------
// Tab management
// ---------------------------------------------------------------------------

void FSInventoryCustomTabs::installAddTab()
{
    if (!mTabs || mAddTabPanel)
    {
        return;
    }

    LLInventoryPanel::Params params;
    params.name(ADD_TAB_PANEL_NAME);
    params.sort_order_setting(LLInventoryPanel::INHERIT_SORT_ORDER);
    params.preinitialize_views(false);
    params.follows.flags(FOLLOWS_ALL);

    mAddTabPanel = LLUICtrlFactory::create<LLInventoryPanel>(params);
    if (!mAddTabPanel)
    {
        return;
    }

    mTabs->addTabPanel(LLTabContainer::TabPanelParams()
        .panel(mAddTabPanel)
        .label(ADD_TAB_LABEL)
        .select_tab(false)
        .insert_at(LLTabContainer::END));

    if (auto* btn = findTabButton(mAddTabPanel))
    {
        const S32 height = btn->getRect().getHeight();
        const S32 width = llmax(mTabs->getMinTabWidth() / 2, ADD_TAB_MIN_WIDTH);
        btn->reshape(width, height);
        btn->setLeftHPad(2);
        btn->setRightHPad(2);
        mAddClickConnection = btn->setClickedCallback(boost::bind(&FSInventoryCustomTabs::onAddClicked, this));
    }
}

LLInventoryPanel* FSInventoryCustomTabs::addTab(const std::string& explicit_name, const std::string& filter, bool select)
{
    if (!mTabs || !mParent)
    {
        return nullptr;
    }

    if (mPanels.size() >= MAX_TABS)
    {
        LL_WARNS() << "FSInventoryCustomTabs: refusing to add tab; reached cap of " << MAX_TABS << LL_ENDL;
        return nullptr;
    }

    static S32 sCounter = 0;
    const auto internal_name = llformat("FSCustomInvTab_%d", ++sCounter);

    LLInventoryPanel::Params params;
    params.name(internal_name);
    params.sort_order_setting(LLInventoryPanel::INHERIT_SORT_ORDER);
    params.show_item_link_overlays(true);
    params.preinitialize_views(false);
    params.follows.flags(FOLLOWS_ALL);

    auto* panel = LLUICtrlFactory::create<LLInventoryPanel>(params);
    if (!panel)
    {
        return nullptr;
    }

    panel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::DEFAULT_SORT_ORDER));
    panel->getFilter().markDefault();
    panel->setSelectCallback(boost::bind(&LLPanelMainInventory::onSelectionChange, mParent, panel, _1, _2));
    mPanels.insert(panel);
    mPendingRootOpen.insert(panel);

    const std::string auto_label = filter.empty() ? getDefaultTabName() : filter;
    if (!explicit_name.empty() && explicit_name != auto_label)
    {
        mExplicitlyNamed.insert(panel);
    }
    if (!filter.empty())
    {
        panel->setFilterSubString(filter);
    }

    const std::string& display_label = explicit_name.empty() ? auto_label : explicit_name;

    auto where = LLTabContainer::END;
    if (mAddTabPanel)
    {
        const S32 add_idx = mTabs->getIndexForPanel(mAddTabPanel);
        if (add_idx >= 0)
        {
            where = static_cast<LLTabContainer::eInsertionPoint>(add_idx);
        }
    }

    mTabs->addTabPanel(LLTabContainer::TabPanelParams()
        .panel(panel)
        .label(display_label)
        .select_tab(select)
        .insert_at(where));

    decorateCustomTabButton(panel);
    if (auto* tab_btn = findTabButton(panel))
    {
        tab_btn->setToolTip(display_label);
    }
    if (const S32 idx = mTabs->getIndexForPanel(panel); idx >= 0)
    {
        mTabs->setTabPadding(idx, CLOSE_BUTTON_RESERVE);
    }
    return panel;
}

void FSInventoryCustomTabs::onRenameClicked()
{
    if (!isCustomTab(mContextPanel))
    {
        return;
    }

    if (mRenameNotification)
    {
        LLNotifications::instance().cancel(mRenameNotification);
        mRenameNotification.reset();
    }

    LLSD args;
    args["NAME"] = mContextPanel->getLabel();
    LLSD payload;
    payload["panel_name"] = mContextPanel->getName();

    auto handle = getHandle();
    mRenameNotification = LLNotificationsUtil::add(RENAME_NOTIFY, args, payload,
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (auto* self = handle.get())
            {
                self->onRenameConfirmed(notification, response);
            }
        });
}

void FSInventoryCustomTabs::onRenameConfirmed(const LLSD& notification, const LLSD& response)
{
    mRenameNotification.reset();

    if (!mTabs || LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return;
    }

    std::string new_name = sanitizeTabName(response["new_name"].asString());
    if (new_name.empty())
    {
        return;
    }

    auto* panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByName(notification["payload"]["panel_name"].asString()));
    if (!isCustomTab(panel))
    {
        return;
    }

    mExplicitlyNamed.insert(panel);
    applyTabLabel(panel, new_name);
    save();
}

void FSInventoryCustomTabs::onCloneClicked()
{
    if (!isCustomTab(mContextPanel))
    {
        return;
    }
    auto& src_filter = mContextPanel->getFilter();
    const std::string explicit_name = mExplicitlyNamed.count(mContextPanel) ? mContextPanel->getLabel() : std::string();
    auto* new_panel = addTab(explicit_name, src_filter.getFilterSubStringOrig(), true);
    if (!new_panel)
    {
        return;
    }
    LLInventoryFilter::Params filter_params;
    src_filter.toParams(filter_params);
    if (filter_params.validateBlock(false))
    {
        auto& new_filter = new_panel->getFilter();
        new_filter.fromParams(filter_params);
        new_filter.setShowFolderState(src_filter.getShowFolderState());
        new_filter.setFilterCreator(src_filter.getFilterCreatorType());
        new_filter.setFilterLinks(src_filter.getFilterLinks());
        new_filter.setSearchType(src_filter.getSearchType());
    }
    save();
}

void FSInventoryCustomTabs::onCloseClicked()
{
    if (!isCustomTab(mContextPanel) || !mTabs)
    {
        return;
    }

    auto* to_remove = mContextPanel;
    mContextPanel = nullptr;
    if (mLastActivePanel == to_remove)
    {
        mLastActivePanel = nullptr;
    }

    if (mTabs->getCurrentPanel() == to_remove)
    {
        const S32 idx = mTabs->getIndexForPanel(to_remove);
        if (idx > 0)
        {
            mTabs->selectTab(idx - 1);
        }
        else
        {
            mTabs->selectFirstTab();
        }
    }

    mPanels.erase(to_remove);
    mExplicitlyNamed.erase(to_remove);
    mPendingRootOpen.erase(to_remove);
    mTabs->removeTabPanel(to_remove);
    to_remove->die();
    save();
}

void FSInventoryCustomTabs::clearCustomTabs()
{
    if (!mTabs)
    {
        return;
    }

    mLoadedOk = false;
    mContextPanel = nullptr;
    mLastActivePanel = nullptr;

    auto* current = mTabs->getCurrentPanel();
    if (current && (current == mAddTabPanel || mPanels.count(dynamic_cast<LLInventoryPanel*>(current))))
    {
        mTabs->selectFirstTab();
    }

    auto panels_copy = mPanels;
    mPanels.clear();
    mExplicitlyNamed.clear();
    mPendingRootOpen.clear();
    for (auto* panel : panels_copy)
    {
        mTabs->removeTabPanel(panel);
        panel->die();
    }

    mLastActivePanel = nullptr;
    mContextPanel = nullptr;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void FSInventoryCustomTabs::load()
{
    if (mLoading)
    {
        return;
    }
    mLoading = true;

    for (auto* other : allInstances())
    {
        if (other != this)
        {
            other->save();
        }
    }

    doLoad();
    subscribeToSetting();
    mLoading = false;
}

void FSInventoryCustomTabs::doLoad()
{
    mLoadedOk = false;

    const LLSD tabs = gSavedPerAccountSettings.getLLSD(SETTING_KEY);
    if (!tabs.isArray())
    {
        mLoadedOk = true;
        return;
    }

    size_t attempted = 0;
    size_t added = 0;
    for (auto it = tabs.beginArray(); it != tabs.endArray(); ++it)
    {
        if (mPanels.size() >= MAX_TABS)
        {
            LL_WARNS() << "FSInventoryCustomTabs: settings array truncated at " << MAX_TABS << " entries" << LL_ENDL;
            break;
        }
        const LLSD& entry = *it;
        if (!entry.isMap())
        {
            continue;
        }

        const std::string explicit_name = entry["name"].asString();

        std::string filter = entry["filter"].asString();
        if (filter.size() > MAX_FILTER_LENGTH)
        {
            filter = utf8str_truncate(filter, static_cast<S32>(MAX_FILTER_LENGTH));
        }

        ++attempted;
        if (auto* new_panel = addTab(explicit_name, filter, false))
        {
            ++added;
            if (entry.has("states") && entry["states"].isMap())
            {
                const LLSD& states = entry["states"];
                LLInventoryFilter::Params filter_params;
                LLParamSDParser().readSD(states, filter_params, true);
                auto& new_filter = new_panel->getFilter();
                new_filter.fromParams(filter_params);
                const LLSD& ops = states["filter_ops"];
                if (ops.isMap())
                {
                    if (ops.has("show_folder_state"))
                    {
                        new_filter.setShowFolderState(static_cast<LLInventoryFilter::EFolderShow>(ops["show_folder_state"].asInteger()));
                    }
                    if (ops.has("creator_type"))
                    {
                        new_filter.setFilterCreator(static_cast<LLInventoryFilter::EFilterCreatorType>(ops["creator_type"].asInteger()));
                    }
                    if (ops.has("links"))
                    {
                        new_filter.setFilterLinks(static_cast<U64>(ops["links"].asInteger()));
                    }
                }
                if (states.has("search_type"))
                {
                    new_filter.setSearchType(static_cast<LLInventoryFilter::ESearchType>(states["search_type"].asInteger()));
                }
            }
        }
    }

    mLoadedOk = (added == attempted);
    if (!mLoadedOk)
    {
        LL_WARNS() << "FSInventoryCustomTabs: load incomplete (" << added << "/" << attempted << " entries created); save() will refuse until this loads cleanly." << LL_ENDL;
    }
}

void FSInventoryCustomTabs::subscribeToSetting()
{
    if (mSettingConnection.connected())
    {
        return;
    }
    if (auto control = gSavedPerAccountSettings.getControl(SETTING_KEY))
    {
        mSettingConnection = control->getSignal()->connect(boost::bind(&FSInventoryCustomTabs::onSettingChangedExternally, this));
    }
}

void FSInventoryCustomTabs::attachToFilterEditor(LLFilterEditor* filter_editor)
{
    if (!filter_editor)
    {
        return;
    }
    auto* inner = filter_editor->findChild<LLLineEditor>("filter edit box");
    if (!inner)
    {
        return;
    }
    auto handle = getHandle();
    inner->setFocusLostCallback([handle](LLFocusableElement*)
    {
        if (auto* self = handle.get())
        {
            self->onFilterFocusLost();
        }
    });
}

void FSInventoryCustomTabs::save()
{
    if (!mTabs || mLoading || !mLoadedOk)
    {
        return;
    }

    LLSD array = LLSD::emptyArray();
    const S32 count = mTabs->getTabCount();
    for (S32 i = 0; i < count; ++i)
    {
        auto* inv_panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByIndex(i));
        if (!inv_panel || !mPanels.count(inv_panel))
        {
            continue;
        }
        LLSD entry;
        entry["name"] = mExplicitlyNamed.count(inv_panel) ? inv_panel->getLabel() : std::string();
        entry["filter"] = inv_panel->getFilter().getFilterSubStringOrig();

        auto& filter = inv_panel->getFilter();
        LLInventoryFilter::Params filter_params;
        filter.toParams(filter_params);
        if (filter_params.validateBlock(false))
        {
            LLSD states_sd;
            LLParamSDParser().writeSD(states_sd, filter_params);
            if (states_sd.isMap() && states_sd.has("substring"))
            {
                states_sd.erase("substring");
            }
            states_sd["filter_ops"]["show_folder_state"] = static_cast<S32>(filter.getShowFolderState());
            states_sd["filter_ops"]["creator_type"] = static_cast<S32>(filter.getFilterCreatorType());
            states_sd["filter_ops"]["links"] = static_cast<S32>(filter.getFilterLinks());
            states_sd["search_type"] = static_cast<S32>(filter.getSearchType());
            entry["states"] = states_sd;
        }

        array.append(entry);
    }

    if (isUnchangedSetting(array))
    {
        return;
    }
    mSaving = true;
    gSavedPerAccountSettings.setLLSD(SETTING_KEY, array);
    mSaving = false;
}

bool FSInventoryCustomTabs::isUnchangedSetting(const LLSD& array) const
{
    const LLSD current = gSavedPerAccountSettings.getLLSD(SETTING_KEY);
    if (!current.isArray())
    {
        return array.size() == 0;
    }
    if (current.size() != array.size())
    {
        return false;
    }
    for (LLSD::Integer i = 0; i < array.size(); ++i)
    {
        if (current[i]["name"].asString() != array[i]["name"].asString() || current[i]["filter"].asString() != array[i]["filter"].asString() || !llsd_equals(current[i]["states"], array[i]["states"]))
        {
            return false;
        }
    }
    return true;
}

void FSInventoryCustomTabs::onSettingChangedExternally()
{
    if (mSaving || mLoading)
    {
        return;
    }
    if (mRenameNotification)
    {
        LLNotifications::instance().cancel(mRenameNotification);
        mRenameNotification.reset();
    }
    mLoading = true;
    clearCustomTabs();
    doLoad();
    mLoading = false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool FSInventoryCustomTabs::isCustomTab(const LLPanel* panel) const
{
    auto* nonconst = const_cast<LLInventoryPanel*>(dynamic_cast<const LLInventoryPanel*>(panel));
    return nonconst && mPanels.count(nonconst);
}

bool FSInventoryCustomTabs::isAddTab(const LLPanel* panel) const
{
    return panel && panel == static_cast<const LLPanel*>(mAddTabPanel);
}

LLButton* FSInventoryCustomTabs::findTabButton(const LLPanel* panel) const
{
    if (!mTabs || !panel)
    {
        return nullptr;
    }
    return mTabs->findChild<LLButton>(TAB_BUTTON_PREFIX + panel->getName());
}

LLInventoryPanel* FSInventoryCustomTabs::hitTestCustomTab(S32 x, S32 y, S32& tab_local_x, S32& tab_local_y) const
{
    if (!mTabs || !mParent)
    {
        return nullptr;
    }
    tab_local_x = 0;
    tab_local_y = 0;
    if (!mParent->localPointToOtherView(x, y, &tab_local_x, &tab_local_y, mTabs))
    {
        return nullptr;
    }
    const S32 idx = mTabs->getTabContainedAtPoint(tab_local_x, tab_local_y);
    if (idx < 0)
    {
        return nullptr;
    }
    auto* inv_panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByIndex(idx));
    if (!inv_panel || !mPanels.count(inv_panel))
    {
        return nullptr;
    }
    return inv_panel;
}

void FSInventoryCustomTabs::decorateCustomTabButton(LLInventoryPanel* panel)
{
    if (!mTabs || !panel)
    {
        return;
    }
    auto* btn = findTabButton(panel);
    if (!btn)
    {
        return;
    }

    const S32 btn_w = btn->getRect().getWidth();
    const S32 btn_h = btn->getRect().getHeight();
    if (btn_w < CLOSE_BUTTON_RESERVE || btn_h < CLOSE_ICON_SIZE)
    {
        return;
    }

    btn->setRightHPad(CLOSE_BUTTON_RESERVE);

    auto handle = getHandle();
    LLButton::Params close_params;
    close_params.name("fs_inv_tab_close_" + panel->getName());
    close_params.rect(LLRect(btn_w - CLOSE_ICON_MARGIN - CLOSE_ICON_SIZE, (btn_h + CLOSE_ICON_SIZE) / 2, btn_w - CLOSE_ICON_MARGIN, (btn_h - CLOSE_ICON_SIZE) / 2));
    close_params.follows.flags(FOLLOWS_RIGHT);
    close_params.image_unselected.name(CLOSE_ICON_NAME);
    close_params.image_selected.name(CLOSE_ICON_PRESS);
    close_params.image_hover_unselected.name(CLOSE_ICON_PRESS);
    close_params.image_pressed.name(CLOSE_ICON_PRESS);
    close_params.tab_stop(false);
    close_params.label(LLStringUtil::null);
    close_params.click_callback.function([handle, panel](LLUICtrl*, const LLSD&)
    {
        if (auto* self = handle.get())
        {
            self->onClosePressed(panel);
        }
    });

    if (auto* close_btn = LLUICtrlFactory::create<LLButton>(close_params))
    {
        btn->addChild(close_btn);
    }
}

void FSInventoryCustomTabs::applyTabLabel(LLInventoryPanel* panel, const std::string& label)
{
    if (!panel || !mTabs)
    {
        return;
    }
    panel->setLabel(label);
    if (const S32 idx = mTabs->getIndexForPanel(panel); idx >= 0)
    {
        mTabs->setPanelTitle(idx, label);
    }
    if (auto* btn = findTabButton(panel))
    {
        btn->setToolTip(label);
    }
    if (mParent && mParent->getActivePanel() == panel)
    {
        mParent->refreshFinderFromFilter();
    }
}

std::string FSInventoryCustomTabs::computeDisplayLabel(LLInventoryPanel* panel) const
{
    if (!panel)
    {
        return getDefaultTabName();
    }
    if (mExplicitlyNamed.count(panel))
    {
        return panel->getLabel();
    }
    const std::string& filter = panel->getFilter().getFilterSubStringOrig();
    return filter.empty() ? getDefaultTabName() : filter;
}

void FSInventoryCustomTabs::updateAutoLabel(LLInventoryPanel* panel)
{
    if (!panel || mExplicitlyNamed.count(panel))
    {
        return;
    }
    const std::string new_label = computeDisplayLabel(panel);
    if (panel->getLabel() == new_label)
    {
        return;
    }
    applyTabLabel(panel, new_label);
}

std::string FSInventoryCustomTabs::getDefaultTabName()
{
    std::string name;
    if (LLTrans::findString(name, DEFAULT_NAME_KEY) && !name.empty())
    {
        return name;
    }
    return FALLBACK_LABEL;
}

std::string FSInventoryCustomTabs::sanitizeTabName(std::string name)
{
    std::string out;
    out.reserve(name.size());

    for (size_t i = 0; i < name.size(); )
    {
        const auto c = static_cast<unsigned char>(name[i]);
        if (c < 0x20 || c == 0x7F)
        {
            ++i;
            continue;
        }
        if (c == 0xC2 && i + 1 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xAD)
        {
            i += 2;
            continue;
        }
        if (c == 0xE2 && i + 2 < name.size())
        {
            const auto c1 = static_cast<unsigned char>(name[i + 1]);
            const auto c2 = static_cast<unsigned char>(name[i + 2]);
            if (c1 == 0x80 && ((c2 >= 0x8B && c2 <= 0x8F) // U+200B ... U+200F
                || (c2 >= 0xA8 && c2 <= 0xA9) // U+2028 ... U+2029
                || (c2 >= 0xAA && c2 <= 0xAE))) // U+202A ... U+202E
            {
                i += 3;
                continue;
            }
            if (c1 == 0x81 && c2 >= 0xA6 && c2 <= 0xA9) // U+2066 ... U+2069
            {
                i += 3;
                continue;
            }
        }
        if (c == 0xEF && i + 2 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xBB && static_cast<unsigned char>(name[i + 2]) == 0xBF)
        {
            i += 3; // U+FEFF (BOM)
            continue;
        }
        out += static_cast<char>(c);
        ++i;
    }

    LLStringUtil::trim(out);
    if (out.size() > MAX_NAME_LENGTH)
    {
        out = utf8str_truncate(out, static_cast<S32>(MAX_NAME_LENGTH));
    }

    return out;
}
