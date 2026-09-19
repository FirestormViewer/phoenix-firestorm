#ifndef LL_LLVLCAUDIOBRIDGE_H
#define LL_LLVLCAUDIOBRIDGE_H

#include "llvlcaudio.h"
#if defined(_MSC_VER)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif
#include <vlc/vlc.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace llvlc
{
class PluginAudio final
{
public:
    struct Snapshot
    {
        Status engine{};
        const char* failure = nullptr;
        std::uint64_t gainIssued = 0;
        std::uint64_t discontinuities = 0;
    };

    explicit PluginAudio(const Options& options = {});
    ~PluginAudio();
    PluginAudio(const PluginAudio&) = delete;
    PluginAudio& operator=(const PluginAudio&) = delete;
    void attach(libvlc_media_player_t* player, Role role);
    void cancelAndStop();
    void closeAfterVLC();
    std::uint64_t gain(float target, double seconds, bool mute);
    std::uint64_t transition(float target, double seconds);
    bool spatial(float right, float forward);
    Snapshot snapshot();
    struct PlaybackReport
    {
        const char* state;
        const char* detail;
    };
    static PlaybackReport playbackReport(const Status& status, float buffering, bool paused);
    static bool transitionComplete(const Status& status, std::uint64_t command) noexcept;

private:
    friend class PluginAudioTest;
    enum class Kind { Setup, PCM, Pause, Resume, Flush, Drain, Cleanup, Gain, Transition, Spatial };
    struct Request
    {
        Kind kind = Kind::Cleanup;
        std::uint64_t epoch = 0;
        std::uint64_t ticket = 0;
        std::uint64_t revision = 0;
        unsigned rate = 0;
        unsigned channels = 0;
        unsigned frames = 0;
        std::int64_t pts = 0;
        float target = 0.f;
        float forward = 1.f;
        double seconds = 0.;
        bool mute = false;
        bool preserveTransition = false;
        std::array<std::int16_t, 1024 * MaxChannels> pcm{};
    };
    struct Context
    {
        PluginAudio* owner = nullptr;
        std::uint64_t epoch = 0;
        unsigned rate = 0;
        unsigned channels = 0;
        std::mutex producer;
    };
    static int setup(void** opaque, char* format, unsigned* rate, unsigned* channels);
    static void play(void* opaque, const void* samples, unsigned frames, std::int64_t pts);
    static void pause(void* opaque, std::int64_t);
    static void resume(void* opaque, std::int64_t);
    static void flush(void* opaque, std::int64_t);
    static void drain(void* opaque);
    static void cleanup(void* opaque);
    static int volume(void*, float, bool);
    static void event(void* opaque, Kind kind);
    void prepare(Role role);
    bool enqueue(Request& request);
    bool waitFor(std::uint64_t epoch, std::uint64_t ticket, bool setupRequest,
                 std::uint64_t revision = 0);
    std::uint64_t control(Request& request);
    void fail(const char* detail);
    void run();
    Result apply(const Request& request);

    AudioEngine mEngine;
    const Options mOptions;
    Context mContext;
    std::mutex mMutex;
    std::condition_variable mWake;
    std::array<Request, 64> mAudioQueue{};
    std::array<Request, 64> mControls{};
    std::uint64_t mAudioRead = 0;
    std::uint64_t mAudioWrite = 0;
    std::uint64_t mControlRead = 0;
    std::uint64_t mControlWrite = 0;
    std::uint64_t mTicket = 0;
    std::uint64_t mEpoch = 0;
    std::uint64_t mRevision = 0;
    std::uint64_t mSetupDone = 0;
    std::uint64_t mDrainDone = 0;
    bool mCancelled = true;
    bool mCancelDone = true;
    bool mCloseRequested = false;
    bool mQuit = false;
    Role mRole = Role::Music;
    Snapshot mSnapshot{};
    Generation mGeneration{};
    Format mSource{};
    bool mConfigured = false;
    bool mSourceReady = false;
    std::uint64_t mSetupTicket = 0;
    std::size_t mSetupStage = 0;
    std::array<Request, 60> mStagedControls{};
    std::size_t mStagedCount = 0;
    float mGain = 0.f;
    double mGainSeconds = 0.;
    bool mMute = false;
    float mRight = 0.f;
    float mForward = 1.f;
    std::uint64_t mPendingDrain = 0;
    std::thread mWorker;
};
}
#endif