#include "../llvlcaudio.h"
#include "../llvlcendpoint.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <malloc.h>
#include <new>
#include <thread>
#include <vector>

using namespace llvlc;

thread_local bool inRender = false;
std::atomic<std::uint64_t> renderAllocations{0};
std::atomic<std::size_t> allocatedBytes{0};

void* operator new(std::size_t bytes)
{
    if (inRender) ++renderAllocations;
    allocatedBytes.fetch_add(bytes);
    if (void* memory = std::malloc(bytes ? bytes : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new(std::size_t bytes, std::align_val_t alignment)
{
    if (inRender) ++renderAllocations;
    allocatedBytes.fetch_add(bytes);
    if (void* memory = _aligned_malloc(bytes ? bytes : 1, static_cast<std::size_t>(alignment))) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes, std::align_val_t alignment) { return ::operator new(bytes, alignment); }
void operator delete(void* memory, std::align_val_t) noexcept { _aligned_free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { _aligned_free(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { _aligned_free(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { _aligned_free(memory); }

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void near(float actual, float expected, const char* message, float tolerance = 2.e-6f)
{
    if (std::abs(actual - expected) > tolerance || !std::isfinite(actual))
    {
        std::fprintf(stderr, "%s: actual %.9g expected %.9g\n", message, actual, expected);
        check(false, message);
    }
}

Format layout(std::uint32_t channels, std::uint32_t rate = 48000)
{
    Format result;
    result.channels = channels;
    result.sampleRate = rate;
    if (channels == 1) result.speakers = {Speaker::Mono};
    if (channels == 6) result.speakers = {Speaker::FrontLeft, Speaker::FrontRight,
        Speaker::FrontCenter, Speaker::LFE, Speaker::BackLeft, Speaker::BackRight};
    if (channels == 8) result.speakers = {Speaker::FrontLeft, Speaker::FrontRight,
        Speaker::FrontCenter, Speaker::LFE, Speaker::BackLeft, Speaker::BackRight,
        Speaker::SideLeft, Speaker::SideRight};
    return result;
}

Options headless(const Format& output = {})
{
    Options options;
    options.output = OutputMode::Headless;
    options.headlessOutput = output;
    options.reserveMilliseconds = 1;
    options.primeMilliseconds = 1;
    return options;
}

void render(AudioEngine& engine, float* output, std::uint32_t frames)
{
    inRender = true;
    const auto result = engine.render(output, frames);
    inRender = false;
    check(result == Result::Ok, "headless render result");
}

void enqueue(AudioEngine& engine, const std::vector<std::int16_t>& samples,
             std::uint32_t channels, std::int64_t pts = 1000000, Generation generation = {1, 1})
{
    inRender = true;
    const auto result = engine.enqueuePCM(samples.data(), static_cast<std::uint32_t>(samples.size() / channels),
                                          pts, generation);
    inRender = false;
    check(result == Result::Ok, "enqueue PCM");
}

void testIdentity()
{
    for (const auto channels : {2u, 6u, 8u})
    {
        AudioEngine engine;
        const auto format = layout(channels);
        check(engine.configure(Role::Music, format, {1, 1}, headless(format)) == Result::Ok, "identity configure");
        std::vector<std::int16_t> input(1024 * channels);
        for (std::uint32_t frame = 0; frame < 1024; ++frame)
            for (std::uint32_t channel = 0; channel < channels; ++channel)
                input[frame * channels + channel] = static_cast<std::int16_t>((1023 - frame) * (channel + 1) * 3);
        enqueue(engine, input, channels);
        check(engine.drain() == Result::Ok, "identity drain");
        std::vector<float> output(input.size());
        render(engine, output.data(), 1024);
        for (std::size_t sample = 0; sample < output.size(); ++sample)
            near(output[sample], static_cast<float>(input[sample]) / 32768.f, "encoded fade preserved", 0.f);
        check(engine.status().state == State::Drained, "identity EOS state");
        check(engine.status().renderedFrames == 1024, "identity exact frame count");
    }
    std::puts("PASS: 2/6/8 identity preserves encoded sample fades exactly");
}

std::vector<float> envelopeRun(std::uint32_t channels, std::uint32_t partition, float target)
{
    AudioEngine engine;
    const auto format = layout(channels);
    check(engine.configure(Role::Music, format, {1, 1}, headless(format)) == Result::Ok, "envelope configure");
    check(engine.gain(target, .01) == Result::Ok, "duration target");
    std::vector<float> beforePCM(1200 * channels, -1.f);
    render(engine, beforePCM.data(), 1200);
    check(std::all_of(beforePCM.begin(), beforePCM.end(), [](float value) { return value == 0.f; }), "no fake PCM");
    near(engine.status().currentGain, 1.f, "gain frozen without PCM");
    std::vector<std::int16_t> input(2048 * channels, 16384);
    enqueue(engine, input, channels);
    check(engine.drain() == Result::Ok, "envelope drain");
    std::vector<float> output(input.size());
    std::uint32_t position = 0;
    while (position < 2048)
    {
        const auto count = std::min(partition, 2048 - position);
        render(engine, output.data() + position * channels, count);
        position += count;
    }
    near(output[0], .5f, "first actual PCM starts fade");
    near(output[480 * channels], .5f * target, "exact target endpoint");
    for (std::uint32_t frame = 0; frame < 2048; ++frame)
    {
        const float expected = .5f * (frame >= 480 ? target :
            1.f + (target - 1.f) * static_cast<float>(frame) / 480.f);
        for (std::uint32_t channel = 0; channel < channels; ++channel)
            near(output[frame * channels + channel], expected, "sample-index gain");
    }
    return output;
}

void testEnvelopes()
{
    for (const auto channels : {2u, 6u, 8u})
        for (const float target : {.1f, .01f})
        {
            const auto reference = envelopeRun(channels, 1, target);
            check(reference == envelopeRun(channels, 37, target), "37-frame envelope partition invariance");
            check(reference == envelopeRun(channels, 2048, target), "large envelope partition invariance");
        }
    std::puts("PASS: 2/6/8 envelopes, 0.1/0.01 endpoints, first-PCM timing, partition invariance");
}

void testRetargetMutePause()
{
    AudioEngine engine;
    check(engine.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "retarget configure");
    std::vector<std::int16_t> input(4096 * 2, 16384);
    enqueue(engine, input, 2);
    std::vector<float> output(1000 * 2);
    check(engine.gain(0.f, .01) == Result::Ok, "initial fade");
    render(engine, output.data(), 120);
    near(engine.status().currentGain, .75f, "fade current sample");
    check(engine.gain(.1f, .005) == Result::Ok, "retarget .1");
    render(engine, output.data(), 60);
    near(output[0], .375f, "continuous retarget origin");
    near(engine.status().currentGain, .5875f, "retarget phase");
    check(engine.gain(.01f, .0025) == Result::Ok, "rapid retarget .01");
    check(engine.pause() == Result::Ok, "pause command");
    const auto consumed = engine.status().renderedFrames;
    render(engine, output.data(), 700);
    check(engine.status().state == State::Paused, "paused state");
    check(engine.status().renderedFrames == consumed, "pause holds source position");
    check(std::all_of(output.begin(), output.begin() + 1400, [](float value) { return value == 0.f; }), "pause silence");
    check(engine.resume() == Result::Ok, "resume command");
    render(engine, output.data(), 121);
    near(output[0], .29375f, "resume continuous gain");
    near(output[240], .005f, "rapid retarget endpoint");
    check(engine.gain(1.f, 1.) == Result::Ok, "pending normal gain");
    check(engine.gain(0.f, 1., true) == Result::Ok, "hard mute overrides pending command");
    render(engine, output.data(), 13);
    for (std::uint32_t sample = 0; sample < 26; ++sample) near(output[sample], 0.f, "prompt hard mute", 0.f);
    check(engine.status().hardMuted, "mute latched");
    const auto unmuteOrigin = engine.status().currentGain;
    check(engine.gain(.1f, .001) == Result::Ok, "unmute target");
    render(engine, output.data(), 49);
    near(output[0], .5f * unmuteOrigin, "mute does not reset gain envelope");
    near(output[96], .05f, "unmute duration endpoint");
    check(!engine.status().hardMuted, "unmute acknowledgement");
    std::puts("PASS: rapid continuous retarget, pause/resume phase, hard mute and timed unmute");
}

std::vector<float> starvationRun(std::uint32_t partition)
{
    AudioEngine engine;
    check(engine.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "starve configure");
    std::vector<std::int16_t> input(192 * 2, 16384);
    enqueue(engine, input, 2);
    std::vector<float> output(384 * 2);
    for (std::uint32_t position = 0; position < 384;)
    {
        const auto count = std::min(partition, 384 - position);
        render(engine, output.data() + position * 2, count);
        position += count;
    }
    for (std::uint32_t frame = 0; frame < 384; ++frame)
    {
        const float expected = frame < 144 ? .5f : frame < 192 ?
            .5f * static_cast<float>(191 - frame) / 47.f : 0.f;
        near(output[frame * 2], expected, "retained PCM starvation ramp");
    }
    check(engine.status().starvationCount == 1, "one starvation transition");
    check(engine.status().state == State::Priming, "waiting for reprime");
    std::vector<std::int16_t> partial(80 * 2, 16384);
    enqueue(engine, partial, 2, 1004000);
    float waiting[40]{};
    render(engine, waiting, 20);
    for (float sample : waiting) near(sample, 0.f, "incomplete reprime remains silent", 0.f);
    std::vector<std::int16_t> rest(112 * 2, 16384);
    enqueue(engine, rest, 2, 1005666);
    float recovery[160]{};
    render(engine, recovery, 80);
    near(recovery[0], 0.f, "recovery begins from zero");
    near(recovery[48], .25f, "recovery midpoint");
    near(recovery[96], .5f, "recovery endpoint");
    return output;
}

void testStarvation()
{
    check(starvationRun(1) == starvationRun(97), "starvation callback partition invariance");
    AudioEngine engine;
    check(engine.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "silence configure");
    std::vector<std::int16_t> silence(1024 * 2, 0);
    enqueue(engine, silence, 2);
    float output[512]{};
    render(engine, output, 256);
    check(engine.status().state == State::Playing && engine.status().starvationCount == 0,
          "decoded silence is not starvation");
    std::puts("PASS: unsubmitted reserve ramp, silence, reprime recovery, quiet PCM, partition invariance");
}

void testBoundsGenerations()
{
    AudioEngine engine;
    std::vector<std::int16_t> input(256 * 2, 16384);
    check(engine.enqueuePCM(input.data(), 1, 0, {1, 1}) == Result::NotConfigured, "unconfigured enqueue");
    auto options = headless();
    options.queueFrames = 256;
    check(engine.configure(Role::Music, {}, {1, 1}, options) == Result::Ok, "bounded configure");
        check(engine.start() == Result::Ok && !engine.status().deviceStarted &&
            engine.status().outputMode == OutputMode::Headless, "headless is not a physical device");
    enqueue(engine, input, 2);
    check(engine.enqueuePCM(input.data(), 1, 1005333, {1, 1}) == Result::QueueFull, "full queue all-or-none");
    check(engine.enqueuePCM(input.data(), 1, 0, {1, 2}) == Result::StaleGeneration, "stale format");
    check(engine.enqueuePCM(input.data(), 257, 0, {1, 1}) == Result::InvalidArgument, "oversize block");
    check(engine.enqueuePCM(nullptr, 1, 0, {1, 1}) == Result::InvalidArgument, "null PCM");
    check(engine.enqueuePCM(input.data(), 0, 0, {1, 1}) == Result::InvalidArgument, "zero frames");
    check(engine.enqueuePCM(input.data(), 1, std::numeric_limits<std::int64_t>::max(), {1, 1}) == Result::InvalidArgument,
          "PTS overflow bound");
    check(engine.pause() == Result::Ok, "pause before flush");
    check(engine.flush({2, 1}) == Result::Ok, "generation flush");
    float output[1024]{};
    render(engine, output, 0);
    check(engine.status().queuedFrames == 0 && engine.status().reservedFrames == 0, "paused flush reclaims stale PCM");
    check(engine.status().renderedGeneration.stream == 2, "flush acknowledgement");
    check(engine.enqueuePCM(input.data(), 1, 0, {1, 1}) == Result::StaleGeneration, "old stream rejected");
    check(engine.flush({2, 1}) == Result::StaleGeneration, "no generation reuse");
    check(engine.flush({3, 2}) == Result::StaleGeneration, "format requires configure");
    std::vector<std::int16_t> shortInput(7 * 2, -16384);
    enqueue(engine, shortInput, 2, 5000000, {2, 1});
    check(engine.enqueuePCM(shortInput.data(), 7, 6000000, {2, 1}) == Result::PtsDiscontinuity, "discontinuous PTS rejected");
    check(engine.resume() == Result::Ok && engine.drain() == Result::Ok, "short EOS below prime threshold");
    check(engine.enqueuePCM(shortInput.data(), 7, 5000145, {2, 1}) == Result::EndOfStream, "EOS rejects new PCM");
    render(engine, output, 32);
    for (std::uint32_t frame = 0; frame < 32; ++frame)
        near(output[frame * 2], frame < 7 ? -.5f : 0.f, "flush content isolation and short drain", 0.f);
    check(engine.status().state == State::Drained && engine.status().renderedFrames == 7, "short drain exact count");
    check(engine.status().lastSourcePts == 5000125, "last emitted source PTS");
    check(engine.flush({3, 1}) == Result::Ok, "flush reopens after EOS");
    render(engine, output, 0);
    for (std::uint32_t index = 0; index < 64; ++index)
        check(engine.gain(.1f, 1.) == Result::Ok, "bounded command enqueue");
    check(engine.gain(.01f, 1.) == Result::ControlQueueFull, "control overflow explicit");
    check(engine.flush({4, 1}) == Result::ControlQueueFull, "full command flush rejected transactionally");
    check(engine.status().generation.stream == 3, "failed flush does not publish generation");
    check(engine.gain(0.f, 1., true) == Result::Ok, "hard mute bypasses command capacity");
    render(engine, output, 0);
    check(engine.stop() == Result::Ok, "headless stop");
    check(engine.enqueuePCM(input.data(), 1, 0, {3, 1}) == Result::Stopped, "stop rejects producer");
    render(engine, output, 32);
    for (std::uint32_t sample = 0; sample < 64; ++sample) near(output[sample], 0.f, "stopped output", 0.f);
    engine.close();
    check(engine.status().state == State::Closed, "closed state");
    check(engine.render(output, 1) == Result::NotConfigured, "closed render");
    check(engine.drain() == Result::NotConfigured, "closed drain cannot report stale success");
    check(engine.configure(Role::Music, {}, {3, 1}, options) == Result::StaleGeneration, "configure forbids token reuse");
    check(engine.configure(Role::Music, {}, {4, 2}, options) == Result::Ok, "configure new stream and format epoch");
    check(engine.enqueuePCM(input.data(), 1, 0, {3, 1}) == Result::StaleGeneration, "old format callback rejected after configure");
    std::puts("PASS: queue/command bounds, PTS checks, generation flush, paused reclamation, drain, stop/close");
}

void testValidation()
{
    AudioEngine engine;
    auto format = layout(2);
    format.channels = 9;
    check(engine.configure(Role::Music, format, {1, 1}, headless()) == Result::InvalidFormat, "max channels");
    format = layout(2, 0);
    check(engine.configure(Role::Music, format, {1, 1}, headless()) == Result::InvalidFormat, "zero rate");
    format = layout(2);
    format.speakers[1] = Speaker::Unknown;
    check(engine.configure(Role::Music, format, {1, 1}, headless()) == Result::InvalidFormat, "unknown labels");
    format.speakers[1] = Speaker::FrontLeft;
    check(engine.configure(Role::Music, format, {1, 1}, headless()) == Result::InvalidFormat, "duplicate labels");
    check(engine.configure(Role::Music, layout(6), {1, 1}, headless()) == Result::UnsupportedLayout, "no silent multichannel loss");
    check(engine.configure(Role::Object, layout(6), {1, 1}, headless(layout(6))) == Result::UnsupportedLayout, "explicit object channel limit");
    auto options = headless();
    options.queueFrames = MaxQueueFrames + 1;
    check(engine.configure(Role::Music, {}, {1, 1}, options) == Result::InvalidArgument, "queue allocation bound");
    options = headless();
    options.reserveMilliseconds = 0;
    check(engine.configure(Role::Music, {}, {1, 1}, options) == Result::InvalidArgument, "reserve required");
    check(engine.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "reconfigure after invalid config");
    check(engine.gain(std::numeric_limits<float>::quiet_NaN(), 1.) == Result::InvalidArgument, "NaN gain");
    check(engine.gain(1.f, std::numeric_limits<double>::infinity()) == Result::InvalidArgument, "infinite duration");
    check(engine.gain(1.1f, 1.) == Result::InvalidArgument, "gain headroom bound");
    check(engine.gain(.1f, -1.) == Result::InvalidArgument, "negative duration");
    check(engine.spatialDirection(0.f, 1.f) == Result::InvalidArgument, "music is nonpositional");
    options = headless(layout(2, 192000));
    options.primeMilliseconds = 20;
    options.reserveMilliseconds = 5;
    check(engine.configure(Role::Music, {}, {2, 2}, options) == Result::Ok, "default buffering at 192 kHz");
    std::puts("PASS: formats, labels, limits and invalid control arguments reject explicitly");
}

void testChannelMaps()
{
    for (const auto channels : {6u, 8u})
    {
        const auto outputFormat = layout(channels);
        auto sourceFormat = outputFormat;
        sourceFormat.speakers = channels == 6 ? std::array<Speaker, MaxChannels>{Speaker::FrontLeft,
            Speaker::FrontRight, Speaker::BackLeft, Speaker::BackRight, Speaker::FrontCenter, Speaker::LFE} :
            std::array<Speaker, MaxChannels>{Speaker::FrontLeft, Speaker::FrontRight, Speaker::SideLeft,
            Speaker::SideRight, Speaker::BackLeft, Speaker::BackRight, Speaker::FrontCenter, Speaker::LFE};
        for (std::uint32_t impulse = 0; impulse < channels; ++impulse)
        {
            AudioEngine engine;
            check(engine.configure(Role::Music, sourceFormat, {1, 1}, headless(outputFormat)) == Result::Ok, "WG4 configure");
            std::vector<std::int16_t> input(channels, 0);
            input[impulse] = 16384;
            enqueue(engine, input, channels);
            check(engine.drain() == Result::Ok, "impulse drain");
            float output[MaxChannels]{};
            render(engine, output, 1);
            for (std::uint32_t channel = 0; channel < channels; ++channel)
                near(output[channel], outputFormat.speakers[channel] == sourceFormat.speakers[impulse] ? .5f : 0.f,
                     "labelled WG4 to Windows permutation", 0.f);
        }
        for (const auto sourceChannels : {1u, 2u})
        {
            AudioEngine engine;
            check(engine.configure(Role::Music, layout(sourceChannels), {1, 1}, headless(outputFormat)) == Result::Ok,
                  "music upmix configure");
            std::vector<std::int16_t> input(sourceChannels, 16384);
            if (sourceChannels == 2) input[1] = 8192;
            enqueue(engine, input, sourceChannels);
            check(engine.drain() == Result::Ok, "upmix drain");
            float output[MaxChannels]{};
            render(engine, output, 1);
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                float expected = sourceChannels == 1 || channel % 2 == 0 ? .5f : .25f;
                if (channel == 2 && sourceChannels == 2) expected = .375f;
                if (channel == 3) expected = 0.f;
                near(output[channel], expected * .70710678f, "all mains upmix and LFE exclusion");
            }
        }
    }
    auto permuted = layout(8);
    std::swap(permuted.speakers[0], permuted.speakers[7]);
    AudioEngine engine;
    check(engine.configure(Role::Music, layout(8), {1, 1}, headless(permuted)) == Result::Ok, "nonstandard device order");
    std::vector<std::int16_t> input(8, 0);
    input[0] = 16384;
    enqueue(engine, input, 8);
    check(engine.drain() == Result::Ok, "permuted drain");
    float output[8]{};
    render(engine, output, 1);
    near(output[7], .5f, "actual device map honored", 0.f);
    near(output[0], 0.f, "not count based", 0.f);
    auto reversed = layout(2);
    std::swap(reversed.speakers[0], reversed.speakers[1]);
    check(engine.configure(Role::Music, reversed, {2, 2}, headless(layout(8))) == Result::Ok, "labelled reversed stereo");
    const std::int16_t reversedInput[2] = {0, 16384};
    check(engine.enqueuePCM(reversedInput, 1, 0, {2, 2}) == Result::Ok, "reversed stereo input");
    check(engine.drain() == Result::Ok, "reversed stereo drain");
    render(engine, output, 1);
    near(output[0], .5f * .70710678f, "left source is identified by label");
    near(output[4], output[0], "upmix reversed stereo back left");
    near(output[6], output[0], "upmix reversed stereo side left");
    near(output[1], 0.f, "right source is identified by label", 0.f);
    std::puts("PASS: WG4 6/8 impulses, authored LFE, actual output order, mono/stereo all-mains upmix");
}

void testSpatial()
{
    for (const auto channels : {2u, 6u, 8u})
        for (const auto sourceChannels : {1u, 2u})
        {
            AudioEngine engine;
            auto format = layout(channels);
            std::reverse(format.speakers.begin(), format.speakers.begin() + channels);
            check(engine.configure(Role::Object, layout(sourceChannels), {1, 1}, headless(format)) == Result::Ok,
                  "object configure");
            check(engine.spatialDirection(1.f, 1.f) == Result::InvalidArgument, "normalized direction required");
            check(engine.spatialDirection(-1.f, 0.f) == Result::Ok, "left spatial direction");
            std::vector<std::int16_t> input(128 * sourceChannels, 32767);
            enqueue(engine, input, sourceChannels);
            check(engine.gain(.25f, 0.) == Result::Ok, "single external attenuation");
            check(engine.drain() == Result::Ok, "spatial drain");
            float output[MaxChannels]{};
            render(engine, output, 1);
            float energy = 0.f;
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                check(std::isfinite(output[channel]) && output[channel] >= 0.f && output[channel] <= .25f,
                      "spatial headroom bound");
                if (format.speakers[channel] == Speaker::LFE) near(output[channel], 0.f, "spatial no LFE", 0.f);
                if (channels == 8 && sourceChannels == 1)
                    near(output[channel], format.speakers[channel] == Speaker::SideLeft ? 32767.f / 32768.f * .25f : 0.f,
                         "actual speaker azimuth and attenuation once");
                energy += output[channel] * output[channel];
            }
            check(energy > .01f, "spatial signal present");
        }
    std::puts("PASS: mono/stereo object virtual sources, 2/6/8 actual speaker angles and one attenuation");
}

std::vector<float> resampleRun(std::uint32_t sourceRate, std::uint32_t outputRate, std::uint32_t partition)
{
    AudioEngine engine;
    check(engine.configure(Role::Music, layout(2, sourceRate), {1, 1}, headless(layout(2, outputRate))) == Result::Ok,
          "resample configure");
    std::vector<std::int16_t> input(4096 * 2, 8192);
    input[input.size() - 2] = 30000;
    input[input.size() - 1] = -30000;
    for (std::uint32_t position = 0; position < 4096;)
    {
        const auto count = std::min(partition == 1 ? 4096u : 113u, 4096 - position);
        check(engine.enqueuePCM(input.data() + position * 2, count,
                                1000000 + static_cast<std::int64_t>(position) * 1000000 / sourceRate,
                                {1, 1}) == Result::Ok, "variable PCM block partition");
        position += count;
    }
    check(engine.drain() == Result::Ok, "resample drain");
    const auto nominal = static_cast<std::uint32_t>(4096ull * outputRate / sourceRate);
    const auto filterRate = std::min(sourceRate, outputRate);
    const auto tail = static_cast<std::uint32_t>((64ull * outputRate + filterRate - 1) / filterRate);
    const auto total = nominal + tail + 128;
    std::vector<float> output(total * 2);
    for (std::uint32_t position = 0; position < total;)
    {
        const auto count = std::min(partition, total - position);
        render(engine, output.data() + position * 2, count);
        position += count;
    }
    check(engine.status().state == State::Drained, "resampler drains filter tail");
    check(engine.status().renderedFrames >= nominal && engine.status().renderedFrames <= nominal + tail + 1,
          "nominal resampler clock and bounded tail");
    near(output[(nominal / 2) * 2], .25f, "resampler constant plateau", .001f);
    bool tailImpulse = false;
    for (std::uint32_t frame = nominal > 64 ? nominal - 64 : 0; frame < total; ++frame)
        if (std::abs(output[frame * 2] - output[frame * 2 + 1]) > .0001f) tailImpulse = true;
    check(tailImpulse, "last authored impulse survives resampler drain");
    return output;
}

void testResampling()
{
    for (const auto rates : {std::array<std::uint32_t, 2>{44100, 48000}, {96000, 48000}, {192000, 8000}, {8000, 192000}})
        check(resampleRun(rates[0], rates[1], 1) == resampleRun(rates[0], rates[1], 97),
              "miniaudio callback partition invariance");
    AudioEngine reused;
    AudioEngine fresh;
    check(reused.configure(Role::Music, layout(2, 44100), {1, 1}, headless()) == Result::Ok, "resampled flush configure");
    check(fresh.configure(Role::Music, layout(2, 44100), {1, 1}, headless()) == Result::Ok, "resampled reference configure");
    std::vector<std::int16_t> oldInput(512 * 2, 16384);
    enqueue(reused, oldInput, 2);
    float previous[74]{};
    render(reused, previous, 37);
    check(reused.flush({2, 1}) == Result::Ok, "resampled flush");
    std::vector<std::int16_t> newInput(512 * 2, -16384);
    enqueue(reused, newInput, 2, 10000000, {2, 1});
    enqueue(fresh, newInput, 2, 10000000);
    check(reused.drain() == Result::Ok && fresh.drain() == Result::Ok, "resampled flush drain");
    std::vector<float> actual(700 * 2);
    std::vector<float> reference(actual.size());
    render(reused, actual.data(), 700);
    render(fresh, reference.data(), 700);
    check(actual == reference, "flush clears all old reserve and resampler filter state");
    std::puts("PASS: miniaudio 44.1/48, 96/48, 192/8, 8/192 clocks, partitioning and EOS tails");
    std::puts("PASS: resampled flush matches a fresh engine sample-for-sample");
}

void testConcurrent()
{
    AudioEngine engine;
    auto options = headless();
    options.queueFrames = 512;
    check(engine.configure(Role::Music, {}, {1, 1}, options) == Result::Ok, "concurrent configure");
    std::atomic<bool> finish{false};
    std::atomic<std::uint64_t> generation{1};
    std::atomic<std::uint64_t> successful{0};
    std::thread producer([&]
    {
        std::int16_t samples[128];
        std::fill_n(samples, 128, std::int16_t{8192});
        std::uint64_t localGeneration = 0;
        std::uint64_t frames = 0;
        while (!finish.load(std::memory_order_acquire))
        {
            const auto nextGeneration = generation.load(std::memory_order_acquire);
            if (nextGeneration != localGeneration)
            {
                localGeneration = nextGeneration;
                frames = 0;
            }
            const auto result = engine.enqueuePCM(samples, 64,
                1000000 + static_cast<std::int64_t>(frames * 1000000 / 48000), {localGeneration, 1});
            check(result == Result::Ok || result == Result::QueueFull || result == Result::StaleGeneration,
                  "concurrent producer results");
            if (result == Result::Ok)
            {
                frames += 64;
                successful.fetch_add(1);
            }
            else std::this_thread::yield();
        }
    });
    std::thread consumer([&]
    {
        float output[74];
        while (!finish.load(std::memory_order_acquire))
        {
            render(engine, output, 37);
            for (float sample : output) check(std::isfinite(sample) && std::abs(sample) <= 1.f, "concurrent finite output");
            std::this_thread::yield();
        }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (std::uint64_t nextGeneration = 2; nextGeneration <= 201; ++nextGeneration)
    {
        check(engine.flush({nextGeneration, 1}) == Result::Ok, "concurrent flush command");
        generation.store(nextGeneration, std::memory_order_release);
        while (engine.status().renderedGeneration.stream != nextGeneration)
        {
            check(std::chrono::steady_clock::now() < deadline, "concurrent flush progress");
            std::this_thread::yield();
        }
        const auto progress = successful.load();
        while (successful.load() == progress)
        {
            check(std::chrono::steady_clock::now() < deadline, "concurrent producer progress");
            std::this_thread::yield();
        }
        check(engine.status().queuedFrames <= 512, "concurrent queue memory bound");
    }
    finish.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    check(engine.flush({202, 1}) == Result::Ok, "final isolation flush");
    float output[64]{};
    render(engine, output, 0);
    std::vector<std::int16_t> finalInput(16 * 2, -16384);
    enqueue(engine, finalInput, 2, 9000000, {202, 1});
    check(engine.drain() == Result::Ok, "final isolation drain");
    render(engine, output, 32);
    for (std::uint32_t frame = 0; frame < 32; ++frame)
        near(output[frame * 2], frame < 16 ? -.5f : 0.f, "final concurrent generation isolation", 0.f);
    std::puts("PASS: three-thread PCM/control/render stress with 200 flushes and final content isolation");
}

void testIndependentTransition()
{
    {
        AudioEngine recovery;
        check(recovery.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "recovery configure");
        check(recovery.transition(0.f, .01, 1) == Result::Ok, "recovery transition");
        float output[960]{};
        render(recovery, output, 240);
        check(recovery.flush({2, 1}, true) == Result::Ok, "timestamp recovery flush");
        render(recovery, output, 241);
        check(recovery.status().transitionCompleted == 1, "timestamp recovery must retain transition identity");
        const auto status = recovery.status();
        check(status.endpointTransitionCompleted == 0, "recovery must await a new endpoint fence");
        check(recovery.advanceHeadlessEndpoint(status.generation, status.transitionFenceFrame) == Result::Ok,
            "recovery endpoint consumes target");
        check(recovery.status().endpointTransitionCompleted == 1, "recovery transition acknowledged");
        check(recovery.flush({3, 1}) == Result::Ok, "ordinary flush");
        render(recovery, output, 1);
        check(recovery.status().transitionConsumed == 0 && recovery.status().transitionCompleted == 0,
            "ordinary flush still invalidates transitions");
    }
    AudioEngine engine;
    check(engine.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "transition configure");
    enqueue(engine, std::vector<std::int16_t>(4096 * 2, 16384), 2);
    check(engine.transition(0.f, .01, 1) == Result::Ok, "transition published");
    check(engine.status().transitionConsumed == 0, "publication is not consumption");
    float output[960]{};
    render(engine, output, 240);
    check(engine.status().transitionConsumed == 1 && engine.status().transitionCompleted == 0,
          "consumed but not completed");
    check(engine.gain(.2f, 0.) == Result::Ok, "slider during fade");
    render(engine, output, 240);
    near(output[0], .05f, "volume and transition multiply once");
    check(engine.status().transitionCompleted == 0, "target next sample is not completion");
    render(engine, output, 1);
    near(output[0], 0.f, "completed frame is zero");
    check(engine.status().transitionCompleted == 1, "zero frame completes fence");
    check(engine.transition(1.f, .01, 2) == Result::Ok, "fade in");
    check(engine.gain(.2f, 0., true) == Result::Ok, "mute during transition");
    render(engine, output, 240);
    near(output[0], 0.f, "hard mute overrides factors");
    check(engine.gain(.2f, 0.) == Result::Ok, "release mute");
    render(engine, output, 1);
    near(output[0], .05f, "unmute preserves transition phase");
    check(engine.pause() == Result::Ok, "pause transition");
    check(engine.transition(0.f, .001, 3) == Result::Ok, "paused transition");
    render(engine, output, 49);
    check(engine.status().transitionCompleted == 3, "paused silence completes transition");
    check(engine.flush({2, 1}) == Result::Ok, "handover generation");
    check(engine.resume() == Result::Ok, "resume empty stream");
    check(engine.transition(1.f, .001, 4) == Result::Ok, "empty transition");
    render(engine, output, 49);
    check(engine.status().transitionCompleted == 4, "empty silence completes transition");
    check(engine.status().renderedGeneration.stream == 2, "handover consumed");
    check(engine.transition(0.f, 0., 4) == Result::InvalidArgument, "duplicate command rejected");
        const auto monoSource = layout(1, 44100);
        const auto consumed = engine.status().renderedFrames;
        check(engine.handover(Role::Music, monoSource, {3, 2}) == Result::Ok, "source format handover");
        std::vector<std::int16_t> nextPCM(2048, 8192);
        check(engine.enqueuePCM(nextPCM.data(), 2048, 0, {3, 2}) == Result::QueueFull,
            "new format cannot race pending handover");
        render(engine, output, 1);
        check(engine.status().renderedGeneration.format == 2, "format consumed fence");
        check(engine.enqueuePCM(nextPCM.data(), 2048, 0, {3, 2}) == Result::Ok, "new format PCM");
        render(engine, output, 480);
        check(engine.status().output.sampleRate == 48000 && engine.status().output.channels == 2,
            "handover retains negotiated output");
        check(engine.status().renderedFrames > consumed, "new source rendered");
    std::puts("PASS: independent volume/transition, consumed/completed fences, mute, pause, empty and handover");
}

void testEndpointTimeline()
{
    EndpointTimeline large;
    EndpointObservation fractional{1, 7, 100, 3, 1};
    const auto largeSubmitted = (std::uint64_t{1} << 33) + 10;
    check(large.observe(fractional, largeSubmitted) == Result::Ok && !large.consumed,
        "fractional clock guard rounds upward, submission count exceeds 32 bits");
    check(large.observe(fractional, largeSubmitted) == Result::Ok && !large.consumed,
        "stationary device clock cannot discharge positive latency");
    ++fractional.position;
    check(large.observe(fractional, largeSubmitted) == Result::Ok && large.consumed == largeSubmitted - 7,
        "large content boundary survives conservative tick conversion");
    EndpointTimeline overflow;
    fractional.frequency = std::numeric_limits<std::uint64_t>::max();
    fractional.latency100ns = 2;
    check(overflow.observe(fractional, largeSubmitted) == Result::DeviceError, "latency arithmetic overflow fails closed");
    for (const auto sourceRate : {44100u, 48000u, 96000u})
    {
      std::uint64_t referenceDrain = 0;
      for (const auto partition : {1u, 37u, 2048u})
      {
        AudioEngine engine;
        check(engine.configure(Role::Music, layout(2, sourceRate), {1, 1}, headless()) == Result::Ok,
            "endpoint configure");
        enqueue(engine, std::vector<std::int16_t>(1024 * 2, 16384), 2);
        check(engine.transition(0.f, .01, 1) == Result::Ok && engine.drain() == Result::Ok, "endpoint commands");
        std::vector<float> output(4096 * 2);
        for (std::uint32_t frame = 0; frame < 4096;)
        {
            const auto count = std::min(partition, 4096 - frame);
            render(engine, output.data() + frame * 2, count);
            frame += count;
        }
        const auto status = engine.status();
        check(status.transitionFenceFrame == 481, "exact partition-independent transition boundary");
        if (!referenceDrain) referenceDrain = status.drainFenceFrame;
        check(status.drainFenceFrame == referenceDrain && referenceDrain == status.renderedFrames,
            "exact partition-independent EOS resampler boundary");
        EndpointObservation observation{status.deviceEpoch, status.submittedFrames, 0, 10000000, 10000};
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "padding held");
        observation.position = 1000000;
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "clock alone cannot consume content");
        check(!engine.status().endpointTransitionCompleted && !engine.status().endpointDrainComplete,
            "no completion from free-running clock with held content");
        observation.paddingFrames = status.submittedFrames - status.transitionFenceFrame;
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "content left application buffer");
        observation.position += 9999;
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "latency almost elapsed");
        check(!engine.status().endpointTransitionCompleted, "held physical tail prevents early completion");
        ++observation.position;
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "latency guard crossed");
        check(engine.status().endpointTransitionCompleted == 1 && !engine.status().endpointDrainComplete,
            "only crossed content fence completes");
        observation.paddingFrames = 0;
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "EOS released");
        check(!engine.status().endpointDrainComplete, "EOS tail still held");
        observation.position += 10000;
        check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "EOS physical guard crossed");
        check(engine.status().endpointDrainComplete, "EOS completes after latency guard");
      }
    }
    for (const auto failure : {0u, 1u, 2u, 3u})
    {
      AudioEngine engine;
      check(engine.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "failure configure");
      check(engine.gain(0.f, 0.) == Result::Ok && engine.pause() == Result::Ok &&
          engine.transition(.1f, 0., 1) == Result::Ok, "zero initial gain paused nonzero target");
      float output[64]{};
      render(engine, output, 32);
      EndpointObservation observation{engine.status().deviceEpoch, 0, 100, 48000, 100000};
      check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::Ok, "establish pending physical boundary");
      auto stale = observation;
      ++stale.deviceEpoch;
      check(engine.observeHeadlessEndpoint({1, 1}, stale) == Result::StaleGeneration, "wrong endpoint epoch rejected");
      check(engine.flush({2, 1}) == Result::Ok, "flush with pending endpoint boundary");
      render(engine, output, 1);
      check(engine.observeHeadlessEndpoint({1, 1}, observation) == Result::StaleGeneration,
          "old stream observation rejected");
      check(!engine.status().endpointTransitionCompleted, "flush cancels old transition identity");
      if (failure == 0) observation.position = 99;
      if (failure == 1) observation.valid = false;
      if (failure == 2) ++observation.frequency;
      if (failure == 3) observation.paddingFrames = 10000;
      check(engine.observeHeadlessEndpoint({2, 1}, observation) == Result::DeviceError, "bad clock/padding fails explicitly");
    check(engine.status().deviceFailure == DeviceFailure::Timeline, "clock failure location retained");
      check(engine.status().state == State::Error && !engine.status().endpointQualified &&
          !engine.status().endpointTransitionCompleted, "endpoint failure is not success");
      const auto oldEpoch = observation.deviceEpoch;
      check(engine.configure(Role::Music, {}, {3, 2}, headless()) == Result::Ok, "explicit device reopen");
    check(engine.status().deviceFailure == DeviceFailure::None, "reopen resets failure diagnostic");
      check(engine.status().deviceEpoch > oldEpoch, "reopen renews device epoch");
      check(engine.observeHeadlessEndpoint({3, 2}, observation) == Result::StaleGeneration, "old device cannot complete reopened stream");
      check(engine.stop() == Result::Ok && !engine.status().endpointQualified, "teardown invalidates physical qualification");
    }
    AudioEngine failedSubmission;
    check(failedSubmission.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "failed submission configure");
    check(failedSubmission.transition(0.f, 0., 1) == Result::Ok, "failed submission transition");
    float output[64]{};
    check(failedSubmission.render(output, 32, false) == Result::DeviceError, "failed ReleaseBuffer model");
    check(failedSubmission.status().deviceFailure == DeviceFailure::Submission, "submission failure location retained");
    const EndpointObservation invalidObservation{};
    check(failedSubmission.observeHeadlessEndpoint({1, 1}, invalidObservation) != Result::Ok,
        "failed endpoint remains failed");
    check(failedSubmission.status().deviceFailure == DeviceFailure::Submission, "first failure is not overwritten");
    check(failedSubmission.status().submittedFrames == 0 && failedSubmission.status().transitionCompleted == 1 &&
          !failedSubmission.status().endpointTransitionCompleted, "rendered but failed submission never completes");
        AudioEngine saturated;
        check(saturated.configure(Role::Music, {}, {1, 1}, headless()) == Result::Ok, "endpoint saturation configure");
        for (std::uint64_t command = 1; command <= 64; ++command)
          check(saturated.transition(.1f, 0., command) == Result::Ok, "fill transition queue");
        check(saturated.transition(0.f, 0., 65) == Result::ControlQueueFull, "unpublished transition cannot own a fence");
        check(saturated.gain(0.f, 0., true) == Result::Ok, "hard mute still bypasses saturated transition queue");
        render(saturated, output, 32);
        check(!saturated.status().endpointTransitionCompleted, "hard mute and render do not imply physical completion");
        check(saturated.advanceHeadlessEndpoint({1, 1}, saturated.status().transitionFenceFrame) == Result::Ok &&
            saturated.status().endpointTransitionCompleted == 64, "only latest accepted transition completes");
        check(saturated.transition(0.f, 0., 65) == Result::Ok, "rejected serial can be retried without reuse violation");
    std::puts("PASS: endpoint padding/physical lag, exact rate/partition fences, pause/flush, epochs and explicit clock/submission failures");
}

int main()
{
    const auto before = allocatedBytes.load();
    { AudioEngine engine; }
    const auto storage = allocatedBytes.load() - before;
    check(storage < 2 * 1024 * 1024, "fixed engine allocation bound");
    std::printf("Engine construction storage: %zu bytes\n", storage);
    testIdentity();
    testEnvelopes();
    testRetargetMutePause();
    testIndependentTransition();
    testStarvation();
    testBoundsGenerations();
    testValidation();
    testChannelMaps();
    testSpatial();
    testResampling();
    testEndpointTimeline();
    testConcurrent();
    check(renderAllocations.load() == 0, "no C++ heap allocations inside render");
    std::puts("PASS: zero C++ enqueue/render allocations; all headless tests passed (no device opened)");
}