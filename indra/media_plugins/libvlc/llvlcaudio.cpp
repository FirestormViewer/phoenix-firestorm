#include "llvlcaudio.h"
#include "llvlcendpoint.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <type_traits>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#if defined(_WIN64)
#include <audioclient.h>
struct ma_device;
namespace llvlc
{
struct WasapiEndpoint
{
    static void invalidate(ma_device* device) noexcept;
    static bool healthy(ma_device* device) noexcept;
    static bool stopping(ma_device* device) noexcept;
    static bool submitted(ma_device* device, std::uint32_t frames) noexcept;
    static bool observe(ma_device* device, const EndpointObservation& observation) noexcept;
    static std::uint64_t epoch(ma_device* device) noexcept;
};
}
#endif
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#if defined(_WIN64)
#define MA_ENABLE_WASAPI
#endif
#define MINIAUDIO_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <llvlc_miniaudio.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace llvlc
{
namespace
{
constexpr std::uint32_t CommandCapacity = 64;
constexpr float Pi = 3.14159265358979323846f;
using Matrix = std::array<std::array<float, MaxChannels>, MaxChannels>;
static_assert(MA_VERSION_MAJOR == 0 && MA_VERSION_MINOR == 10 && MA_VERSION_REVISION == 42,
              "Reaudit the audio engine before changing the packaged miniaudio version.");
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<std::int64_t>::is_always_lock_free);
static_assert(std::atomic<State>::is_always_lock_free);
static_assert(std::atomic<Result>::is_always_lock_free);
static_assert(static_cast<int>(Speaker::FrontLeft) == MA_CHANNEL_FRONT_LEFT);
static_assert(static_cast<int>(Speaker::LFE) == MA_CHANNEL_LFE);
static_assert(static_cast<int>(Speaker::SideRight) == MA_CHANNEL_SIDE_RIGHT);
static_assert(std::is_trivially_copyable<ma_linear_resampler>::value);

bool validFormat(const Format& format)
{
    if (format.sampleRate < 8000 ||
        format.sampleRate > 192000 || format.channels == 0 || format.channels > MaxChannels)
    {
        return false;
    }
    for (std::uint32_t channel = 0; channel < format.channels; ++channel)
    {
        const auto speaker = format.speakers[channel];
        if (speaker == Speaker::Unknown || speaker > Speaker::SideRight ||
            (speaker == Speaker::Mono && format.channels != 1))
        {
            return false;
        }
        for (std::uint32_t previous = 0; previous < channel; ++previous)
        {
            if (speaker == format.speakers[previous]) return false;
        }
    }
    return true;
}

bool stereo(const Format& format)
{
        return format.channels == 2 &&
            ((format.speakers[0] == Speaker::FrontLeft && format.speakers[1] == Speaker::FrontRight) ||
             (format.speakers[1] == Speaker::FrontLeft && format.speakers[0] == Speaker::FrontRight));
}

bool mono(const Format& format)
{
    return format.channels == 1 &&
           (format.speakers[0] == Speaker::Mono || format.speakers[0] == Speaker::FrontCenter);
}

float angle(Speaker speaker)
{
    switch (speaker)
    {
    case Speaker::FrontLeft: return -30.f;
    case Speaker::FrontRight: return 30.f;
    case Speaker::FrontLeftCenter: return -15.f;
    case Speaker::FrontRightCenter: return 15.f;
    case Speaker::SideLeft: return -90.f;
    case Speaker::SideRight: return 90.f;
    case Speaker::BackLeft: return -150.f;
    case Speaker::BackRight: return 150.f;
    case Speaker::BackCenter: return 180.f;
    default: return 0.f;
    }
}

Result musicMatrix(const Format& source, const Format& output, float headroom, Matrix& matrix)
{
    matrix = {};
    if (mono(source) || stereo(source))
    {
        const bool identity = stereo(source) && stereo(output);
        const float scale = identity || (mono(source) && mono(output)) ? 1.f : headroom;
        const auto left = source.speakers[0] == Speaker::FrontLeft ? 0u : 1u;
        const auto right = 1u - left;
        bool hasMain = false;
        for (std::uint32_t destination = 0; destination < output.channels; ++destination)
        {
            const auto speaker = output.speakers[destination];
            if (speaker == Speaker::LFE) continue;
            hasMain = true;
            if (mono(source)) matrix[destination][0] = scale;
            else if (angle(speaker) < 0.f) matrix[destination][left] = scale;
            else if (angle(speaker) > 0.f && speaker != Speaker::BackCenter)
                matrix[destination][right] = scale;
            else
            {
                matrix[destination][0] = scale * .5f;
                matrix[destination][1] = scale * .5f;
            }
        }
        return hasMain ? Result::Ok : Result::UnsupportedLayout;
    }
    for (std::uint32_t channel = 0; channel < source.channels; ++channel)
    {
        bool found = false;
        for (std::uint32_t destination = 0; destination < output.channels; ++destination)
        {
            if (source.speakers[channel] == output.speakers[destination])
            {
                matrix[destination][channel] = 1.f;
                found = true;
            }
        }
        if (!found) return Result::UnsupportedLayout;
    }
    return Result::Ok;
}

Result objectMatrix(const Format& source, const Format& output,
                    float right, float forward, Matrix& matrix)
{
    if (!mono(source) && !stereo(source)) return Result::UnsupportedLayout;
    const float length = std::hypot(right, forward);
    if (!std::isfinite(length) || std::abs(length - 1.f) > .001f)
        return Result::InvalidArgument;
    matrix = {};
    struct Position { float degrees; std::uint32_t channel; };
    std::array<Position, MaxChannels> positions{};
    std::uint32_t count = 0;
    for (std::uint32_t channel = 0; channel < output.channels; ++channel)
    {
        if (output.speakers[channel] != Speaker::LFE)
            positions[count++] = {angle(output.speakers[channel]), channel};
    }
    if (count == 0) return Result::UnsupportedLayout;
    std::sort(positions.begin(), positions.begin() + count,
              [](const Position& left, const Position& other) { return left.degrees < other.degrees; });
    for (std::uint32_t channel = 0; channel < source.channels; ++channel)
    {
        const float headroom = source.channels == 2 ? .5f : 1.f;
        if (count == 1)
        {
            matrix[positions[0].channel][channel] = headroom;
            continue;
        }
        float direction = std::atan2(right, forward) * 180.f / Pi;
        if (source.channels == 2)
            direction += source.speakers[channel] == Speaker::FrontLeft ? -15.f : 15.f;
        while (direction < positions[0].degrees) direction += 360.f;
        while (direction >= positions[0].degrees + 360.f) direction -= 360.f;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const auto next = (index + 1) % count;
            const float start = positions[index].degrees;
            const float end = positions[next].degrees + (next == 0 ? 360.f : 0.f);
            if (direction >= start && direction <= end)
            {
                const float phase = (direction - start) / (end - start) * Pi * .5f;
                matrix[positions[index].channel][channel] = headroom * std::clamp(std::cos(phase), 0.f, 1.f);
                matrix[positions[next].channel][channel] = headroom * std::clamp(std::sin(phase), 0.f, 1.f);
                break;
            }
        }
    }
    return Result::Ok;
}

struct Envelope
{
    double start = 1.;
    double target = 1.;
    std::uint64_t duration = 0;
    std::uint64_t position = 0;

    float value() const noexcept
    {
        if (position >= duration) return static_cast<float>(target);
        return static_cast<float>(start + (target - start) *
                                  static_cast<double>(position) / static_cast<double>(duration));
    }

    void set(float next, std::uint64_t frames) noexcept
    {
        start = value();
        target = next;
        duration = frames;
        position = 0;
    }

    void advance() noexcept { if (position < duration) ++position; }
};
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
struct AudioEngine::Impl
{
    struct PCMFrame
    {
        std::array<std::int16_t, MaxChannels> samples{};
        std::int64_t pts = 0;
        std::uint64_t stream = 0;
    };
    struct FloatFrame
    {
        std::array<float, MaxChannels> samples{};
        std::int64_t pts = -1;
    };
    enum class CommandType { Gain, Transition, Handover, Pause, Resume, Flush, Drain, Spatial };
    struct Command
    {
        CommandType type = CommandType::Pause;
        float target = 0.f;
        std::uint64_t frames = 0;
        std::uint64_t token = 0;
        Matrix matrix{};
        Format source{};
        std::uint64_t format = 0;
    };

    std::array<PCMFrame, MaxQueueFrames> pcm{};
    std::array<FloatFrame, MaxReserveFrames> reserve{};
    std::array<Command, CommandCapacity> commands{};
    alignas(64) std::atomic<std::uint64_t> write{0};
    alignas(64) std::atomic<std::uint64_t> read{0};
    alignas(64) std::atomic<std::uint64_t> commandWrite{0};
    alignas(64) std::atomic<std::uint64_t> commandRead{0};
    std::atomic<std::uint64_t> stream{0};
    std::atomic<std::uint64_t> appliedStream{0};
    std::atomic<std::uint64_t> appliedFormat{0};
    std::atomic<std::uint64_t> muteToken{0};
    std::atomic<bool> accepting{false};
    std::atomic<bool> started{false};
    std::atomic<bool> stopping{false};
    std::atomic<State> publishedState{State::Closed};
    std::atomic<Result> error{Result::Ok};
    std::atomic<std::uint32_t> publishedReserve{0};
    std::atomic<std::uint64_t> rendered{0};
    std::atomic<std::uint64_t> starvations{0};
    std::atomic<std::uint64_t> rejected{0};
    std::atomic<std::int64_t> publishedPts{-1};
    std::atomic<float> publishedGain{1.f};
    std::atomic<float> publishedTransition{1.f};
    std::atomic<std::uint64_t> transitionConsumed{0};
    std::atomic<std::uint64_t> transitionCompleted{0};
    std::atomic<std::uint64_t> clockFrames{0};
    std::atomic<std::uint64_t> fenceSequence{0};
    std::atomic<std::uint64_t> transitionFenceFrame{0};
    std::atomic<std::uint64_t> drainFenceFrame{0};
    std::atomic<std::uint64_t> endpointFrames{0};
    std::atomic<std::uint64_t> submittedFrames{0};
    std::atomic<bool> endpointQualified{false};
    std::uint64_t deviceEpoch = 0;
    EndpointTimeline endpoint;
    std::uint64_t transitionId = 0;
    std::uint64_t lastTransitionId = 0;
    Format source{};
    Format output{};
    Options options{};
    Role role = Role::Music;
    std::uint64_t formatGeneration = 0;
    std::uint64_t renderStream = 0;
    std::uint64_t producerStream = 0;
    std::uint64_t producerFrames = 0;
    std::int64_t producerAnchor = 0;
    std::uint32_t reserveFrames = 0;
    std::uint32_t primeFrames = 0;
    std::uint32_t reserveRead = 0;
    std::uint32_t reserveCount = 0;
    std::uint32_t starvingRemaining = 0;
    std::uint64_t sourceConsumed = 0;
    std::uint64_t converted = 0;
    std::int64_t lastConvertedPts = -1;
    bool configured = false;
    bool deviceInitialized = false;
    bool resamplerInitialized = false;
    bool pendingInput = false;
    bool eos = false;
    bool controlEos = false;
    bool paused = false;
    bool recovering = false;
    State state = State::Priming;
    Matrix matrix{};
    Envelope mainGain{};
    Envelope transitionGain{};
    Envelope continuityGain{};
    FloatFrame input{};
    ma_linear_resampler resampler{};
    ma_linear_resampler initialResampler{};
    ma_linear_resampler handoverResampler{};
    ma_device device{};

    Result endpointFailure() noexcept
    {
        endpointQualified.store(false, std::memory_order_release);
        accepting.store(false, std::memory_order_release);
        error.store(Result::DeviceError, std::memory_order_release);
        publishedState.store(State::Error, std::memory_order_release);
        return Result::DeviceError;
    }

    Result submitFrames(std::uint32_t frames, bool success) noexcept
    {
        if (error.load(std::memory_order_acquire) != Result::Ok) return error.load();
        const auto previous = submittedFrames.load(std::memory_order_relaxed);
        if (!success || frames > std::numeric_limits<std::uint64_t>::max() - previous ||
            previous + frames != clockFrames.load(std::memory_order_acquire)) return endpointFailure();
        submittedFrames.store(previous + frames, std::memory_order_release);
        return Result::Ok;
    }

    Result observeEndpoint(const EndpointObservation& observation) noexcept
    {
        if (observation.deviceEpoch != deviceEpoch) return Result::StaleGeneration;
        if (stopping.load(std::memory_order_acquire)) return Result::Stopped;
        if (error.load(std::memory_order_acquire) != Result::Ok) return error.load();
        const auto result = endpoint.observe(observation, submittedFrames.load(std::memory_order_acquire));
        if (result != Result::Ok)
        {
            return endpointFailure();
        }
        endpointFrames.store(endpoint.consumed, std::memory_order_release);
        endpointQualified.store(true, std::memory_order_release);
        return Result::Ok;
    }

    Result command(const Command& next) noexcept
    {
        if (!configured) return Result::NotConfigured;
        if (stopping.load(std::memory_order_acquire)) return Result::Stopped;
        if (error.load(std::memory_order_acquire) != Result::Ok)
            return error.load(std::memory_order_relaxed);
        const auto position = commandWrite.load(std::memory_order_relaxed);
        if (position - commandRead.load(std::memory_order_acquire) >= CommandCapacity)
            return Result::ControlQueueFull;
        commands[position % CommandCapacity] = next;
        commandWrite.store(position + 1, std::memory_order_release);
        return Result::Ok;
    }

    void resetConsumer(std::uint64_t nextStream) noexcept
    {
        renderStream = nextStream;
        transitionId = 0;
        transitionConsumed.store(0, std::memory_order_relaxed);
        transitionCompleted.store(0, std::memory_order_relaxed);
        transitionFenceFrame.store(0, std::memory_order_relaxed);
        drainFenceFrame.store(0, std::memory_order_relaxed);
        reserveRead = reserveCount = starvingRemaining = 0;
        sourceConsumed = converted = 0;
        lastConvertedPts = -1;
        pendingInput = eos = recovering = false;
        state = State::Priming;
        continuityGain = {};
        resampler = initialResampler;
        publishedPts.store(-1, std::memory_order_relaxed);
    }

    void consumeCommands() noexcept
    {
        auto position = commandRead.load(std::memory_order_relaxed);
        const auto end = commandWrite.load(std::memory_order_acquire);
        while (position != end)
        {
            const auto& next = commands[position % CommandCapacity];
            switch (next.type)
            {
            case CommandType::Gain:
            {
                auto expected = next.token;
                mainGain.set(next.target, next.frames);
                muteToken.compare_exchange_strong(expected, next.token & ~std::uint64_t{1},
                                                  std::memory_order_acq_rel);
                break;
            }
            case CommandType::Transition:
                transitionGain.set(next.target, next.frames);
                transitionId = next.token;
                transitionFenceFrame.store(0, std::memory_order_relaxed);
                transitionCompleted.store(0, std::memory_order_relaxed);
                transitionConsumed.store(next.token, std::memory_order_release);
                break;
            case CommandType::Pause: paused = true; break;
            case CommandType::Handover:
                source = next.source;
                matrix = next.matrix;
                initialResampler = handoverResampler;
                resetConsumer(next.token);
                transitionGain = {};
                paused = false;
                appliedFormat.store(next.format, std::memory_order_release);
                break;
            case CommandType::Resume: paused = false; break;
            case CommandType::Flush: resetConsumer(next.token); break;
            case CommandType::Drain: eos = true; break;
            case CommandType::Spatial: matrix = next.matrix; break;
            }
            ++position;
        }
        commandRead.store(position, std::memory_order_release);
    }

    void discardStale() noexcept
    {
        auto position = read.load(std::memory_order_relaxed);
        const auto end = write.load(std::memory_order_acquire);
        while (position != end && pcm[position % options.queueFrames].stream < renderStream)
            ++position;
        read.store(position, std::memory_order_release);
    }

    bool popInput(FloatFrame& frame) noexcept
    {
        auto position = read.load(std::memory_order_relaxed);
        const auto end = write.load(std::memory_order_acquire);
        while (position != end)
        {
            const auto& next = pcm[position % options.queueFrames];
            if (next.stream > renderStream) break;
            ++position;
            if (next.stream != renderStream) continue;
            for (std::uint32_t channel = 0; channel < source.channels; ++channel)
                frame.samples[channel] = static_cast<float>(next.samples[channel]) / 32768.f;
            frame.pts = next.pts;
            read.store(position, std::memory_order_release);
            return true;
        }
        read.store(position, std::memory_order_release);
        return false;
    }

    bool nextFrame(FloatFrame& frame) noexcept
    {
        if (source.sampleRate == output.sampleRate) return popInput(frame);
        for (std::uint32_t attempt = 0; attempt < 64; ++attempt)
        {
            if (!pendingInput) pendingInput = popInput(input);
            const bool padding = !pendingInput && eos;
            if (padding)
            {
                const auto filterRate = std::min(source.sampleRate, output.sampleRate);
                const auto tail = (64ull * output.sampleRate + filterRate - 1) / filterRate;
                const auto limit = (sourceConsumed * output.sampleRate + source.sampleRate - 1) /
                                   source.sampleRate + tail;
                if (sourceConsumed == 0 || converted >= limit) return false;
                input.samples.fill(0.f);
            }
            ma_uint64 inputCount = pendingInput || padding ? 1 : 0;
            ma_uint64 outputCount = 1;
            const auto result = ma_linear_resampler_process_pcm_frames(
                &resampler, input.samples.data(), &inputCount, frame.samples.data(), &outputCount);
            if (result != MA_SUCCESS)
            {
                error.store(Result::ResamplerError, std::memory_order_release);
                state = State::Error;
                return false;
            }
            if (inputCount && pendingInput)
            {
                ++sourceConsumed;
                lastConvertedPts = input.pts;
                pendingInput = false;
            }
            if (outputCount)
            {
                ++converted;
                frame.pts = lastConvertedPts;
                return true;
            }
            if (inputCount == 0) return false;
        }
        error.store(Result::ResamplerError, std::memory_order_release);
        state = State::Error;
        return false;
    }

    void fill(std::uint32_t target) noexcept
    {
        while (reserveCount < target)
        {
            auto& frame = reserve[(reserveRead + reserveCount) % MaxReserveFrames];
            if (!nextFrame(frame)) break;
            ++reserveCount;
        }
    }

    void renderFrames(float* destination, std::uint32_t frames) noexcept
    {
        if (frames > std::numeric_limits<std::uint64_t>::max() - clockFrames.load(std::memory_order_relaxed))
        {
            std::fill_n(destination, static_cast<std::size_t>(frames) * output.channels, 0.f);
            endpointFailure();
            return;
        }
        fenceSequence.fetch_add(1, std::memory_order_acq_rel);
        consumeCommands();
        discardStale();
        std::uint64_t completed = 0;
        std::uint64_t completedFrame = 0;
        const auto beginFrame = clockFrames.load(std::memory_order_relaxed);
        for (std::uint32_t index = 0; index < frames; ++index)
        {
            auto* outputFrame = destination + static_cast<std::size_t>(index) * output.channels;
            std::fill_n(outputFrame, output.channels, 0.f);
            const float transitionLevel = transitionGain.value();
            if (!completed && transitionGain.position >= transitionGain.duration)
            {
                completed = transitionId;
                completedFrame = beginFrame + index + 1;
            }
            transitionGain.advance();
            if (paused || stopping.load(std::memory_order_acquire) ||
                stream.load(std::memory_order_acquire) != renderStream ||
                error.load(std::memory_order_acquire) != Result::Ok) continue;
            if (state == State::Priming)
            {
                fill(primeFrames + reserveFrames);
                if (reserveCount < primeFrames + reserveFrames && !eos) continue;
                if (reserveCount == 0)
                {
                    if (eos)
                    {
                        state = State::Drained;
                        if (!drainFenceFrame.load(std::memory_order_relaxed))
                            drainFenceFrame.store(beginFrame + index + 1, std::memory_order_relaxed);
                    }
                    continue;
                }
                state = State::Playing;
                if (recovering)
                {
                    continuityGain.set(0.f, 0);
                    continuityGain.set(1.f, reserveFrames);
                }
            }
            if (state == State::Playing)
            {
                fill(reserveFrames + 1);
                if (reserveCount <= reserveFrames && !eos)
                {
                    state = State::Starving;
                    starvingRemaining = reserveCount;
                    continuityGain.set(0.f, reserveCount > 1 ? reserveCount - 1 : 0);
                    starvations.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (reserveCount == 0)
            {
                if (eos) state = State::Drained;
                continue;
            }
            const auto& frame = reserve[reserveRead];
            const float level = mainGain.value() * transitionLevel * continuityGain.value();
            for (std::uint32_t destinationChannel = 0; destinationChannel < output.channels;
                 ++destinationChannel)
            {
                float mixed = 0.f;
                for (std::uint32_t channel = 0; channel < source.channels; ++channel)
                    mixed += frame.samples[channel] * matrix[destinationChannel][channel];
                outputFrame[destinationChannel] = std::clamp(mixed * level, -1.f, 1.f);
            }
            if (muteToken.load(std::memory_order_acquire) & 1)
                std::fill_n(outputFrame, output.channels, 0.f);
            mainGain.advance();
            continuityGain.advance();
            publishedPts.store(frame.pts, std::memory_order_relaxed);
            rendered.fetch_add(1, std::memory_order_relaxed);
            reserveRead = (reserveRead + 1) % MaxReserveFrames;
            --reserveCount;
            if (state == State::Starving && --starvingRemaining == 0)
            {
                state = State::Priming;
                recovering = true;
            }
            if (eos && reserveCount == 0)
            {
                state = State::Drained;
                if (!drainFenceFrame.load(std::memory_order_relaxed))
                    drainFenceFrame.store(beginFrame + index + 1, std::memory_order_relaxed);
            }
        }
        publishedReserve.store(reserveCount, std::memory_order_relaxed);
        publishedGain.store(mainGain.value(), std::memory_order_relaxed);
        publishedTransition.store(transitionGain.value(), std::memory_order_relaxed);
        const auto endFrame = clockFrames.fetch_add(frames, std::memory_order_relaxed) + frames;
        if (completed && transitionCompleted.load(std::memory_order_relaxed) != completed)
        {
            transitionFenceFrame.store(completedFrame, std::memory_order_relaxed);
            transitionCompleted.store(completed, std::memory_order_release);
        }
        if (state == State::Drained && !drainFenceFrame.load(std::memory_order_relaxed))
            drainFenceFrame.store(endFrame, std::memory_order_relaxed);
        appliedStream.store(renderStream, std::memory_order_release);
        publishedState.store(error.load(std::memory_order_relaxed) != Result::Ok ? State::Error :
                             stopping.load(std::memory_order_relaxed) ? State::Stopped :
                             paused ? State::Paused : state, std::memory_order_release);
                fenceSequence.fetch_add(1, std::memory_order_release);
    }

    static void dataCallback(ma_device* device, void* output, const void*, ma_uint32 frames)
    {
        static_cast<Impl*>(device->pUserData)->renderFrames(static_cast<float*>(output), frames);
    }

    static void stopCallback(ma_device* device)
    {
        auto& engine = *static_cast<Impl*>(device->pUserData);
        engine.started.store(false, std::memory_order_release);
        if (!engine.stopping.load(std::memory_order_acquire))
        {
            engine.accepting.store(false, std::memory_order_release);
            engine.error.store(Result::DeviceError, std::memory_order_release);
            engine.publishedState.store(State::Error, std::memory_order_release);
        }
    }
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

AudioEngine::AudioEngine() : mImpl(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() { close(); }

#if defined(_WIN64)
void WasapiEndpoint::invalidate(ma_device* device) noexcept
{
    auto& engine = *static_cast<AudioEngine::Impl*>(device->pUserData);
    engine.endpointFailure();
    SetEvent(device->wasapi.hEventPlayback);
}

bool WasapiEndpoint::healthy(ma_device* device) noexcept
{
    auto& engine = *static_cast<AudioEngine::Impl*>(device->pUserData);
    return !engine.stopping.load(std::memory_order_acquire) && engine.error.load(std::memory_order_acquire) == Result::Ok;
}

bool WasapiEndpoint::submitted(ma_device* device, std::uint32_t frames) noexcept
{
    auto& engine = *static_cast<AudioEngine::Impl*>(device->pUserData);
    return engine.submitFrames(frames, true) == Result::Ok;
}

bool WasapiEndpoint::stopping(ma_device* device) noexcept
{
    return static_cast<AudioEngine::Impl*>(device->pUserData)->stopping.load(std::memory_order_acquire);
}

bool WasapiEndpoint::observe(ma_device* device, const EndpointObservation& observation) noexcept
{
    return static_cast<AudioEngine::Impl*>(device->pUserData)->observeEndpoint(observation) == Result::Ok;
}

std::uint64_t WasapiEndpoint::epoch(ma_device* device) noexcept
{
    return static_cast<AudioEngine::Impl*>(device->pUserData)->deviceEpoch;
}
#endif

Result AudioEngine::configure(Role role, const Format& source, Generation generation,
                              const Options& options)
{
    close();
    auto& engine = *mImpl;
    const auto fail = [&engine](Result result)
    {
        engine.error.store(result);
        engine.publishedState.store(State::Error);
        return result;
    };
    if (!validFormat(source) || generation.stream == 0 || generation.format == 0)
        return fail(Result::InvalidFormat);
    if (generation.stream <= engine.stream.load() || generation.format <= engine.formatGeneration)
        return fail(Result::StaleGeneration);
    if ((role != Role::Music && role != Role::Object) || options.queueFrames == 0 ||
        options.queueFrames > MaxQueueFrames || options.reserveMilliseconds == 0 ||
        options.reserveMilliseconds > 10 || options.primeMilliseconds == 0 ||
        options.primeMilliseconds > 100 || !std::isfinite(options.musicUpmixHeadroom) ||
        options.musicUpmixHeadroom <= 0.f || options.musicUpmixHeadroom > 1.f)
        return fail(Result::InvalidArgument);
    engine.source = source;
    engine.options = options;
    engine.role = role;
    engine.stopping.store(false);
    engine.error.store(Result::Ok);
    if (options.output == OutputMode::Headless)
    {
        engine.output = options.headlessOutput;
    }
    else if (options.output == OutputMode::Wasapi)
    {
#if defined(_WIN64)
        auto config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 0;
        config.sampleRate = 0;
        config.wasapi.noAutoConvertSRC = MA_TRUE;
        config.wasapi.noAutoStreamRouting = MA_TRUE;
        config.dataCallback = Impl::dataCallback;
        config.stopCallback = Impl::stopCallback;
        config.pUserData = &engine;
        const ma_backend backend = ma_backend_wasapi;
        if (ma_device_init_ex(&backend, 1, nullptr, &config, &engine.device) != MA_SUCCESS)
            return fail(Result::DeviceError);
        engine.deviceInitialized = true;
        engine.output.sampleRate = engine.device.sampleRate;
        engine.output.channels = engine.device.playback.channels;
        if (engine.output.channels > MaxChannels ||
            engine.device.playback.shareMode != ma_share_mode_shared ||
            !engine.device.playback.converter.isPassthrough ||
            !engine.device.wasapi.pDeviceEnumerator ||
            engine.output.channels != engine.device.playback.internalChannels ||
            engine.output.sampleRate != engine.device.playback.internalSampleRate)
        {
            close();
            return fail(Result::UnsupportedLayout);
        }
        for (std::uint32_t channel = 0; channel < engine.output.channels; ++channel)
        {
            const auto label = engine.device.playback.internalChannelMap[channel];
            if (label != engine.device.playback.channelMap[channel] || label > MA_CHANNEL_SIDE_RIGHT)
            {
                close();
                return fail(Result::UnsupportedLayout);
            }
            engine.output.speakers[channel] = static_cast<Speaker>(label);
        }
#else
        return fail(Result::UnsupportedPlatform);
#endif
    }
    else return fail(Result::InvalidArgument);
    if (!validFormat(engine.output))
    {
        close();
        return fail(Result::UnsupportedLayout);
    }
    engine.reserveFrames = engine.output.sampleRate * options.reserveMilliseconds / 1000;
    engine.primeFrames = engine.output.sampleRate * options.primeMilliseconds / 1000;
    const auto requiredInput = (static_cast<std::uint64_t>(engine.reserveFrames + engine.primeFrames) *
                               source.sampleRate + engine.output.sampleRate - 1) / engine.output.sampleRate + 32;
    if (engine.reserveFrames + engine.primeFrames >= MaxReserveFrames ||
        options.queueFrames < requiredInput)
    {
        close();
        return fail(Result::InvalidArgument);
    }
    const auto matrixResult = role == Role::Music ?
        musicMatrix(source, engine.output, options.musicUpmixHeadroom, engine.matrix) :
        objectMatrix(source, engine.output, 0.f, 1.f, engine.matrix);
    if (matrixResult != Result::Ok)
    {
        close();
        return fail(matrixResult);
    }
    const auto resamplerConfig = ma_linear_resampler_config_init(
        ma_format_f32, source.channels, source.sampleRate, engine.output.sampleRate);
    if (ma_linear_resampler_init(&resamplerConfig, &engine.resampler) != MA_SUCCESS)
    {
        close();
        return fail(Result::ResamplerError);
    }
    engine.resamplerInitialized = true;
    engine.initialResampler = engine.resampler;
    engine.write.store(0);
    engine.read.store(0);
    engine.commandWrite.store(0);
    engine.commandRead.store(0);
    engine.stream.store(generation.stream);
    engine.appliedStream.store(generation.stream);
    engine.appliedFormat.store(generation.format);
    engine.formatGeneration = generation.format;
    engine.producerStream = 0;
    engine.producerFrames = 0;
    engine.mainGain = {};
    engine.transitionGain = {};
    engine.transitionId = engine.lastTransitionId = 0;
    engine.transitionConsumed.store(0);
    engine.transitionCompleted.store(0);
    engine.clockFrames.store(0);
    engine.fenceSequence.store(0);
    engine.endpointFrames.store(0);
    engine.submittedFrames.store(0);
    engine.endpoint = {};
    if (engine.deviceEpoch == std::numeric_limits<std::uint64_t>::max())
    {
        close();
        return fail(Result::DeviceError);
    }
    ++engine.deviceEpoch;
    engine.endpointQualified.store(options.output == OutputMode::Headless);
    engine.publishedTransition.store(1.f);
    engine.paused = engine.controlEos = false;
    engine.resetConsumer(generation.stream);
    engine.muteToken.store(0);
    engine.rendered.store(0);
    engine.starvations.store(0);
    engine.rejected.store(0);
    engine.publishedReserve.store(0);
    engine.publishedGain.store(1.f);
    engine.stopping.store(false);
    engine.publishedState.store(State::Priming);
    engine.configured = true;
    engine.accepting.store(true, std::memory_order_release);
    if (engine.error.load(std::memory_order_acquire) != Result::Ok)
    {
        close();
        return fail(Result::DeviceError);
    }
    return Result::Ok;
}

Result AudioEngine::start()
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.stopping.load()) return Result::Stopped;
    if (engine.error.load() != Result::Ok) return engine.error.load();
    if (engine.options.output == OutputMode::Headless) return Result::Ok;
    if (engine.started.load()) return Result::Ok;
    engine.started.store(true, std::memory_order_release);
    if (engine.deviceInitialized && ma_device_start(&engine.device) != MA_SUCCESS)
    {
        engine.started.store(false);
        engine.accepting.store(false);
        engine.error.store(Result::DeviceError);
        engine.publishedState.store(State::Error);
        return Result::DeviceError;
    }
    return Result::Ok;
}

Result AudioEngine::handover(Role role, const Format& source, Generation generation) noexcept
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.appliedFormat.load(std::memory_order_acquire) != engine.formatGeneration)
        return Result::ControlQueueFull;
    if (!validFormat(source)) return Result::InvalidFormat;
    if (generation.stream <= engine.stream.load() || generation.format <= engine.formatGeneration)
        return Result::StaleGeneration;
    const auto required = (static_cast<std::uint64_t>(engine.reserveFrames + engine.primeFrames) *
        source.sampleRate + engine.output.sampleRate - 1) / engine.output.sampleRate + 32;
    if (engine.options.queueFrames < required) return Result::InvalidArgument;
    Impl::Command command;
    command.type = Impl::CommandType::Handover;
    command.source = source;
    command.token = generation.stream;
    command.format = generation.format;
    const auto matrixResult = role == Role::Music ?
        musicMatrix(source, engine.output, engine.options.musicUpmixHeadroom, command.matrix) :
        role == Role::Object ? objectMatrix(source, engine.output, 0.f, 1.f, command.matrix) : Result::InvalidArgument;
    if (matrixResult != Result::Ok) return matrixResult;
    const auto config = ma_linear_resampler_config_init(
        ma_format_f32, source.channels, source.sampleRate, engine.output.sampleRate);
    if (ma_linear_resampler_init(&config, &engine.handoverResampler) != MA_SUCCESS)
        return Result::ResamplerError;
    const auto result = engine.command(command);
    if (result == Result::Ok)
    {
        engine.role = role;
        engine.formatGeneration = generation.format;
        engine.stream.store(generation.stream, std::memory_order_release);
        engine.controlEos = false;
        engine.accepting.store(true, std::memory_order_release);
    }
    return result;
}

Result AudioEngine::enqueuePCM(const std::int16_t* samples, std::uint32_t frames,
                               std::int64_t ptsMicroseconds, Generation generation) noexcept
{
    auto& engine = *mImpl;
    const auto reject = [&engine](Result result)
    {
        engine.rejected.fetch_add(1, std::memory_order_relaxed);
        return result;
    };
    if (!engine.configured) return reject(Result::NotConfigured);
    if (!engine.accepting.load(std::memory_order_acquire))
        return reject(engine.stopping.load() ? Result::Stopped :
                      engine.error.load() != Result::Ok ? engine.error.load() : Result::EndOfStream);
    if (generation.stream != engine.stream.load(std::memory_order_acquire) ||
        generation.format != engine.formatGeneration) return reject(Result::StaleGeneration);
    if (engine.appliedFormat.load(std::memory_order_acquire) != generation.format)
        return reject(Result::QueueFull);
    if (!samples || frames == 0 || frames > engine.options.queueFrames || ptsMicroseconds < 0 ||
        ptsMicroseconds > std::numeric_limits<std::int64_t>::max() - 10000000)
        return reject(Result::InvalidArgument);
    const auto position = engine.write.load(std::memory_order_relaxed);
    if (position - engine.read.load(std::memory_order_acquire) + frames > engine.options.queueFrames)
        return reject(Result::QueueFull);
    if (engine.producerStream != generation.stream)
    {
        engine.producerStream = generation.stream;
        engine.producerFrames = 0;
        engine.producerAnchor = ptsMicroseconds;
    }
    if (engine.producerFrames > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / 1000000)
        return reject(Result::InvalidArgument);
    const auto elapsed = static_cast<std::int64_t>(engine.producerFrames * 1000000 / engine.source.sampleRate);
    if (elapsed > std::numeric_limits<std::int64_t>::max() - engine.producerAnchor)
        return reject(Result::InvalidArgument);
    const auto expected = engine.producerAnchor + elapsed;
    if (std::abs(ptsMicroseconds - expected) > 2000) return reject(Result::PtsDiscontinuity);
    for (std::uint32_t index = 0; index < frames; ++index)
    {
        auto& frame = engine.pcm[(position + index) % engine.options.queueFrames];
        std::copy_n(samples + static_cast<std::size_t>(index) * engine.source.channels,
                    engine.source.channels, frame.samples.begin());
        frame.pts = ptsMicroseconds + static_cast<std::int64_t>(index) * 1000000 / engine.source.sampleRate;
        frame.stream = generation.stream;
    }
    engine.producerFrames += frames;
    engine.write.store(position + frames, std::memory_order_release);
    return Result::Ok;
}

Result AudioEngine::gain(float target, double durationSeconds, bool hardMute) noexcept
{
    auto& engine = *mImpl;
    if (!std::isfinite(target) || target < 0.f || target > 1.f ||
        !std::isfinite(durationSeconds) || durationSeconds < 0. || durationSeconds > 60.)
        return Result::InvalidArgument;
    if (!engine.configured) return Result::NotConfigured;
    if (hardMute)
    {
        engine.muteToken.store((engine.muteToken.load() + 2) | 1, std::memory_order_release);
        return Result::Ok;
    }
    Impl::Command command;
    command.type = Impl::CommandType::Gain;
    command.target = target;
    command.frames = static_cast<std::uint64_t>(std::llround(durationSeconds * engine.output.sampleRate));
    command.token = engine.muteToken.load(std::memory_order_acquire);
    return engine.command(command);
}

Result AudioEngine::transition(float target, double durationSeconds, std::uint64_t commandId) noexcept
{
    auto& engine = *mImpl;
    if (!std::isfinite(target) || target < 0.f || target > 1.f ||
        !std::isfinite(durationSeconds) || durationSeconds < 0. || durationSeconds > 60. ||
        commandId == 0 || commandId <= engine.lastTransitionId) return Result::InvalidArgument;
    Impl::Command command;
    command.type = Impl::CommandType::Transition;
    command.target = target;
    command.frames = static_cast<std::uint64_t>(std::llround(durationSeconds * engine.output.sampleRate));
    command.token = commandId;
    const auto result = engine.command(command);
    if (result == Result::Ok) engine.lastTransitionId = commandId;
    return result;
}

Result AudioEngine::pause() noexcept
{
    Impl::Command command;
    command.type = Impl::CommandType::Pause;
    return mImpl->command(command);
}

Result AudioEngine::resume() noexcept
{
    Impl::Command command;
    command.type = Impl::CommandType::Resume;
    return mImpl->command(command);
}

Result AudioEngine::flush(Generation nextGeneration) noexcept
{
    auto& engine = *mImpl;
    if (nextGeneration.stream <= engine.stream.load(std::memory_order_acquire) ||
        nextGeneration.format != engine.formatGeneration) return Result::StaleGeneration;
    Impl::Command command;
    command.type = Impl::CommandType::Flush;
    command.token = nextGeneration.stream;
    const auto result = engine.command(command);
    if (result == Result::Ok)
    {
        engine.stream.store(nextGeneration.stream, std::memory_order_release);
        engine.controlEos = false;
        engine.accepting.store(true, std::memory_order_release);
    }
    return result;
}

Result AudioEngine::drain() noexcept
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.stopping.load(std::memory_order_acquire)) return Result::Stopped;
    if (engine.controlEos) return Result::Ok;
    Impl::Command command;
    command.type = Impl::CommandType::Drain;
    const auto result = engine.command(command);
    if (result == Result::Ok)
    {
        engine.controlEos = true;
        engine.accepting.store(false, std::memory_order_release);
    }
    return result;
}

Result AudioEngine::spatialDirection(float right, float forward) noexcept
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.role != Role::Object) return Result::InvalidArgument;
    if (engine.appliedFormat.load(std::memory_order_acquire) != engine.formatGeneration)
        return Result::ControlQueueFull;
    Impl::Command command;
    command.type = Impl::CommandType::Spatial;
    const auto result = objectMatrix(engine.source, engine.output, right, forward, command.matrix);
    return result == Result::Ok ? engine.command(command) : result;
}

Result AudioEngine::stop() noexcept
{
    auto& engine = *mImpl;
    engine.accepting.store(false, std::memory_order_release);
    engine.stopping.store(true, std::memory_order_release);
    engine.muteToken.fetch_or(1, std::memory_order_acq_rel);
    Result result = Result::Ok;
    if (engine.deviceInitialized && engine.started.load(std::memory_order_acquire) &&
        ma_device_stop(&engine.device) != MA_SUCCESS)
    {
        engine.error.store(Result::DeviceError);
        result = Result::DeviceError;
    }
    engine.started.store(false, std::memory_order_release);
    engine.publishedState.store(engine.configured ? State::Stopped : State::Closed);
    return result;
}

void AudioEngine::close() noexcept
{
    auto& engine = *mImpl;
    stop();
    if (engine.deviceInitialized)
    {
        ma_device_uninit(&engine.device);
        engine.deviceInitialized = false;
    }
    if (engine.resamplerInitialized)
    {
        ma_linear_resampler_uninit(&engine.resampler);
        engine.resamplerInitialized = false;
    }
    engine.configured = false;
    engine.publishedState.store(State::Closed);
}

Status AudioEngine::status() const noexcept
{
    const auto& engine = *mImpl;
    Status result;
    result.state = engine.publishedState.load(std::memory_order_acquire);
    result.error = engine.error.load(std::memory_order_acquire);
    result.generation = {engine.stream.load(std::memory_order_acquire), engine.formatGeneration};
    result.renderedGeneration = {engine.appliedStream.load(std::memory_order_acquire), engine.appliedFormat.load(std::memory_order_acquire)};
    result.outputMode = engine.options.output;
    result.output = engine.output;
    const auto read = engine.read.load(std::memory_order_acquire);
    const auto write = engine.write.load(std::memory_order_acquire);
    result.queuedFrames = static_cast<std::uint32_t>(std::min<std::uint64_t>(write - read, engine.options.queueFrames));
    result.reservedFrames = engine.publishedReserve.load(std::memory_order_relaxed);
    result.renderedFrames = engine.rendered.load(std::memory_order_relaxed);
    result.starvationCount = engine.starvations.load(std::memory_order_relaxed);
    result.rejectedBlocks = engine.rejected.load(std::memory_order_relaxed);
    result.lastSourcePts = engine.publishedPts.load(std::memory_order_relaxed);
    result.currentGain = engine.publishedGain.load(std::memory_order_relaxed);
    result.currentTransition = engine.publishedTransition.load(std::memory_order_relaxed);
    result.transitionConsumed = engine.transitionConsumed.load(std::memory_order_acquire);
    result.transitionCompleted = engine.transitionCompleted.load(std::memory_order_acquire);
    result.clockFrames = engine.clockFrames.load(std::memory_order_relaxed);
    result.deviceEpoch = engine.deviceEpoch;
    result.submittedFrames = engine.submittedFrames.load(std::memory_order_acquire);
    result.endpointFrames = engine.endpointFrames.load(std::memory_order_acquire);
    result.deviceStarted = engine.started.load(std::memory_order_acquire);
    result.hardMuted = (engine.muteToken.load(std::memory_order_acquire) & 1) != 0;
    result.endpointQualified = engine.configured && !engine.stopping.load(std::memory_order_acquire) &&
        engine.endpointQualified.load(std::memory_order_acquire);
    for (unsigned attempt = 0; attempt < 8; ++attempt)
    {
        const auto sequence = engine.fenceSequence.load(std::memory_order_acquire);
        if (sequence & 1) continue;
        const auto completed = engine.transitionCompleted.load(std::memory_order_relaxed);
        const auto transitionFrame = engine.transitionFenceFrame.load(std::memory_order_relaxed);
        const auto drainFrame = engine.drainFenceFrame.load(std::memory_order_relaxed);
        const auto consumed = engine.endpointFrames.load(std::memory_order_acquire);
        const auto stream = engine.appliedStream.load(std::memory_order_relaxed);
        const auto format = engine.appliedFormat.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (sequence != engine.fenceSequence.load(std::memory_order_relaxed)) continue;
        result.transitionFenceFrame = transitionFrame;
        result.drainFenceFrame = drainFrame;
        if (result.endpointQualified && result.error == Result::Ok &&
            stream == result.generation.stream && format == result.generation.format)
        {
            if (transitionFrame && consumed >= transitionFrame) result.endpointTransitionCompleted = completed;
            result.endpointDrainComplete = drainFrame && consumed >= drainFrame;
        }
        break;
    }
    result.error = engine.error.load(std::memory_order_acquire);
    if (result.error != Result::Ok || engine.stopping.load(std::memory_order_acquire))
    {
        result.endpointQualified = false;
        result.endpointTransitionCompleted = 0;
        result.endpointDrainComplete = false;
        if (result.error != Result::Ok) result.state = State::Error;
    }
    return result;
}

Result AudioEngine::advanceHeadlessEndpoint(Generation generation, std::uint64_t consumedFrames) noexcept
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.options.output != OutputMode::Headless) return Result::UnsupportedPlatform;
    if (generation.stream != engine.stream.load(std::memory_order_acquire) ||
        generation.stream != engine.appliedStream.load(std::memory_order_acquire) ||
        generation.format != engine.appliedFormat.load(std::memory_order_acquire)) return Result::StaleGeneration;
    if (consumedFrames < engine.endpointFrames.load(std::memory_order_relaxed) ||
        consumedFrames > engine.clockFrames.load(std::memory_order_acquire)) return Result::InvalidArgument;
    EndpointObservation observation;
    observation.deviceEpoch = engine.deviceEpoch;
    observation.paddingFrames = engine.submittedFrames.load(std::memory_order_acquire) - consumedFrames;
    observation.position = consumedFrames;
    observation.frequency = engine.output.sampleRate;
    return observeHeadlessEndpoint(generation, observation);
}

Result AudioEngine::observeHeadlessEndpoint(Generation generation, const EndpointObservation& observation) noexcept
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.options.output != OutputMode::Headless) return Result::UnsupportedPlatform;
    if (generation.stream != engine.stream.load(std::memory_order_acquire) ||
        generation.stream != engine.appliedStream.load(std::memory_order_acquire) ||
        generation.format != engine.appliedFormat.load(std::memory_order_acquire)) return Result::StaleGeneration;
    return engine.observeEndpoint(observation);
}

Result AudioEngine::render(float* interleavedOutput, std::uint32_t frames, bool submissionSucceeded) noexcept
{
    auto& engine = *mImpl;
    if (!engine.configured) return Result::NotConfigured;
    if (engine.options.output != OutputMode::Headless || !interleavedOutput ||
        frames > MaxQueueFrames) return Result::InvalidArgument;
    engine.renderFrames(interleavedOutput, frames);
    if (!engine.stopping.load(std::memory_order_acquire))
        return engine.submitFrames(frames, submissionSucceeded);
    return engine.error.load(std::memory_order_acquire);
}
}