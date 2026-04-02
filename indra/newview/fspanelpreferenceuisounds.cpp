/**
 * @file fspanelpreferenceuisounds.cpp
 * @brief FSPanelPreferenceUISounds class: UI Sounds preferences tab
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Firestorm Viewer Project
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fspanelpreferenceuisounds.h"

#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "llfiltereditor.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llscrolllistcell.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"

#ifdef OPENSIM
#include "llviewernetwork.h"
#endif

static LLPanelInjector<FSPanelPreferenceUISounds> t_pref_ui_sounds("fs_panel_preference_ui_sounds");

static constexpr S32 COL_LABEL  = 0;
static constexpr S32 COL_CHECK  = 1;
static constexpr S32 COL_STATUS = 2;

// ---------------------------------------------------------------------------

FSPanelPreferenceUISounds::FSPanelPreferenceUISounds()
    : LLPanelPreference()
{
    mCommitCallbackRegistrar.add("Pref.SelectUISound",          boost::bind(&FSPanelPreferenceUISounds::onSelectSound, this));
    mCommitCallbackRegistrar.add("Pref.UpdateUISoundFilter",    boost::bind(&FSPanelPreferenceUISounds::onUpdateFilter, this));
    mCommitCallbackRegistrar.add("Pref.CommitUISoundUUID",      boost::bind(&FSPanelPreferenceUISounds::onCommitUUID, this));
    mCommitCallbackRegistrar.add("Pref.CommitUISoundPlayCheck", boost::bind(&FSPanelPreferenceUISounds::onCommitPlayCheck, this));
    mCommitCallbackRegistrar.add("Pref.CommitUISoundPlayCombo", boost::bind(&FSPanelPreferenceUISounds::onCommitPlayCombo, this));
    mCommitCallbackRegistrar.add("Pref.PreviewSelectedUISound", boost::bind(&FSPanelPreferenceUISounds::onPreviewSound, this));
    mCommitCallbackRegistrar.add("Pref.ResetSelectedUISound",   boost::bind(&FSPanelPreferenceUISounds::onResetSound, this));
}

bool FSPanelPreferenceUISounds::postBuild()
{
    mSoundsList    = getChild<LLScrollListCtrl>("ui_sounds_list");
    mFilter        = getChild<LLFilterEditor>("ui_sound_filter");
    mUUIDEditor    = getChild<LLLineEditor>("ui_sound_uuid");
    mPlayCheck     = getChild<LLCheckBoxCtrl>("ui_sound_play_checkbox");
    mPlayCombo     = getChild<LLComboBox>("ui_sound_play_combo");
    mSelectedLabel = getChild<LLTextBox>("ui_sound_selected_label");
    mSettingName   = getChild<LLTextBox>("ui_sound_setting_name");

    mSoundsList->setFilterColumn(COL_LABEL);
    mSoundsList->setDoubleClickCallback(boost::bind(&FSPanelPreferenceUISounds::onDoubleClick, this));
    mSoundsList->setRightMouseDownCallback(boost::bind(&FSPanelPreferenceUISounds::onRightClick, this, _1, _2, _3, _4));

    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    registrar.add("UISounds.CopyUUID", boost::bind(&FSPanelPreferenceUISounds::onCopyUUID, this));

    LLContextMenu::Params menu_params;
    menu_params.name("ui_sounds_context_menu");
    menu_params.visible(false);
    LLContextMenu* menu = LLUICtrlFactory::create<LLContextMenu>(menu_params);

    LLMenuItemCallGL::Params item_params;
    item_params.label("Copy sound UUID");
    item_params.name("copy_uuid");
    item_params.on_click.function_name = "UISounds.CopyUUID";
    menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item_params));

    mContextMenuHandle = menu->getHandle();
    gMenuHolder->addChild(menu);

    applyLocalizedLabels();
    buildList();
    refreshEditor();

    return LLPanelPreference::postBuild();
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void FSPanelPreferenceUISounds::refreshList()
{
    applyLocalizedLabels();
    buildList();
    refreshEditor();
}

// ---------------------------------------------------------------------------
// List building
// ---------------------------------------------------------------------------

FSPanelPreferenceUISounds::UISoundEntry FSPanelPreferenceUISounds::makeEntry(
    std::string_view sound,
    const std::optional<std::string_view>& label_override,
    bool combo,
    bool inverted)
{
    UISoundEntry e;
    e.sound_setting    = sound;
    e.playmode_setting = "PlayMode" + e.sound_setting;
    e.label_control    = label_override.value_or("textFS" + e.sound_setting.substr(5));
    e.uses_combo       = combo;
    e.inverted_bool    = inverted;
    return e;
}

void FSPanelPreferenceUISounds::buildList()
{
    if (!mSoundsList)
    {
        return;
    }

    const S32 prev_sel = getSelectedIndex();

    if (mEntries.empty())
    {
        mEntries = {
            makeEntry("UISndAlert"),
            makeEntry("UISndBadKeystroke"),
            makeEntry("UISndClick"),
            makeEntry("UISndClickRelease"),
            makeEntry("UISndHealthReductionF"),
            makeEntry("UISndHealthReductionM"),
            makeEntry("UISndMoneyChangeDown"),
            makeEntry("UISndMoneyChangeUp"),
            makeEntry("UISndNearbyChat"),
            makeEntry("UISndNewIncomingIMSession", std::nullopt, true),
            makeEntry("UISndNewIncomingGroupIMSession", std::nullopt, true),
            makeEntry("UISndNewIncomingConfIMSession", std::nullopt, true),
            makeEntry("UISndStartIM"),
            makeEntry("UISndChatMention"),
            makeEntry("UISndObjectCreate"),
            makeEntry("UISndObjectDelete"),
            makeEntry("UISndObjectRezIn"),
            makeEntry("UISndObjectRezOut"),
            makeEntry("UISndSnapshot", std::nullopt, false, true),
            makeEntry("UISndTeleportOut"),
            makeEntry("UISndPieMenuAppear"),
            makeEntry("UISndPieMenuHide"),
            makeEntry("UISndPieMenuSliceHighlight0"),
            makeEntry("UISndPieMenuSliceHighlight1"),
            makeEntry("UISndPieMenuSliceHighlight2"),
            makeEntry("UISndPieMenuSliceHighlight3"),
            makeEntry("UISndPieMenuSliceHighlight4"),
            makeEntry("UISndPieMenuSliceHighlight5"),
            makeEntry("UISndPieMenuSliceHighlight6"),
            makeEntry("UISndPieMenuSliceHighlight7"),
            makeEntry("UISndTyping"),
            makeEntry("UISndWindowClose"),
            makeEntry("UISndWindowOpen"),
            makeEntry("UISndScriptFloaterOpen"),
            makeEntry("UISndScriptFloaterClose"),
            makeEntry("UISndFriendOnline"),
            makeEntry("UISndFriendOffline"),
            makeEntry("UISndFriendshipOffer"),
            makeEntry("UISndTeleportOffer"),
            makeEntry("UISndInventoryOffer"),
            makeEntry("UISndIncomingVoiceCall"),
            makeEntry("UISndGroupInvitation"),
            makeEntry("UISndGroupNotice"),
            makeEntry("UISndQuestionExperience"),
            makeEntry("UISndInvalidOp"),
            makeEntry("UISndMovelockToggle"),
            makeEntry("UISndFootsteps"),
            makeEntry("UISndTrackerBeacon"),
            makeEntry("UISndMicToggle"),
            makeEntry("UISndRestart"),
            makeEntry("UISndRestartOpenSim"),
        };
    }

    mSoundsList->deleteAllItems();

    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        UISoundEntry& entry = mEntries[i];

#ifdef OPENSIM
        const bool show_opensim = LLGridManager::instance().isInOpenSim();
#else
        const bool show_opensim = false;
#endif
        if (entry.sound_setting == "UISndRestartOpenSim" && !show_opensim)
        {
            continue;
        }

        entry.display_label = resolveEntryLabel(entry);

        LLSD row;
        row["id"] = LLSD::Integer(i);
        row["columns"][COL_LABEL]["column"] = "ui_sound_label";
        row["columns"][COL_LABEL]["value"]  = entry.display_label;

        if (entry.uses_combo)
        {
            row["columns"][COL_CHECK]["column"]  = "ui_sound_check";
            row["columns"][COL_CHECK]["value"]   = "";
            row["columns"][COL_STATUS]["column"] = "ui_sound_status";
            row["columns"][COL_STATUS]["value"]  = getModeLabel(entry);
        }
        else
        {
            bool val = gSavedSettings.getBOOL(entry.playmode_setting);
            bool is_enabled = entry.inverted_bool ? !val : val;
            row["columns"][COL_CHECK]["type"]    = "checkbox";
            row["columns"][COL_CHECK]["column"]  = "ui_sound_check";
            row["columns"][COL_CHECK]["value"]   = is_enabled;
            row["columns"][COL_STATUS]["column"] = "ui_sound_status";
            row["columns"][COL_STATUS]["value"]  = getModeLabel(entry);
        }

        mSoundsList->addElement(row);
    }

    if (prev_sel >= 0)
    {
        for (LLScrollListItem* row : mSoundsList->getAllData())
        {
            if (row && row->getValue().asInteger() == prev_sel)
            {
                mSoundsList->selectNthItem(mSoundsList->getItemIndex(row));
                break;
            }
        }
    }

    if (mSoundsList->getItemCount() > 0 && !mSoundsList->getFirstSelected())
    {
        mSoundsList->selectFirstItem();
    }
}

// ---------------------------------------------------------------------------
// Editor refresh
// ---------------------------------------------------------------------------

void FSPanelPreferenceUISounds::refreshEditor()
{
    if (!mSelectedLabel || !mSettingName || !mUUIDEditor || !mPlayCheck || !mPlayCombo)
    {
        return;
    }

    const auto entry = getSelectedEntry();
    const bool has_sel = entry.has_value();

    mUUIDEditor->setEnabled(has_sel);

    if (!has_sel)
    {
        mSelectedLabel->setValue(getPanelString("ui_sound_select_prompt"));
        mSettingName->setValue(LLStringUtil::null);
        mUUIDEditor->setValue(LLSD());
        mPlayCheck->setVisible(false);
        mPlayCombo->setVisible(false);
        return;
    }

    mSelectedLabel->setValue(entry->display_label.empty() ? entry->sound_setting : entry->display_label);
    mSettingName->setValue(entry->sound_setting);
    mUUIDEditor->setValue(gSavedSettings.getString(entry->sound_setting));

    if (LLControlVariable* ctrl = gSavedSettings.getControl(entry->sound_setting))
    {
        mUUIDEditor->setToolTip(ctrl->getDefault().asString());
    }

    if (entry->uses_combo)
    {
        mPlayCheck->setVisible(false);
        mPlayCombo->setVisible(true);
        mPlayCombo->setValue(static_cast<int>(gSavedSettings.getU32(entry->playmode_setting)));
    }
    else
    {
        mPlayCombo->setVisible(false);
        mPlayCheck->setVisible(true);
        bool val = gSavedSettings.getBOOL(entry->playmode_setting);
        mPlayCheck->setValue(entry->inverted_bool ? !val : val);
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void FSPanelPreferenceUISounds::onSelectSound()
{
    if (!mSoundsList)
    {
        return;
    }

    if (LLScrollListItem* selected = mSoundsList->getFirstSelected())
    {
        S32 idx = selected->getValue().asInteger();
        if (idx >= 0 && idx < static_cast<S32>(mEntries.size()))
        {
            const UISoundEntry& entry = mEntries[idx];
            if (!entry.uses_combo)
            {
                bool checked = selected->getColumn(COL_CHECK)->getValue().asBoolean();
                bool val = gSavedSettings.getBOOL(entry.playmode_setting);
                bool was_enabled = entry.inverted_bool ? !val : val;
                if (checked != was_enabled)
                {
                    gSavedSettings.setBOOL(entry.playmode_setting, entry.inverted_bool ? !checked : checked);
                    if (LLScrollListText* text_cell = dynamic_cast<LLScrollListText*>(selected->getColumn(COL_STATUS)))
                    {
                        text_cell->setText(getModeLabel(entry));
                    }
                }
            }
        }
    }

    refreshEditor();
}

void FSPanelPreferenceUISounds::onUpdateFilter()
{
    if (mSoundsList && mFilter)
    {
        mSoundsList->setFilterString(mFilter->getValue().asString());
    }
}

void FSPanelPreferenceUISounds::onCommitUUID()
{
    const auto entry = getSelectedEntry();
    if (!entry || !mUUIDEditor)
    {
        return;
    }
    gSavedSettings.setString(entry->sound_setting, mUUIDEditor->getValue().asString());
}

void FSPanelPreferenceUISounds::onCommitPlayCheck()
{
    const auto entry = getSelectedEntry();
    if (!entry || entry->uses_combo || !mPlayCheck)
    {
        return;
    }

    bool checked = mPlayCheck->getValue().asBoolean();
    gSavedSettings.setBOOL(entry->playmode_setting, entry->inverted_bool ? !checked : checked);

    LLScrollListItem* selected = mSoundsList ? mSoundsList->getFirstSelected() : nullptr;
    if (selected)
    {
        selected->getColumn(COL_CHECK)->setValue(checked);
        if (LLScrollListText* text_cell = dynamic_cast<LLScrollListText*>(selected->getColumn(COL_STATUS)))
        {
            text_cell->setText(getModeLabel(*entry));
        }
    }
}

void FSPanelPreferenceUISounds::onCommitPlayCombo()
{
    const auto entry = getSelectedEntry();
    if (!entry || !entry->uses_combo || !mPlayCombo)
    {
        return;
    }

    U32 mode = static_cast<U32>(mPlayCombo->getValue().asInteger());
    gSavedSettings.setU32(entry->playmode_setting, mode);

    LLScrollListItem* selected = mSoundsList ? mSoundsList->getFirstSelected() : nullptr;
    if (selected)
    {
        if (LLScrollListText* text_cell = dynamic_cast<LLScrollListText*>(selected->getColumn(COL_STATUS)))
        {
            text_cell->setText(getModeLabel(*entry));
        }
    }
}

void FSPanelPreferenceUISounds::onPreviewSound()
{
    const auto entry = getSelectedEntry();
    if (entry)
    {
        make_ui_sound(entry->sound_setting.c_str(), true);
    }
}

void FSPanelPreferenceUISounds::onResetSound()
{
    const auto entry = getSelectedEntry();
    if (!entry)
    {
        return;
    }

    if (LLControlVariable* ctrl = gSavedSettings.getControl(entry->sound_setting))
    {
        ctrl->resetToDefault();
    }
    if (LLControlVariable* ctrl = gSavedSettings.getControl(entry->playmode_setting))
    {
        ctrl->resetToDefault();
    }

    LLScrollListItem* selected = mSoundsList ? mSoundsList->getFirstSelected() : nullptr;
    if (selected)
    {
        if (entry->uses_combo)
        {
            if (LLScrollListText* text_cell = dynamic_cast<LLScrollListText*>(selected->getColumn(COL_STATUS)))
            {
                text_cell->setText(getModeLabel(*entry));
            }
        }
        else
        {
            bool val = gSavedSettings.getBOOL(entry->playmode_setting);
            bool is_enabled = entry->inverted_bool ? !val : val;
            selected->getColumn(COL_CHECK)->setValue(is_enabled);
            if (LLScrollListText* text_cell = dynamic_cast<LLScrollListText*>(selected->getColumn(COL_STATUS)))
            {
                text_cell->setText(getModeLabel(*entry));
            }
        }
    }

    refreshEditor();
}

void FSPanelPreferenceUISounds::onDoubleClick()
{
    onPreviewSound();
}

void FSPanelPreferenceUISounds::onRightClick(LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
    mSoundsList->selectItemAt(x, y, mask);
    refreshEditor();

    const auto entry = getSelectedEntry();
    if (!entry)
    {
        return;
    }

    LLContextMenu* menu = mContextMenuHandle.get();
    if (menu)
    {
        S32 screen_x, screen_y;
        mSoundsList->localPointToScreen(x, y, &screen_x, &screen_y);
        menu->show(screen_x, screen_y, mSoundsList);
    }
}

void FSPanelPreferenceUISounds::onCopyUUID()
{
    const auto entry = getSelectedEntry();
    if (!entry)
    {
        return;
    }

    std::string uuid = gSavedSettings.getString(entry->sound_setting);
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(uuid), 0, static_cast<S32>(uuid.size()));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void FSPanelPreferenceUISounds::applyLocalizedLabels()
{
    if (mPlayCheck)
    {
        std::string lbl = getPanelString("ui_sound_play_this");
        if (!lbl.empty()) mPlayCheck->setLabel(lbl);
    }

    LLContextMenu* menu = mContextMenuHandle.get();
    if (menu)
    {
        std::string lbl = getPanelString("ui_sound_copy_uuid");
        if (!lbl.empty())
        {
            if (LLMenuItemGL* item = menu->findChild<LLMenuItemGL>("copy_uuid"))
            {
                item->setLabel(lbl);
            }
        }
    }

    if (mPlayCombo)
    {
        static const struct { const char* value; const char* key; } rows[] = {
            {"1", "ui_sound_playmode_new_session"},
            {"2", "ui_sound_playmode_every_message"},
            {"3", "ui_sound_playmode_not_focus"},
            {"0", "ui_sound_playmode_mute"},
        };

        for (const auto& r : rows)
        {
            LLScrollListItem* item = mPlayCombo->getItemByValue(LLSD(r.value));
            if (!item)
                continue;

            LLScrollListCell* cell = item->getColumn(0);
            if (LLScrollListText* tc = dynamic_cast<LLScrollListText*>(cell))
            {
                std::string s = getPanelString(r.key);
                if (!s.empty()) tc->setText(s);
            }
        }
        mPlayCombo->updateLabel();
    }
}

std::string FSPanelPreferenceUISounds::getPanelString(std::string_view name) const
{
    if (hasString(name))
    {
        return getString(name);
    }
    LLView* ancestor = getParent();
    while (ancestor)
    {
        LLPanel* panel = dynamic_cast<LLPanel*>(ancestor);
        if (panel && panel->hasString(name))
        {
            return panel->getString(name);
        }
        ancestor = ancestor->getParent();
    }
    return LLStringUtil::null;
}

std::string FSPanelPreferenceUISounds::resolveEntryLabel(const UISoundEntry& entry) const
{
    std::string label = getPanelString(entry.label_control);
    if (label.empty())
    {
        label = entry.sound_setting;
    }
    LLStringUtil::trim(label);
    if (!label.empty() && label.back() == ':')
    {
        label.pop_back();
    }
    return label;
}

std::string FSPanelPreferenceUISounds::getModeLabel(const UISoundEntry& entry) const
{
    if (entry.uses_combo)
    {
        const U32 mode = gSavedSettings.getU32(entry.playmode_setting);
        switch (mode)
        {
            case 1:
                return getPanelString("ui_sound_playmode_new_session");
            case 2:
                return getPanelString("ui_sound_playmode_every_message");
            case 3:
                return getPanelString("ui_sound_playmode_not_focus");
            case 0:
            default:
                return getPanelString("ui_sound_playmode_mute");
        }
    }

    const bool enabled = gSavedSettings.getBOOL(entry.playmode_setting);
    if (entry.inverted_bool)
    {
        return enabled ? getPanelString("ui_sound_playmode_mute") : getPanelString("ui_sound_play_this");
    }

    return enabled ? getPanelString("ui_sound_play_this") : getPanelString("ui_sound_playmode_mute");
}

S32 FSPanelPreferenceUISounds::getSelectedIndex() const
{
    if (!mSoundsList)
    {
        return -1;
    }
    LLScrollListItem* selected = mSoundsList->getFirstSelected();
    if (!selected)
    {
        return -1;
    }
    return selected->getValue().asInteger();
}

std::optional<FSPanelPreferenceUISounds::UISoundEntry> FSPanelPreferenceUISounds::getSelectedEntry() const
{
    S32 idx = getSelectedIndex();
    if (idx < 0 || idx >= static_cast<S32>(mEntries.size()))
    {
        return {};
    }
    return mEntries[idx];
}
