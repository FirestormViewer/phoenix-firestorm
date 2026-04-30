/**
 * @file llpositionalstreammgr.h
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

#ifndef LL_POSITIONAL_STREAM_MGR_H
#define LL_POSITIONAL_STREAM_MGR_H

#include "lluuid.h"
#include "stdtypes.h"
#include "v3math.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

class LLPositionalStream;

class LLPositionalStreamMgr
{
public:
    static LLPositionalStreamMgr& instance();

    // Per-frame: poll FMOD async opens, refresh positions of bound prims,
    // drop bindings whose object disappeared.
    void update();

    // Called from the message handlers that decode ObjectProperties /
    // ObjectPropertiesFamily. Caches the description, and (re)evaluates
    // whether this prim should be bound to a stream.
    void onObjectPropertiesReceived(const LLUUID& id, const std::string& description);

    // Apply a global default rolloff to all currently-active streams.
    // Per-prim tags that explicitly set min/max are NOT overridden.
    void applyDefaultRolloff(F32 default_min, F32 default_max);

    // Debug toggle stream (driven by AYAStreamDebugPlay). Independent of
    // the prim binding map.
    void startDebug(const std::string& url, const LLVector3& world_pos);
    void stopDebug();

    // Tag parsed from a prim Description.
    struct TagData
    {
        std::string url;
        std::optional<F32> min;
        std::optional<F32> max;
    };

    // Returns parsed [ayastream:{...},{...}] tag, or nullopt if none /
    // malformed / missing URL.
    static std::optional<TagData> parseTag(const std::string& description);

private:
    LLPositionalStreamMgr();
    ~LLPositionalStreamMgr();
    LLPositionalStreamMgr(const LLPositionalStreamMgr&) = delete;
    LLPositionalStreamMgr& operator=(const LLPositionalStreamMgr&) = delete;

    struct Binding
    {
        std::string url;
        // Values explicitly set by the prim's tag — nullopt means "fall
        // back to the global default" so global slider changes still apply.
        std::optional<F32> tag_min;
        std::optional<F32> tag_max;
        F32 applied_min = 1.f;
        F32 applied_max = 20.f;
        std::unique_ptr<LLPositionalStream> stream;
    };

    void evaluateBinding(const LLUUID& id);
    void resolveRolloff(const TagData& tag, F32& out_min, F32& out_max) const;

    std::map<LLUUID, std::string> mDescriptionCache;
    std::map<LLUUID, Binding> mBindings;
    std::unique_ptr<LLPositionalStream> mDebugStream;
};

#endif // LL_POSITIONAL_STREAM_MGR_H
