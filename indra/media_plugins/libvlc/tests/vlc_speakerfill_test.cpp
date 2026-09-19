#include <winsock2.h>
#include <basetsd.h>
typedef SSIZE_T ssize_t;
static inline int poll(struct pollfd* descriptors, unsigned count, int timeout)
{
    return WSAPoll(descriptors, count, timeout);
}
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_modules.h>
#include <vlc/vlc.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#pragma warning(push)
#pragma warning(disable: 4244)
#include "audio_output/mmdevice.h"
#pragma warning(pop)
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../llvlcspeakerfillconfig.h"

struct Descriptor
{
    void* open = nullptr;
    void* close = nullptr;
    int score = -1;
};

static void require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static int Describe(void* opaque, void*, int property, ...)
{
    auto& descriptor = *static_cast<Descriptor*>(opaque);
    va_list arguments;
    va_start(arguments, property);
    if (property == VLC_MODULE_CREATE) *va_arg(arguments, module_t**) = reinterpret_cast<module_t*>(opaque);
    if (property == VLC_MODULE_CB_OPEN)
    {
        va_arg(arguments, const char*);
        descriptor.open = va_arg(arguments, void*);
    }
    if (property == VLC_MODULE_CB_CLOSE)
    {
        va_arg(arguments, const char*);
        descriptor.close = va_arg(arguments, void*);
    }
    if (property == VLC_MODULE_SCORE) descriptor.score = va_arg(arguments, int);
    va_end(arguments);
    return 0;
}

static Descriptor load(const char* filename, HMODULE& module)
{
    module = LoadLibraryA(filename);
    require(module != nullptr, "load actual VLC module DLL");
    using Entry = int (*)(vlc_set_cb, void*);
    const auto entry = reinterpret_cast<Entry>(GetProcAddress(module, "vlc_entry__3_0_0f"));
    require(entry != nullptr, "VLC 3 module ABI entry");
    Descriptor descriptor;
    require(entry(Describe, &descriptor) == 0 && descriptor.open && descriptor.score == 0,
        "module is explicit opt-in, never auto-selected");
    return descriptor;
}

struct FakeClient final : IAudioClient
{
    WAVEFORMATEXTENSIBLE mix{};
    HRESULT mixResult = S_OK;
    HRESULT initializeResult = S_OK;
    bool rejectSurround = false;
    unsigned channels = 0;
    unsigned refs = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE Initialize(AUDCLNT_SHAREMODE mode, DWORD, REFERENCE_TIME, REFERENCE_TIME,
        const WAVEFORMATEX* format, LPCGUID) override
    {
        require(mode == AUDCLNT_SHAREMODE_SHARED, "speaker fill retains VLC shared-mode playback");
        channels = format->nChannels;
        return initializeResult;
    }
    HRESULT STDMETHODCALLTYPE GetBufferSize(UINT32* frames) override { *frames = 2048; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetStreamLatency(REFERENCE_TIME* latency) override { *latency = 100000; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentPadding(UINT32* frames) override { *frames = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE IsFormatSupported(AUDCLNT_SHAREMODE, const WAVEFORMATEX* format, WAVEFORMATEX** closest) override
    { *closest = nullptr; return rejectSurround && format->nChannels > 2 ? AUDCLNT_E_UNSUPPORTED_FORMAT : S_OK; }
    HRESULT STDMETHODCALLTYPE GetMixFormat(WAVEFORMATEX** format) override
    {
        if (FAILED(mixResult)) return mixResult;
        *format = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(mix)));
        require(*format != nullptr, "fake mix allocation");
        memcpy(*format, &mix, sizeof(mix));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDevicePeriod(REFERENCE_TIME* normal, REFERENCE_TIME* minimum) override
    { *normal = 100000; *minimum = 30000; return S_OK; }
    HRESULT STDMETHODCALLTYPE Start() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Stop() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Reset() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetEventHandle(HANDLE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetService(REFIID, void**) override { return E_NOINTERFACE; }
};

struct FakeProperties final : IPropertyStore
{
    ULONG form = Speakers;
    HRESULT result = S_OK;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetCount(DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetAt(DWORD, PROPERTYKEY*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY, PROPVARIANT* value) override
    { value->vt = VT_UI4; value->ulVal = form; return result; }
    HRESULT STDMETHODCALLTYPE SetValue(REFPROPERTYKEY, REFPROPVARIANT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Commit() override { return E_NOTIMPL; }
};

struct FakeDevice final : IMMDevice
{
    FakeClient client;
    FakeProperties properties;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE Activate(REFIID, DWORD, PROPVARIANT*, void** output) override
    { *output = &client; client.AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE OpenPropertyStore(DWORD, IPropertyStore** output) override
    { *output = &properties; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetId(LPWSTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetState(DWORD*) override { return E_NOTIMPL; }
};

static void negotiation(const Descriptor& descriptor)
{
    using Start = HRESULT (*)(aout_stream_t*, audio_sample_format_t*, const GUID*);
    using Stop = HRESULT (*)(aout_stream_t*);
    const auto start = reinterpret_cast<Start>(descriptor.open);
    const auto stop = reinterpret_cast<Stop>(descriptor.close);
    require(stop != nullptr, "VLC output cleanup entry");
    const DWORD masks[] = {SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
        SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY,
        SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_LOW_FREQUENCY,
        SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
        KSAUDIO_SPEAKER_5POINT1, KSAUDIO_SPEAKER_5POINT1_SURROUND, KSAUDIO_SPEAKER_7POINT1_SURROUND};
    for (unsigned scenario = 0; scenario < 7; ++scenario)
    for (const auto mask : masks)
    for (const int layout : {0, 21, 41, 51, 71, 99})
    {
        FakeDevice device;
        if (scenario == 1) device.properties.form = Headphones;
        if (scenario == 2) device.client.mixResult = E_FAIL;
        if (scenario == 4) device.properties.result = E_FAIL;
        if (scenario == 5) device.client.initializeResult = E_FAIL;
        if (scenario == 6) device.client.rejectSurround = true;
        auto& mix = device.client.mix;
        mix.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        mix.Format.nSamplesPerSec = 48000;
        for (DWORD bits = mask; bits; bits >>= 1) mix.Format.nChannels += bits & 1;
        mix.Format.wBitsPerSample = 32;
        mix.Format.nBlockAlign = mix.Format.nChannels * 4;
        mix.Format.nAvgBytesPerSec = mix.Format.nBlockAlign * 48000;
        mix.Format.cbSize = sizeof(mix) - sizeof(mix.Format);
        mix.Samples.wValidBitsPerSample = 32;
        mix.dwChannelMask = mask;
        mix.SubFormat = {3, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
        auto* parent = static_cast<vlc_object_t*>(vlc_object_create(static_cast<vlc_object_t*>(nullptr), sizeof(vlc_object_t)));
        require(parent != nullptr, "fake output parent");
        parent->obj.flags |= OBJECT_FLAGS_QUIET;
        var_Create(parent, "speakerfill-layout", VLC_VAR_INTEGER);
        var_SetInteger(parent, "speakerfill-layout", layout);
        auto* stream = static_cast<aout_stream_t*>(vlc_object_create(parent, sizeof(aout_stream_t)));
        require(stream != nullptr, "fake stream object");
        stream->owner.device = &device;
        stream->owner.activate = [](void* opaque, REFIID identifier, PROPVARIANT* parameters, void** output)
        { return static_cast<FakeDevice*>(opaque)->Activate(identifier, CLSCTX_ALL, parameters, output); };
        audio_sample_format_t format{};
        format.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
        format.i_format = VLC_CODEC_FL32;
        format.i_rate = 44100;
        format.i_physical_channels = scenario == 3 ? AOUT_CHANS_5_1 : AOUT_CHANS_2_0;
        aout_FormatPrepare(&format);
        const unsigned inputChannels = format.i_channels;
        const auto result = start(stream, &format, nullptr);
        if (scenario == 5)
        {
            require(FAILED(result), "initialization failure propagated");
            require(var_GetInteger(parent, "speakerfill-mask") == 0, "failed output publishes no layout");
        }
        else
        {
            require(SUCCEEDED(result), "upstream Start with fake device");
            DWORD requested = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY;
            uint32_t expected = AOUT_CHANS_2_0 | AOUT_CHAN_LFE;
            if (layout == 41 || layout == 51)
            {
                const bool rear = (mask & (SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT)) == (SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT);
                requested |= rear ? SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT : SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
                expected |= rear ? AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT : AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT;
                if (layout == 51) { requested |= SPEAKER_FRONT_CENTER; expected |= AOUT_CHAN_CENTER; }
            }
            else if (layout == 71) { requested = KSAUDIO_SPEAKER_7POINT1_SURROUND; expected = AOUT_CHANS_7_1; }
            const bool fill = scenario == 0 && (layout == 21 || layout == 41 || layout == 51 || layout == 71) &&
                (requested & mask) == requested;
            const unsigned requestedChannels = layout == 21 ? 3 : layout == 41 ? 5 : layout == 51 ? 6 : 8;
            require(device.client.channels == (fill ? requestedChannels : inputChannels),
                "only supported requested layout is negotiated");
            require(var_GetInteger(parent, "speakerfill-mask") == (fill ? expected : 0),
                "filter sees successful final negotiated mask, or bypass");
            require(SUCCEEDED(stop(stream)), "upstream Stop releases fake client");
        }
        require(device.client.refs == 1, "no leaked client on success or failure");
        vlc_object_release(stream);
        vlc_object_release(parent);
    }
}

static char* inheritedString(vlc_object_t* owner, const char* name)
{
    vlc_value_t value{};
    return var_Inherit(owner, name, VLC_VAR_STRING, &value) == VLC_SUCCESS ? value.psz_string : nullptr;
}

int main(int argc, char** argv)
{
    require(argc == 3, "filter and output module paths required");
    HMODULE filterModule = nullptr;
    HMODULE outputModule = nullptr;
    const auto descriptor = load(argv[1], filterModule);
    const auto outputDescriptor = load(argv[2], outputModule);
    const char* arguments[] = {"--no-plugins-cache", "--no-audio", "--no-video", "--quiet"};
    libvlc_instance_t* instance = libvlc_new(4, arguments);
    require(instance != nullptr, "initialize VLC module discovery without playback");
    require(module_exists("speakerfill") && module_exists("speakerfill_output"), "VLC discovers both packaged modules");
    for (const int layout : {0, 21, 41, 51, 71})
    {
        auto* media = libvlc_media_new_location(instance, "https://example.invalid/test");
        require(media != nullptr, "media option test input");
        libvlc_media_add_option(media, ":audio-filter=speakerfill");
        libvlc_media_add_option(media, ":speakerfill-layout=51");
        auto* player = libvlc_media_player_new_from_media(media);
        require(player != nullptr, "real media player for option inheritance");
        auto* owner = reinterpret_cast<vlc_object_t*>(player);
        char* before = inheritedString(owner, "audio-filter");
        require(!before || !*before, "per-media filter option does not configure the audio output owner");
        libvlc_free(before);
        require(configureSpeakerFill(player, layout), "configure actual production option owner");
        auto* child = static_cast<vlc_object_t*>(vlc_object_create(owner, sizeof(vlc_object_t)));
        require(child != nullptr, "audio-output inheritance probe");
        char* filterName = inheritedString(child, "audio-filter");
        require(layout ? filterName && strcmp(filterName, "speakerfill") == 0 : !filterName || !*filterName,
            "audio filter inherited from player only when enabled");
        libvlc_free(filterName);
        if (layout)
        {
            require(var_InheritInteger(child, "speakerfill-layout") == layout, "layout reaches output child");
            char* backend = inheritedString(child, "mmdevice-backend");
            require(backend && strcmp(backend, "speakerfill_output,none") == 0, "output variant reaches output child");
            libvlc_free(backend);
        }
        require(!configureSpeakerFill(player, 99), "invalid layout rejected");
        vlc_object_release(child);
        libvlc_media_player_release(player);
        libvlc_media_release(media);
    }
    negotiation(outputDescriptor);
    using Open = int (*)(vlc_object_t*);
    const auto open = reinterpret_cast<Open>(descriptor.open);
    const uint32_t layouts[] = {0, AOUT_CHANS_2_0, AOUT_CHANS_4_0,
        AOUT_CHANS_2_0 | AOUT_CHAN_LFE, AOUT_CHANS_4_0 | AOUT_CHAN_LFE,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT,
        AOUT_CHANS_5_1, AOUT_CHANS_5_0_MIDDLE | AOUT_CHAN_LFE, AOUT_CHANS_7_1};
    for (const uint32_t sourceMask : {uint32_t(AOUT_CHANS_2_0), uint32_t(AOUT_CHANS_5_1)})
    for (const auto mask : layouts)
    {
        auto* filter = static_cast<filter_t*>(vlc_object_create(static_cast<vlc_object_t*>(nullptr), sizeof(filter_t)));
        require(filter != nullptr, "allocate isolated VLC object");
        var_Create(filter, "speakerfill-mask", VLC_VAR_INTEGER);
        var_SetInteger(filter, "speakerfill-mask", mask);
        filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
        filter->fmt_in.audio.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
        filter->fmt_in.audio.i_rate = 48000;
        filter->fmt_in.audio.i_physical_channels = sourceMask;
        aout_FormatPrepare(&filter->fmt_in.audio);
        require(open(VLC_OBJECT(filter)) == VLC_SUCCESS, "open actual filter");
        const bool fill = sourceMask == AOUT_CHANS_2_0 && mask != 0 && mask != AOUT_CHANS_2_0;
        require(filter->fmt_out.audio.i_physical_channels == (fill ? mask : sourceMask), "negotiated layout or bypass");
        const unsigned frames = 128;
        block_t* input = block_Alloc(frames * filter->fmt_in.audio.i_channels * sizeof(float));
        require(input != nullptr, "allocate PCM block");
        input->i_nb_samples = frames;
        input->i_pts = 1234567;
        input->i_dts = 1234500;
        input->i_length = 2666;
        input->i_flags = BLOCK_FLAG_DISCONTINUITY;
        auto* samples = reinterpret_cast<float*>(input->p_buffer);
        for (size_t sample = 0; sample < input->i_buffer / sizeof(float); ++sample)
            samples[sample] = sample % 2 ? -.5f : 1.f;
        block_t* output = filter->pf_audio_filter(filter, input);
        require(output && output->i_nb_samples == frames && output->i_pts == 1234567 &&
            output->i_dts == 1234500 && output->i_length == 2666 && output->i_flags == BLOCK_FLAG_DISCONTINUITY,
            "timestamps, duration, flags and frame count preserved");
        if (!fill) require(output == input, "native source bypass has no allocation or gain change");
        else
        {
            unsigned channel = 0;
            for (unsigned index = 0; pi_vlc_chan_order_wg4[index]; ++index)
            {
                const auto label = pi_vlc_chan_order_wg4[index];
                if (!(mask & label)) continue;
                float expected = 0.f;
                if (label & (AOUT_CHAN_LEFT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_REARLEFT)) expected = .70710678f;
                else if (label & (AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARRIGHT)) expected = -.35355339f;
                else if (label == AOUT_CHAN_CENTER) expected = .176776695f;
                for (unsigned frame = 0; frame < frames; ++frame)
                    require(std::abs(reinterpret_cast<float*>(output->p_buffer)[frame * filter->fmt_out.audio.i_channels + channel] - expected) < 1.e-6f,
                        "correct main-speaker routing, headroom and silent LFE");
                ++channel;
            }
        }
        block_Release(output);
        vlc_object_release(filter);
    }
    FreeLibrary(outputModule);
    FreeLibrary(filterModule);
    libvlc_release(instance);
    std::puts("PASS: real VLC DLL ABI, explicit Stereo/2.1/4.1/5.1/7.1 negotiation, unsupported-device bypass, main-speaker routing, silent LFE and timing; no device opened");
    return 0;
}