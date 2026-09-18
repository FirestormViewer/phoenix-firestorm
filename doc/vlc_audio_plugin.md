# VLC plugin PCM integration

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