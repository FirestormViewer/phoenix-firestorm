# Viewer-owned VLC audio protocol

## Current integrated contract (2026-09-18)

This section supersedes the archived agent handoff below. The plugin and engine
now implement independent user/transition gain, generation-tagged controls and
no-old-URL configure semantics. A conservative WASAPI consumption fence is now
implemented and compiled; real-device/acoustic acceptance is NOT qualified.

### Completion, failure and recovery

`LLPluginClassMedia::AudioTransitionResult` and
`LLStreamingAudioInterface::AudioFadeResult` distinguish Pending, Complete,
Failed and Cancelled. Optional-backend defaults preserve existing behavior.
Matching nonzero-serial `silent` acknowledges zero only under the endpoint
contract; matching `running` also requires nonzero endpoint consumption. Routine
serial-zero state continues updating playback without clearing completed
acknowledgement. Buffering/paused/drained are never treated as audible silence.

Current-generation failure is terminal even with serial zero (setup/device
failures can precede a transition). Stale generations/serials are rejected;
late success cannot undo failure. Explicit stop returns Cancelled. A fresh URI
configuration resets the result and increments generation. Process exit is
Cancelled, not proof of a successful fade.

`LLViewerAudio::onIdleUpdate` handles live Failed/Cancelled before its normal
fade state machine: log the hard-stop outcome, clear the pending URI, leave the
fade state, hard-stop the stream, and unregister the idle listener. It does NOT
set successful fade completion or silently begin the queued replacement. A new
explicit start is required. `getFadeVolume` retains the backend path on terminal
failure until that recovery executes; it does not switch to a frame-timer fade.
This policy prevents live failed plugins from holding the viewer forever and
prevents old/replacement URL replay as an implicit failure recovery.

The production plugin now waits for a submission/padding/device-clock/maximum
latency fence for both zero and nonzero transitions. Device/clock failure still
follows this hard-stop policy, never a successful timer fallback. See the engine
contract for the explicit private miniaudio dependency extension and limitations.
User-requested hard stop/fades-disabled behavior remains immediate. Hard mute
affects future rendered frames and cannot retract submitted endpoint buffers.

### Retained routing and controls

Exact `media_audio: 1.0` negotiation precedes commands. Configure/gain/direction/
initial transition commands precede the URI; each control carries canonical
decimal generation and each transition its serial. Configure clears the old
decoder URL without replay. Preplay user gain and transition commands remain
independent; gain changes do not restart the transition envelope. Muting does
not change its phase. Music is nonpositional all-main-speaker audio. Only
explicit audio MIME objects use positional routing; category/master/distance
attenuation is applied once. HUD exemption is retained. Video/browser sources
remain on native VLC/CEF and are not claimed as PCM/A-V qualified.

Metadata policy is unchanged by this review pass. FMOD, OpenAL and SoLoud effects
implementations are untouched; only the media-plugin streaming implementation
overrides the optional fade-result API.

### Verification and limits

12/12 process-free protocol tests passed in
`build-vc170-64/logs/vlc-protocol-review-build-3.log`. Target:
`INTEGRATION_TEST_llviewermedia_audio`; artifact:
`build-vc170-64/sharedlibs/RelWithDebInfo/INTEGRATION_TEST_llviewermedia_audio.exe`.
The focused target is enabled by `LL_VLC_AUDIO_PROTOCOL_TESTS=ON` without broad
`LL_TESTS`. The test uses `linden_common.h` and a named test-owned control group;
it does not initialize a plugin process, viewer settings or audio device.

Coverage includes initial control/URI order, decimal U64 boundaries, invalid
and stale tokens, gain deduplication, direction normalization, reset, stop/replay,
submitted-drain rejection, current-generation setup/device/clock/fence failures
while alive, terminal-failure latching, new-generation recovery, and completed
fade followed by buffering/recovery/pause/EOS without stale-state masking.
Bridge production state selection and delayed endpoint consumption have separate
passing headless coverage in the plugin contract. These tests do not execute
the complete LLViewerAudio/gAudiop idle lifecycle or real IPC transport.

Full RelWithDebInfo viewer link PASSED with `/m:2 /p:CL_MPCount=2`.
Log: `build-vc170-64/logs/vlc-endpoint-viewer-build-2.log`; artifact:
`build-vc170-64/newview/RelWithDebInfo/vulkanstorm-bin.exe`.
The preceding agent build had exited without link-success evidence. The first
new attempt exposed a missing `llviewercamera.h` include in the existing audio
direction code; adding that declaration fixed C2027/C3861. Failed-attempt log:
`build-vc170-64/logs/vlc-endpoint-viewer-build.log`. No camera/render behaviour
changed. This link is not a viewer runtime test.
The final post-endpoint-guard rebuild also passed, including protocol 12/12:
`build-vc170-64/logs/vlc-endpoint-final-build.log`. No build remains pending.

Remaining gates: physical qualification of the implemented fence; actual libVLC/IPC playback;
audible switch/starvation/mute/pause recovery; hardware surround and device loss;
full viewer failure/idle lifecycle execution; video regression/A-V clocks;
effects-off and non-Windows builds. No viewer/audio launch, settings mutation,
Release build, installer, commit, push, PR or release was performed.

## Archived independent-agent handoff

The remaining sections are historical source/ownership notes. Their integration
disagreements and UNRUN/frozen API statements describe an earlier snapshot and
are superseded above; they are not the current completion checklist.

## Source contract, design and discriminating checks

Scope is audio-only in the `vlc-smooth-fades` worktree. The engine and the
plugin adapter/build integration are owned elsewhere and are not edited here.

Source contract: `LLViewerAudio::startFading`, `getFadeVolume` and
`onIdleUpdate` currently drive gain and stream replacement from a frame timer.
`LLStreamingAudio_MediaPlugins::stop` immediately deletes the plugin.
`LLViewerMediaImpl::updateVolume` already applies category/user volume and
distance attenuation; HUDs are exempt through the existing proximity policy.
`LLStreamingAudioInterface` has no completion handshake; its FMOD and other
implementations must retain their existing optional-method defaults.

Design: negotiate `media_audio` version `1.0` before sending commands. Queue
configuration and controls ahead of load/start while negotiation is pending.
Use decimal U64 generation/serial strings, never LLSD integer/real tokens.
Separate the user/category gain from the transition envelope. Send full target
and duration once; do not multiply frame-clock fade gain into engine gain.
Only matching generation and serial completion releases a soft transition.
Hard mute is separate from a transition. A dead plugin terminates waiting;
a live buffering plugin does not become complete merely because time elapsed.

Discriminating checks: stale-generation and stale-serial state messages must
not complete a current transition; a queued fade must precede first PCM;
buffering beyond the viewer fade timer must not destroy the stream; unsupported
plugins must receive no media_audio commands and retain legacy set_volume.
Repeated unchanged gain/spatial updates must not restart an envelope.

Initial qualification policy: music is nonpositional all-main-speaker music.
Only explicitly audio MIME object sources may use positional object audio.
Video and browser media retain their legacy output until audio/video PTS
ownership is qualified. No buffering root cause or cache-size fix is claimed.

Validation in this agent session is source inspection and editor diagnostics
only. No terminal, build, configure, package or viewer launch is authorized.

## Implemented interface

`llpluginmessageclasses.h` defines `LLPLUGIN_MESSAGE_CLASS_MEDIA_AUDIO` and
`LLPLUGIN_MESSAGE_CLASS_MEDIA_AUDIO_VERSION` as `media_audio` and `1.0`.
`LLPluginClassMedia` exposes `setAudioRole`, `setAudioGain`, `setAudioSpatial`,
`transitionAudio`, `audioTransitionComplete`, `getAudioState`,
`pluginSupportsMediaAudio`, `audioControlsAvailable`, `isAudioPlaying` and
`isAudioPaused`. `audioControlsAvailable` includes negotiation-pending, whereas
`pluginSupportsMediaAudio` requires the exact advertised version.

Wire messages retain the approved fields without adding another message class:

| Direction / name | Fields |
| --- | --- |
| viewer configure | role: music/object/nonpositional, generation: decimal U64 string |
| viewer set_gain | target: real, duration: real seconds, hard_mute: boolean |
| viewer spatial | right: real, forward: real |
| viewer transition | target: real, duration: real seconds, serial: decimal U64 string |
| plugin state | generation: decimal U64 string, serial: decimal U64 string, state, detail: redacted |

Each audio URI load increments the instance generation, resets its serial to
zero, and sends configure/gain/direction before load_uri. A fade-in additionally
sends transition(0, 0), then transition(1, duration), before load_uri, because
load_uri can start decoding immediately. Transition serials begin at one.
Generation exhaustion refuses another URI; it never wraps. Explicit stop
invalidates acknowledgements; replay configures a new generation. Pause/resume
retains the current generation. Reset clears queued messages.

Queued media_audio messages are filtered only after capability negotiation.
Each initial gain also carries a legacy set_volume fallback which is filtered
out for participating audio sources. Legacy fade-in starts at zero before the
URI is sent. Unchanged gain/direction is deduplicated. Invalid/nonfinite gain,
direction and transition values are rejected. Music never gets spatial.

`LLStreamingAudioInterface` adds four default-unsupported optional methods:
`hasAudioFade`, `beginAudioFade`, `isAudioFadeComplete`, `setAudioHardMute`.
The media-plugin implementation alone overrides them. FMOD's existing squared
gain remains untouched; OpenAL/other streaming implementations retain defaults.
`LLAudioEngine::setInternetStreamGain` was inspected and simply forwards gain.

Native music receives master * music category once, with mute/minimized state
as an immediate hard-mute latch. The viewer fade multiplier is not applied.
The FS fade enable preference and in/out durations remain the policy source;
the existing 0.01-second minimum remains, and native durations are bounded to
the engine's 60-second maximum (the normal UI range is 0-10 seconds).
Disabled fades retain immediate replacement/stop. Enabled native soft stop
waits for zero acknowledgement before deletion. Unsupported backends retain
legacy stop behavior and timed fades. If negotiation resolves unsupported,
the viewer starts the legacy timer then, rather than expiring during startup.

The viewer does not complete an engine fade from `getFadeVolume` elapsed time.
Matching `silent` completes a zero transition; matching `running` completes a
nonzero transition only under the adapter acknowledgement contract below.
Neither `failed` nor `drained` proves audible zero. Plugin process exit releases
waiting because no new stream shares that instance. A live failed/buffering
plugin is not automatically destroyed or retried on a timer. Explicit user
immediate-stop policy (fades disabled) remains available; no unsafe timeout
fallback is hidden in this implementation.

Object routing is limited to explicit audio/* MIME types. The existing first
object, ear location setting, camera orientation, distance rolloff and HUD
exemption are preserved. Direction is projected onto listener right/forward,
normalized, and defaults forward when coincident. Nonobject and HUD audio uses
nonpositional. Existing media gain remains requested * global media * distance;
the engine must not add attenuation. Priming/buffering remain active in both
music and media playing queries. Stopped/failed/drained media is inactive.

## Integration disagreements and release gates

The plugin source changed concurrently during this session. A read-only
snapshot of `receiveAudio`, `submitGain`, `idleAudio`, `configure` and
`init_response` showed the following. These are actionable integration gates,
not claims that the concurrently owned files remain unchanged:

1. **Independent gain and transition:** viewer set_gain is the persistent
	user/category factor; transition is the 0..1 fade factor. The snapshot maps
	both to submitGain, overwrites mCurVolume, and reports
	transition_superseded_by_gain on a volume update. It would fade to unity
	regardless of the slider, and cancel fades on slider changes. The adapter
	must combine both exactly once while preserving sample-clock envelope phase.
	The frozen AudioEngine only has one gain envelope and no independent base
	gain API. This needs an agreed adapter solution or an explicitly authorized
	engine/interface change; viewer frame-clock retargeting is not a solution.
2. **Acknowledgement serial:** serial zero denotes state without a completed
	transition. A pending transition must not be reported as completed running.
	Emit running with the requested nonzero serial only when the nonzero target
	is reached; emit silent with that serial only when zero and endpoint safety
	are established. The snapshot echoes mAudioSerial immediately on running,
	reports empty serial before a transition, and never emits silent. The viewer
	requires canonical decimal strings and ignores empty/stale serials.
3. **Physical tail:** snapshot zero fades fail with endpoint_tail_unqualified;
	drained says submitted_not_endpoint_complete. The engine documentation also
	distinguishes submission from audible completion. Neither can release a
	soft stop. Endpoint-latency/drain qualification is missing. The viewer waits,
	rather than converting that failure into a destructive timeout.
4. **Start/configure ordering:** configure must stop/reset the old owner and
	await the next URI. Snapshot configure calls playMedia on the previous URL.
	It must not replay old content under the new generation. Controls sent before
	load_uri must survive until setup/first PCM, including the initial zero and
	full-duration fade. New configure defaults the transition factor to unity.
5. **Video qualification:** viewer chooses audio-only objects initially, not
	video object positional output. No A/V sync is claimed. MIME classification
	is not proof of decoded tracks: music URLs can redirect to video and audio/*
	may be wrong. Adapter must detect video and retain qualified VLC video/audio
	together, or explicitly reject that native-audio mode without breaking the
	legacy video path. Clearing the viewer audio role for a subsequent video URI
	does not itself reset the adapter's persistent negotiated audio flag; the
	adapter must gate each decoded item, not treat class support as permission
	to replace audio for every URI. There is no disagreement with preserving
	native VLC video until sync qualification; claiming all object video spatial
	support now would disagree with this implementation and the engine limits.
6. **Failure and layout:** handle queue/control rejection and setup failure
	explicitly, qualify actual channel labels and device output, and do not
	silently infer surround layout from count. Buffering cause remains unknown;
	no network-cache change or audible success is asserted.

The wrapper ignores detail, but the process parent's raw IPC debug logging is
outside this ownership scope. The adapter must redact detail before sending.
Role eligibility is not a claim of runtime-qualified spatial/video hardware.

## Source validation and main-agent work

All edited files returned no editor diagnostics via get_errors. This is not a
compile or test pass. Ten process-free TUT tests are in
`indra/newview/tests/llviewermedia_audio_test.cpp`; they are **not registered or
executed** because CMake belongs to the other agent. They cover queue field
types/order, stale generation/serial, submitted-only drain rejection, invalid
tokens, normalized spatial direction, U64 exhaustion, unnegotiated state,
reset, duplicate gain, and explicit stop/replay.

Main agent: after resolving the above protocol semantics, add alongside the
existing newview integration tests (where test_libs includes llplugin):

```cmake
LL_ADD_INTEGRATION_TEST(llviewermedia_audio "" "${test_libs}")
```

The test supplies its own gSavedSettings and Darwin gHiDPISupport globals, and
links the production llplugin implementation. It never initializes a plugin
process or audio device. Validate test linkage on each platform.

Build requests for the main agent, using the worktree's non-packaged
RelWithDebInfo CMake Tools configuration, not the baseline build directory:

1. ListBuildTargets_CMakeTools, then Build_CMakeTools for llplugin and the
	discovered viewer target (normally vulkanstorm-bin).
2. After test registration/configuration by its owner, Build_CMakeTools for
	INTEGRATION_TEST_llviewermedia_audio; ListTests_CMakeTools and
	RunCtest_CMakeTools for its discovered test entry, if registered with CTest.
3. Build the plugin/adapter and run their existing headless engine tests using
	the targets their owner supplies. Do not change the frozen engine.

Required integration tests beyond these source checks: delayed negotiation
with/without capability; first-PCM startup ordering; repeated A/B/C replacement;
zero acknowledgement held beyond viewer timeout; mute during fade and unmute;
slider changes during fade; paused/empty/EOS/device-loss transitions; disabled
fades; FMOD and OpenAL fallback; CEF/video unchanged; camera/avatar ear movement,
HUD exemption and distance gain once; labelled stereo/surround playback and
qualified endpoint tail. Runtime tests and audible acceptance remain outstanding.