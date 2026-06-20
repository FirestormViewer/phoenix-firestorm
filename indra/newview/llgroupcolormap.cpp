/**
 * @file   llgroupcolormap.cpp
 * @brief  Per-group nameplate tint registry.
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 */

#include "llviewerprecompiledheaders.h"

#include "llgroupcolormap.h"

#include "lldir.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"   // ll_sd_from_color4 / ll_color4_from_sd
#include "llvoavatar.h"      // LLVOAvatar::invalidateNameTags()

static constexpr char GROUP_COLOR_FILE[] = "settings_group_colors.xml";

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LLGroupColorMap::LLGroupColorMap()
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void LLGroupColorMap::setGroupColor(const LLUUID& group_id, const LLColor4& color)
{
    if (group_id.isNull())
        return;

    // Treat fully-transparent as "no color" (remove entry)
    if (color.mV[VW] < 0.01f)
    {
        clearGroupColor(group_id);
        return;
    }

    mColors[group_id] = color;
    saveToDisk();
    invalidateAllNameTags();
}

LLColor4 LLGroupColorMap::getGroupColor(const LLUUID& group_id) const
{
    if (group_id.isNull())
        return LLColor4::transparent;

    auto it = mColors.find(group_id);
    if (it != mColors.end())
        return it->second;

    return LLColor4::transparent;
}

bool LLGroupColorMap::hasGroupColor(const LLUUID& group_id) const
{
    if (group_id.isNull())
        return false;
    return mColors.find(group_id) != mColors.end();
}

void LLGroupColorMap::clearGroupColor(const LLUUID& group_id)
{
    if (mColors.erase(group_id) > 0)
    {
        saveToDisk();
        invalidateAllNameTags();
    }
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

// static
std::string LLGroupColorMap::getFilePath()
{
    std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");
    if (!path.empty())
        return gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, GROUP_COLOR_FILE);
    // Fall back to app_settings (before first login, path not yet set)
    return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, GROUP_COLOR_FILE);
}

void LLGroupColorMap::loadFromDisk()
{
    mColors.clear();

    const std::string filepath = getFilePath();
    if (!gDirUtilp->fileExists(filepath))
        return;

    llifstream stream(filepath.c_str());
    if (!stream.is_open())
    {
        LL_WARNS("GroupColor") << "Cannot open " << filepath << LL_ENDL;
        return;
    }

    LLSD data;
    if (LLSDSerialize::fromXMLDocument(data, stream) < 0)
    {
        LL_WARNS("GroupColor") << "Failed to parse " << filepath << LL_ENDL;
        return;
    }

    // Format: map of { group_uuid_string : [r, g, b, a] }
    for (LLSD::map_const_iterator it = data.beginMap(); it != data.endMap(); ++it)
    {
        LLUUID group_id(it->first);
        if (group_id.isNull()) continue;
        LLColor4 color = ll_color4_from_sd(it->second);
        if (color.mV[VW] >= 0.01f)
            mColors[group_id] = color;
    }

    LL_INFOS("GroupColor") << "Loaded " << mColors.size()
                           << " group color(s) from " << filepath << LL_ENDL;
}

void LLGroupColorMap::saveToDisk() const
{
    const std::string filepath = getFilePath();

    LLSD data = LLSD::emptyMap();
    for (const auto& [group_id, color] : mColors)
        data[group_id.asString()] = ll_sd_from_color4(color);

    llofstream stream(filepath.c_str());
    if (!stream.is_open())
    {
        LL_WARNS("GroupColor") << "Cannot write " << filepath << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(data, stream);
}

// ---------------------------------------------------------------------------
// Invalidation
// ---------------------------------------------------------------------------

// static
void LLGroupColorMap::invalidateAllNameTags()
{
    LLVOAvatar::invalidateNameTags();
}
