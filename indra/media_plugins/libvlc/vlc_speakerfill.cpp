#include <winsock2.h>
#include <basetsd.h>
typedef SSIZE_T ssize_t;
static inline int poll(struct pollfd* descriptors, unsigned count, int timeout)
{
    return WSAPoll(descriptors, count, timeout);
}
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_block.h>
#include <limits>

static block_t* Fill(filter_t* filter, block_t* input)
{
    if (!input || !input->i_nb_samples) return input;
    const auto channels = filter->fmt_out.audio.i_channels;
    if (input->i_nb_samples > std::numeric_limits<size_t>::max() / (channels * sizeof(float)) ||
        input->i_nb_samples > input->i_buffer / (2 * sizeof(float)))
    {
        block_Release(input);
        return nullptr;
    }
    block_t* output = block_Alloc(input->i_nb_samples * channels * sizeof(float));
    if (!output)
    {
        block_Release(input);
        return nullptr;
    }
    output->i_nb_samples = input->i_nb_samples;
    output->i_pts = input->i_pts;
    output->i_dts = input->i_dts;
    output->i_length = input->i_length;
    output->i_flags = input->i_flags;
    const auto* source = reinterpret_cast<const float*>(input->p_buffer);
    auto* destination = reinterpret_cast<float*>(output->p_buffer);
    for (size_t frame = 0; frame < input->i_nb_samples; ++frame)
    {
        const float left = source[frame * 2] * .70710678f;
        const float right = source[frame * 2 + 1] * .70710678f;
        unsigned channel = 0;
        for (unsigned index = 0; pi_vlc_chan_order_wg4[index]; ++index)
        {
            const auto label = pi_vlc_chan_order_wg4[index];
            if (!(filter->fmt_out.audio.i_physical_channels & label)) continue;
            float value = 0.f;
            if (label & (AOUT_CHAN_LEFT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_REARLEFT)) value = left;
            else if (label & (AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARRIGHT)) value = right;
            else if (label == AOUT_CHAN_CENTER) value = (left + right) * .5f;
            destination[frame * channels + channel++] = value;
        }
    }
    block_Release(input);
    return output;
}

static block_t* Pass(filter_t*, block_t* input) { return input; }

static int Open(vlc_object_t* object)
{
    auto* filter = reinterpret_cast<filter_t*>(object);
    filter->fmt_out.audio = filter->fmt_in.audio;
    const auto requested = var_InheritInteger(object, "speakerfill-mask");
    const uint32_t mask = static_cast<uint32_t>(requested);
    const bool supported = mask == (AOUT_CHANS_2_0 | AOUT_CHAN_LFE) ||
        mask == (AOUT_CHANS_4_0 | AOUT_CHAN_LFE) ||
        mask == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE) ||
        mask == AOUT_CHANS_4_0 ||
        mask == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT) ||
        mask == AOUT_CHANS_5_1 || mask == (AOUT_CHANS_5_0_MIDDLE | AOUT_CHAN_LFE) || mask == AOUT_CHANS_7_1;
    if (filter->fmt_in.audio.channel_type != AUDIO_CHANNEL_TYPE_BITMAP ||
        filter->fmt_in.audio.i_physical_channels != AOUT_CHANS_2_0 || !supported)
    {
        filter->pf_audio_filter = Pass;
        return VLC_SUCCESS;
    }
    filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&filter->fmt_in.audio);
    filter->fmt_out.audio = filter->fmt_in.audio;
    filter->fmt_out.audio.i_physical_channels = mask;
    aout_FormatPrepare(&filter->fmt_out.audio);
    filter->pf_audio_filter = Fill;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("Speaker fill")
    set_description("Stereo music speaker fill")
    set_capability("audio filter", 0)
    add_shortcut("speakerfill")
    set_callbacks(Open, nullptr)
vlc_module_end()