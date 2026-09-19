#include "llvlcaudiobridge.h"
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace llvlc
{
class PluginAudioTest
{
    static void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    static std::unique_ptr<PluginAudio> make()
    {
        Options options;
        options.output = OutputMode::Headless;
        auto audio = std::make_unique<PluginAudio>(options);
        audio->prepare(Role::Music);
        return audio;
    }

    static int setup(PluginAudio& audio, unsigned channels = 2)
    {
        void* context = &audio.mContext;
        char format[4] = {};
        unsigned rate = 48000;
        const int result = PluginAudio::setup(&context, format, &rate, &channels);
        if (result == 0)
        {
            require(std::string(format, 4) == "S16N", "setup must negotiate S16N");
            require(rate == 48000 && channels == 2, "setup must not fake rate or channel count");
        }
        return result;
    }

    static float render(PluginAudio& audio, unsigned frames = 256)
    {
        std::array<float, 512> output{};
        require(audio.mEngine.render(output.data(), frames) == Result::Ok, "headless render failed");
        return output[0];
    }

    template<class Predicate>
    static void until(PluginAudio& audio, Predicate predicate, bool pump)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate())
        {
            require(std::chrono::steady_clock::now() < deadline, "bridge test deadline exceeded");
            if (pump) render(audio);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    static void largeBlockAndDrain()
    {
        auto audio = make();
        require(audio->gain(0.f, 0., false) != 0, "initial gain rejected");
        require(audio->gain(1.f, .01, false) != 0, "initial fade rejected");
        require(setup(*audio) == 0, "setup failed");
        constexpr unsigned frames = 70000;
        const std::vector<std::int16_t> input(frames * 2, 1000);
        auto producer = std::async(std::launch::async, [&]
        {
            PluginAudio::play(&audio->mContext, input.data(), frames, 1000000);
        });
        until(*audio, [&] { return audio->snapshot().engine.queuedFrames >= 2048; }, false);
        require(render(*audio, 1) == 0.f, "pre-setup fade lost its initial zero");
        float endpoint = 0.f;
        for (unsigned index = 0; index < 480; ++index) endpoint = render(*audio, 1);
        require(std::abs(endpoint - 1000.f / 32768.f) < 1e-6f, "fade duration changed during setup");
        until(*audio, [&] { return producer.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }, true);
        producer.get();
        auto draining = std::async(std::launch::async, [&] { PluginAudio::drain(&audio->mContext); });
        until(*audio, [&] { return audio->snapshot().engine.state == State::Drained; }, true);
        require(draining.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
            "rendered drain acknowledged before endpoint consumption");
        const auto rendered = audio->snapshot().engine;
        require(audio->mEngine.advanceHeadlessEndpoint(rendered.generation, rendered.clockFrames) == Result::Ok,
            "headless endpoint consumption rejected");
        until(*audio, [&] { return draining.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }, false);
        draining.get();
        const auto snapshot = audio->snapshot();
        require(snapshot.engine.renderedFrames == frames, "large VLC block lost or duplicated PCM");
        const auto expectedPts = 1000000 + static_cast<std::int64_t>(frames - 1) * 1000000 / 48000;
        require(std::abs(snapshot.engine.lastSourcePts - expectedPts) <= 1, "split PTS changed clock or units");
        require(!snapshot.engine.deviceStarted && snapshot.engine.outputMode == OutputMode::Headless,
                "tests must never start an audio device");
        require(!snapshot.failure, "submitted drain is not a device failure");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

    static void pauseFlushResume()
    {
        auto audio = make();
        require(audio->gain(1.f, 0., false) != 0 && setup(*audio) == 0, "setup failed");
        const std::vector<std::int16_t> input(4096 * 2, 1000);
        PluginAudio::play(&audio->mContext, input.data(), 4096, 1000000);
        until(*audio, [&] { return audio->snapshot().engine.queuedFrames == 4096; }, false);
        PluginAudio::pause(&audio->mContext, 1000000);
        until(*audio, [&] { return audio->snapshot().engine.state == State::Paused; }, true);
        require(render(*audio) == 0.f, "pause did not silence the callback");
        const auto stream = audio->snapshot().engine.generation.stream;
        PluginAudio::flush(&audio->mContext, std::numeric_limits<std::int64_t>::min());
        until(*audio, [&] { return audio->snapshot().engine.renderedGeneration.stream > stream; }, true);
        require(audio->snapshot().engine.state == State::Paused, "flush lost pause state");
        PluginAudio::resume(&audio->mContext, 9000000);
        PluginAudio::play(&audio->mContext, input.data(), 4096, 9000000);
        until(*audio, [&] { return audio->snapshot().engine.lastSourcePts >= 9000000; }, true);
        require(!audio->snapshot().failure, "flush/resume unexpectedly failed");
        require(PluginAudio::volume(&audio->mContext, .01f, true) == 0, "amem volume ABI must return success");
        require(audio->snapshot().engine.currentGain == 1.f, "VLC volume callback applied a second gain");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

    static void saturatedCancellation()
    {
        auto audio = make();
        require(setup(*audio) == 0, "setup failed");
        PluginAudio::Request stale;
        stale.epoch = audio->mContext.epoch - 1;
        require(!audio->enqueue(stale), "stale callback epoch was accepted");
        const std::vector<std::int16_t> input(500000 * 2, 1000);
        auto producer = std::async(std::launch::async, [&]
        {
            PluginAudio::play(&audio->mContext, input.data(), 500000, 1000000);
        });
        until(*audio, [&] { return audio->snapshot().engine.queuedFrames >= 32000; }, false);
        require(producer.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
                "saturated producer silently dropped its remaining PCM");
        audio->cancelAndStop();
        require(producer.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
                "cancellation failed to release a blocked producer");
        producer.get();
        audio->closeAfterVLC();
    }

    static void saturatedRecovery()
    {
        auto audio = make();
        require(setup(*audio) == 0, "recovery setup failed");
        PluginAudio::pause(&audio->mContext, 0);
        until(*audio, [&] { return audio->snapshot().engine.state == State::Paused; }, true);
        const std::vector<std::int16_t> input(500000 * 2, 1000);
        auto producer = std::async(std::launch::async, [&]
        {
            PluginAudio::play(&audio->mContext, input.data(), 500000, 1000000);
        });
        until(*audio, [&]
        {
            std::lock_guard<std::mutex> guard(audio->mMutex);
            return audio->mAudioWrite - audio->mAudioRead == audio->mAudioQueue.size();
        }, false);
        auto resume = std::async(std::launch::async, [&] { PluginAudio::resume(&audio->mContext, 0); });
        require(resume.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "resume blocked behind saturated producer");
        resume.get();
        auto flush = std::async(std::launch::async, [&] { PluginAudio::flush(&audio->mContext, 0); });
        require(flush.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "flush blocked behind saturated producer");
        flush.get();
        require(producer.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "flush did not cancel old producer remainder");
        producer.get();
        until(*audio, [&] { return audio->snapshot().engine.renderedGeneration.stream > 1; }, true);
        PluginAudio::pause(&audio->mContext, 0);
        until(*audio, [&] { return audio->snapshot().engine.state == State::Paused; }, true);
        std::uint64_t beforeDrain = 0;
        {
            std::lock_guard<std::mutex> guard(audio->mMutex);
            beforeDrain = audio->mTicket;
        }
        auto drain = std::async(std::launch::async, [&] { PluginAudio::drain(&audio->mContext); });
        until(*audio, [&]
        {
            std::lock_guard<std::mutex> guard(audio->mMutex);
            return audio->mTicket > beforeDrain;
        }, false);
        PluginAudio::flush(&audio->mContext, 0);
        require(drain.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "flush did not cancel drain wait");
        drain.get();
        require(!audio->snapshot().failure, "recovery became a timeout failure");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

    static void independentTransition()
    {
        auto audio = make();
        require(audio->gain(.5f, 0., false) != 0 && setup(*audio) == 0, "transition setup");
        const auto ticket = audio->transition(0.f, .01);
        require(ticket != 0, "transition ticket");
        require(audio->gain(.2f, 0., false) != 0, "concurrent slider");
        until(*audio, [&] { return audio->snapshot().engine.transitionCompleted == ticket; }, true);
        const auto completed = audio->snapshot().engine;
        require(completed.endpointTransitionCompleted == 0, "rendered fade is not physical completion");
        require(audio->mEngine.advanceHeadlessEndpoint(completed.generation, completed.transitionFenceFrame - 1) == Result::Ok,
            "partial endpoint progress");
        require(audio->mEngine.status().endpointTransitionCompleted == 0, "delayed endpoint completed early");
        require(audio->mEngine.advanceHeadlessEndpoint(completed.generation, completed.transitionFenceFrame) == Result::Ok,
            "full endpoint progress");
        require(audio->mEngine.status().endpointTransitionCompleted == ticket, "endpoint command fence lost");
        require(audio->snapshot().engine.currentGain == .2f, "transition overwrote user gain");
        audio->cancelAndStop();
        require(audio->snapshot().engine.state != State::Stopped, "decoder retirement stopped output device");
        audio->prepare(Role::Music);
        require(audio->gain(.3f, 0., false) != 0, "persistent output gain");
        auto reattach = std::async(std::launch::async, [&] { return setup(*audio); });
        until(*audio, [&] { return reattach.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }, true);
        require(reattach.get() == 0, "persistent output reattach");
        require(audio->mEngine.advanceHeadlessEndpoint(completed.generation, completed.clockFrames) == Result::StaleGeneration,
            "old endpoint generation accepted");
        require(audio->mEngine.status().endpointTransitionCompleted == 0, "handover retained stale endpoint fence");
        const auto next = audio->transition(1.f, .001);
        until(*audio, [&] { return audio->snapshot().engine.transitionCompleted == next; }, true);
        require(next > ticket, "command IDs must survive handover");
        const auto nonzero = audio->mEngine.status();
        require(!PluginAudio::transitionComplete(nonzero, next), "nonzero rendered target acknowledged before endpoint");
        require(audio->mEngine.advanceHeadlessEndpoint(nonzero.generation, nonzero.transitionFenceFrame) == Result::Ok,
            "nonzero endpoint advance");
        require(PluginAudio::transitionComplete(audio->mEngine.status(), next), "nonzero endpoint fence not acknowledged");
        require(!PluginAudio::transitionComplete(audio->mEngine.status(), ticket), "stale serial acknowledged");
        auto stale = audio->mEngine.status();
        ++stale.generation.format;
        require(!PluginAudio::transitionComplete(stale, next), "stale format acknowledged");
        stale = audio->mEngine.status();
        stale.error = Result::DeviceError;
        require(!PluginAudio::transitionComplete(stale, next), "failure acknowledged as complete");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

        static void endpointLagAndFailure()
        {
        for (const bool invalidate : {false, true})
        {
            auto audio = make();
            require(setup(*audio) == 0, "endpoint drain setup");
            const std::vector<std::int16_t> input(1024 * 2, 1000);
            PluginAudio::play(&audio->mContext, input.data(), 1024, 1000000);
            auto drain = std::async(std::launch::async, [&] { PluginAudio::drain(&audio->mContext); });
            until(*audio, [&] { return audio->snapshot().engine.state == State::Drained; }, true);
            const auto status = audio->mEngine.status();
            EndpointObservation observation{status.deviceEpoch, 0, 100, 48000, 100000};
            require(audio->mEngine.observeHeadlessEndpoint(status.generation, observation) == Result::Ok,
                "observe released application buffer");
            observation.position = 579;
            require(audio->mEngine.observeHeadlessEndpoint(status.generation, observation) == Result::Ok,
                "observe held endpoint tail");
            require(!audio->mEngine.status().endpointDrainComplete &&
                drain.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
                "drain returned before physical latency guard");
            if (invalidate)
            {
            observation.valid = false;
            require(audio->mEngine.observeHeadlessEndpoint(status.generation, observation) == Result::DeviceError,
                "endpoint invalidation must fail");
            until(*audio, [&] { return audio->snapshot().failure != nullptr; }, false);
            require(!audio->snapshot().engine.endpointDrainComplete, "failed drain became completed");
            }
            else
            {
            observation.position = 580;
            require(audio->mEngine.observeHeadlessEndpoint(status.generation, observation) == Result::Ok,
                "endpoint tail crossed");
            }
            until(*audio, [&] { return drain.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }, false);
            drain.get();
            require(invalidate || !audio->snapshot().failure, "qualified drain unexpectedly failed");
            audio->cancelAndStop();
            audio->closeAfterVLC();
        }
        }

        static void stoppedEngineRecovery()
    {
        auto audio = make();
        require(setup(*audio) == 0, "stopped recovery setup");
        require(audio->mEngine.stop() == Result::Ok, "headless stop");
        audio->fail("audio_device_error");
        audio->cancelAndStop();
        require(audio->snapshot().failure != nullptr, "failure disappeared before owner recovery");
        audio->prepare(Role::Music);
        require(setup(*audio) == 0, "explicit setup did not recover stopped engine");
        require(!audio->snapshot().failure, "old failure poisoned fresh setup");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

    static void playbackAfterCompletion()
    {
        Status status;
        status.transitionCompleted = 7;
        status.endpointTransitionCompleted = 7;
        for (const auto state : {State::Playing, State::Starving, State::Playing, State::Paused, State::Drained})
        {
            status.state = state;
            const auto report = PluginAudio::playbackReport(status, 100.f, false);
            const std::string expected = state == State::Playing ? "running" : state == State::Starving ?
                "buffering" : state == State::Paused ? "paused" : "drained";
            require(report.state == expected, "completed fade masked ongoing playback state");
        }
    }

    static void rejectUnqualifiedLayout()
    {
        auto audio = make();
        require(setup(*audio, 6) != 0, "unqualified source layout was accepted");
        const auto snapshot = audio->snapshot();
        require(snapshot.failure && !snapshot.engine.deviceStarted, "layout failure must be explicit and device-free");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

    static void discontinuityDuringTransition()
    {
        auto audio = make();
        require(setup(*audio) == 0, "discontinuity setup");
        const auto ticket = audio->transition(1.f, .02);
        require(ticket != 0, "discontinuity transition ticket");
        until(*audio, [&] { return audio->snapshot().engine.transitionConsumed == ticket; }, true);
        const std::vector<std::int16_t> input(2048 * 2, 1000);
        PluginAudio::play(&audio->mContext, input.data(), 2048, 1000000);
        until(*audio, [&] { return audio->snapshot().engine.lastSourcePts >= 1000000; }, true);
        PluginAudio::play(&audio->mContext, input.data(), 2048, 2000000);
        until(*audio, [&] { return audio->snapshot().discontinuities == 1; }, true);
        until(*audio, [&] { return audio->snapshot().engine.lastSourcePts >= 2000000; }, true);
        require(audio->snapshot().engine.transitionConsumed == ticket, "timestamp recovery lost transition identity");
        until(*audio, [&] { return audio->snapshot().engine.transitionCompleted == ticket; }, true);
        const auto status = audio->snapshot().engine;
        require(!PluginAudio::transitionComplete(status, ticket), "recovery acknowledged before endpoint");
        require(audio->mEngine.advanceHeadlessEndpoint(status.generation, status.transitionFenceFrame) == Result::Ok,
            "recovered endpoint progress");
        until(*audio, [&] { return PluginAudio::transitionComplete(audio->snapshot().engine, ticket); }, false);
        require(!audio->snapshot().failure, "timestamp recovery failed the stream");
        audio->cancelAndStop();
        audio->closeAfterVLC();
    }

public:
    static void run()
    {
        largeBlockAndDrain();
        pauseFlushResume();
        saturatedCancellation();
        saturatedRecovery();
        independentTransition();
        playbackAfterCompletion();
        stoppedEngineRecovery();
        endpointLagAndFailure();
        discontinuityDuringTransition();
        rejectUnqualifiedLayout();
    }
};
}

int main()
{
    try
    {
        llvlc::PluginAudioTest::run();
        std::cout << "VLC PCM bridge headless checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}