/**
 * @file fspanelpreferenceuisounds.h
 * @brief FSPanelPreferenceUISounds class — UI Sounds preferences tab
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

#ifndef FS_PANEL_PREFERENCE_UI_SOUNDS_H
#define FS_PANEL_PREFERENCE_UI_SOUNDS_H

#include "llfloaterpreference.h"
#include <optional>
#include <vector>

class LLCheckBoxCtrl;
class LLContextMenu;
class LLScrollListCtrl;
class LLFilterEditor;
class LLLineEditor;
class LLComboBox;
class LLTextBox;

class FSPanelPreferenceUISounds : public LLPanelPreference
{
public:
    FSPanelPreferenceUISounds();
    bool postBuild() override;

    void refreshList();

private:
    struct UISoundEntry
    {
        std::string sound_setting;
        std::string playmode_setting;
        std::string label_control;
        std::string display_label;
        bool uses_combo = false;
        bool inverted_bool = false;
    };

    void buildList();
    static UISoundEntry makeEntry(std::string_view sound, const std::optional<std::string_view>& label_override = std::nullopt, bool combo = false, bool inverted = false);
    void refreshEditor();
    void onSelectSound();
    void onUpdateFilter();
    void onCommitUUID();
    void onCommitPlayCheck();
    void onCommitPlayCombo();
    void onPreviewSound();
    void onResetSound();
    void onDoubleClick();
    void onRightClick(LLUICtrl* ctrl, S32 x, S32 y, MASK mask);
    void onCopyUUID();
    void applyLocalizedLabels();

    std::string resolveEntryLabel(const UISoundEntry& entry) const;
    std::string getModeLabel(const UISoundEntry& entry) const;
    std::string getPanelString(std::string_view name) const;

    S32 getSelectedIndex() const;
    std::optional<UISoundEntry> getSelectedEntry() const;

    LLScrollListCtrl*  mSoundsList = nullptr;
    LLFilterEditor*    mFilter = nullptr;
    LLLineEditor*      mUUIDEditor = nullptr;
    LLCheckBoxCtrl*    mPlayCheck = nullptr;
    LLComboBox*        mPlayCombo = nullptr;
    LLTextBox*         mSelectedLabel = nullptr;
    LLTextBox*         mSettingName = nullptr;
    std::vector<UISoundEntry> mEntries;
    LLHandle<LLContextMenu>   mContextMenuHandle;

    LOG_CLASS(FSPanelPreferenceUISounds);
};

#endif // FS_PANEL_PREFERENCE_UI_SOUNDS_H
