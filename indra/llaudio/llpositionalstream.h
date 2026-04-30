/**
 * @file llpositionalstream.h
 * @brief 3D-positioned HTTP audio stream playback (AYAstorm).
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

#ifndef LL_POSITIONAL_STREAM_H
#define LL_POSITIONAL_STREAM_H

#include "stdtypes.h"
#include "v3math.h"
#include <string>

namespace FMOD
{
    class Sound;
    class Channel;
    class System;
}

class LLPositionalStream
{
public:
    static LLPositionalStream& instance();

    // Open a streaming URL and schedule it for playback at world_pos.
    // Playback actually begins once update() observes the async open
    // completing; until then the stream is buffering.
    bool start(const std::string& url, const LLVector3& world_pos);

    // Stop and release the current stream (no-op if not playing).
    void stop();

    bool isOpen() const { return mSound != nullptr; }
    bool isPlaying() const { return mChannel != nullptr; }

    void setPosition(const LLVector3& world_pos);
    void setVolume(F32 volume);
    void setRolloffDistances(F32 min_distance, F32 max_distance);

    // Drives async open polling and any deferred state. Call once per frame.
    void update();

private:
    LLPositionalStream();
    ~LLPositionalStream();
    LLPositionalStream(const LLPositionalStream&) = delete;
    LLPositionalStream& operator=(const LLPositionalStream&) = delete;

    FMOD::System* getFmodSystem() const;
    void releaseSound();

    FMOD::Sound* mSound;
    FMOD::Channel* mChannel;
    LLVector3 mPosition;
    F32 mVolume;
    F32 mRolloffMin;
    F32 mRolloffMax;
    std::string mUrl;
};

#endif // LL_POSITIONAL_STREAM_H
