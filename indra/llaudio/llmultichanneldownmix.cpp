/**
 * @file llmultichanneldownmix.cpp
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

#include "linden_common.h"
#include "llmultichanneldownmix.h"

namespace
{
    // Vorbis / Opus channel-mapping-family-1 order:
    //   frame interleave: [FL, C, FR, SL, SR, LFE]
    constexpr LLMultichannelDownmix::Layout kLayoutVorbisOpus = LLMultichannelDownmix::Layout::VorbisOpus6;
    constexpr LLMultichannelDownmix::Layout kLayoutFlac       = LLMultichannelDownmix::Layout::Flac6;

    // BS.775 mix coefficients (§4.4). The 0.5 LFE term is conservative — the
    // standard recipe omits LFE; we include it because broadcasters routinely
    // route the LFE channel as a creative element rather than a sub-only feed.
    constexpr F32 kCoefCenter   = 0.707f;
    constexpr F32 kCoefSurround = 0.707f;
    constexpr F32 kCoefLfe      = 0.5f;

    // 1 / (1 + 0.707 + 0.707 + 0.5) ≈ 0.34317 — keeps full-scale-on-every-channel
    // input inside ±1.0 at the cost of ~9 dB headroom on typical material.
    constexpr F32 kNormGain = 1.0f / (1.0f + kCoefCenter + kCoefSurround + kCoefLfe);
}

// static
LLMultichannelDownmix LLMultichannelDownmix::forSourceFormat(FMOD_SOUND_TYPE type, int channels)
{
    LLMultichannelDownmix d;
    if (channels != 6)
    {
        return d; // mLayout stays Unsupported
    }

    switch (type)
    {
    case FMOD_SOUND_TYPE_OGGVORBIS:
    case FMOD_SOUND_TYPE_VORBIS:
    case FMOD_SOUND_TYPE_OPUS:
        d.mLayout = kLayoutVorbisOpus;
        d.mIndices = {0, 2, 1, 5, 3, 4};
        break;
    case FMOD_SOUND_TYPE_FLAC:
        d.mLayout = kLayoutFlac;
        d.mIndices = {0, 1, 2, 3, 4, 5};
        break;
    default:
        // 6ch source from an unexpected codec (WAV, AIFF, MPEG, ...). r9
        // declines these so we don't ship a guess that turns out to put C
        // on the wrong index for some user.
        break;
    }
    return d;
}

const char* LLMultichannelDownmix::layoutName() const
{
    switch (mLayout)
    {
    case Layout::VorbisOpus6: return "VorbisOpus6";
    case Layout::Flac6:       return "Flac6";
    case Layout::Unsupported: break;
    }
    return "Unsupported";
}

void LLMultichannelDownmix::mix6chToMono(const F32* in_6ch, F32* out_mono,
                                         std::size_t frames, MixRole role) const
{
    if (mLayout == Layout::Unsupported)
    {
        // Defensive: caller is supposed to gate on isSupported() before
        // entering this path, so reaching here is a programmer error.
        for (std::size_t i = 0; i < frames; ++i) out_mono[i] = 0.f;
        return;
    }

    const int iFL  = mIndices.FL;
    const int iFR  = mIndices.FR;
    const int iC   = mIndices.C;
    const int iLFE = mIndices.LFE;
    const int iSL  = mIndices.SL;
    const int iSR  = mIndices.SR;

    for (std::size_t i = 0; i < frames; ++i)
    {
        const F32* f = in_6ch + i * 6;
        const F32 c   = f[iC];
        const F32 lfe = f[iLFE];

        const F32 L = kNormGain * (f[iFL] + kCoefCenter * c + kCoefSurround * f[iSL] + kCoefLfe * lfe);
        const F32 R = kNormGain * (f[iFR] + kCoefCenter * c + kCoefSurround * f[iSR] + kCoefLfe * lfe);

        switch (role)
        {
        case MixRole::L:      out_mono[i] = L;             break;
        case MixRole::R:      out_mono[i] = R;             break;
        case MixRole::MonoLR: out_mono[i] = (L + R) * 0.5f; break;
        }
    }
}
