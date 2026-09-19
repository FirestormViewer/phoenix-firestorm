#include "linden_common.h"
#include "llpluginclassmedia.h"
#include "llpluginmessageclasses.h"
#include "llcontrol.h"
#include "lltut.h"
#include <limits>

LLControlGroup gSavedSettings("VlcAudioProtocolTest");
#if LL_DARWIN
bool gHiDPISupport = false;
#endif

namespace tut
{
class AudioProtocolProbe : public LLPluginClassMedia
{
public:
    AudioProtocolProbe() : LLPluginClassMedia(nullptr) {}

    using LLPluginClassMedia::receiveAudioState;
    using LLPluginClassMedia::mAudioGeneration;
    using LLPluginClassMedia::mAudioSerial;
    using LLPluginClassMedia::mAudioTransitionTarget;
    using LLPluginClassMedia::mSendQueue;

    LLPluginMessage take()
    {
        ensure("queued message exists", !mSendQueue.empty());
        LLPluginMessage message = mSendQueue.front();
        mSendQueue.pop();
        return message;
    }

    void pending(F32 target)
    {
        setAudioRole("music");
        loadURI("https://example.invalid/audio");
        mAudioSerial = 7;
        mAudioTransitionTarget = target;
    }

    LLPluginMessage state(const std::string& generation, const std::string& serial,
                          const std::string& value)
    {
        LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_AUDIO, "state");
        message.setValue("generation", generation);
        message.setValue("serial", serial);
        message.setValue("state", value);
        message.setValue("detail", "redacted");
        return message;
    }
};

struct AudioProtocolData
{
    AudioProtocolProbe media;
};

typedef test_group<AudioProtocolData> AudioProtocolGroup;
typedef AudioProtocolGroup::object AudioProtocolObject;
AudioProtocolGroup audioProtocolGroup("viewer_media_audio");

template<> template<>
void AudioProtocolObject::test<1>()
{
    media.setAudioRole("music");
    media.setAudioGain(0.25f, true);
    media.loadURI("https://example.invalid/audio");
    LLPluginMessage configure = media.take();
    ensure_equals("configure first", configure.getName(), "configure");
    ensure_equals("version", std::string(LLPLUGIN_MESSAGE_CLASS_MEDIA_AUDIO_VERSION), "1.0");
    ensure_equals("role", configure.getValue("role"), "music");
    ensure("generation string", configure.getValueLLSD("generation").isString());
    ensure_equals("generation decimal", configure.getValue("generation"), "1");
    LLPluginMessage gain = media.take();
    ensure_equals("gain before URI", gain.getName(), "set_gain");
    ensure_equals("gain", gain.getValueReal("target"), 0.25);
    ensure_equals("duration seconds", gain.getValueReal("duration"), 0.0);
    ensure("hard mute", gain.getValueBoolean("hard_mute"));
    LLPluginMessage legacy = media.take();
    ensure_equals("legacy fallback", legacy.getName(), "set_volume");
    ensure_equals("legacy muted", legacy.getValueReal("volume"), 0.0);
    ensure_equals("URI last", media.take().getName(), "load_uri");
    ensure("no music spatial command", media.mSendQueue.empty());
}

template<> template<>
void AudioProtocolObject::test<2>()
{
    media.pending(0.f);
    media.receiveAudioState(media.state("0", "7", "silent"));
    ensure("old generation rejected", !media.audioTransitionComplete());
    media.receiveAudioState(media.state("1", "6", "silent"));
    ensure("old serial rejected", !media.audioTransitionComplete());
    media.receiveAudioState(media.state("1", "8", "silent"));
    ensure("future serial rejected", !media.audioTransitionComplete());
    media.receiveAudioState(media.state("1", "7", "silent"));
    ensure("matching zero acknowledged", media.audioTransitionComplete());
    media.loadURI("https://example.invalid/replacement");
    media.receiveAudioState(media.state("1", "7", "silent"));
    ensure("old acknowledgement cannot stop replacement", !media.audioTransitionComplete());
}

template<> template<>
void AudioProtocolObject::test<3>()
{
    media.pending(0.f);
    for (const std::string value : {"priming", "running", "buffering", "paused", "unknown"})
    {
        media.receiveAudioState(media.state("1", "7", value));
        ensure("not proof of zero", !media.audioTransitionComplete());
    }
    media.receiveAudioState(media.state("1", "7", "drained"));
    ensure("submitted drain is not endpoint zero", !media.audioTransitionComplete());
    media.receiveAudioState(media.state("1", "7", "silent"));
    ensure("endpoint zero acknowledged", media.audioTransitionComplete());
}

template<> template<>
void AudioProtocolObject::test<4>()
{
    media.pending(1.f);
    media.receiveAudioState(media.state("1", "7", "buffering"));
    ensure("buffering does not finish fade-in", !media.audioTransitionComplete());
    media.receiveAudioState(media.state("1", "7", "running"));
    ensure("matching completed fade-in", media.audioTransitionComplete());
}

template<> template<>
void AudioProtocolObject::test<5>()
{
    media.pending(0.f);
    LLPluginMessage message = media.state("1", "7", "silent");
    message.setValueS32("generation", 1);
    media.receiveAudioState(message);
    ensure("integer generation rejected", !media.audioTransitionComplete());
    message = media.state("1", "7", "silent");
    message.setValueReal("serial", 7.0);
    media.receiveAudioState(message);
    ensure("real serial rejected", !media.audioTransitionComplete());
    media.receiveAudioState(media.state("01", "7", "silent"));
    ensure("noncanonical token rejected", !media.audioTransitionComplete());
}

template<> template<>
void AudioProtocolObject::test<6>()
{
    media.setAudioRole("object");
    media.setAudioSpatial(3.f, 4.f);
    media.loadURI("https://example.invalid/audio");
    media.take();
    media.take();
    media.take();
    LLPluginMessage direction = media.take();
    ensure_equals("spatial before URI", direction.getName(), "spatial");
    ensure_approximately_equals("right", (F32)direction.getValueReal("right"), 0.6f, 16);
    ensure_approximately_equals("forward", (F32)direction.getValueReal("forward"), 0.8f, 16);
    media.take();
    media.setAudioSpatial(3.f, 4.f);
    media.setAudioSpatial(0.f, 0.f);
    media.setAudioSpatial(std::numeric_limits<F32>::quiet_NaN(), 1.f);
    ensure("unchanged and invalid directions do not enqueue", media.mSendQueue.empty());
}

template<> template<>
void AudioProtocolObject::test<7>()
{
    media.setAudioRole("music");
    media.mAudioGeneration = std::numeric_limits<U64>::max() - 1;
    media.loadURI("https://example.invalid/audio");
    ensure_equals("full U64 decimal", media.take().getValue("generation"), "18446744073709551615");
    const size_t queued = media.mSendQueue.size();
    media.loadURI("https://example.invalid/replacement");
    ensure_equals("overflow cannot load another stream", media.mSendQueue.size(), queued);
    ensure_equals("overflow explicit", media.getAudioState(), "failed");
}

template<> template<>
void AudioProtocolObject::test<8>()
{
    media.pending(0.f);
    media.receivePluginMessage(media.state("1", "7", "silent"));
    ensure("unnegotiated acknowledgement rejected", !media.audioTransitionComplete());
    ensure("unnegotiated transition rejected", !media.transitionAudio(0.f, 1.f));
    media.reset();
    ensure("reset clears queued controls", media.mSendQueue.empty());
    media.loadURI("https://example.invalid/video");
    ensure_equals("unqualified media remains legacy", media.take().getName(), "load_uri");
    ensure("no unqualified audio controls", media.mSendQueue.empty());
}

template<> template<>
void AudioProtocolObject::test<9>()
{
    media.setAudioRole("music");
    media.setAudioGain(0.5f, false);
    media.loadURI("https://example.invalid/audio");
    const size_t queued = media.mSendQueue.size();
    media.setAudioGain(0.5f, false);
    media.setAudioGain(std::numeric_limits<F32>::quiet_NaN(), false);
    ensure_equals("steady gain is not a frame fade", media.mSendQueue.size(), queued);
}

template<> template<>
void AudioProtocolObject::test<10>()
{
    media.pending(0.f);
    ensure("priming is active", media.isAudioPlaying());
    media.stop();
    media.receiveAudioState(media.state("1", "7", "silent"));
    ensure("stop invalidates pending acknowledgement", !media.audioTransitionComplete());
    ensure("explicit stop is inactive", !media.isAudioPlaying());
    media.start();
    ensure_equals("replay has a new generation", media.mAudioGeneration, U64(2));
    ensure_equals("new generation resets serial", media.mAudioSerial, U64(0));
    media.receiveAudioState(media.state("1", "7", "silent"));
    ensure("stale replay acknowledgement rejected", !media.audioTransitionComplete());
}

template<> template<>
void AudioProtocolObject::test<11>()
{
    using Result = LLPluginClassMedia::AudioTransitionResult;
    for (const std::string detail : {"setup_timeout", "audio_device_error", "transition_clock_timeout_retry_or_stop", "endpoint_tail_unqualified"})
    {
        AudioProtocolProbe probe;
        probe.pending(0.f);
        auto failed = probe.state("1", "0", "failed");
        failed.setValue("detail", detail);
        LLSD diagnostic = LLSD::emptyMap();
        diagnostic["expected_transition"] = "18446744073709551615";
        diagnostic["completed_transition"] = "0";
        diagnostic["queued"] = 1024;
        diagnostic["gain"] = .5;
        diagnostic["endpoint_qualified"] = true;
        diagnostic["device_failure"] = 10;
        diagnostic["device_failure_code"] = "2148880388";
        diagnostic["stream"] = "invalid\ntext";
        diagnostic["reserved"] = "not_numeric";
        diagnostic["unknown"] = "must_not_be_logged";
        failed.setValueLLSD("diagnostic", diagnostic);
        probe.receiveAudioState(probe.state("0", "0", "failed"));
        ensure("stale failure ignored", probe.audioTransitionResult() == Result::Pending);
        probe.receiveAudioState(failed);
        ensure("live current-generation failure terminal", probe.audioTransitionResult() == Result::Failed);
        ensure("failure is not silence", !probe.audioTransitionComplete());
        probe.receiveAudioState(probe.state("1", "7", "silent"));
        ensure("late completion cannot undo failure", probe.audioTransitionResult() == Result::Failed);
        probe.stop();
        ensure("hard stop cancels", probe.audioTransitionResult() == Result::Cancelled);
        probe.loadURI("https://example.invalid/new-audio");
        ensure("new configuration recovers", probe.audioTransitionResult() == Result::Pending);
        probe.receiveAudioState(failed);
        ensure("old failure cannot poison replacement", probe.audioTransitionResult() == Result::Pending);
    }
}

template<> template<>
void AudioProtocolObject::test<13>()
{
    media.setAudioRole("music");
    const auto load = [&]()
    {
        media.loadURI("https://example.invalid/music");
        LLPluginMessage message;
        do { message = media.take(); } while (message.getName() != "load_uri");
        return message;
    };
    ensure("speaker fill absent by default", load().getValueLLSD("speaker_fill").isUndefined());
    for (const S32 layout : {21, 41, 51, 71})
    {
        media.setMusicSpeakerFill(layout);
        const auto enabled = load();
        ensure("integer layout", enabled.getValueLLSD("speaker_fill").isInteger());
        ensure_equals("explicit layout", enabled.getValueS32("speaker_fill"), layout);
        ensure_equals("music-only role", enabled.getValue("audio_role"), "music");
    }
    media.setMusicSpeakerFill(0);
    ensure("disable restores baseline message", load().getValueLLSD("speaker_fill").isUndefined());
    media.setMusicSpeakerFill(99);
    ensure("invalid layout bypasses fill", load().getValueLLSD("speaker_fill").isUndefined());
    media.setMusicSpeakerFill(51);
    media.setAudioRole("object");
    ensure("object media never upmixed", load().getValueLLSD("speaker_fill").isUndefined());
    media.reset();
    media.setAudioRole("music");
    ensure("reset clears opt-in", load().getValueLLSD("speaker_fill").isUndefined());
}

template<> template<>
void AudioProtocolObject::test<12>()
{
    media.pending(1.f);
    media.receiveAudioState(media.state("1", "7", "running"));
    ensure("fade-in acknowledged", media.audioTransitionComplete());
    for (const std::string state : {"buffering", "running", "paused", "drained"})
    {
        media.receiveAudioState(media.state("1", "0", state));
        ensure_equals("playback state independent of completed envelope", media.getAudioState(), state);
        ensure("acknowledgement retained", media.audioTransitionComplete());
    }
    media.receiveAudioState(media.state("1", "6", "running"));
    ensure_equals("stale serial cannot mask EOS", media.getAudioState(), "drained");
}
}