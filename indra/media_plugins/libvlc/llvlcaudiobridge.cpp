#include "llvlcaudiobridge.h"
#include <vlc/libvlc_version.h>
#include <algorithm>
#include <cstring>
#include <limits>

static_assert(LIBVLC_VERSION_MAJOR == 3 && LIBVLC_VERSION_MINOR == 0 &&
              LIBVLC_VERSION_REVISION == 21, "Reaudit amem ABI and masks for this VLC version.");
static_assert(sizeof(void*) == 8, "The amem callback ABI adapter is qualified only for Windows x64.");

namespace llvlc
{
namespace
{
const char* resultDetail(Result result)
{
    switch (result)
    {
    case Result::Ok: return nullptr;
    case Result::NotConfigured: return "engine_not_configured";
    case Result::InvalidFormat: return "invalid_format";
    case Result::InvalidArgument: return "invalid_audio_argument";
    case Result::UnsupportedLayout: return "unsupported_layout";
    case Result::UnsupportedPlatform: return "unsupported_platform";
    case Result::QueueFull: return "pcm_backpressure_timeout";
    case Result::ControlQueueFull: return "control_queue_full";
    case Result::StaleGeneration: return "stale_decoder_generation";
    case Result::EndOfStream: return "pcm_after_drain";
    case Result::Stopped: return "engine_stopped";
    case Result::DeviceError: return "audio_device_error";
    case Result::ResamplerError: return "resampler_error";
    case Result::PtsDiscontinuity: return "pts_discontinuity";
    }
    return "unknown_audio_error";
}
}

PluginAudio::PluginAudio(const Options& options) : mOptions(options), mWorker(&PluginAudio::run, this)
{
    mContext.owner = this;
}

PluginAudio::~PluginAudio()
{
    cancelAndStop();
    {
        std::lock_guard<std::mutex> guard(mMutex);
        mQuit = true;
        mWake.notify_all();
    }
    mWorker.join();
}

void PluginAudio::prepare(Role role)
{
    {
        std::lock_guard<std::mutex> guard(mMutex);
        ++mEpoch;
        mContext.epoch = mEpoch;
        mContext.rate = mContext.channels = 0;
        mRole = role;
        mCancelled = false;
        mCancelDone = false;
        mSnapshot = {};
        mAudioRead = mAudioWrite = mControlRead = mControlWrite = 0;
        mSetupDone = mDrainDone = 0;
    }
}

void PluginAudio::attach(libvlc_media_player_t* player, Role role)
{
    prepare(role);
    libvlc_audio_set_callbacks(player, play, pause, resume, flush, drain, &mContext);
    libvlc_audio_set_format_callbacks(player, setup, cleanup);
    libvlc_audio_set_volume_callback(player, reinterpret_cast<libvlc_audio_set_volume_cb>(volume));
}

void PluginAudio::cancelAndStop()
{
    std::unique_lock<std::mutex> guard(mMutex);
    mCancelled = true;
    mWake.notify_all();
    mWake.wait(guard, [this] { return mCancelDone; });
}

void PluginAudio::closeAfterVLC()
{
    std::unique_lock<std::mutex> guard(mMutex);
    mCloseRequested = true;
    mWake.notify_all();
    mWake.wait(guard, [this] { return !mCloseRequested; });
}

PluginAudio::Snapshot PluginAudio::snapshot()
{
    std::lock_guard<std::mutex> guard(mMutex);
    return mSnapshot;
}

PluginAudio::PlaybackReport PluginAudio::playbackReport(const Status& status, float buffering, bool paused)
{
    if (status.state == State::Error) return {"failed", "engine_error"};
    if (status.state == State::Paused || paused) return {"paused", "pcm_paused_transition_clock_active"};
    if (status.state == State::Drained)
        return {"drained", status.endpointDrainComplete ? "endpoint_complete" : "submitted_not_endpoint_complete"};
    if (buffering < 100.f || status.state == State::Starving ||
        (status.state == State::Priming && status.starvationCount != 0))
        return {"buffering", buffering < 100.f ? "vlc_buffering" :
                status.reservedFrames ? "reserved_pcm_ramp" : "queue_empty_repriming"};
    if (status.state == State::Playing)
        return {"running", status.queuedFrames ? "pcm_queued" : "pcm_reserve_only"};
    return {"priming", "audio_only_no_av_sync"};
}

bool PluginAudio::transitionComplete(const Status& status, std::uint64_t command) noexcept
{
    return command != 0 && status.error == Result::Ok && status.endpointQualified &&
        status.state != State::Stopped && status.state != State::Closed && status.state != State::Error &&
        status.generation.stream == status.renderedGeneration.stream &&
        status.generation.format == status.renderedGeneration.format &&
        status.transitionConsumed == command && status.endpointTransitionCompleted == command;
}

void PluginAudio::fail(const char* detail)
{
    std::lock_guard<std::mutex> guard(mMutex);
    if (!mSnapshot.failure) mSnapshot.failure = detail;
    mCancelled = true;
    mWake.notify_all();
}

bool PluginAudio::enqueue(Request& request)
{
    std::unique_lock<std::mutex> guard(mMutex);
    const bool ready = mWake.wait_for(guard, std::chrono::seconds(5), [this, &request]
    {
        return mCancelled || request.epoch != mEpoch ||
               (request.kind != Kind::Setup && request.revision != mRevision) ||
               mAudioWrite - mAudioRead < mAudioQueue.size();
    });
    if (mCancelled || request.epoch != mEpoch ||
        (request.kind != Kind::Setup && request.revision != mRevision)) return false;
    if (!ready)
    {
        guard.unlock();
        fail("callback_backpressure_timeout");
        return false;
    }
    request.ticket = ++mTicket;
    mAudioQueue[mAudioWrite++ % mAudioQueue.size()] = request;
    mWake.notify_all();
    return true;
}

bool PluginAudio::waitFor(std::uint64_t epoch, std::uint64_t ticket, bool setupRequest,
                          std::uint64_t revision)
{
    std::unique_lock<std::mutex> guard(mMutex);
    const bool ready = mWake.wait_for(guard, std::chrono::seconds(10), [this, epoch, ticket, setupRequest, revision]
    {
        return mCancelled || epoch != mEpoch || (!setupRequest && revision != mRevision) ||
               (setupRequest ? mSetupDone : mDrainDone) >= ticket;
    });
    if (mCancelled || epoch != mEpoch || (!setupRequest && revision != mRevision)) return false;
    if (!ready)
    {
        guard.unlock();
        fail(setupRequest ? "setup_timeout" : "drain_timeout");
        return false;
    }
    return true;
}

int PluginAudio::setup(void** opaque, char* format, unsigned* rate, unsigned* channels)
{
    auto& context = *static_cast<Context*>(*opaque);
    std::lock_guard<std::mutex> producer(context.producer);
    if (*channels < 1 || *channels > 2 || *rate < 8000 || *rate > 192000)
    {
        context.owner->fail("source_layout_or_rate_unqualified");
        return -1;
    }
    Request request;
    request.kind = Kind::Setup;
    request.epoch = context.epoch;
    request.rate = *rate;
    request.channels = *channels;
    if (!context.owner->enqueue(request) || !context.owner->waitFor(context.epoch, request.ticket, true))
        return -1;
    std::memcpy(format, "S16N", 4);
    context.rate = *rate;
    context.channels = *channels;
    return 0;
}

void PluginAudio::play(void* opaque, const void* samples, unsigned frames, std::int64_t pts)
{
    auto& context = *static_cast<Context*>(opaque);
    Request request;
    request.kind = Kind::PCM;
    request.epoch = context.epoch;
    {
        std::lock_guard<std::mutex> guard(context.owner->mMutex);
        request.revision = context.owner->mRevision;
    }
    std::lock_guard<std::mutex> producer(context.producer);
    if (!samples || !context.rate || !context.channels || pts < 0 ||
        pts > std::numeric_limits<std::int64_t>::max() -
              static_cast<std::int64_t>(frames) * 1000000 / context.rate - 10000000)
    {
        context.owner->fail("invalid_pcm_or_pts");
        return;
    }
    const auto* input = static_cast<const std::int16_t*>(samples);
    for (unsigned offset = 0; offset < frames; offset += request.frames)
    {
        request.frames = std::min(1024u, frames - offset);
        request.pts = pts + static_cast<std::int64_t>(offset) * 1000000 / context.rate;
        std::copy_n(input + static_cast<std::size_t>(offset) * context.channels,
                    request.frames * context.channels, request.pcm.begin());
        if (!context.owner->enqueue(request)) return;
    }
}

void PluginAudio::event(void* opaque, Kind kind)
{
    auto& context = *static_cast<Context*>(opaque);
    Request request;
    request.kind = kind;
    request.epoch = context.epoch;
    if (kind == Kind::Pause || kind == Kind::Resume || kind == Kind::Flush)
    {
        context.owner->control(request);
        return;
    }
    {
        std::lock_guard<std::mutex> guard(context.owner->mMutex);
        request.revision = context.owner->mRevision;
    }
    std::unique_lock<std::mutex> producer(context.producer);
    const bool accepted = context.owner->enqueue(request);
    producer.unlock();
    if (accepted && kind == Kind::Drain)
        context.owner->waitFor(context.epoch, request.ticket, false, request.revision);
}

void PluginAudio::pause(void* opaque, std::int64_t) { event(opaque, Kind::Pause); }
void PluginAudio::resume(void* opaque, std::int64_t) { event(opaque, Kind::Resume); }
void PluginAudio::flush(void* opaque, std::int64_t) { event(opaque, Kind::Flush); }
void PluginAudio::drain(void* opaque) { event(opaque, Kind::Drain); }
void PluginAudio::cleanup(void* opaque) { event(opaque, Kind::Cleanup); }
int PluginAudio::volume(void*, float, bool) { return 0; }

std::uint64_t PluginAudio::control(Request& request)
{
    std::unique_lock<std::mutex> guard(mMutex);
    if (mCancelled || (request.epoch && request.epoch != mEpoch)) return 0;
    if (mControlWrite - mControlRead == mControls.size())
    {
        guard.unlock();
        fail("adapter_control_queue_full");
        return 0;
    }
    request.epoch = mEpoch;
    request.ticket = ++mTicket;
    if (request.kind == Kind::Flush)
    {
        ++mRevision;
        mAudioRead = mAudioWrite;
    }
    mControls[mControlWrite++ % mControls.size()] = request;
    mWake.notify_all();
    return request.ticket;
}

std::uint64_t PluginAudio::gain(float target, double seconds, bool mute)
{
    Request request;
    request.kind = Kind::Gain;
    request.target = target;
    request.seconds = seconds;
    request.mute = mute;
    return control(request);
}

bool PluginAudio::spatial(float right, float forward)
{
    Request request;
    request.kind = Kind::Spatial;
    request.target = right;
    request.forward = forward;
    return control(request) != 0;
}

std::uint64_t PluginAudio::transition(float target, double seconds)
{
    Request request;
    request.kind = Kind::Transition;
    request.target = target;
    request.seconds = seconds;
    return control(request);
}

Result PluginAudio::apply(const Request& request)
{
    switch (request.kind)
    {
    case Kind::Setup:
    {
        if (mGeneration.stream == std::numeric_limits<std::uint64_t>::max() ||
            mGeneration.format == std::numeric_limits<std::uint64_t>::max())
            return Result::StaleGeneration;
        if (mSetupTicket != request.ticket)
        {
            mSource.sampleRate = request.rate;
            mSource.channels = request.channels;
            mSource.speakers = {};
            mSource.speakers[0] = request.channels == 1 ? Speaker::FrontCenter : Speaker::FrontLeft;
            if (request.channels == 2) mSource.speakers[1] = Speaker::FrontRight;
            const Generation generation{mGeneration.stream + 1, mGeneration.format + 1};
            const auto result = mConfigured ? mEngine.handover(mRole, mSource, generation) :
                mEngine.configure(mRole, mSource, generation, mOptions);
            if (result != Result::Ok) return result;
            mGeneration = generation;
            mConfigured = true;
            mSetupTicket = request.ticket;
            mSetupStage = 0;
        }
        if (mEngine.status().renderedGeneration.format != mGeneration.format) return Result::ControlQueueFull;
        std::uint64_t lastGain = 0;
        if (mSetupStage == 0)
        {
            const auto result = mEngine.gain(mGain, 0., mMute);
            if (result != Result::Ok) return result;
            ++mSetupStage;
        }
        if (mSetupStage == 1)
        {
            const auto result = mRole == Role::Object ? mEngine.spatialDirection(mRight, mForward) : Result::Ok;
            if (result != Result::Ok) return result;
            ++mSetupStage;
        }
        while (mSetupStage - 2 < mStagedCount)
        {
            const auto& staged = mStagedControls[mSetupStage - 2];
            const auto result = staged.kind == Kind::Gain ? mEngine.gain(staged.target, staged.seconds, staged.mute) :
                     staged.kind == Kind::Transition ? mEngine.transition(staged.target, staged.seconds, staged.ticket) :
                     mEngine.spatialDirection(staged.target, staged.forward);
            if (result != Result::Ok) return result;
            if (staged.kind == Kind::Gain) lastGain = staged.ticket;
            ++mSetupStage;
        }
        mStagedCount = 0;
        mSourceReady = true;
        {
            std::lock_guard<std::mutex> guard(mMutex);
            if (lastGain) mSnapshot.gainIssued = lastGain;
        }
        return mEngine.start();
    }
    case Kind::PCM:
        return mEngine.enqueuePCM(request.pcm.data(), request.frames, request.pts, mGeneration);
    case Kind::Pause: return mEngine.pause();
    case Kind::Resume: return mEngine.resume();
    case Kind::Flush:
    {
        if (mGeneration.stream == std::numeric_limits<std::uint64_t>::max()) return Result::StaleGeneration;
        const auto result = mEngine.flush({mGeneration.stream + 1, mGeneration.format});
        if (result == Result::Ok) { ++mGeneration.stream; mPendingDrain = 0; }
        return result;
    }
    case Kind::Drain:
    {
        const auto result = mEngine.drain();
        if (result == Result::Ok) mPendingDrain = request.ticket;
        return result;
    }
    case Kind::Cleanup: return Result::Ok;
    case Kind::Transition:
        if (!mSourceReady)
        {
            if (mStagedCount == mStagedControls.size()) return Result::ControlQueueFull;
            mStagedControls[mStagedCount++] = request;
            return Result::Ok;
        }
        return mEngine.transition(request.target, request.seconds, request.ticket);
    case Kind::Gain:
    {
        if (!mSourceReady)
        {
            if (mStagedCount == mStagedControls.size()) return Result::ControlQueueFull;
            mStagedControls[mStagedCount++] = request;
        }
        const auto result = mSourceReady ? mEngine.gain(request.target, request.seconds, request.mute) : Result::Ok;
        if (result == Result::Ok)
        {
            mGain = request.target;
            mGainSeconds = request.seconds;
            mMute = request.mute;
        }
        return result;
    }
    case Kind::Spatial:
    {
        if (!mSourceReady)
        {
            if (mStagedCount == mStagedControls.size()) return Result::ControlQueueFull;
            mStagedControls[mStagedCount++] = request;
        }
        const auto result = mSourceReady ? mEngine.spatialDirection(request.target, request.forward) : Result::Ok;
        if (result == Result::Ok) { mRight = request.target; mForward = request.forward; }
        return result;
    }
    }
    return Result::InvalidArgument;
}

void PluginAudio::run()
{
    Request pending;
    bool havePending = false;
    auto pendingSince = std::chrono::steady_clock::now();
    Request pendingControl;
    bool havePendingControl = false;
    auto controlSince = pendingSince;
    for (;;)
    {
        Request request;
        bool haveRequest = false;
        bool isControl = false;
        {
            std::unique_lock<std::mutex> guard(mMutex);
            mWake.wait_for(guard, std::chrono::milliseconds(2), [this, havePending, havePendingControl]
            {
                return mQuit || mCloseRequested || (mCancelled && !mCancelDone) ||
                       (!mCancelled && ((!havePendingControl && mControlRead != mControlWrite) ||
                        (!havePendingControl && !havePending && mAudioRead != mAudioWrite)));
            });
            if (mQuit) break;
            if (mCancelled && !mCancelDone)
            {
                guard.unlock();
                const auto result = mConfigured ? mEngine.gain(mGain, 0., true) : Result::Ok;
                const auto stopped = mEngine.status();
                if (stopped.error != Result::Ok || stopped.state == State::Stopped)
                {
                    mEngine.close();
                    mConfigured = false;
                }
                guard.lock();
                mSnapshot.engine = stopped;
                if (result != Result::Ok && !mSnapshot.failure) mSnapshot.failure = resultDetail(result);
                mAudioRead = mAudioWrite;
                mControlRead = mControlWrite;
                havePending = false;
                havePendingControl = false;
                mPendingDrain = 0;
                mStagedCount = 0;
                mSourceReady = false;
                mCancelDone = true;
                mWake.notify_all();
            }
            if (mCloseRequested)
            {
                guard.unlock();
                mEngine.close();
                mConfigured = false;
                const auto closed = mEngine.status();
                guard.lock();
                mSnapshot.engine = closed;
                mCloseRequested = false;
                mWake.notify_all();
            }
            if (mCancelled) continue;
            if (havePendingControl)
            {
                request = pendingControl;
                haveRequest = isControl = true;
            }
            else if (mControlRead != mControlWrite)
            {
                request = mControls[mControlRead++ % mControls.size()];
                haveRequest = isControl = true;
                controlSince = std::chrono::steady_clock::now();
            }
            else if (havePending) { request = pending; haveRequest = true; }
            else if (mAudioRead != mAudioWrite)
            {
                request = mAudioQueue[mAudioRead++ % mAudioQueue.size()];
                haveRequest = true;
                pendingSince = std::chrono::steady_clock::now();
            }
            mWake.notify_all();
            if (haveRequest && request.epoch != mEpoch) continue;
            if (haveRequest && !isControl && request.kind != Kind::Setup && request.revision != mRevision)
            {
                havePending = false;
                continue;
            }
        }
        if (haveRequest)
        {
            auto result = apply(request);
            if (result == Result::PtsDiscontinuity && request.kind == Kind::PCM)
            {
                Request discontinuity;
                discontinuity.kind = Kind::Flush;
                result = apply(discontinuity);
                if (result == Result::Ok)
                {
                    std::lock_guard<std::mutex> guard(mMutex);
                    ++mSnapshot.discontinuities;
                    result = Result::QueueFull;
                }
            }
            if (result == Result::QueueFull || result == Result::ControlQueueFull)
            {
                if (isControl)
                {
                    pendingControl = request;
                    havePendingControl = true;
                }
                else { pending = request; havePending = true; }
                if (!isControl && mEngine.status().state == State::Paused)
                    pendingSince = std::chrono::steady_clock::now();
                if (std::chrono::steady_clock::now() - (isControl ? controlSince : pendingSince) >
                    std::chrono::seconds(5))
                    fail(resultDetail(result));
            }
            else if (result != Result::Ok) fail(resultDetail(result));
            else
            {
                if (!isControl || request.kind == Kind::Flush) havePending = false;
                if (isControl) havePendingControl = false;
                std::lock_guard<std::mutex> guard(mMutex);
                if (request.kind == Kind::Setup) mSetupDone = request.ticket;
                if (request.kind == Kind::Gain && mConfigured) mSnapshot.gainIssued = request.ticket;
                mWake.notify_all();
            }
        }
        const auto status = mEngine.status();
        {
            std::lock_guard<std::mutex> guard(mMutex);
            mSnapshot.engine = status;
            if (mPendingDrain && status.endpointDrainComplete)
            {
                mDrainDone = mPendingDrain;
                mPendingDrain = 0;
                mWake.notify_all();
            }
        }
        if (mPendingDrain && status.state == State::Drained && !status.endpointQualified)
            fail("endpoint_tail_unqualified");
        if (mConfigured && status.error != Result::Ok) fail(resultDetail(status.error));
    }
    mEngine.close();
}
}