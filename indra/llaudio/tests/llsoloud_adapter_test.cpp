#include "linden_common.h"
#include "llaudioengine_soloud.h"
#include "llstreamingaudio.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

struct LLSoLoudTestAccess
{
    static void init(LLAudioEngine_SoLoud& engine, unsigned int channels)
    {
        engine.LLAudioEngine::init(nullptr, "SoLoud headless test");
        engine.sSoloud = new SoLoud::Soloud();
        require(engine.startBackend(SoLoud::Soloud::NULLDRIVER, channels), "null backend init");
    }

    static void commit(LLAudioEngine_SoLoud& engine)
    {
        engine.mListenerp->commitDeferredChanges();
    }

    static void retainBuffer(LLAudioEngine_SoLoud& engine, LLAudioBuffer* buffer)
    {
        engine.mBuffers[0] = buffer;
    }

    static void restart(LLAudioEngine_SoLoud& engine, unsigned int channels)
    {
        require(engine.reinitBackend(SoLoud::Soloud::NULLDRIVER, channels), "null backend restart");
    }

    static void unavailable(LLAudioEngine_SoLoud& engine)
    {
        engine.sBackendReady = false;
        engine.mDeviceListTimer.reset();
    }
};

class TestStream : public LLStreamingAudioInterface
{
public:
    void start(const std::string&) override {}
    void stop() override {}
    void pause(int) override {}
    void update() override { ++updates; }
    int isPlaying() override { return 1; }
    void setGain(F32) override {}
    F32 getGain() override { return 1.f; }
    std::string getURL() override { return {}; }
    unsigned int updates = 0;
};

class TestBuffer : public LLAudioBufferSoLoud
{
public:
    SoLoud::Wav& wav() { return mWav; }

    TestBuffer()
    {
        std::vector<float> waveform(512, 0.1f);
        require(mWav.loadRawWave(waveform.data(), static_cast<unsigned int>(waveform.size()), 44100.f, 1, true) == SoLoud::SO_NO_ERROR,
                "synthetic buffer");
        mWav.set3dAttenuation(SoLoud::AudioSource::INVERSE_DISTANCE, 1.f);
    }
};

class TestChannel : public LLAudioChannelSoLoud
{
public:
    using LLAudioChannelSoLoud::play;
    using LLAudioChannelSoLoud::playSynced;
    using LLAudioChannelSoLoud::cleanup;
    using LLAudioChannelSoLoud::isPlaying;
    using LLAudioChannelSoLoud::update3DPosition;
    using LLAudioChannelSoLoud::updateLoop;

    void bind(LLAudioSource& source, LLAudioBuffer& buffer)
    {
        mCurrentSourcep = &source;
        mCurrentBufferp = &buffer;
    }

    SoLoud::handle handle() const { return mHandle; }
    bool looped() const { return mLoopedThisFrame; }
};

int main()
{
    try
    {
        for (const unsigned int channels : {2u, 6u, 8u})
        {
            LLAudioEngine_SoLoud engine;
            gAudiop = &engine;
            LLSoLoudTestAccess::init(engine, channels);
            auto* soloud = engine.getSoloud();
            require(soloud && soloud->getBackendId() == SoLoud::Soloud::NULLDRIVER, "no physical device");
            require(soloud->getGlobalVolume() == 0.f, "startup must be silent");
            require(engine.createDefaultStreamingAudioImpl() == nullptr, "retain media plugin streaming");
            engine.setMasterGain(0.5f);
            require(soloud->getGlobalVolume() == 0.5f, "master gain");
            engine.setMuted(true);
            require(soloud->getGlobalVolume() == 0.f, "mute");
            engine.setMuted(false);
            require(soloud->getGlobalVolume() == 0.5f, "unmute");

            if (channels == 8)
            {
                float speaker_x, speaker_y, speaker_z;
                soloud->getSpeakerPosition(4, speaker_x, speaker_y, speaker_z);
                require(speaker_z == -1.f, "Windows 7.1 back channel order");
                soloud->getSpeakerPosition(6, speaker_x, speaker_y, speaker_z);
                require(speaker_z == 0.f, "Windows 7.1 side channel order");
            }

            engine.setListener(LLVector3::zero, LLVector3(1.f, 2.f, 3.f),
                               LLVector3(0.f, 0.f, 1.f), LLVector3(1.f, 0.f, 0.f));
            LLSoLoudTestAccess::commit(engine);
            require(soloud->m3dVelocity[0] == 1.f && soloud->m3dVelocity[2] == 3.f, "listener velocity");

            require(engine.initWind(), "wind initialization");
            engine.enableWind(true);
            engine.updateWind(LLVector3(5.f, 0.f, 0.f), 0.f);
            const unsigned int frames = 1024;
            std::vector<float> samples(frames * channels + 2, 12345.f);
            soloud->mix(samples.data() + 1, frames);
            require(samples.front() == 12345.f && samples.back() == 12345.f, "mix bounds");
            for (unsigned int sample = 1; sample <= frames * channels; ++sample)
            {
                require(std::isfinite(samples[sample]), "finite wind output");
            }
            engine.cleanupWind();
            require(soloud->getVoiceCount() == 0, "wind retirement");
                {
                TestBuffer buffer;
                LLAudioSource source(LLUUID::null, LLUUID::null, 0.8f);
                source.setLoop(true);
                source.setPositionGlobal(LLVector3d(10.0, 0.0, 0.0));
                TestChannel master;
                master.bind(source, buffer);
                master.setSecondaryGain(0.5f);
                engine.setRolloffFactor(2.f);
                engine.setDopplerFactor(0.f);
                master.play();
                require(master.isPlaying() && soloud->getLooping(master.handle()), "looping channel start");
                require(std::abs(soloud->getVolume(master.handle()) - 0.4f) < 0.0001f, "category gain at start");
                require(std::abs(soloud->getOverallVolume(master.handle()) - 0.4f / 19.f) < 0.0001f,
                    "rolloff committed before unpause");
                master.setSecondaryGain(0.25f);
                engine.setRolloffFactor(1.f);
                master.update3DPosition();
                LLSoLoudTestAccess::commit(engine);
                require(std::abs(soloud->getOverallVolume(master.handle()) - 0.02f) < 0.0001f,
                    "live category gain and rolloff");
                soloud->mix(samples.data() + 1, frames);
                master.updateLoop();
                require(master.looped(), "loop completion detection");
                TestChannel slave;
                slave.bind(source, buffer);
                slave.playSynced(&master);
                require(slave.isPlaying(), "synchronized channel start");
                source.setLoop(false);
                master.update3DPosition();
                require(!soloud->getLooping(master.handle()), "loop toggle");
                master.cleanup();
                slave.cleanup();
                require(!master.isPlaying() && !slave.isPlaying(), "channel handle retirement");
                }
                require(soloud->getVoiceCount() == 0, "buffer destruction retires faded voices");
                engine.enableWind(false);
                auto* retained_buffer = new TestBuffer();
                LLSoLoudTestAccess::retainBuffer(engine, retained_buffer);
                for (unsigned int restart = 0; restart < 5; ++restart)
                {
                    const auto voice = soloud->play(retained_buffer->wav());
                    require(soloud->isValidVoiceHandle(voice), "retained WAV playback");
                    LLSoLoudTestAccess::restart(engine, channels);
                    soloud = engine.getSoloud();
                    require(soloud->getVoiceCount() == 0, "restart retires old voices");
                    require(retained_buffer->wav().mSoloud == nullptr && retained_buffer->wav().mAudioSourceID == 0,
                            "restart detaches WAV ownership and source IDs");
                    require(soloud->getGlobalVolume() == 0.5f, "restart restores gain");
                }
                auto* stream = new TestStream();
                engine.setStreamingAudioImpl(stream);
                LLSoLoudTestAccess::unavailable(engine);
                engine.idle();
                require(stream->updates == 1, "media updates survive unavailable effects device");
            engine.shutdown();
            require(engine.getSoloud() == nullptr, "engine retirement");
            gAudiop = nullptr;
            std::cout << "PASS: null backend adapter, " << channels << " channels\n";
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}