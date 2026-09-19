#ifndef LL_LLVLCAUDIO_H
#define LL_LLVLCAUDIO_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace llvlc
{
constexpr std::uint32_t MaxChannels = 8;
constexpr std::uint32_t MaxQueueFrames = 32768;
constexpr std::uint32_t MaxReserveFrames = 8192;

enum class Role { Music, Object };
enum class OutputMode { Wasapi, Headless };
enum class Speaker : std::uint8_t
{
    Unknown, Mono, FrontLeft, FrontRight, FrontCenter, LFE,
    BackLeft, BackRight, FrontLeftCenter, FrontRightCenter,
    BackCenter, SideLeft, SideRight
};
enum class Result
{
    Ok, NotConfigured, InvalidFormat, InvalidArgument, UnsupportedLayout,
    UnsupportedPlatform, QueueFull, ControlQueueFull, StaleGeneration,
    EndOfStream, Stopped, DeviceError, ResamplerError, PtsDiscontinuity
};
enum class State { Closed, Priming, Playing, Starving, Paused, Drained, Stopped, Error };
enum class DeviceFailure : std::uint32_t
{
    None, Notification, Timeline, Submission, FrameOverflow, UnexpectedStop,
    ClockService, ClockFrequency, StreamLatency, Padding, ClockPosition,
    GetBuffer, ReleaseBuffer, Start, Wait
};

struct Generation
{
    std::uint64_t stream = 0;
    std::uint64_t format = 0;
};

struct Format
{
    std::uint32_t sampleRate = 48000;
    std::uint32_t channels = 2;
    std::array<Speaker, MaxChannels> speakers{Speaker::FrontLeft, Speaker::FrontRight};
};

struct Options
{
    OutputMode output = OutputMode::Wasapi;
    Format headlessOutput{};
    std::uint32_t queueFrames = MaxQueueFrames;
    std::uint32_t reserveMilliseconds = 5;
    std::uint32_t primeMilliseconds = 20;
    float musicUpmixHeadroom = 0.70710678f;
};

struct Status
{
    State state = State::Closed;
    Result error = Result::Ok;
    DeviceFailure deviceFailure = DeviceFailure::None;
    std::uint32_t deviceFailureCode = 0;
    Generation generation{};
    Generation renderedGeneration{};
    OutputMode outputMode = OutputMode::Wasapi;
    Format output{};
    std::uint32_t queuedFrames = 0;
    std::uint32_t reservedFrames = 0;
    std::uint64_t renderedFrames = 0;
    std::uint64_t starvationCount = 0;
    std::uint64_t rejectedBlocks = 0;
    std::int64_t lastSourcePts = -1;
    float currentGain = 1.f;
    float currentTransition = 1.f;
    std::uint64_t transitionConsumed = 0;
    std::uint64_t transitionCompleted = 0;
    std::uint64_t clockFrames = 0;
    std::uint64_t deviceEpoch = 0;
    std::uint64_t submittedFrames = 0;
    std::uint64_t endpointFrames = 0;
    std::uint64_t endpointTransitionCompleted = 0;
    std::uint64_t transitionFenceFrame = 0;
    std::uint64_t drainFenceFrame = 0;
    bool endpointDrainComplete = false;
    bool endpointQualified = false;
    bool deviceStarted = false;
    bool hardMuted = false;
};

struct EndpointObservation
{
    std::uint64_t deviceEpoch = 0;
    std::uint64_t paddingFrames = 0;
    std::uint64_t position = 0;
    std::uint64_t frequency = 0;
    std::uint64_t latency100ns = 0;
    bool valid = true;
};

class AudioEngine final
{
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    Result configure(Role role, const Format& source, Generation generation,
                     const Options& options = {});
    Result start();
    Result handover(Role role, const Format& source, Generation generation) noexcept;
    Result enqueuePCM(const std::int16_t* samples, std::uint32_t frames,
                      std::int64_t ptsMicroseconds, Generation generation) noexcept;
    Result gain(float target, double durationSeconds, bool hardMute = false) noexcept;
    Result transition(float target, double durationSeconds, std::uint64_t commandId) noexcept;
    Result pause() noexcept;
    Result resume() noexcept;
    Result flush(Generation nextGeneration, bool preserveTransition = false) noexcept;
    Result drain() noexcept;
    Result spatialDirection(float right, float forward) noexcept;
    Result stop() noexcept;
    Status status() const noexcept;
    void close() noexcept;

    Result render(float* interleavedOutput, std::uint32_t frames, bool submissionSucceeded = true) noexcept;
    Result advanceHeadlessEndpoint(Generation generation, std::uint64_t consumedFrames) noexcept;
    Result observeHeadlessEndpoint(Generation generation, const EndpointObservation& observation) noexcept;

private:
    friend struct WasapiEndpoint;
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}

#endif