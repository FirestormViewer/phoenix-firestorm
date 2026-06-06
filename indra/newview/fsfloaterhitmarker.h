/**
 * @file fsfloaterhitmarker.h
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

#ifndef FS_FLOATERHITMARKER_H
#define FS_FLOATERHITMARKER_H

#include "llfloater.h"

class LLLineEditor;
class LLScrollListCtrl;

// Hitmarker options. All controls are settings-bound; this class only
// exists to wire the "Customize symbols" button.
class FSFloaterHitMarker : public LLFloater
{
public:
    FSFloaterHitMarker(const LLSD& key);
    virtual ~FSFloaterHitMarker() = default;

    bool postBuild() override;
};

// Damage type symbols editor: per-type symbol assignments persisted into
// the FSHitMarkerEmojiMap setting.
class FSFloaterHitMarkerEmojis : public LLFloater
{
public:
    FSFloaterHitMarkerEmojis(const LLSD& key);
    virtual ~FSFloaterHitMarkerEmojis() = default;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

private:
    void refreshList();
    void onSelectRow();
    void onSetSymbol();
    void onResetSelected();
    void onResetAll();

    LLScrollListCtrl* mTypeList{ nullptr };
    LLLineEditor*     mSymbolEdit{ nullptr };
};

#endif // FS_FLOATERHITMARKER_H
