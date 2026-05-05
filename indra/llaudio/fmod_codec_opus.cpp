/**
 * @file fmod_codec_opus.cpp
 * @brief AYAstorm FMOD codec plugin for Ogg Opus (P4 mono / stereo / multistream).
 *
 * Bundled libfmod ships without an Opus codec. We extend FMOD via the
 * registerCodec() API: libogg handles the Ogg framing, libopus (shipped
 * inside the FMOD SDK staging) handles the per-packet decode.
 *
 * Scope:
 *   - Channel mapping family 0 (native mono / stereo) via opus_decoder.
 *   - Channel mapping family 1 (Vorbis-compatible 3..8 ch surround) via
 *     opus_multistream_decoder. Mapping table is parsed from OpusHead.
 *   - PCMFLOAT output at 48 kHz (FMOD resamples downstream as needed).
 *   - Streamed playback: only setposition(0) is accepted (FMOD prebuffer);
 *     real seeking and getlength are not advertised.
 */

#include "fmod_codec_opus.h"
#include "fmodstudio/fmod_errors.h"
#include "llerror.h"

#include <ogg/ogg.h>
#include <opus/opus.h>
#include <opus/opus_multistream.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
    constexpr int kOpusOutputRate      = 48000;
    constexpr int kOpusMaxFrameSamples = 5760;   // 120 ms @ 48 kHz, per channel
    constexpr int kOpusMaxChannels     = 8;      // family 1 supports up to 8 ch
    constexpr unsigned int kFeedChunkSize = 4096;
    constexpr unsigned int kProbeMaxBytes = 65536;

    struct OpusCodecState
    {
        ogg_sync_state   oy{};
        ogg_stream_state os{};
        // Exactly one of these is non-null after a successful opusOpen:
        // family 0 uses opus_decoder, family 1 uses opus_multistream_decoder.
        OpusDecoder*     decoder = nullptr;
        OpusMSDecoder*   ms_decoder = nullptr;
        int              channels = 0;
        int              pre_skip_remaining = 0;
        bool             ogg_sync_init_done = false;
        bool             stream_init_done   = false;
        int              header_packets_seen = 0;   // expect 2 (OpusHead + OpusTags)
        bool             eof = false;

        std::vector<float> pending;
        size_t           pending_pos = 0;

        FMOD_CODEC_WAVEFORMAT waveformat{};
    };

    inline std::uint16_t read_u16le(const unsigned char* p)
    {
        return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
    }

    bool feed_ogg_sync(FMOD_CODEC_STATE* codec, OpusCodecState* state, unsigned int sizebytes)
    {
        char* buf = ogg_sync_buffer(&state->oy, static_cast<long>(sizebytes));
        if (!buf)
        {
            LL_WARNS("FmodOpus") << "ogg_sync_buffer returned null (size=" << sizebytes << ")" << LL_ENDL;
            return false;
        }

        unsigned int bytes_read = 0;
        FMOD_RESULT r = FMOD_CODEC_FILE_READ(codec, buf, sizebytes, &bytes_read);
        if (r == FMOD_ERR_FILE_EOF)
        {
            state->eof = true;
            if (bytes_read == 0) return false;
        }
        else if (r != FMOD_OK)
        {
            state->eof = true;
            return false;
        }
        if (bytes_read == 0)
        {
            state->eof = true;
            return false;
        }
        ogg_sync_wrote(&state->oy, static_cast<long>(bytes_read));
        return true;
    }

    void destroy_state(OpusCodecState* state)
    {
        if (!state) return;
        if (state->decoder)
        {
            opus_decoder_destroy(state->decoder);
            state->decoder = nullptr;
        }
        if (state->ms_decoder)
        {
            opus_multistream_decoder_destroy(state->ms_decoder);
            state->ms_decoder = nullptr;
        }
        if (state->stream_init_done)
        {
            ogg_stream_clear(&state->os);
            state->stream_init_done = false;
        }
        if (state->ogg_sync_init_done)
        {
            ogg_sync_clear(&state->oy);
            state->ogg_sync_init_done = false;
        }
        delete state;
    }

    FMOD_RESULT F_CALL opusOpen(FMOD_CODEC_STATE* codec,
                                FMOD_MODE /*usermode*/,
                                FMOD_CREATESOUNDEXINFO* /*userexinfo*/)
    {
        if (!codec) return FMOD_ERR_FORMAT;

        OpusCodecState* state = new OpusCodecState();
        ogg_sync_init(&state->oy);
        state->ogg_sync_init_done = true;

        ogg_page page;
        ogg_packet first_packet;
        bool got_first_packet = false;
        unsigned int total_probed = 0;

        while (!got_first_packet)
        {
            int sync = ogg_sync_pageout(&state->oy, &page);
            if (sync == 1)
            {
                if (!ogg_page_bos(&page))
                {
                    destroy_state(state);
                    return FMOD_ERR_FORMAT;
                }
                if (ogg_stream_init(&state->os, ogg_page_serialno(&page)) != 0)
                {
                    destroy_state(state);
                    return FMOD_ERR_FORMAT;
                }
                state->stream_init_done = true;

                if (ogg_stream_pagein(&state->os, &page) != 0)
                {
                    destroy_state(state);
                    return FMOD_ERR_FORMAT;
                }
                if (ogg_stream_packetout(&state->os, &first_packet) != 1)
                {
                    destroy_state(state);
                    return FMOD_ERR_FORMAT;
                }
                got_first_packet = true;
                break;
            }
            // sync == 0 or -1: keep feeding more bytes.
            if (total_probed >= kProbeMaxBytes)
            {
                destroy_state(state);
                return FMOD_ERR_FORMAT;
            }
            if (!feed_ogg_sync(codec, state, kFeedChunkSize))
            {
                destroy_state(state);
                return FMOD_ERR_FORMAT;
            }
            total_probed += kFeedChunkSize;
        }

        // Parse OpusHead.
        if (first_packet.bytes < 19)
        {
            destroy_state(state);
            return FMOD_ERR_FORMAT;
        }
        const unsigned char* p = first_packet.packet;
        if (std::memcmp(p, "OpusHead", 8) != 0)
        {
            destroy_state(state);
            return FMOD_ERR_FORMAT;
        }
        unsigned char version = p[8];
        if ((version & 0xF0) != 0)
        {
            destroy_state(state);
            return FMOD_ERR_FORMAT;
        }
        int channel_count = p[9];
        int pre_skip      = read_u16le(p + 10);
        // p[12..15] = original input sample rate (informational)
        std::int16_t output_gain_q8 = static_cast<std::int16_t>(read_u16le(p + 16));
        unsigned char channel_mapping_family = p[18];

        if (channel_count < 1 || channel_count > kOpusMaxChannels)
        {
            destroy_state(state);
            return FMOD_ERR_FORMAT;
        }

        int err = 0;
        if (channel_mapping_family == 0)
        {
            // Family 0: native mono / stereo, no mapping table.
            if (channel_count > 2)
            {
                destroy_state(state);
                return FMOD_ERR_FORMAT;
            }
            state->decoder = opus_decoder_create(kOpusOutputRate, channel_count, &err);
            if (!state->decoder || err != OPUS_OK)
            {
                LL_WARNS("FmodOpus") << "opus_decoder_create failed err=" << err << LL_ENDL;
                destroy_state(state);
                return FMOD_ERR_FORMAT;
            }
            if (output_gain_q8 != 0)
            {
                opus_decoder_ctl(state->decoder, OPUS_SET_GAIN(output_gain_q8));
            }
        }
        else if (channel_mapping_family == 1)
        {
            // Family 1: Vorbis-compatible mapping. OpusHead extends past the
            // 19-byte fixed header with stream_count, coupled_count, and a
            // per-output-channel mapping table.
            const int kMapHeaderSize = 2;   // stream_count + coupled_count
            const int needed = 19 + kMapHeaderSize + channel_count;
            if (first_packet.bytes < needed)
            {
                destroy_state(state);
                return FMOD_ERR_FORMAT;
            }
            int stream_count  = p[19];
            int coupled_count = p[20];
            const unsigned char* mapping = p + 21;
            // libopus validates stream_count / coupled_count / mapping itself;
            // any inconsistency surfaces as a non-OK err below.
            state->ms_decoder = opus_multistream_decoder_create(
                kOpusOutputRate,
                channel_count,
                stream_count,
                coupled_count,
                mapping,
                &err);
            if (!state->ms_decoder || err != OPUS_OK)
            {
                LL_WARNS("FmodOpus") << "opus_multistream_decoder_create failed err=" << err
                                      << " ch=" << channel_count
                                      << " streams=" << stream_count
                                      << " coupled=" << coupled_count << LL_ENDL;
                destroy_state(state);
                return FMOD_ERR_FORMAT;
            }
            if (output_gain_q8 != 0)
            {
                opus_multistream_decoder_ctl(state->ms_decoder, OPUS_SET_GAIN(output_gain_q8));
            }
            LL_INFOS("FmodOpus") << "Opus multistream: ch=" << channel_count
                                  << " streams=" << stream_count
                                  << " coupled=" << coupled_count << LL_ENDL;
        }
        else
        {
            // Family 2 / 3 (ambisonic) deferred — would land alongside HRTF/SOFA
            // work, not r9 5.1 source receiving.
            destroy_state(state);
            return FMOD_ERR_FORMAT;
        }

        state->channels            = channel_count;
        state->pre_skip_remaining  = pre_skip;
        state->header_packets_seen = 1;   // OpusHead consumed; OpusTags drained lazily

        // Match the FMOD raw-codec example shape: only fill the fields that
        // are unambiguous for a streamed source. Letting pcmblocksize / mode /
        // channelmask / channelorder / lengthbytes stay at 0 lets FMOD pick
        // safe defaults rather than rejecting the descriptor for a mismatch.
        state->waveformat.name         = "Ogg Opus";
        state->waveformat.format       = FMOD_SOUND_FORMAT_PCMFLOAT;
        state->waveformat.channels     = channel_count;
        state->waveformat.frequency    = kOpusOutputRate;
        state->waveformat.lengthpcm    = 0xFFFFFFFFu;   // unknown / streaming

        codec->waveformat   = &state->waveformat;
        codec->plugindata   = state;
        codec->numsubsounds = 0;

        LL_INFOS("FmodOpus") << "Opus stream opened: "
                              << channel_count << "ch 48000Hz PCMFLOAT pre_skip=" << pre_skip << LL_ENDL;
        return FMOD_OK;
    }

    FMOD_RESULT F_CALL opusClose(FMOD_CODEC_STATE* codec)
    {
        if (codec && codec->plugindata)
        {
            destroy_state(static_cast<OpusCodecState*>(codec->plugindata));
            codec->plugindata = nullptr;
        }
        return FMOD_OK;
    }

    // Pull one packet from the Ogg stream and turn it into PCM in state->pending.
    // Returns true if a packet was processed (audio or header), false if no
    // packet was available without more input from the file.
    bool decode_next_packet(OpusCodecState* state)
    {
        ogg_packet packet;
        int rc = ogg_stream_packetout(&state->os, &packet);
        if (rc != 1) return false;

        // Drop OpusTags transparently; everything afterwards is audio.
        if (state->header_packets_seen < 2)
        {
            state->header_packets_seen++;
            return true;
        }

        std::vector<float> tmp(static_cast<size_t>(kOpusMaxFrameSamples) * static_cast<size_t>(state->channels));
        int frames = 0;
        if (state->ms_decoder)
        {
            frames = opus_multistream_decode_float(state->ms_decoder,
                                                   packet.packet,
                                                   static_cast<opus_int32>(packet.bytes),
                                                   tmp.data(),
                                                   kOpusMaxFrameSamples,
                                                   0);
        }
        else
        {
            frames = opus_decode_float(state->decoder,
                                       packet.packet,
                                       static_cast<opus_int32>(packet.bytes),
                                       tmp.data(),
                                       kOpusMaxFrameSamples,
                                       0);
        }
        if (frames <= 0)
        {
            LL_WARNS("FmodOpus") << "opus_decode_float -> " << frames
                                  << " (packet bytes=" << packet.bytes << ")" << LL_ENDL;
            return true;
        }

        int frames_offset = 0;
        int frames_keep   = frames;
        if (state->pre_skip_remaining > 0)
        {
            int skip = std::min(state->pre_skip_remaining, frames);
            frames_offset             += skip;
            frames_keep               -= skip;
            state->pre_skip_remaining -= skip;
        }
        if (frames_keep > 0)
        {
            const size_t off = static_cast<size_t>(frames_offset) * static_cast<size_t>(state->channels);
            const size_t n   = static_cast<size_t>(frames_keep)   * static_cast<size_t>(state->channels);
            state->pending.assign(tmp.begin() + off, tmp.begin() + off + n);
            state->pending_pos = 0;
        }
        return true;
    }

    FMOD_RESULT F_CALL opusRead(FMOD_CODEC_STATE* codec,
                                void* buffer,
                                unsigned int samples_in,
                                unsigned int* samples_out)
    {
        if (!codec || !codec->plugindata || !buffer)
        {
            if (samples_out) *samples_out = 0;
            return FMOD_ERR_FORMAT;
        }
        OpusCodecState* state = static_cast<OpusCodecState*>(codec->plugindata);
        float* out = static_cast<float*>(buffer);

        unsigned int frames_written = 0;

        while (frames_written < samples_in)
        {
            // 1) Drain any leftover decoded PCM.
            if (state->pending_pos < state->pending.size())
            {
                const size_t avail_floats = state->pending.size() - state->pending_pos;
                const size_t avail_frames = avail_floats / static_cast<size_t>(state->channels);
                const size_t want_frames  = static_cast<size_t>(samples_in - frames_written);
                const size_t take_frames  = std::min(avail_frames, want_frames);
                const size_t take_floats  = take_frames * static_cast<size_t>(state->channels);

                std::memcpy(out + static_cast<size_t>(frames_written) * static_cast<size_t>(state->channels),
                            state->pending.data() + state->pending_pos,
                            take_floats * sizeof(float));
                state->pending_pos += take_floats;
                frames_written     += static_cast<unsigned int>(take_frames);

                if (state->pending_pos >= state->pending.size())
                {
                    state->pending.clear();
                    state->pending_pos = 0;
                }
                continue;
            }

            // 2) Try to decode the next packet from the stream.
            if (decode_next_packet(state)) continue;

            // 3) No packet ready — pull more pages.
            ogg_page page;
            int sync = ogg_sync_pageout(&state->oy, &page);
            if (sync == 1)
            {
                ogg_stream_pagein(&state->os, &page);
                continue;
            }
            if (sync < 0)
            {
                // Hole/desync — let libogg resync from existing buffer.
                continue;
            }

            // 4) Need more bytes from the file.
            if (state->eof) break;
            if (!feed_ogg_sync(codec, state, kFeedChunkSize)) break;
        }

        if (samples_out) *samples_out = frames_written;
        return (frames_written == 0) ? FMOD_ERR_FILE_EOF : FMOD_OK;
    }

    FMOD_RESULT F_CALL opusSetPosition(FMOD_CODEC_STATE* /*codec*/,
                                       int /*subsound*/,
                                       unsigned int position,
                                       FMOD_TIMEUNIT /*postype*/)
    {
        // FMOD's prebuffer flow may issue setposition(0) before the first
        // read; treat that as a no-op so streamed playback isn't rejected.
        // Real seeking is deferred to a later phase (offline files).
        if (position == 0) return FMOD_OK;
        return FMOD_ERR_FILE_COULDNOTSEEK;
    }

    FMOD_CODEC_DESCRIPTION sOpusCodec =
    {
        FMOD_CODEC_PLUGIN_VERSION,
        "AYAstorm Opus codec",
        0x00000002,                  // 0.0.0.2 (P3.2)
        1,                           // defaultasstream: streamed by default
        FMOD_TIMEUNIT_PCM,
        &opusOpen,
        &opusClose,
        &opusRead,
        nullptr,                     // getlength: unknown for streams
        &opusSetPosition,
        nullptr,                     // getposition
        nullptr,                     // soundcreate
        nullptr                      // getwaveformat
    };
}

FMOD_CODEC_DESCRIPTION* F_CALL FMODGetCodecDescriptionOpus()
{
    return &sOpusCodec;
}
