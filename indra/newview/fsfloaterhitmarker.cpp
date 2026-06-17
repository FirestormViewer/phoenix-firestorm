/**
 * @file fsfloaterhitmarker.cpp
 * @brief Hitmarker options floater and the damage type symbols editor.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterhitmarker.h"

#include "fscombathitmarker.h"
#include "llbutton.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"

// ---------------------------------------------------------------------------
// FSFloaterHitMarker
// ---------------------------------------------------------------------------

FSFloaterHitMarker::FSFloaterHitMarker(const LLSD& key)
:   LLFloater(key)
{
}

bool FSFloaterHitMarker::postBuild()
{
    getChild<LLButton>("customize_symbols_btn")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("fs_hit_marker_emojis"); });
    getChild<LLButton>("test_hit_sound_btn")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { FSCombatHitMarker::playHitSoundPreview(); });
    getChild<LLButton>("test_kill_sound_btn")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { FSCombatHitMarker::playKillSoundPreview(); });
    return true;
}

// ---------------------------------------------------------------------------
// FSFloaterHitMarkerEmojis
// ---------------------------------------------------------------------------

FSFloaterHitMarkerEmojis::FSFloaterHitMarkerEmojis(const LLSD& key)
:   LLFloater(key)
{
}

bool FSFloaterHitMarkerEmojis::postBuild()
{
    mTypeList = getChild<LLScrollListCtrl>("type_list");
    mTypeList->setCommitCallback(boost::bind(&FSFloaterHitMarkerEmojis::onSelectRow, this));

    mSymbolEdit = getChild<LLLineEditor>("symbol_edit");

    getChild<LLButton>("set_btn")->setCommitCallback(
        boost::bind(&FSFloaterHitMarkerEmojis::onSetSymbol, this));
    getChild<LLButton>("reset_btn")->setCommitCallback(
        boost::bind(&FSFloaterHitMarkerEmojis::onResetSelected, this));
    getChild<LLButton>("reset_all_btn")->setCommitCallback(
        boost::bind(&FSFloaterHitMarkerEmojis::onResetAll, this));

    return true;
}

void FSFloaterHitMarkerEmojis::onOpen(const LLSD& key)
{
    refreshList();
}

void FSFloaterHitMarkerEmojis::refreshList()
{
    const S32 selected = mTypeList->getSelectedValue().asInteger();
    const bool had_selection = (mTypeList->getFirstSelected() != nullptr);
    const S32 scroll_pos = mTypeList->getScrollPos();
    mTypeList->clearRows();

    for (S32 i = 0; i < FSCombatHitMarker::getDamageTypeCount(); ++i)
    {
        const FSCombatHitMarker::DamageTypeInfo& info = FSCombatHitMarker::getDamageTypeInfo(i);
        const std::string current = FSCombatHitMarker::getEmojiForType(info.mType);
        const bool customized = (current != info.mDefaultEmoji);

        LLSD row;
        row["value"] = info.mType;
        row["columns"][0]["column"] = "type";
        row["columns"][0]["value"] = llformat("%s (%d)", info.mName, info.mType);
        row["columns"][1]["column"] = "symbol";
        row["columns"][1]["value"] = current;
        row["columns"][2]["column"] = "customized";
        row["columns"][2]["value"] = customized ? "*" : "";
        mTypeList->addElement(row);
    }

    mTypeList->setScrollPos(scroll_pos);
    if (had_selection)
    {
        mTypeList->selectByValue(LLSD(selected));
    }
}

void FSFloaterHitMarkerEmojis::onSelectRow()
{
    LLScrollListItem* item = mTypeList->getFirstSelected();
    if (item)
    {
        mSymbolEdit->setText(FSCombatHitMarker::getEmojiForType(item->getValue().asInteger()));
    }
}

void FSFloaterHitMarkerEmojis::onSetSymbol()
{
    LLScrollListItem* item = mTypeList->getFirstSelected();
    if (item)
    {
        FSCombatHitMarker::setEmojiOverride(item->getValue().asInteger(), mSymbolEdit->getText());
        refreshList();
    }
}

void FSFloaterHitMarkerEmojis::onResetSelected()
{
    LLScrollListItem* item = mTypeList->getFirstSelected();
    if (item)
    {
        FSCombatHitMarker::setEmojiOverride(item->getValue().asInteger(), "");
        refreshList();
        onSelectRow();
    }
}

void FSFloaterHitMarkerEmojis::onResetAll()
{
    FSCombatHitMarker::clearEmojiOverrides();
    refreshList();
    onSelectRow();
}
