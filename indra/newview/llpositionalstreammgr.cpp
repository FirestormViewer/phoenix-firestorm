/**
 * @file llpositionalstreammgr.cpp
 * @brief Manager for prim-bound 3D positional audio streams (AYAstorm).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Phoenix Firestorm Project, Inc.
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

#include "llpositionalstreammgr.h"

#include "llpositionalstream.h"

#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"

#include "llstring.h"

#include <algorithm>
#include <cctype>

namespace
{
    LLVector3 toFloatVec(const LLVector3d& v)
    {
        LLVector3 out;
        out.setVec(v);
        return out;
    }

    bool tryParseFloat(const std::string& s, F32& out)
    {
        if (s.empty())
        {
            return false;
        }
        try
        {
            size_t consumed = 0;
            F32 val = std::stof(s, &consumed);
            if (consumed == 0)
            {
                return false;
            }
            out = val;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    std::string toLowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    // Case-insensitive substring search (ASCII).
    size_t findCaseInsensitive(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty() || haystack.size() < needle.size())
        {
            return std::string::npos;
        }
        const size_t end = haystack.size() - needle.size();
        for (size_t i = 0; i <= end; ++i)
        {
            bool match = true;
            for (size_t j = 0; j < needle.size(); ++j)
            {
                unsigned char a = static_cast<unsigned char>(haystack[i + j]);
                unsigned char b = static_cast<unsigned char>(needle[j]);
                if (std::tolower(a) != std::tolower(b))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return i;
            }
        }
        return std::string::npos;
    }
}

LLPositionalStreamMgr& LLPositionalStreamMgr::instance()
{
    static LLPositionalStreamMgr sInstance;
    return sInstance;
}

LLPositionalStreamMgr::LLPositionalStreamMgr() = default;
LLPositionalStreamMgr::~LLPositionalStreamMgr() = default;

// static
std::optional<LLPositionalStreamMgr::TagData>
LLPositionalStreamMgr::parseTag(const std::string& description)
{
    static const std::string kPrefix = "[ayastream:";

    size_t begin = findCaseInsensitive(description, kPrefix);
    if (begin == std::string::npos)
    {
        return std::nullopt;
    }
    size_t content_start = begin + kPrefix.size();
    size_t end = description.find(']', content_start);
    if (end == std::string::npos)
    {
        return std::nullopt;
    }

    // Split content on ',' and require each element to be a {key:value} unit.
    TagData data;
    bool got_url = false;

    size_t cursor = content_start;
    while (cursor < end)
    {
        size_t next = description.find(',', cursor);
        if (next == std::string::npos || next > end)
        {
            next = end;
        }

        std::string item = description.substr(cursor, next - cursor);
        cursor = next + 1;

        LLStringUtil::trim(item);
        if (item.size() < 2 || item.front() != '{' || item.back() != '}')
        {
            continue;
        }

        std::string inner = item.substr(1, item.size() - 2);
        size_t colon = inner.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        std::string key = inner.substr(0, colon);
        std::string val = inner.substr(colon + 1);
        LLStringUtil::trim(key);
        LLStringUtil::trim(val);
        key = toLowerAscii(key);

        if (key == "url")
        {
            data.url = val;
            got_url = !val.empty();
        }
        else if (key == "min")
        {
            F32 f;
            if (tryParseFloat(val, f))
            {
                data.min = f;
            }
        }
        else if (key == "max")
        {
            F32 f;
            if (tryParseFloat(val, f))
            {
                data.max = f;
            }
        }
    }

    if (!got_url)
    {
        return std::nullopt;
    }
    return data;
}

void LLPositionalStreamMgr::resolveRolloff(const TagData& tag, F32& out_min, F32& out_max) const
{
    out_min = tag.min.value_or(gSavedSettings.getF32("AYAStreamRolloffMin"));
    out_max = tag.max.value_or(gSavedSettings.getF32("AYAStreamRolloffMax"));
}

void LLPositionalStreamMgr::onObjectPropertiesReceived(const LLUUID& id,
                                                       const std::string& description)
{
    if (id.isNull())
    {
        return;
    }

    // Always re-evaluate: if the description is unchanged the diff inside
    // evaluateBinding is cheap, and re-arrivals after object recreate
    // (e.g., teleport out and back) need to re-bind even when the text matches.
    mDescriptionCache[id] = description;
    evaluateBinding(id);
}

void LLPositionalStreamMgr::evaluateBinding(const LLUUID& id)
{
    auto desc_it = mDescriptionCache.find(id);
    if (desc_it == mDescriptionCache.end())
    {
        return;
    }

    auto tag = parseTag(desc_it->second);
    auto bind_it = mBindings.find(id);

    if (!tag.has_value())
    {
        if (bind_it != mBindings.end())
        {
            LL_INFOS("AYAStream") << "Removing positional binding for " << id
                                  << " (tag gone)" << LL_ENDL;
            mBindings.erase(bind_it);
        }
        return;
    }

    LLViewerObject* obj = gObjectList.findObject(id);
    if (!obj || obj->isDead())
    {
        // Object isn't resolvable in the world right now; defer until
        // update() picks it up (or it gets re-evaluated on a later props msg).
        return;
    }

    F32 want_min, want_max;
    resolveRolloff(*tag, want_min, want_max);
    LLVector3 pos = toFloatVec(obj->getPositionGlobal());

    if (bind_it != mBindings.end())
    {
        Binding& b = bind_it->second;
        if (b.url == tag->url)
        {
            b.tag_min = tag->min;
            b.tag_max = tag->max;
            if (b.applied_min != want_min || b.applied_max != want_max)
            {
                b.stream->setRolloffDistances(want_min, want_max);
                b.applied_min = want_min;
                b.applied_max = want_max;
            }
            // Position is refreshed each frame by update().
            return;
        }
        LL_INFOS("AYAStream") << "Rebinding " << id << ": URL changed" << LL_ENDL;
        mBindings.erase(bind_it);
    }

    const S32 cap = gSavedSettings.getS32("AYAStreamMaxConcurrent");
    if (cap > 0 && static_cast<S32>(mBindings.size()) >= cap)
    {
        LL_WARNS("AYAStream") << "Max concurrent streams (" << cap
                              << ") reached; not binding " << id << LL_ENDL;
        return;
    }

    auto stream = std::make_unique<LLPositionalStream>();
    stream->setRolloffDistances(want_min, want_max);
    if (!stream->start(tag->url, pos))
    {
        return;
    }

    Binding b;
    b.url = tag->url;
    b.tag_min = tag->min;
    b.tag_max = tag->max;
    b.applied_min = want_min;
    b.applied_max = want_max;
    b.stream = std::move(stream);
    mBindings.emplace(id, std::move(b));

    LL_INFOS("AYAStream") << "Bound positional stream to " << id
                          << " url=" << tag->url << LL_ENDL;
}

void LLPositionalStreamMgr::update()
{
    if (mDebugStream)
    {
        mDebugStream->update();
    }

    for (auto it = mBindings.begin(); it != mBindings.end(); )
    {
        const LLUUID& id = it->first;
        Binding& b = it->second;

        LLViewerObject* obj = gObjectList.findObject(id);
        if (!obj || obj->isDead())
        {
            LL_INFOS("AYAStream") << "Object " << id
                                  << " gone; releasing positional stream" << LL_ENDL;
            it = mBindings.erase(it);
            continue;
        }

        b.stream->setPosition(toFloatVec(obj->getPositionGlobal()));
        b.stream->update();
        ++it;
    }
}

void LLPositionalStreamMgr::applyDefaultRolloff(F32 default_min, F32 default_max)
{
    if (mDebugStream)
    {
        mDebugStream->setRolloffDistances(default_min, default_max);
    }

    for (auto& [id, b] : mBindings)
    {
        F32 want_min = b.tag_min.value_or(default_min);
        F32 want_max = b.tag_max.value_or(default_max);
        if (b.applied_min != want_min || b.applied_max != want_max)
        {
            b.stream->setRolloffDistances(want_min, want_max);
            b.applied_min = want_min;
            b.applied_max = want_max;
        }
    }
}

void LLPositionalStreamMgr::startDebug(const std::string& url, const LLVector3& world_pos)
{
    if (!mDebugStream)
    {
        mDebugStream = std::make_unique<LLPositionalStream>();
    }
    mDebugStream->setRolloffDistances(
        gSavedSettings.getF32("AYAStreamRolloffMin"),
        gSavedSettings.getF32("AYAStreamRolloffMax"));
    mDebugStream->start(url, world_pos);
}

void LLPositionalStreamMgr::stopDebug()
{
    if (mDebugStream)
    {
        mDebugStream->stop();
        mDebugStream.reset();
    }
}
