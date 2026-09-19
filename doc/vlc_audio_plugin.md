# VLC plugin PCM integration

## Active VLC-native speaker-fill implementation (2026-09-19)

This section supersedes the PCM implementation history below. The operator
accepted VLC-native music and effects: stereo with sound-card surround off,
all speakers with it on. Production `media_plugin_libvlc` has
`LL_VLC_PCM_AUDIO=0`; the custom PCM device/bridge is not linked into it.

`FSMusicSpatialSound` is a persisted Windows-only integer option exposed as
**Spatial sound** in the **Sounds** tab of Sound & Media preferences, beneath
Output device. Choices are Stereo (0, default), 2.1 (21), 4.1 (41), 5.1 (51)
and 7.1 (71). It applies at the next parcel-music stream start, not to effects
or voice. Stereo sends no speaker-fill layout options and uses the original
VLC output backend, with the music-gain filter described below. Object media and video are not
opted in. Avoid enabling both software fill and sound-card surround processing.

Enabled music selects VLC's `mmdevice` output with the explicit
`speakerfill_output,none` backend and the `speakerfill` audio filter. These are
separate module DLLs; no packaged VLC binary is overwritten. The output module
is built from hash-checked VLC 3.0.21 `wasapi.c`, `mmdevice.h`, and `vlc_codecs.h`
using `prepare_speakerfill.cmake`. Its playback and timing algorithms remain
upstream VLC code, with progress reporting added for native fades below; this
is not the rejected custom WASAPI loop.
The existing VLC MMDevice owner continues managing device changes and recovery.

### Player option ownership correction

The initial listening acceptance was withdrawn: with sound-card surround off,
music remained front-only. The initial implementation attached output/filter
options to the media item, but VLC 3.0.21 creates its audio output under the
media player, which also owns an initially empty `audio-filter` variable.
Those media options did not configure that owner.

`llvlcspeakerfillconfig` now sets the filter, output variant and layout on the
player before `libvlc_audio_output_set` recreates the MMDevice output. This
uses the bundled VLC core variable API and the audited 3.0.21 player prefix
(`VLC_COMMON_MEMBERS`), guarded by compile-time and runtime version checks.
Stereo mode does not enable channel expansion. Native music fades now use the
output variant for playback-progress reporting even in Stereo. Missing modules or configuration
failure do not silently masquerade as enabled speaker-fill.

The device-free harness creates actual libVLC media/player objects, demonstrates
that per-media options leave the player filter empty, and verifies the production
helper's inherited output/filter/layout values. After this correction, the
operator confirmed: "Sound output nominal across speakers with soundcard
surround off." Software speaker-fill therefore passes the listening test on
that setup; this does not qualify every selectable layout or device recovery.

The added negotiation applies only to ordinary stereo on non-headphone
endpoints. It uses the selected endpoint's mix format rate and sample type,
requesting the chosen layout only when its speaker mask fits the endpoint.
4.1 and 5.1 accept rear or side speaker pairs. Unsupported selections and
rejected multichannel formats fall back to source-layout negotiation.
A successful output initialization
publishes a fill mask only if the final negotiated layout matches the request. The filter
inherits that mask; stereo/headphone/unknown layouts and native multichannel
sources bypass filling. Failed property/mix-format queries leave source-layout
negotiation unchanged. A failed initialization does not publish a fill mask.

The filter uses float PCM and VLC's channel order. Left/right feed corresponding
main speakers at 0.70710678 gain; the center receives their average. Synthesized
LFE is zero. Source multichannel audio is untouched, including authored LFE.
PCM frame counts, rate, timestamps, duration and block flags are preserved;
there is no extra device, queue, timer or presentation clock in the filter.

### Build and verification

Windows builds additionally require an x64 Windows GNU C compiler for the
unchanged GNU C VLC module (validated with LLVM-MinGW 22.1.8). Set
`VLC_MODULE_CC` if it is not on PATH. Sources are fetched once over HTTPS,
SHA-256 checked, and cached in the build tree. The packaged VLC 3.0.21 plugin
SDK and core import library supply the matching ABI. Dependency source and
the transformation are included under `llplugin/speakerfill-source`.

`vlc_speakerfill_test` loads both actual DLL entry points and confirms real
libVLC module discovery without opening a device. It tests negotiated speaker
masks against fake IMMDevice/IAudioClient interfaces, headphone/query-failure
bypass, initialization-failure cleanup, actual filter samples, LFE silence,
native multichannel passthrough and timing metadata. The protocol test verifies
default Stereo, all four music-only multichannel selections, invalid-selection
bypass, disable and reset behavior. The operator's listening acceptance is
recorded above. Exhaustive physical layout, long-playback and device-recovery
testing remain open.

### Native duration fades (2026-09-19)

The 50 ms smoothing-only attempt below failed operator listening acceptance.
It joined frame-timed targets but did not control the complete fade timeline.

Windows VLC now advertises the music-only `media_music: 1.0` capability. The
viewer sends its existing generation/serial-tagged configure, user gain/mute,
and full-duration transition messages; object media remains on its existing
path. The retired custom PCM capability is not enabled. Native transitions
disable the viewer's frame-based fade multiplier and completion timer through
the existing backend-fade interface.

The filter applies a separate transition envelope sample by sample from its
current value to the commanded target over the requested duration. One packed
control value publishes serial, milliseconds and target together. User volume
retains the cubic curve and short smoothing; mute gates samples independently.
Initial zero and pending fade-in commands are installed before decoding.

The final target sample publishes a scheduled PTS fence and output epoch.
The VLC output variant records successful block submission and reports the
last submitted PTS end minus VLC's existing `TimeGet` queued delay, clamped
to the submitted tail. The plugin acknowledges only the matching command
after that reported playback position reaches the fence. Flush/stop epochs
invalidate old fences; output errors cannot acknowledge success. This is VLC
playback-clock completion, not an acoustic guarantee or the former custom
physical-endpoint contract. There is no new device loop or clock extrapolator.
No progress beyond duration plus 30 seconds reports failure, not completion.

Tests exercise one three-second command without viewer updates, exact final
samples across block boundaries, independent volume/mute, stale command/epoch
rejection, and the actual VLC DLL `Play`, `TimeGet`, `Flush` and `Stop` functions
against fake render/clock interfaces. The operator confirmed on 2026-09-19:
"This crossfade fix works. Stream transition is smooth." Transition listening
acceptance passes on the tested setup; this does not qualify every device or
establish overlapping, gapless playback of two streams.

### Superseded smoothing-only attempt (2026-09-19)

The operator accepted software surround but reported stepped stream fades.
Native playback had restored the legacy frame-timed volume updates, truncated
to integer VLC percentages and also applied through waveOut volume.

Windows parcel music now identifies its role even with Spatial sound set to
Stereo. The VLC filter receives a floating-point gain target from the player,
retaining VLC's cubic volume curve without integer-percent quantization. Each
new target is joined from the current gain by a 50 ms linear per-sample ramp,
including continuous retargeting and exact zero. Initial gain is set before
decoding. The filter handles gain in place when no channel expansion is needed;
the accepted speaker routing is unchanged. VLC output and the legacy Windows
session-volume workaround are set to unity on setup/playing, not updated with
each fade step. Object media/video retain their existing volume path.

This attempt removed gain discontinuities, not the viewer's fade scheduler: transition
duration and stream stop/start still use the viewer timer. There is up to 50 ms
of smoothing lag plus VLC's queued audio, and no new endpoint-completion claim
or gapless/crossfade guarantee. Low frame rates and queued fade tails remain
listening-test risks. This change is not the rejected custom PCM output.

Actual-DLL tests cover sub-percent targets, 2/6/8 channels, 1/37/512-frame
partitions, interrupted ramps, initial gain and exact zero. Physical fade
smoothness has not yet been accepted.

## Timestamp recovery retry (2026-09-18)

The diagnostic physical run captured 28 timestamp-recovery flushes followed by
`transition_clock_timeout_retry_or_stop` at 13.0067 seconds. The plugin awaited
transition 5, but recovery had cleared its consumed/completed IDs to zero.

At the operator's request, the first fix is restored: only internally generated
timestamp-recovery flushes preserve transition identity and fade progress.
Completion and endpoint fences are cleared and established again for the new
PCM generation. Ordinary flushes and handovers still invalidate transitions.
The second, sample-count timeline alternative is not included; timestamp checks
and the watchdog remain unchanged. Plugin failure snapshots remain enabled.

The bridge regression fails without the fix and passes with it. Engine tests
cover recovery completion and ordinary-flush invalidation. Physical playback
acceptance after reapplying the fix remains pending; the operator suspects the
reboot resolved the earlier no-sound issue, but that attribution is unverified.

### Retry outcome and backend diagnostics

The subsequent rebooted test failed: music was choppy, then cut out, and the
operator reported no effects or music afterward. The first snapshot reported
`audio_device_error`, 67 timestamp flushes, transition 5 consumed/completed,
and an unqualified endpoint. This is not the earlier lost-ID timeout. Later
retries reported the same failed engine state. The effects failure is not yet
attributed; its successful startup log is not proof of continued playback.

Backend diagnostics now retain the first failure location and native result
atomically, without logging from the audio callback or changing error policy.
The failure snapshot includes `device_failure` and unsigned decimal
`device_failure_code` (HRESULT for API calls, wait result or GetLastError for
wait failures). Locations follow `DeviceFailure`: 1 notification, 2 timeline
validation, 3 submission validation, 4 frame overflow, 5 unexpected stop,
6 clock service, 7 frequency, 8 latency, 9 padding, 10 clock position,
11 GetBuffer, 12 ReleaseBuffer, 13 Start, 14 wait. Zero means no instrumented
location was captured. Reopening resets the record; later failures cannot
overwrite the first one. This is instrumentation, not an accepted playback fix.

## Current integrated contract (2026-09-18)

This section supersedes the archived agent handoff below. Sole-owner integration
now includes an implemented, compiled WASAPI consumption fence. Physical device
and audible-fade acceptance are still pending; software tests do not qualify it.

Windows x64 advertises `media_audio: 1.0`; unconfigured/non-Windows instances use
native VLC. Explicit music/object/nonpositional roles are audio-only with
`:no-video`; detected video is rejected explicitly. Native video and CEF are not
PCM-qualified. Setup accepts unchanged mono/stereo S16N only. Multichannel amem
mask/order qualification remains required before accepting more channels.
Normal music is nonpositional, reaches every configured MAIN speaker with the
documented headroom, and never duplicates full-range audio into LFE. Effects
remain independently selectable through SoLoud/FMOD/OpenAL.

### Wire and state

- Configure carries role and a canonical positive decimal-U64 generation;
  accepted generations strictly increase. Configure retires the old decoder,
  clears the old URL and awaits the next URI; it never replays the old URL.
- `set_gain`, `spatial` and `transition` ALL carry the generation. Older controls
  are ignored. Transition also carries a strictly increasing decimal serial.
- User gain/mute and transition factor are independent, applied once by the
  engine. Preplay commands preserve order through setup. No frame-clock gain
  stepping, integer-volume retries or PCM-amplitude silence detection is used.
- Outgoing state carries generation, serial, state and fixed/redacted detail.
  A successful transition acknowledgement is a one-shot nonzero-serial message;
  routine priming/running/buffering/paused/drained reports use serial zero.
  The completion latch never overrides current playback state.
- ALL transitions require `endpointTransitionCompleted`, NOT rendered completion.
  The production `transitionComplete` predicate also requires matching accepted/
  rendered stream and format, current command consumption, no error/stopped state
  and a qualified endpoint. Nonzero completion still emits `running`; zero emits
  `silent`, meaning the conservative OS endpoint fence, not acoustic measurement.
  No callback/device-clock progress produces
  `failed/transition_clock_timeout_retry_or_stop` after duration plus 10 seconds.
  These deadlines report failure, not silence.

### Callback ownership, recovery and drain

One worker owns engine lifecycle/control. Bounded adapter queues contain 64 PCM
requests of at most 1024 frames and 64 controls; the engine queue remains bounded
at 32768 source frames. Play callbacks serialize PCM publication. Pause/resume/
flush bypass that producer mutex, including when a play callback is blocked on
capacity or a drain callback is waiting. Flush increments a revision under the
queue mutex, invalidates queued/pending PCM and wakes capacity/drain waiters.
PCM and ordered drain capture that revision BEFORE waiting for producer access,
so an already-entered old callback cannot publish its remainder after flush.
Context epoch changes only after synchronous VLC stop/release quiescence.

Drain releases producer serialization before waiting. It succeeds only after
the matching engine endpoint drain fence. Headless consumption is explicitly
advanced by the test; delayed consumption cannot prematurely return drain.
Real-device drain uses successful submission, padding and device-clock/maximum
latency observations from the owned miniaudio extension described in the engine
contract. Invalidated endpoints fail and cancel the callback wait. Flush and
stop also cancel drain waits. Queue
saturation and setup/drain waits retain their finite failure deadlines (5/10s);
the worker's PCM backlog deadline is suspended while paused. This is not an
unbounded-pause retention guarantee for a still-producing saturated decoder.

Decoder retirement cancels/wakes callbacks and hard-mutes future render output,
then detaches registered events and synchronously stops/releases VLC. It retains
the healthy output device for handover. Failed/stopped engines are closed during
retirement so a later explicit setup can configure a fresh device/generation.
Final plugin teardown closes the engine after VLC callback quiescence. Already
submitted endpoint audio cannot be revoked or reported as completed by hard mute.
Metadata/event callbacks publish atomic/coalesced data; IPC remains in idle.

### Closed review disposition

1. P1 physical completion: premature `silent` and submitted-only drain success
   removed. Generation/command/frame-bound headless endpoint abstraction tested.
  WASAPI production fence is now implemented using the same bounded accounting;
  hardware qualification remains pending. See the engine contract for the exact
  pinned dependency extension, source proof and conservative latency bound.
2. P1 live failure hang: explicit Failed/Cancelled results reach viewer policy;
   failure triggers a labelled hard stop and clears pending replacement, not a
   successful fade or automatic old-URL replay. Explicit new start can recover.
3. P1 saturated callback deadlock: controls bypass producer/drain waits, flush
   revisions cancel old publication, and paused saturation/concurrent resume/
   flush/drain-cancel regressions pass.
4. P2 completion masking: completion acknowledgement is independent of ongoing
   playback state. Fade-complete/starve/recover/pause/EOS and stale serials tested.

### Verified build and tests

Endpoint-extension validation on 2026-09-18: production DLL and generated bridge
target build passed; protocol 12/12 passed. Log:
`build-vc170-64/logs/vlc-endpoint-integration-build.log`.
Nine bridge cases pass (including two outcomes in endpointLagAndFailure), with
new nonzero/stale serial/format/error predicate checks. The shared accounting
holds the drain callback until the physical-latency guard and cancels explicitly
on endpoint failure. Standalone command is the engine command plus `-Bridge`;
generated executable remains `vlc_audio_plugin_bridge_test.exe` below. Final
generated execution log: `build-vc170-64/logs/vlc-endpoint-bridge-test.log`.
CMake Tools target/test discovery returned empty and configure failed without
diagnostics; validation used the existing generated worktree targets instead.
Final post-guard-change build: `build-vc170-64/logs/vlc-endpoint-final-build.log`
confirms the production DLL, bridge executable, protocol 12/12 and viewer link.
No warnings/errors were found in that final build log. With the environment
below loaded, the exact final build command was:

```sh
cmake --build build-vc170-64 --config RelWithDebInfo --target media_plugin_libvlc vlc_audio_plugin_bridge_test INTEGRATION_TEST_llviewermedia_audio vulkanstorm-bin -- /m:2 /p:CL_MPCount=2 /verbosity:minimal /nologo
```

Configured worktree `build-vc170-64`: RelWithDebInfoFS_open, Windows x64,
`--no-opensim --soloud --zink --avx2 --no-package`, `USE_DISCORD=OFF`,
`OPENSIM=FALSE`, `LL_TESTS=FALSE`, `USE_LTO=OFF`,
`LL_VLC_AUDIO_BRIDGE_TESTS=ON`, `LL_VLC_AUDIO_PROTOCOL_TESTS=ON`.
CMake Tools target/test discovery returned empty; the exact generated targets
were built with the existing Cygwin convenience environment:

```sh
source /cygdrive/c/Users/Anne\ Skydancer/AppData/Local/VulkanStorm/fs-build-variables/convenience RelWithDebInfo
export LL_BUILD AUTOBUILD_VSVER=170 AUTOBUILD_BUILD_ID=$(git rev-list --count HEAD)
export AUTOBUILD=$(cygpath -d "$(command -v autobuild)")
cmake --build build-vc170-64 --config RelWithDebInfo --target INTEGRATION_TEST_llviewermedia_audio media_plugin_libvlc vlc_audio_plugin_bridge_test -- /m:2 /p:CL_MPCount=2 /verbosity:minimal /nologo
export PATH="$PWD/build-vc170-64/packages/bin/release:$PWD/build-vc170-64/sharedlibs/RelWithDebInfo:$PATH"
build-vc170-64/media_plugins/libvlc/RelWithDebInfo/vlc_audio_plugin_bridge_test.exe
```

Run from this worktree in Cygwin. Production plugin build and 12/12 protocol
tests passed: `build-vc170-64/logs/vlc-protocol-review-build-3.log`.
Final bridge suite: 8 cases passed, log
`build-vc170-64/logs/vlc-bridge-recovery-test.log`, compile log
`build-vc170-64/logs/vlc-bridge-recovery-build.log`. Artifact:
`build-vc170-64/media_plugins/libvlc/RelWithDebInfo/vlc_audio_plugin_bridge_test.exe`.
Production DLL: same directory, `media_plugin_libvlc.dll`.
The first delayed-fence test used the latest clock rather than the exact fence;
its failure is preserved in `vlc-bridge-fence-test.log`, corrected boundary test
passed in `vlc-bridge-fence-test-2.log`.

Bridge tests never create a VLC player or physical device. Real decode, callback
ABI execution, teardown races under libVLC, hardware surround, device failures,
audible buffering recovery, native-video regressions and non-Windows/effects-off
build variants remain unqualified. Full viewer link evidence is tracked in the
viewer contract; no viewer launch, package, installer or dependency publication.

## Archived independent-agent handoff

All remaining sections record the pre-integration stage. Frozen-engine claims,
missing generation fields, device reopen limitations, and UNRUN test statements
below are historical and superseded by the current contract above.

## Source contract recorded before implementation

Scope is the VLC plugin and its private adapter/build integration. The frozen
`llvlcaudio.h/.cpp` and engine tests are not modified. Native Vulkan, viewer,
protocol-owner files, SoLoud effects selection and dependency publication are
outside this change.

Baseline `MediaPluginLibVLC::setVolumeVLC` uses integer VLC volume and Windows
waveOut volume. `playMedia` stops but does not release the previous player/media.
VLC event callbacks currently send IPC and alter control-thread state. Preserve
URL, metadata and video callbacks while fixing lifecycle ownership locally.

Verified sources: packaged VLC public callback declarations and upstream tag
3.0.21 `modules/audio_output/amem.c`, `src/audio_output/common.c`; engine contract
`doc/vlc_audio_engine.md` and the frozen implementation.

- amem accepts S16N only. Counts are frames, PTS is nonnegative microseconds in
  the libVLC playback clock, not media-position milliseconds. Keep original PTS
  and adjust only by source-frame offset when splitting blocks.
- amem Start assigns an explicit output mask by negotiated channel count. This
  is evidence about amem output, not evidence of the original authored mask.
  Initial integration accepts unchanged mono/stereo only; multichannel is
  explicitly rejected pending mask/order impulse qualification, never fabricated
  or silently reduced to stereo.
- amem invokes flush with ONE argument, although the public typedef has a
  timestamp. Never read that nonexistent timestamp.
- amem invokes the volume callback as returning int although the public typedef
  says void. Windows x64 adapter uses an int-returning implementation through the
  public callback typedef and returns zero. Installing it prevents amem's cubic
  software gain path; decoded amplitude remains unity, engine gain applies once.
- No libVLC stop/release, IPC, logging or engine lifecycle call occurs on a VLC
  callback. A private serialized worker owns engine control. VLC producer calls
  copy bounded PCM/events; cancellation-aware waits occur only off the device RT
  thread. Idle publishes states. The device RT path remains the frozen engine.

Hypothesis: bounded, ordered callback forwarding plus a separate engine owner
avoids VLC/control reentrancy deadlocks while preserving variable blocks and
full-duration engine gain commands. Discriminating checks: callback cancellation
during queue saturation/setup/drain; stale epoch rejection; exact split PTS;
gain-to-zero without PCM must NOT acknowledge completion. Immediate checks are
editor diagnostics; main agent runs serialized builds/tests (no terminal use by
this agent).

## Wire contract and qualification gates

Windows x64 compiled support advertises `media_audio: 1.0` in init_response
`versions`. Explicit configure opts into PCM; dimensions never select the role.
Unconfigured instances retain native VLC output, including video. Other platforms
retain native VLC without advertising this feature.

Incoming: `configure {role: music|object|nonpositional, generation: decimal-U64
string}`; `set_gain {target: real, duration: real seconds, hard_mute: boolean}`;
`spatial {right: real, forward: real}`; `transition {target: real, duration: real,
serial: string}`. Nonpositional uses the Music engine policy. Decimal U64 includes
zero; subsequent configure generations must strictly increase. Internal decoder epochs
are separate from the viewer generation. No URL is fabricated.

Outgoing `state` contains exactly `generation`, `serial`, `state`, `detail`.
Detail is a fixed diagnostic code, never a URL, title or device/user identifier.
States use `priming|running|buffering|paused|drained|failed|silent`.

IMPORTANT: frozen engine `currentGain` describes the next sample and `Drained`
means submitted to WASAPI, not heard. There is no endpoint padding/latency or
gain-command-consumed fence. This adapter must NOT emit `silent` based on a
timer, nor let a viewer treat `drained` as audible completion. Report
`endpoint_tail_unqualified` instead. Full destroy-after-fade acknowledgement is
blocked on an engine-owned endpoint-tail fence/API extension, returned to the
engine owner rather than editing that API here.

Explicit PCM roles are audio-only (`:no-video`). A visible video request in that
mode is unsupported; default nonnegotiating video stays native. A/V clock sync,
positional video, multichannel amem impulses, endpoint-tail completion and audible
device qualification remain open gates, not completed parity.

## Implementation

`llvlcaudiobridge.h/.cpp` is private plugin integration, not an engine API change.
One worker owns the engine for the plugin lifetime. One serialized callback
producer copies up to 1024 S16N frames per request into a fixed 64-entry adapter
queue. The engine has its own bounded 32768-frame queue. Ready requests are not
timer-paced. QueueFull retains the exact request and PTS; neither callback nor
engine saturation silently drops valid frames. A five-second non-RT saturation
deadline reports failure and cancels publication. Engine backlog timeout is
suspended while paused. Setup/drain waits have a ten-second deadline and an
immediate cancellation predicate. These deadlines are explicit failure policies,
not dropout concealment or real-time guarantees.

Pause/resume and flush have control priority. Flush discards earlier adapter
requests and the worker's pending PCM, then advances the engine stream token;
the unused public flush timestamp is never inspected. Drain remains ordered
after the final PCM and blocks the VLC producer until submitted drain or explicit
failure/cancellation. A source-PTS discontinuity triggers an explicit tokenized
flush and retries the original pending block; idle reports
`buffering/pts_discontinuity_flushed`. It does not rewrite PTS to hide a gap.

Pre-setup gain commands retain order in a bounded staging array. The plugin
preserves the initial gain when a target/duration arrives before playback.
Compatibility `media_time:set_volume` becomes a float, zero-duration engine
command on PCM instances. Neither integer VLC volume nor waveOut volume is used
there. Installing the success-returning amem volume callback prevents VLC's
software gain branch. Legacy native instances retain their old volume path and
small-surface caching policy; that policy never opts an instance into PCM or
assigns it a protocol role.

All protocol state, metadata, title, duration and dirty-frame IPC now runs from
idle. VLC event callbacks publish fixed atomic fields only. Metadata fields are
coalesced between idle ticks while preserving field-specific handling and title
deduplication. Intermediate metadata updates within one idle interval can coalesce.
Audio state detail contains only fixed diagnostic codes.

Stop/rebuild/unmap sequence: cancel adapter waits, worker `AudioEngine::stop`,
detach registered VLC events, control-thread `libvlc_media_player_stop`, release
player and media, then worker `AudioEngine::close`. Context and shared pixels
remain alive through stop/release. Callback registrations are tracked because
VLC 3.0.21 aborts when detaching a listener that was never registered. Its
`lib/event.c` dispatch/detach share the event-manager lock; `lib/media_player.c`
stop calls `release_input_thread` (input Stop/Close) and resource Terminate.
This is source review, not a completed runtime quiescence stress test.

The plugin owns only one live engine/device, never a SoLoud global backend.
Repeated same-format setup flushes/resumes the existing device. Live format
changes are rejected. After a stopped/released player, playback reconfigures the
same engine with fresh stream AND format tokens. Because frozen `stop()` is
terminal, this reopens the device on restart. A literal single device-open for
the entire plugin lifetime across stop/rebuild cannot be met by this API; an
engine reset/restart contract would be needed. There is no per-block device open.

The producer callback context is retained until synchronous VLC stop/release;
epoch changes happen only after that barrier. Requests with old epochs are
rejected. Wire generation is independent of internal stream/format tokens.
The agreed gain/spatial/transition messages have NO generation field, so stale
control messages from a former viewer generation cannot independently be
detected. This wire limitation is not silently fixed by adding protocol fields.

## Returned API Needs And Gaps

1. Needed from engine owner: an observable gain-command-consumed/completed token
  AND endpoint-tail completion fence in the real device clock. The adapter
  never emits `silent`. Zero transitions report `failed/endpoint_tail_unqualified`
  after conservative sample-progress observation, or
  `failed/transition_no_pcm_or_paused` when PCM cannot carry the envelope.
  Natural EOS also reports `failed/endpoint_tail_unqualified` after engine drain.
  None of these failures authorizes a claim of audible fade completion.
2. Needed for literal once-per-plugin device open: reset/restart on an already
  negotiated stopped device. Current API explicitly rejects start-after-stop.
3. Channel-impulse validation of real VLC amem output remains necessary before
  enabling 3-8 channel sources. Mono/stereo labels have source evidence, not
  physical speaker qualification. No LFE/bass-management feature is added here.
4. Explicit PCM is audio-only, with `:no-video`; detected video tracks report
  `pcm_video_not_qualified`. Unknown/late track discovery is not A/V proof.
  Default video remains native VLC. Native fallback on other platforms does
  not advertise `media_audio` and is explicitly not PCM-qualified.
5. Gain/spatial/transition type/range failures are explicit. Configure rejects
  decimal overflow, non-string tokens, invalid roles and non-increasing tokens.
  Rejected configure diagnostics are coalesced to the latest pending rejection.
  Superseded/interrupted transitions fail rather than acknowledge silence.
  A failure requires owner handling/reconfiguration, never silent native fallback.
6. Real decode, IPC protocol interoperability, WASAPI endpoint latency, audible
  starvation/recovery, device loss, seek/pause clock changes, callback teardown
  stress and native-video regressions have NOT been runtime verified.

## Validation Handoff

No terminal commands, builds, tests, viewer launches, device opens, commits or
branch operations were performed by this implementation agent. Editor
`get_errors` checks immediately followed each implementation edit and reported
no diagnostics. This does NOT establish that the changed translation units were
compiled or that IntelliSense analyzed the enabled PCM branch.

Main agent, serialized after the viewer agent releases the shared build tools:

1. Configure the existing worktree testing build for VS2022 x64 RelWithDebInfo,
  no packaging; enable `LL_VLC_AUDIO_BRIDGE_TESTS=ON` and test registration.
  Use published VLC 3.0.21 and SoLoud package header miniaudio 0.10.42. Reconfigure
  is necessary to add the source files, definition and test target.
2. Build targets `media_plugin_libvlc` and `vlc_audio_plugin_bridge_test` through
  CMake Tools. Capture compiler/link diagnostics, including the intentional
  Windows-x64 amem callback ABI cast. No `soloud.lib` may appear on the link line.
3. Put the published VLC runtime DLL directory in the test process DLL search
  path, then run CTest `vlc_audio_plugin_bridge` through CMake Tools. This test
  drives the production adapter callback entrypoints with explicit Headless
  output; it NEVER creates a VLC player or calls libvlc playback. It is NOT a
  real amem decoder, ABI or channel-impulse runtime test.
4. Added test cases cover pre-setup sample-duration gain, 70000-frame variable
  block splitting without loss, PTS units/offsets, ordered drain's explicit tail
  failure, pause/resume, timestamp-independent flush, no volume double-gain,
  stale callback epochs, saturated producer cancellation and layout rejection.
  Tests are authored but UNRUN. Frozen engine tests and their previous evidence
  are unchanged and must not be represented as adapter validation.
5. Repeat configuration/build with `USE_SOLOUD=OFF`: Windows x64 must still
  acquire the published soloud package and compile miniaudio directly exactly
  once into the plugin. No SoLoud effects module or autobuild publication edits.
6. Check a non-Windows configuration retains the native plugin without either
  new translation unit or `media_audio` advertisement. Real VLC decode/metadata/
  resize/unmap/stop stress and protocol serialization remain separate gates.

Only owned files changed: plugin source, plugin CMake, private bridge header and
implementation, the new bridge test, and this document. Engine/header/existing
tests, viewer/protocol owners, `LibVLCPlugin.cmake`, SoLoud modules and autobuild
were not edited by this agent.