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
#include <vector>
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

struct FakeClock final : IAudioClock
{
    UINT64 position = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetFrequency(UINT64* frequency) override { *frequency = 48000; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPosition(UINT64* value, UINT64* counter) override
    {
        *value = position;
        LARGE_INTEGER ticks, frequency;
        QueryPerformanceCounter(&ticks);
        QueryPerformanceFrequency(&frequency);
        if (counter) *counter = (ticks.QuadPart / frequency.QuadPart) * 10000000 +
            (ticks.QuadPart % frequency.QuadPart) * 10000000 / frequency.QuadPart;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCharacteristics(DWORD* value) override { *value = 0; return S_OK; }
};

struct FakeRenderer final : IAudioRenderClient
{
    std::vector<BYTE> buffer;
    HRESULT result = S_OK;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT32 frames, BYTE** output) override
    { buffer.resize(static_cast<size_t>(frames) * 8 * sizeof(float)); *output = buffer.data(); return S_OK; }
    HRESULT STDMETHODCALLTYPE ReleaseBuffer(UINT32, DWORD) override { return result; }
};

struct FakeClient final : IAudioClient
{
    FakeClock clock;
    FakeRenderer renderer;
    UINT32 bufferFrames = 2048;
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
    HRESULT STDMETHODCALLTYPE GetBufferSize(UINT32* frames) override { *frames = bufferFrames; return S_OK; }
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
    HRESULT STDMETHODCALLTYPE GetService(REFIID id, void** output) override
    {
        if (id == __uuidof(IAudioClock)) { *output = &clock; return S_OK; }
        if (id == __uuidof(IAudioRenderClient)) { *output = &renderer; return S_OK; }
        return E_NOINTERFACE;
    }
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

static void playbackFence(const Descriptor& descriptor)
{
    auto* parent = static_cast<vlc_object_t*>(vlc_object_create(static_cast<vlc_object_t*>(nullptr), sizeof(vlc_object_t)));
    require(parent != nullptr, "playback fence owner");
    parent->obj.flags |= OBJECT_FLAGS_QUIET;
    for (const char* name : {"music-fade-command", "music-clock-pts", "music-clock-epoch", "music-clock-error", "speakerfill-layout"})
    { var_Create(parent, name, VLC_VAR_INTEGER); var_SetInteger(parent, name, 0); }
    auto* stream = static_cast<aout_stream_t*>(vlc_object_create(parent, sizeof(aout_stream_t)));
    require(stream != nullptr, "playback fence stream");
    FakeDevice device;
    device.properties.form = Headphones;
    device.client.bufferFrames = 48000;
    stream->owner.device = &device;
    stream->owner.activate = [](void* opaque, REFIID id, PROPVARIANT* parameters, void** output)
    { return static_cast<FakeDevice*>(opaque)->Activate(id, CLSCTX_ALL, parameters, output); };
    audio_sample_format_t format{};
    format.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    format.i_format = VLC_CODEC_FL32;
    format.i_rate = 48000;
    format.i_physical_channels = AOUT_CHANS_2_0;
    aout_FormatPrepare(&format);
    using Start = HRESULT (*)(aout_stream_t*, audio_sample_format_t*, const GUID*);
    using Stop = HRESULT (*)(aout_stream_t*);
    require(SUCCEEDED(reinterpret_cast<Start>(descriptor.open)(stream, &format, nullptr)), "start playback fence output");
    auto* block = block_Alloc(48000 * 2 * sizeof(float));
    require(block != nullptr, "playback fence block");
    memset(block->p_buffer, 0, block->i_buffer);
    block->i_nb_samples = 48000;
    block->i_pts = 10000000;
    block->i_length = 1000000;
    require(SUCCEEDED(stream->play(stream, block)), "native output submits complete block");
    vlc_tick_t delay = 0;
    device.client.clock.position = 24000;
    require(SUCCEEDED(stream->time_get(stream, &delay)), "native delay query");
    const auto played = var_GetInteger(parent, "music-clock-pts");
    require(played >= 10500000 && played < 10750000, "reported playback excludes queued half second");
    device.client.clock.position = 96000;
    require(SUCCEEDED(stream->time_get(stream, &delay)) && var_GetInteger(parent, "music-clock-pts") == 11000000,
        "negative queued delay completes only the submitted tail, never extrapolates beyond it");
    const auto epoch = var_GetInteger(parent, "music-clock-epoch");
    require(SUCCEEDED(stream->flush(stream)), "native flush");
    require(var_GetInteger(parent, "music-clock-pts") == 0 && var_GetInteger(parent, "music-clock-epoch") > epoch,
        "flush invalidates old position and epoch");
    device.client.renderer.result = E_FAIL;
    block = block_Alloc(2 * sizeof(float));
    require(block != nullptr, "failed submission block");
    block->i_nb_samples = 1;
    block->i_pts = 12000000;
    block->i_length = 20;
    require(FAILED(stream->play(stream, block)) && var_GetInteger(parent, "music-clock-error") == 1,
        "failed ReleaseBuffer cannot report playback success");
    require(SUCCEEDED(reinterpret_cast<Stop>(descriptor.close)(stream)), "stop playback fence output");
    require(device.client.refs == 1, "playback fence releases client");
    vlc_object_release(stream);
    vlc_object_release(parent);
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
        require(configureMusicGain(player, layout, .125f), "configure float music gain on real player");
        require(var_InheritBool(child, "speakerfill-smooth-gain"), "gain smoothing inherited by filter");
        require(std::abs(var_InheritFloat(child, "speakerfill-gain") - .001953125f) < 1.e-8f,
            "VLC cubic volume curve retained without percentage rounding");
        require(setMusicGain(player, .105f), "fractional music target accepted");
        require(std::abs(var_InheritFloat(child, "speakerfill-gain") - .001157625f) < 1.e-8f,
            "float target reaches filter owner");
        require(!setMusicGain(player, -1.f), "invalid gain rejected");
        require(configureMusicFade(player, 1.f), "configure native fade timing");
        require(setMusicFade(player, 0.f, 3.f, 9), "publish full fade command");
        const auto fadeCommand = var_GetInteger(owner, "music-fade-command");
        require(setMusicGain(player, .25f, true) && var_GetBool(owner, "music-hard-mute"), "hard mute stored separately");
        require(var_GetInteger(owner, "music-fade-command") == fadeCommand, "volume and mute do not retarget transition");
        require(!musicFadeComplete(player, 9), "publication is not completion");
        var_SetInteger(owner, "music-clock-epoch", 4);
        var_SetInteger(owner, "music-fade-epoch", 4);
        var_SetInteger(owner, "music-fade-fence", 5000000);
        var_SetInteger(owner, "music-fade-completed", fadeCommand);
        var_SetInteger(owner, "music-clock-pts", 4999999);
        require(!musicFadeComplete(player, 9), "rendered fade waits for VLC playback position");
        var_SetInteger(owner, "music-clock-pts", 5000000);
        require(musicFadeComplete(player, 9) && !musicFadeComplete(player, 8), "only matching played fade completes");
        var_SetInteger(owner, "music-clock-epoch", 5);
        require(!musicFadeComplete(player, 9), "flush cancels stale playback fence");
        var_SetInteger(owner, "music-clock-error", 1);
        require(musicOutputFailed(player) && !musicFadeComplete(player, 9), "output failure cannot complete fade");
        vlc_object_release(child);
        libvlc_media_player_release(player);
        libvlc_media_release(media);
    }
    negotiation(outputDescriptor);
    playbackFence(outputDescriptor);
    using Open = int (*)(vlc_object_t*);
    const auto open = reinterpret_cast<Open>(descriptor.open);
    using Close = void (*)(vlc_object_t*);
    const auto close = reinterpret_cast<Close>(descriptor.close);
    require(close != nullptr, "filter cleanup entry");
    for (const unsigned partition : {37u, 4096u})
    {
        auto* filter = static_cast<filter_t*>(vlc_object_create(static_cast<vlc_object_t*>(nullptr), sizeof(filter_t)));
        require(filter != nullptr, "duration filter");
        for (const char* name : {"speakerfill-mask", "music-fade-command", "music-fade-fence", "music-fade-epoch", "music-fade-completed", "music-clock-epoch"})
        { var_Create(filter, name, VLC_VAR_INTEGER); var_SetInteger(filter, name, 0); }
        var_Create(filter, "speakerfill-smooth-gain", VLC_VAR_BOOL);
        var_SetBool(filter, "speakerfill-smooth-gain", true);
        var_Create(filter, "speakerfill-gain", VLC_VAR_FLOAT);
        var_SetFloat(filter, "speakerfill-gain", .5f);
        var_Create(filter, "music-fade-initial", VLC_VAR_FLOAT);
        var_SetFloat(filter, "music-fade-initial", 1.f);
        var_SetInteger(filter, "music-clock-epoch", 7);
        filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
        filter->fmt_in.audio.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
        filter->fmt_in.audio.i_rate = 48000;
        filter->fmt_in.audio.i_physical_channels = AOUT_CHANS_2_0;
        aout_FormatPrepare(&filter->fmt_in.audio);
        require(open(VLC_OBJECT(filter)) == VLC_SUCCESS, "open duration filter");
        const uint64_t command = (uint64_t{1} << 32) | (uint64_t{3000} << 16);
        var_SetInteger(filter, "music-fade-command", command);
        for (unsigned position = 0; position < 144001;)
        {
            const auto frames = partition < 144001 - position ? partition : 144001 - position;
            auto* block = block_Alloc(frames * 2 * sizeof(float));
            require(block != nullptr, "duration block");
            block->i_nb_samples = frames;
            block->i_pts = 1000000 + static_cast<int64_t>(position) * 1000000 / 48000;
            auto* samples = reinterpret_cast<float*>(block->p_buffer);
            for (unsigned sample = 0; sample < frames * 2; ++sample) samples[sample] = 1.f;
            block = filter->pf_audio_filter(filter, block);
            require(block != nullptr, "duration output");
            for (unsigned frame = 0; frame < frames; ++frame)
                require(std::abs(samples[frame * 2] - .5f * (1.f - static_cast<float>(position + frame) / 144000.f)) < 1.e-6f,
                    "one command produces full three-second gradient without viewer updates");
            position += frames;
            require((var_GetInteger(filter, "music-fade-completed") != 0) == (position == 144001),
                "completion only after final zero sample");
            block_Release(block);
        }
        require(var_GetInteger(filter, "music-fade-completed") == static_cast<int64_t>(command), "matching fade completion");
        require(std::abs(var_GetInteger(filter, "music-fade-fence") - 4000020) <= 1, "scheduled final-sample fence");
        require(var_GetInteger(filter, "music-fade-epoch") == 7, "completion bound to output epoch");
        filter->pf_flush(filter);
        require(var_GetInteger(filter, "music-fade-completed") == 0, "flush invalidates unplayed completion");
        close(VLC_OBJECT(filter));
        vlc_object_release(filter);
    }
    for (const unsigned channels : {2u, 6u, 8u})
    for (const unsigned partition : {1u, 37u, 512u})
    {
        auto* filter = static_cast<filter_t*>(vlc_object_create(static_cast<vlc_object_t*>(nullptr), sizeof(filter_t)));
        require(filter != nullptr, "gain test filter");
        var_Create(filter, "speakerfill-mask", VLC_VAR_INTEGER);
        var_SetInteger(filter, "speakerfill-mask", 0);
        var_Create(filter, "speakerfill-smooth-gain", VLC_VAR_BOOL);
        var_SetBool(filter, "speakerfill-smooth-gain", true);
        var_Create(filter, "speakerfill-gain", VLC_VAR_FLOAT);
        var_SetFloat(filter, "speakerfill-gain", 1.f);
        filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
        filter->fmt_in.audio.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
        filter->fmt_in.audio.i_rate = 48000;
        filter->fmt_in.audio.i_physical_channels = channels == 2 ? AOUT_CHANS_2_0 : channels == 6 ? AOUT_CHANS_5_1 : AOUT_CHANS_7_1;
        aout_FormatPrepare(&filter->fmt_in.audio);
        require(open(VLC_OBJECT(filter)) == VLC_SUCCESS, "open gain filter");
        var_SetFloat(filter, "speakerfill-gain", .005f);
        for (unsigned position = 0; position < 2600;)
        {
            const unsigned frames = partition < 2600 - position ? partition : 2600 - position;
            auto* block = block_Alloc(frames * channels * sizeof(float));
            require(block != nullptr, "gain block");
            block->i_nb_samples = frames;
            auto* samples = reinterpret_cast<float*>(block->p_buffer);
            for (unsigned sample = 0; sample < frames * channels; ++sample) samples[sample] = .5f;
            auto* output = filter->pf_audio_filter(filter, block);
            require(output == block, "gain processing is in place");
            for (unsigned frame = 0; frame < frames; ++frame)
            {
                const float expected = .5f * (position + frame >= 2400 ? .005f :
                    1.f + (.005f - 1.f) * static_cast<float>(position + frame) / 2400.f);
                for (unsigned channel = 0; channel < channels; ++channel)
                    require(std::abs(samples[frame * channels + channel] - expected) < 3.e-5f,
                        "continuous per-sample gain with sub-percent target and partition-independent duration");
            }
            block_Release(output);
            position += frames;
        }
        var_SetFloat(filter, "speakerfill-gain", 1.f);
        auto* rising = block_Alloc(1200 * channels * sizeof(float));
        require(rising != nullptr, "retarget block");
        rising->i_nb_samples = 1200;
        auto* risingSamples = reinterpret_cast<float*>(rising->p_buffer);
        for (unsigned sample = 0; sample < 1200 * channels; ++sample) risingSamples[sample] = 1.f;
        rising = filter->pf_audio_filter(filter, rising);
        require(rising && std::abs(risingSamples[0] - .005f) < 1.e-6f, "rise starts from current gain");
        block_Release(rising);
        var_SetFloat(filter, "speakerfill-gain", 0.f);
        auto* falling = block_Alloc(2401 * channels * sizeof(float));
        require(falling != nullptr, "zero target block");
        falling->i_nb_samples = 2401;
        auto* fallingSamples = reinterpret_cast<float*>(falling->p_buffer);
        for (unsigned sample = 0; sample < 2401 * channels; ++sample) fallingSamples[sample] = 1.f;
        falling = filter->pf_audio_filter(filter, falling);
        require(falling && std::abs(fallingSamples[0] - .5025f) < 3.e-5f,
            "mid-ramp retarget preserves current sample gain");
        for (unsigned channel = 0; channel < channels; ++channel)
            require(fallingSamples[2400 * channels + channel] == 0.f, "zero endpoint is exact on every channel");
        block_Release(falling);
        close(VLC_OBJECT(filter));
        var_SetFloat(filter, "speakerfill-gain", .125f);
        require(open(VLC_OBJECT(filter)) == VLC_SUCCESS, "reopen gain filter");
        auto* first = block_Alloc(channels * sizeof(float));
        require(first != nullptr, "initial gain block");
        first->i_nb_samples = 1;
        auto* firstSamples = reinterpret_cast<float*>(first->p_buffer);
        for (unsigned channel = 0; channel < channels; ++channel) firstSamples[channel] = 1.f;
        first = filter->pf_audio_filter(filter, first);
        require(first && firstSamples[0] == .125f, "startup applies initial gain without full-volume transient");
        block_Release(first);
        close(VLC_OBJECT(filter));
        vlc_object_release(filter);
    }
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
        var_Create(filter, "speakerfill-smooth-gain", VLC_VAR_BOOL);
        var_SetBool(filter, "speakerfill-smooth-gain", false);
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
        close(VLC_OBJECT(filter));
        vlc_object_release(filter);
    }
    FreeLibrary(outputModule);
    FreeLibrary(filterModule);
    libvlc_release(instance);
    std::puts("PASS: VLC module ABI, layouts, sample gain, full-duration fades, independent controls and native playback fences; no device opened");
    return 0;
}