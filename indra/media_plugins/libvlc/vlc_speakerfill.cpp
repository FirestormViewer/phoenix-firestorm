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
#include <cmath>
#include <new>

struct filter_sys_t
{
    float gain = 1.f;
    float target = 1.f;
    float step = 0.f;
    unsigned remaining = 0;
    bool fill = false;
    bool smooth = false;
    vlc_object_t* owner = nullptr;
    uint64_t command = 0;
    float transition = 1.f;
    float origin = 1.f;
    float destination = 1.f;
    uint64_t position = 0;
    uint64_t duration = 0;
    bool completed = false;
};

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

static block_t* Process(filter_t* filter, block_t* input)
{
    auto& state = *filter->p_sys;
    block_t* output = state.fill ? Fill(filter, input) : input;
    if (!output || !state.smooth) return output;
    if (state.owner)
    {
        const auto command = static_cast<uint64_t>(var_GetInteger(state.owner, "music-fade-command"));
        if (command && command != state.command)
        {
            state.command = command;
            state.origin = state.transition;
            state.destination = static_cast<float>(command & 65535) / 65535.f;
            state.duration = ((command >> 16) & 65535) * filter->fmt_out.audio.i_rate / 1000;
            state.position = 0;
            state.completed = false;
            var_SetInteger(state.owner, "music-fade-completed", 0);
        }
    }
    const float requested = var_InheritFloat(filter, "speakerfill-gain");
    if (std::isfinite(requested) && requested >= 0.f && requested <= 1.f && requested != state.target)
    {
        state.target = requested;
        state.remaining = filter->fmt_out.audio.i_rate / 20;
        if (!state.remaining) state.remaining = 1;
        state.step = (state.target - state.gain) / state.remaining;
    }
    auto* samples = reinterpret_cast<float*>(output->p_buffer);
    const bool muted = state.owner && var_Type(state.owner, "music-hard-mute") == VLC_VAR_BOOL &&
        var_GetBool(state.owner, "music-hard-mute");
    const unsigned channels = filter->fmt_out.audio.i_channels;
    if (!channels || output->i_nb_samples > output->i_buffer / (channels * sizeof(float)))
    {
        block_Release(output);
        return nullptr;
    }
    for (size_t frame = 0; frame < output->i_nb_samples; ++frame)
    {
        if (state.command)
        {
            state.transition = state.position >= state.duration ? state.destination :
                state.origin + (state.destination - state.origin) * static_cast<float>(state.position) / state.duration;
            if (state.position < state.duration) ++state.position;
            else if (!state.completed && output->i_pts > 0)
            {
                const auto fence = output->i_pts + static_cast<int64_t>((frame + 1) * 1000000 / filter->fmt_out.audio.i_rate);
                var_SetInteger(state.owner, "music-fade-fence", fence);
                var_SetInteger(state.owner, "music-fade-epoch", var_GetInteger(state.owner, "music-clock-epoch"));
                var_SetInteger(state.owner, "music-fade-completed", static_cast<int64_t>(state.command));
                state.completed = true;
            }
        }
        for (unsigned channel = 0; channel < channels; ++channel)
            samples[frame * channels + channel] *= muted ? 0.f : state.gain * state.transition;
        if (state.remaining)
        {
            state.gain += state.step;
            if (--state.remaining == 0) state.gain = state.target;
        }
    }
    return output;
}

static void Close(vlc_object_t* object)
{
    delete reinterpret_cast<filter_t*>(object)->p_sys;
}

static void Flush(filter_t* filter)
{
    if (filter->p_sys)
    {
        filter->p_sys->command = 0;
        filter->p_sys->completed = false;
        if (filter->p_sys->owner) var_SetInteger(filter->p_sys->owner, "music-fade-completed", 0);
    }
}

static int Open(vlc_object_t* object)
{
    auto* filter = reinterpret_cast<filter_t*>(object);
    filter->p_sys = nullptr;
    filter->fmt_out.audio = filter->fmt_in.audio;
    const bool smooth = var_InheritBool(object, "speakerfill-smooth-gain");
    const auto requested = var_InheritInteger(object, "speakerfill-mask");
    const uint32_t mask = static_cast<uint32_t>(requested);
    const bool supported = mask == (AOUT_CHANS_2_0 | AOUT_CHAN_LFE) ||
        mask == (AOUT_CHANS_4_0 | AOUT_CHAN_LFE) ||
        mask == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE) ||
        mask == AOUT_CHANS_4_0 ||
        mask == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT) ||
        mask == AOUT_CHANS_5_1 || mask == (AOUT_CHANS_5_0_MIDDLE | AOUT_CHAN_LFE) || mask == AOUT_CHANS_7_1;
    const bool fill = filter->fmt_in.audio.channel_type == AUDIO_CHANNEL_TYPE_BITMAP &&
        filter->fmt_in.audio.i_physical_channels == AOUT_CHANS_2_0 && supported;
    if (!fill && !smooth)
    {
        filter->pf_audio_filter = Pass;
        return VLC_SUCCESS;
    }
    if (filter->fmt_in.audio.channel_type != AUDIO_CHANNEL_TYPE_BITMAP) return VLC_EGENERIC;
    auto* state = new (std::nothrow) filter_sys_t;
    if (!state) return VLC_ENOMEM;
    state->fill = fill;
    state->smooth = smooth;
    for (auto* owner = object; owner; owner = owner->obj.parent)
    {
        if (var_Type(owner, "music-fade-command") == VLC_VAR_INTEGER)
        {
            state->owner = owner;
            state->transition = var_GetFloat(owner, "music-fade-initial");
            break;
        }
    }
    if (smooth)
    {
        const float gain = var_InheritFloat(object, "speakerfill-gain");
        state->gain = state->target = std::isfinite(gain) && gain >= 0.f && gain <= 1.f ? gain : 0.f;
    }
    filter->p_sys = state;
    filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&filter->fmt_in.audio);
    filter->fmt_out.audio = filter->fmt_in.audio;
    if (fill) filter->fmt_out.audio.i_physical_channels = mask;
    aout_FormatPrepare(&filter->fmt_out.audio);
    filter->pf_audio_filter = Process;
    filter->pf_flush = Flush;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("Speaker fill")
    set_description("Stereo music speaker fill")
    set_capability("audio filter", 0)
    add_shortcut("speakerfill")
    add_bool("speakerfill-smooth-gain", false, "Smooth music gain", "Interpolate music gain per sample", false)
    add_float("speakerfill-gain", 1.f, "Music gain", "Linear music gain", false)
    set_callbacks(Open, Close)
vlc_module_end()