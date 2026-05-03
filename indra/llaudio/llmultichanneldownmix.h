/**
 * @file llmultichanneldownmix.h
 * @brief 5.1ch → L/R downmix for the distributed-stereo decode worker (r9).
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

#ifndef LL_MULTICHANNEL_DOWNMIX_H
#define LL_MULTICHANNEL_DOWNMIX_H

#include "stdtypes.h"

#include "fmodstudio/fmod_common.h"

#include <cstddef>

// r9 (spec_5_1ch_source.md §4.3 / §4.4 / §4.7): map a 6-channel interleaved
// PCM frame to 2-channel (L/R) using ITU-R BS.775 coefficients with a single
// normalisation factor that keeps full-scale input inside ±1.0 on output.
//
// Codec-specific channel order is captured at construction (Vorbis/Opus use
// channel-mapping-family-1 order [FL,C,FR,SL,SR,LFE]; FLAC uses WAV/SMPTE
// order [FL,FR,C,LFE,SL,SR]) so the per-frame inner loop reads through fixed
// indices and never re-checks the codec.
//
// Scope is intentionally just 6ch → 2ch interleaved. r10 may rename this to
// MultichannelRouter and add a 6ch → 6track passthrough variant when speaker
// placement moves to per-channel routing.
class LLMultichannelDownmix
{
public:
    enum class Layout
    {
        Unsupported,
        VorbisOpus6,
        Flac6,
    };

    LLMultichannelDownmix() = default;

    // Build a downmix for the given FMOD source. Returns an instance with
    // isSupported() == false when the codec/channel-count pair is not in
    // r9's accepted set (only 6ch Vorbis/Opus/FLAC qualify).
    static LLMultichannelDownmix forSourceFormat(FMOD_SOUND_TYPE type, int channels);

    bool isSupported() const { return mLayout != Layout::Unsupported; }
    Layout layout() const { return mLayout; }
    const char* layoutName() const;

    // 6ch interleaved F32 input → 2ch interleaved F32 output (L,R,L,R,...).
    // Output slot count is 2 * frames. Coefficients follow §4.4:
    //   L = c·(FL + 0.707·C + 0.707·SL + 0.5·LFE)
    //   R = c·(FR + 0.707·C + 0.707·SR + 0.5·LFE)
    //   c = 1/2.914
    // Caller has already converted from PCM16 to F32 if needed.
    void mix6chTo2chLR(const F32* in_6ch, F32* out_2ch, std::size_t frames) const;

private:
    struct Indices
    {
        int FL, FR, C, LFE, SL, SR;
    };

    Layout mLayout = Layout::Unsupported;
    Indices mIndices = {};
};

#endif // LL_MULTICHANNEL_DOWNMIX_H
