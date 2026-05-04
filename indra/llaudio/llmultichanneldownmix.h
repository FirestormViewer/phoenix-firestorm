/**
 * @file llmultichanneldownmix.h
 * @brief 5.1ch → mono BS.775 downmix for the distributed-stereo reader (r9/r10).
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
// PCM frame to a single mono output channel using ITU-R BS.775 coefficients
// with a single normalisation factor that keeps full-scale input inside ±1.0.
//
// Codec-specific channel order is captured at construction (Vorbis/Opus use
// channel-mapping-family-1 order [FL,C,FR,SL,SR,LFE]; FLAC uses WAV/SMPTE
// order [FL,FR,C,LFE,SL,SR]) so the per-frame inner loop reads through fixed
// indices and never re-checks the codec.
//
// r10 P3: shape is 6ch → 1ch with a role selector (L / R / MonoLR), called
// per-speaker on the FMOD mixer thread. The reader pulls raw 6ch samples
// from the multi-tail ring and runs them through here to produce the mono
// signal that gets handed to FMOD as the speaker's source PCM.
class LLMultichannelDownmix
{
public:
    enum class Layout
    {
        Unsupported,
        VorbisOpus6,
        Flac6,
    };

    // r10 P3: which BS.775-downmixed channel the caller wants out of a 6ch
    // input frame. L / R map to the BS.775 stereo pair; MonoLR is (L+R)/2,
    // a -6 dB sum-to-mono on top of BS.775 — used by ch:M speakers.
    enum class MixRole
    {
        L,
        R,
        MonoLR,
    };

    LLMultichannelDownmix() = default;

    // Build a downmix for the given FMOD source. Returns an instance with
    // isSupported() == false when the codec/channel-count pair is not in
    // r9's accepted set (only 6ch Vorbis/Opus/FLAC qualify).
    static LLMultichannelDownmix forSourceFormat(FMOD_SOUND_TYPE type, int channels);

    bool isSupported() const { return mLayout != Layout::Unsupported; }
    Layout layout() const { return mLayout; }
    const char* layoutName() const;

    // 6ch interleaved F32 input → 1ch F32 output, one sample per frame.
    // Output slot count is `frames`. Coefficients follow §4.4:
    //   L = c·(FL + 0.707·C + 0.707·SL + 0.5·LFE)
    //   R = c·(FR + 0.707·C + 0.707·SR + 0.5·LFE)
    //   M = (L + R) / 2
    //   c = 1/2.914
    // Caller has already converted from PCM16 to F32 if needed.
    void mix6chToMono(const F32* in_6ch, F32* out_mono,
                      std::size_t frames, MixRole role) const;

    // r10 P4: codec channel indices, used by the §4.2 compatibility matrix
    // when a ch:FL/FR/C/LFE/SL/SR speaker on a 6ch source needs to read its
    // own track directly out of the multi-tail ring. Values are valid only
    // when isSupported() is true.
    struct Indices
    {
        int FL, FR, C, LFE, SL, SR;
    };
    const Indices& indices() const { return mIndices; }

private:
    Layout mLayout = Layout::Unsupported;
    Indices mIndices = {};
};

#endif // LL_MULTICHANNEL_DOWNMIX_H
