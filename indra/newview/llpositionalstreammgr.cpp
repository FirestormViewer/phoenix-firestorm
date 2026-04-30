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
#include "llpositionalstreamstereo.h"

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

    bool tryParseInt(const std::string& s, S32& out)
    {
        if (s.empty()) return false;
        try
        {
            size_t consumed = 0;
            S32 val = std::stoi(s, &consumed);
            if (consumed == 0) return false;
            out = val;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    // Walks a "{key:value},{key:value},..." body between [content_start, end)
    // and invokes onPair(lowered_key, trimmed_val) for every well-formed unit.
    template <typename F>
    void forEachKeyValue(const std::string& description,
                         size_t content_start, size_t end, F&& onPair)
    {
        size_t cursor = content_start;
        while (cursor < end)
        {
            size_t next = description.find(',', cursor);
            if (next == std::string::npos || next > end) next = end;

            std::string item = description.substr(cursor, next - cursor);
            cursor = next + 1;

            LLStringUtil::trim(item);
            if (item.size() < 2 || item.front() != '{' || item.back() != '}') continue;

            std::string inner = item.substr(1, item.size() - 2);
            size_t colon = inner.find(':');
            if (colon == std::string::npos) continue;

            std::string key = inner.substr(0, colon);
            std::string val = inner.substr(colon + 1);
            LLStringUtil::trim(key);
            LLStringUtil::trim(val);
            key = toLowerAscii(key);

            onPair(key, val);
        }
    }

    // Locate the body of "[<prefix>...]" and return [content_start, end).
    // Returns false if prefix not found or no closing bracket.
    bool findTagBody(const std::string& description, const std::string& prefix,
                     size_t& out_content_start, size_t& out_end)
    {
        size_t begin = findCaseInsensitive(description, prefix);
        if (begin == std::string::npos) return false;
        size_t content_start = begin + prefix.size();
        size_t end = description.find(']', content_start);
        if (end == std::string::npos) return false;
        out_content_start = content_start;
        out_end = end;
        return true;
    }

    template <typename TagT>
    void resolveRolloffFromTag(const TagT& tag, F32& out_min, F32& out_max)
    {
        out_min = tag.min.value_or(gSavedSettings.getF32("AYAStreamRolloffMin"));
        out_max = tag.max.value_or(gSavedSettings.getF32("AYAStreamRolloffMax"));
    }

    // Resolve link number to a child object. Link 1 is the root itself, link 2
    // is the first child, etc. Returns nullptr if no such link in this set.
    LLViewerObject* resolveLink(LLViewerObject* root, S32 link_num)
    {
        if (!root || link_num < 1) return nullptr;
        if (link_num == 1) return root;

        const auto& children = root->getChildren();
        S32 want_idx = link_num - 2; // link 2 == children[0]
        if (want_idx < 0 || want_idx >= static_cast<S32>(children.size())) return nullptr;
        auto it = children.begin();
        std::advance(it, want_idx);
        return it->get();
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

    size_t content_start = 0, end = 0;
    if (!findTagBody(description, kPrefix, content_start, end)) return std::nullopt;

    TagData data;
    bool got_url = false;
    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")      { data.url = val; got_url = !val.empty(); }
            else if (key == "min") { F32 f; if (tryParseFloat(val, f)) data.min = f; }
            else if (key == "max") { F32 f; if (tryParseFloat(val, f)) data.max = f; }
        });

    if (!got_url) return std::nullopt;
    return data;
}

// static
std::optional<LLPositionalStreamMgr::StereoTagData>
LLPositionalStreamMgr::parseStereoTag(const std::string& description)
{
    static const std::string kPrefix = "[ayastream-stereo:";

    size_t content_start = 0, end = 0;
    if (!findTagBody(description, kPrefix, content_start, end)) return std::nullopt;

    StereoTagData data;
    bool got_url = false;
    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")      { data.url = val; got_url = !val.empty(); }
            else if (key == "min") { F32 f; if (tryParseFloat(val, f)) data.min = f; }
            else if (key == "max") { F32 f; if (tryParseFloat(val, f)) data.max = f; }
            else if (key == "l")   { S32 i; if (tryParseInt(val, i)) data.l_link = i; }
            else if (key == "r")   { S32 i; if (tryParseInt(val, i)) data.r_link = i; }
        });

    if (!got_url) return std::nullopt;
    if (data.l_link < 1 || data.r_link < 1) return std::nullopt;
    if (data.l_link == data.r_link) return std::nullopt;
    return data;
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

    // Stereo tag wins if both happen to be present (the prefixes don't
    // overlap textually, but a description could in theory contain both).
    auto stereo_tag = parseStereoTag(desc_it->second);
    auto mono_tag = stereo_tag ? std::nullopt : parseTag(desc_it->second);

    auto bind_it = mBindings.find(id);
    auto sbind_it = mStereoBindings.find(id);

    // If a different mode now applies, drop the stale binding first so the
    // new path can recreate it cleanly.
    if (stereo_tag && bind_it != mBindings.end())
    {
        LL_INFOS("AYAStream") << "Replacing mono binding with stereo for " << id << LL_ENDL;
        mBindings.erase(bind_it);
        bind_it = mBindings.end();
    }
    if (mono_tag && sbind_it != mStereoBindings.end())
    {
        LL_INFOS("AYAStream") << "Replacing stereo binding with mono for " << id << LL_ENDL;
        mStereoBindings.erase(sbind_it);
        sbind_it = mStereoBindings.end();
    }

    if (stereo_tag)
    {
        evaluateStereoBinding(id, *stereo_tag);
        return;
    }
    if (mono_tag)
    {
        evaluateMonoBinding(id, *mono_tag);
        return;
    }

    // No tag of either kind: drop any bindings this prim still has.
    if (bind_it != mBindings.end())
    {
        LL_INFOS("AYAStream") << "Removing positional binding for " << id
                              << " (tag gone)" << LL_ENDL;
        mBindings.erase(bind_it);
    }
    if (sbind_it != mStereoBindings.end())
    {
        LL_INFOS("AYAStream") << "Removing stereo binding for " << id
                              << " (tag gone)" << LL_ENDL;
        mStereoBindings.erase(sbind_it);
    }
}

void LLPositionalStreamMgr::evaluateMonoBinding(const LLUUID& id, const TagData& tag)
{
    LLViewerObject* obj = gObjectList.findObject(id);
    if (!obj || obj->isDead())
    {
        return;
    }

    F32 want_min, want_max;
    resolveRolloffFromTag(tag, want_min, want_max);
    LLVector3 pos = toFloatVec(obj->getPositionGlobal());

    auto bind_it = mBindings.find(id);
    if (bind_it != mBindings.end())
    {
        Binding& b = bind_it->second;
        if (b.url == tag.url)
        {
            b.tag_min = tag.min;
            b.tag_max = tag.max;
            if (b.applied_min != want_min || b.applied_max != want_max)
            {
                b.stream->setRolloffDistances(want_min, want_max);
                b.applied_min = want_min;
                b.applied_max = want_max;
            }
            return;
        }
        LL_INFOS("AYAStream") << "Rebinding " << id << ": URL changed" << LL_ENDL;
        mBindings.erase(bind_it);
    }

    const S32 cap = gSavedSettings.getS32("AYAStreamMaxConcurrent");
    if (cap > 0 && static_cast<S32>(mBindings.size() + mStereoBindings.size()) >= cap)
    {
        LL_WARNS("AYAStream") << "Max concurrent streams (" << cap
                              << ") reached; not binding " << id << LL_ENDL;
        return;
    }

    auto stream = std::make_unique<LLPositionalStream>();
    stream->setRolloffDistances(want_min, want_max);
    if (!stream->start(tag.url, pos))
    {
        return;
    }

    Binding b;
    b.url = tag.url;
    b.tag_min = tag.min;
    b.tag_max = tag.max;
    b.applied_min = want_min;
    b.applied_max = want_max;
    b.stream = std::move(stream);
    mBindings.emplace(id, std::move(b));

    LL_INFOS("AYAStream") << "Bound positional stream to " << id
                          << " url=" << tag.url << LL_ENDL;
}

void LLPositionalStreamMgr::evaluateStereoBinding(const LLUUID& id, const StereoTagData& tag)
{
    LLViewerObject* root = gObjectList.findObject(id);
    if (!root || root->isDead())
    {
        return;
    }

    LLViewerObject* l_obj = resolveLink(root, tag.l_link);
    LLViewerObject* r_obj = resolveLink(root, tag.r_link);
    if (!l_obj || !r_obj || l_obj->isDead() || r_obj->isDead())
    {
        // Linkset not fully resolvable yet; defer (will retry on next props
        // msg or when update() re-runs after children arrive).
        return;
    }

    F32 want_min, want_max;
    resolveRolloffFromTag(tag, want_min, want_max);
    LLVector3 l_pos = toFloatVec(l_obj->getPositionGlobal());
    LLVector3 r_pos = toFloatVec(r_obj->getPositionGlobal());

    auto sbind_it = mStereoBindings.find(id);
    if (sbind_it != mStereoBindings.end())
    {
        StereoBinding& b = sbind_it->second;
        const bool topology_same = (b.url == tag.url
                                    && b.l_link == tag.l_link
                                    && b.r_link == tag.r_link
                                    && b.l_prim == l_obj->getID()
                                    && b.r_prim == r_obj->getID());
        if (topology_same)
        {
            b.tag_min = tag.min;
            b.tag_max = tag.max;
            if (b.applied_min != want_min || b.applied_max != want_max)
            {
                b.stream->setRolloffDistances(want_min, want_max);
                b.applied_min = want_min;
                b.applied_max = want_max;
            }
            return;
        }
        LL_INFOS("AYAStream") << "Rebinding stereo " << id
                              << ": URL or linkset topology changed" << LL_ENDL;
        mStereoBindings.erase(sbind_it);
    }

    const S32 cap = gSavedSettings.getS32("AYAStreamMaxConcurrent");
    if (cap > 0 && static_cast<S32>(mBindings.size() + mStereoBindings.size()) >= cap)
    {
        LL_WARNS("AYAStream") << "Max concurrent streams (" << cap
                              << ") reached; not binding stereo " << id << LL_ENDL;
        return;
    }

    auto stream = std::make_unique<LLPositionalStreamStereo>();
    stream->setRolloffDistances(want_min, want_max);
    if (!stream->start(tag.url, l_pos, r_pos))
    {
        return;
    }

    StereoBinding b;
    b.url = tag.url;
    b.tag_min = tag.min;
    b.tag_max = tag.max;
    b.applied_min = want_min;
    b.applied_max = want_max;
    b.l_link = tag.l_link;
    b.r_link = tag.r_link;
    b.l_prim = l_obj->getID();
    b.r_prim = r_obj->getID();
    b.stream = std::move(stream);
    mStereoBindings.emplace(id, std::move(b));

    LL_INFOS("AYAStream") << "Bound stereo stream to " << id
                          << " url=" << tag.url
                          << " L=link" << tag.l_link << "(" << l_obj->getID() << ")"
                          << " R=link" << tag.r_link << "(" << r_obj->getID() << ")"
                          << LL_ENDL;
}

void LLPositionalStreamMgr::update()
{
    if (mDebugStream)
    {
        mDebugStream->update();
    }

    if (mDebugStereoStream)
    {
        mDebugStereoStream->update();
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

    for (auto it = mStereoBindings.begin(); it != mStereoBindings.end(); )
    {
        const LLUUID& id = it->first;
        StereoBinding& b = it->second;

        LLViewerObject* l_obj = gObjectList.findObject(b.l_prim);
        LLViewerObject* r_obj = gObjectList.findObject(b.r_prim);
        if (!l_obj || !r_obj || l_obj->isDead() || r_obj->isDead())
        {
            LL_INFOS("AYAStream") << "Stereo pair (" << id
                                  << ") gone; releasing stereo stream" << LL_ENDL;
            it = mStereoBindings.erase(it);
            continue;
        }

        b.stream->setPositions(toFloatVec(l_obj->getPositionGlobal()),
                               toFloatVec(r_obj->getPositionGlobal()));
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

    if (mDebugStereoStream)
    {
        mDebugStereoStream->setRolloffDistances(default_min, default_max);
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

    for (auto& [id, b] : mStereoBindings)
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

void LLPositionalStreamMgr::startDebugStereo(const std::string& url,
                                             const LLVector3& l_pos,
                                             const LLVector3& r_pos)
{
    if (!mDebugStereoStream)
    {
        mDebugStereoStream = std::make_unique<LLPositionalStreamStereo>();
    }
    mDebugStereoStream->setRolloffDistances(
        gSavedSettings.getF32("AYAStreamRolloffMin"),
        gSavedSettings.getF32("AYAStreamRolloffMax"));
    mDebugStereoStream->start(url, l_pos, r_pos);
}

void LLPositionalStreamMgr::stopDebugStereo()
{
    if (mDebugStereoStream)
    {
        mDebugStereoStream->stop();
        mDebugStereoStream.reset();
    }
}
