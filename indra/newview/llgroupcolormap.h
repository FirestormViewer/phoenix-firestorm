/**
 * @file   llgroupcolormap.h
 * @brief  Per-group nameplate tint registry.
 *
 * Stores a client-side color for each group UUID.  When the group color is
 * set, every avatar whose *active* group tag matches that UUID gets their
 * nameplate rendered in that color instead of the default.
 *
 * Data is persisted to  <account_dir>/settings_group_colors.xml  so colors
 * survive relogs.  The file is keyed by group UUID string → LLColor4 LLSD.
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */

#ifndef LL_LLGROUPCOLORMAP_H
#define LL_LLGROUPCOLORMAP_H

#include "llsingleton.h"
#include "lluuid.h"
#include "v4color.h"
#include <unordered_map>

class LLGroupColorMap : public LLSingleton<LLGroupColorMap>
{
    LLSINGLETON(LLGroupColorMap);
    ~LLGroupColorMap() = default;
    LOG_CLASS(LLGroupColorMap);

public:
    // ---- Color CRUD ---------------------------------------------------------

    /** Set (or clear) the nameplate tint for a group.
     *  Pass LLColor4::transparent (alpha == 0) to remove the color entry. */
    void setGroupColor(const LLUUID& group_id, const LLColor4& color);

    /** Return the tint for @p group_id, or LLColor4::transparent if none. */
    LLColor4 getGroupColor(const LLUUID& group_id) const;

    /** True if a non-transparent color is stored for this group. */
    bool hasGroupColor(const LLUUID& group_id) const;

    /** Remove the color entry for this group. */
    void clearGroupColor(const LLUUID& group_id);

    // ---- Persistence --------------------------------------------------------
    void loadFromDisk();
    void saveToDisk() const;

    // ---- Cache invalidation -------------------------------------------------
    /** Called after a color is changed so nametags rebuild on next idle. */
    static void invalidateAllNameTags();

private:
    static std::string getFilePath();

    // group_uuid → RGBA color  (std::hash<LLUUID> is specialised in lluuid.h)
    std::unordered_map<LLUUID, LLColor4> mColors;
};

#endif // LL_LLGROUPCOLORMAP_H
