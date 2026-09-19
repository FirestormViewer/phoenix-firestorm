# Plugin-owned VLC PCM audio engine

## Current endpoint contract (2026-09-18)

The production WASAPI endpoint fence is implemented and compiled. Deterministic
software tests pass; physical endpoint and acoustic acceptance remain pending.
All sections below "Archived pre-extension record" are historical, including
their former architectural-blocker and frozen-interface assertions.

### Source proof and owned dependency change

Published miniaudio 0.10.42 input SHA256:
`cc050485bc91a4d4e17a509a898b425be92825174bffd56b1576a5bcf15dcd3d`.
`indra/media_plugins/libvlc/prepare_miniaudio.cmake` verifies this exact input and
unique patch anchors, producing a private `vlc_miniaudio/llvlc_miniaudio.h` in
the plugin build directory. It never edits `packages/include/soloud/miniaudio.h`.
The standalone harness produces its own copy in `.audio-test-build`.
No package version, autobuild dependency, SoLoud effects library or external
publication changed. This local dependency extension requires maintainer review
before adoption; its entire transformation is the owned CMake file and its
playback implementation is `llvlcwasapi.h`.

Original published-header evidence (line numbers refer to the unmodified input):

- 12269: `ma_device__read_frames_from_client` invokes the callback directly when
   `playback.converter.isPassthrough`; otherwise it converts stack-buffered PCM
   and may retain resampler state. Qualification now REQUIRES float passthrough,
   matching internal/client rate/count/map, shared mode and registered endpoint
   notifications. Unsupported endpoint formats fail explicitly; there is no
   guessed frame mapping, automatic reroute or legacy-audio fallback.
- 16614-16643: original playback callback precedes `ReleaseBuffer`. The owned
   loop records submissions only after successful release, requiring the sum to
   equal the engine output clock. Failed release never publishes consumption.
- 16000: shared-mode padding counts frames not yet read by the Windows audio
   engine. It is not proof that speakers played the data.
- 32821: the device worker initializes COM before entering its backend loop.
   Clock `GetService`, queries and `Release` now all occur on that same worker.
   RAII releases the clock on every return before backend stop/reset and before
   device uninit can release the client. No raw COM pointer reaches the owner
   polling thread. Setup service acquisition is outside the render callback.
- 14145/14253: the original notifications ignore loss/default changes when
   auto-routing is disabled. The owned playback-only notification branches now
   invalidate atomics and signal the playback event, never stop/reopen from a
   notification. Recovery remains an explicit new device epoch/generation.

Microsoft contracts:
[IAudioClock::GetPosition](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition),
[IAudioClock lifetime](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nn-audioclient-iaudioclock),
[GetCurrentPadding](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getcurrentpadding),
[GetStreamLatency](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getstreamlatency).

### Conservative consumption

`EndpointTimeline` is shared by production and fake endpoints. On each valid
observation, query padding FIRST, then device position. `submitted - padding`
is the content boundary read by Windows. Retain that boundary and its measured
clock position until subsequent device-clock progress reaches
`ceil(maximumStreamLatency100ns * clockFrequency / 10000000)` ticks. Only then
publish that content boundary as conservatively consumed. A single retained
boundary bounds storage and can delay acknowledgement; it cannot advance it.
This deliberately includes an extra full maximum latency after the observation.
Device clock units are never mistaken for content frame counts, including after
underruns. No QPC extrapolation, elapsed timer, zero amplitude or render/submission
counter alone can complete a fence. This is an OS-contract physical-consumption
bound, NOT a claim about acoustic output, external receivers or broken drivers.

Transition fences identify the first rendered target sample's exclusive end;
EOS fences identify the final PCM/resampler-tail sample's exclusive end. Both
are output-rate indices independent of callback partitioning. Source-rate
conversion and the existing bounded EOS filter-tail policy occur BEFORE these
indices; there is no second backend converter to hide unsubmitted samples.
Priming/paused transitions advance on output frames, not source PCM. Hard mute
and zero initial user gain never shortcut endpoint completion. Pause freezes
PCM, not the output device/transition clock. Flush/handover invalidate old
stream/format/serial fences without resetting the device's submission timeline.

Device epochs increase across configure/reopen. Stale observations are rejected.
Clock regression, changed frequency/latency, invalid padding, arithmetic overflow,
query failure (including inaccurate S_FALSE), submission failure and device
notification invalidate qualification and publish DeviceError. Missing progress
can only produce the existing explicit failure deadline, never success. Stop
invalidates acknowledgement immediately; intentional stop races are not reported
as endpoint failures. Close joins the worker before releasing device resources.
The status snapshot has bounded retries; contention yields no acknowledgement.

The headless `render(..., submissionSucceeded)` and `observeHeadlessEndpoint`
exercise the same submission validation and `EndpointTimeline`. The older
`advanceHeadlessEndpoint` convenience call models a zero-latency endpoint through
that same observer. It cannot bypass error, generation or epoch checks.

### Verification and limits

VS2022 optimized/debug-symbol engine tests: 12 top-level suites, resampled-flush
and zero-allocation checks pass; fixed engine storage is 1,423,744 bytes. Coverage
includes delayed padding, held physical tail, nonzero targets, 1/37/large callback
partitions at 44.1/48/96 kHz, 64-bit counters, fractional clock guard rounding,
overflow, clock reset/failure, failed submission, old epochs, pause/flush,
hard-mute/control saturation and teardown. Prior 2/6/8 routing/LFE, extreme-rate
resampling and 200-flush concurrent stress still pass. Command:

```powershell
& ./indra/media_plugins/libvlc/tests/build_audio_tests.ps1 `
   -PackageInclude C:/Dev/vulkanstorm/vlc-smooth-fades/build-vc170-64/packages/include
```

Log: `build-vc170-64/logs/vlc-endpoint-engine-test.log`.
Executable: `.audio-test-build/llvlcaudio_test.exe`. Bridge and production/link
evidence are recorded in the plugin/viewer documents. Tests do not instantiate
the WASAPI COM loop; it is compiled, while its shared accounting is executed.
Physical tests must qualify actual device latency/clock behaviour, all-main
stereo/6/8 output, default changes/disconnect/reopen, mute/pause/seek/EOS tails,
libVLC callback teardown and audible A/B/C stream handoff. No device, viewer or
microphone was opened. No physical-silence or acoustic-parity claim is made.

## Archived pre-extension record

The following proposal and integrated contract preceded the implementation above
and are preserved as source/history, not current status or acceptance evidence.

### Endpoint implementation proposal

Source contract: published miniaudio 0.10.42 calls the playback callback before
`IAudioRenderClient::ReleaseBuffer`. Its passthrough converter preserves frame
indices; its general converter may retain input and resample. Shared-mode
`GetCurrentPadding` measures frames not yet read by the Windows audio engine,
not physical consumption. `IAudioClock::GetPosition` is device-stream progress
in `GetFrequency` units, not a count of successfully submitted content frames
across underruns. `GetStreamLatency` reports a lifetime-constant maximum stream
latency. Clock services must be released on their GetService thread.

Design under implementation: require shared-mode float passthrough at the
negotiated endpoint rate/map. Keep source-rate conversion in the existing engine
before the output-frame fences. Observe successful ReleaseBuffer submissions,
then sample padding followed by the device clock. Retain a content lower bound
(`submitted - padding`) until a full maximum stream latency has elapsed in
subsequent measured device-clock progress. Never extrapolate from wall time.
Bind completion to device epoch, stream/format generation and transition serial;
flush/handover cancel old fences without resetting device submission accounting.
The fake endpoint must exercise this same accounting, not a completed-flag path.

Hypothesis: this conservative two-stage boundary remains safe despite callback
partitioning, rate conversion, underruns and a held device tail. Discriminating
tests hold padding, then release padding while holding the device clock short
of its latency guard; neither stage may acknowledge. Clock regression/failure,
stale epoch/generation, pause/flush and failed submission must not acknowledge.
No headless result proves acoustic silence or physical hardware acceptance.

The scoped dependency extension will be owned and reproducibly applied outside
the installed package, with exact input verification and reviewable changes.
No generated package, package version or external publication is authorized.
This section records intent, not completed implementation or verification.

### Superseded integrated contract (before endpoint extension)

This section supersedes the archived design/agent handoff below. Engine, bridge,
plugin and viewer are now integrated in this worktree; the engine is no longer
frozen. Production WASAPI endpoint completion remains an architectural blocker,
so the integration is NOT complete or runtime-qualified.

The engine owns a bounded S16N queue, labelled channel matrix, published
miniaudio 0.10.42 resampler and WASAPI output. No SoLoud library is linked into
the plugin. User/category gain, transition gain and continuity gain are separate
factors applied exactly once. Hard mute is an independent atomic override.
User gain advances on rendered PCM; transition gain advances on output callback
frames, including priming/paused/empty output. Empty output is not evidence of
an audible fade. Without callbacks a transition cannot complete; the plugin
reports clock failure after its bounded deadline, not successful silence.

In addition to the original interface, the implementation supplies:

```cpp
Result transition(float target, double durationSeconds, std::uint64_t commandId) noexcept;
Result handover(Role role, const Format& source, Generation generation) noexcept;
Result advanceHeadlessEndpoint(Generation generation, std::uint64_t consumedFrames) noexcept;
```

Transition IDs strictly increase. `transitionConsumed` acknowledges command
consumption; `transitionCompleted` acknowledges rendering the target sample.
Neither proves physical playback. `clockFrames` counts callback frames, not PCM
content or DAC progress. `transitionFenceFrame` is the conservative end of the
callback containing completion, captured once for that command. Flush and
handover invalidate old command/fence tokens. Handover also resets transition
gain to unity, installs a fresh source/resampler/matrix on the consumer and
retains the negotiated output device. Fresh stream AND format tokens are
required; new PCM waits for the consumed format token. Flush preserves envelope
phase but cancels its completion identity.

`endpointTransitionCompleted` and `endpointDrainComplete` are distinct from
render completion. They are published only for a qualified endpoint, matching
accepted/consumed generation, error-free and not stopped. The headless adapter
advances an explicit monotonic consumed-frame count no greater than the rendered
clock. Tests hold that count one frame before the command fence, then cross it.
It is never automatically advanced by render or elapsed time. The endpoint
advance API is serialized with the headless consumer/lifecycle, not a second
concurrent control producer. A sequence-checked snapshot binds command, frame
boundary and generation; the real-time render path remains allocation-free.

### WASAPI blocker: rendered is not physically consumed

Source inspected: worktree published
`build-vc170-64/packages/include/soloud/miniaudio.h`, pinned 0.10.42.
The playback branch at lines 16614-16643 queries available space, obtains an
`IAudioRenderClient` buffer, calls `ma_device__read_frames_from_client`, then
calls `IAudioRenderClient::ReleaseBuffer` and increments its private worker-local
`framesWrittenToPlaybackDevice`. Engine render completion occurs BEFORE release.
`ma_device__get_available_frames__wasapi` (around 16000) uses
`IAudioClient::GetCurrentPadding` for shared-mode writable capacity; exclusive
mode follows a different whole-buffer policy. This is not an exposed physical
playback cursor. The pinned public device API supplies no completion fence, and
the header has no IAudioClock/GetPosition integration. Private
`ma_IAudioClient_GetStreamLatency` wrappers do exist (around line 13847), but
are not used by this playback path and do not bind a command to a consumed
frame position. A latency value plus an elapsed timer is not a fence.
Backend client pointers
alone do not establish cross-thread COM ownership, conversion/cache alignment,
successful submission accounting, device-reset identity or DAC completion.

Consequently `endpointQualified` is false for WASAPI. No timer or estimated
padding turns it true. Zero-transition and EOS paths explicitly report
`endpoint_tail_unqualified` instead of `silent` or successful drain. Resolving
this requires a source-backed backend integration binding successful submissions
to an endpoint playback-position/latency contract, with reset/device-loss and
format/conversion handling. A privately owned WASAPI backend or coordinated
miniaudio extension is a possible next design, not an implemented/proven API.
No package or 3p source was modified in this review pass.

### Verification

Device-free engine harness rerun on 2026-09-18 using this worktree's headers:

```powershell
& ./indra/media_plugins/libvlc/tests/build_audio_tests.ps1 `
   -PackageInclude C:/Dev/vulkanstorm/vlc-smooth-fades/build-vc170-64/packages/include
```

VS2022 x64 `/O2 /Zi /MD /DNDEBUG /W4 /WX /std:c++17`, linker `/DEBUG`:
11 top-level suites passed, with separate resampled-flush and zero-allocation
checks. Fixed storage is 1,423,616 bytes. Evidence:
`build-vc170-64/logs/vlc-engine-review-test.log`; artifact
`.audio-test-build/llvlcaudio_test.exe`. This revalidates signal envelopes,
quiet-PCM handling, reserve starvation/recovery, mono/stereo all-main upmix,
labelled multichannel/LFE routing, positional matrices, resampling and 200
concurrent generation flushes. Additional delayed endpoint/drain and handover
regressions pass in the bridge suite (see plugin contract).

No physical device, VLC decoder, viewer or audible acceptance test was run.
Real endpoint fencing, hardware channel maps, loss/hotplug, audible transitions,
PTS drift and A/V synchronization remain unverified. Multichannel engine tests
do not qualify VLC's unlabelled multichannel callback layout.

## Archived standalone design and handoff

Everything below records the earlier standalone stage, not current API or
integration readiness. In particular its frozen ownership, missing transition
API and stop-before-every-handover statements are superseded above.

## Contract before implementation

Scope: worktree `vlc-smooth-fades`, starting commit
`37973cec36dbfe7fd5560c18931bdb3d5d3f0309`. No existing plugin, viewer, CMake,
CEF, SoLoud engine, or native Vulkan implementation is changed. The approved
requirements in the implementation request are the design authority.

Source evidence at this baseline:

- `indra/media_plugins/libvlc/media_plugin_libvlc.cpp`,
  `MediaPluginLibVLC::setVolumeVLC`: converts the current floating gain to an
  integer percentage for `libvlc_audio_set_volume` and, on Windows, also calls
  `waveOutSetVolume`. Current-gain updates are not full-duration target commands.
  Neither these calls nor the previously reported frame-paced gain stepping
  prove a buffering root cause. No such cause is claimed here.
- `indra/newview/llviewermedia.cpp`, `LLViewerMediaImpl::updateVolume`: the
  existing media path already supplies distance attenuation. Apply it once.
- Baseline package `build-vc170-64/packages/include/vlc/libvlc_media_player.h`,
  lines 608-778: play callbacks supply variable-sized interleaved blocks;
  count is frames (samples per channel); PTS is expected playback time in the
  `libvlc_delay` clock. Audio callbacks replace VLC's output. Format setup exposes
  only fourcc, rate and channel count, NOT a speaker mask. The header documents
  S16N. The requested VLC 3.0.21 amem contract is S16N, not float decode.
- Baseline package `include/vlc/plugins/vlc_aout.h`,
  `pi_vlc_chan_order_wg4`: FL, FR, SL, SR, BL, BR, BC, FC, LFE, filtered by
  the actual source mask. Channel count alone cannot prove that mask.
- Published `3p-soloud` package `include/soloud/miniaudio.h`: version 0.10.42;
  device exposes client and internal channel maps and sample rates; resampler
  initialization has the old two-argument API. Compile this implementation
  independently in the plugin; no `soloud.lib`, viewer SoLoud instance,
  `SOLOUD_ROOT`, or unpublished dependency is involved.

Local hypothesis: a bounded PCM consumer with gains evaluated at output sample
indices preserves encoded fades at static gain and makes a requested envelope
independent of callback partition size. A retained, unsubmitted PCM reserve can
carry a starvation ramp; no envelope can fade PCM that has already been sent to
the device. Detect queue depletion, never quiet sample amplitudes.

Discriminating check: render the same S16N blocks headlessly with callback sizes
1, irregular partitions, and large blocks; compare all output samples and exact
envelope endpoints, including 0.1/0.01 targets, retargets, starvation and recovery.
Use labelled 2/6/8-channel impulses to disprove incorrect count-only routing.
Compile optimized with debug symbols using VS2022 x64; never open an audio
device during tests. This is software validation, not audible acceptance or
qualified audio/video synchronization.

## Implementation and readiness

Implemented in `indra/media_plugins/libvlc/llvlcaudio.h` and `.cpp`, namespace
`llvlc`, class `AudioEngine`. This is a functioning PCM engine, not a VLC adapter.
The existing plugin/viewer/build files remain unchanged. No viewer effects
SoLoud instance, OpenGL/native-renderer objects, IPC, VLC volume API, or waveOut
volume API is used by this engine.

The bundled `vlc/libvlc_version.h` confirms 3.0.21. The implementation compiles
the published miniaudio 0.10.42 header exactly once, privately in this translation
unit; a version assertion forces an audit when that package changes. The public
header has no third-party or viewer dependencies. Miniaudio supplies WASAPI,
sample-rate conversion and anti-alias filtering. The local matrix handles
labelled surround routing and horizontal positional policy; miniaudio's stereo
panner does not implement arbitrary speaker-mask azimuths.

`Wasapi` is the default mode: `configure` opens the default playback endpoint in
a stopped state, and `start` starts it. Only the WASAPI backend is requested, with
no fallback to a null device, other backend, VLC output, or stereo truncation.
Non-Windows-64 real-device configuration returns `UnsupportedPlatform`.
`Headless` must be explicitly requested and never opens any device or microphone;
its `start` is optional and `Status::deviceStarted` remains false. The same
`renderFrames` implementation serves headless rendering and the real callback.

## Frozen public interface

The complete source of truth is the new header. Exact method signatures:

```cpp
AudioEngine();
~AudioEngine();
AudioEngine(const AudioEngine&) = delete;
AudioEngine& operator=(const AudioEngine&) = delete;

Result configure(Role role, const Format& source, Generation generation,
                 const Options& options = {});
Result start();
Result enqueuePCM(const std::int16_t* samples, std::uint32_t frames,
                  std::int64_t ptsMicroseconds, Generation generation) noexcept;
Result gain(float target, double durationSeconds, bool hardMute = false) noexcept;
Result pause() noexcept;
Result resume() noexcept;
Result flush(Generation nextGeneration) noexcept;
Result drain() noexcept;
Result spatialDirection(float right, float forward) noexcept;
Result stop() noexcept;
Status status() const noexcept;
void close() noexcept;
Result render(float* interleavedOutput, std::uint32_t frames) noexcept;
```

- `Role`: `Music` (nonpositional) or `Object` (horizontal mono/stereo sources).
- `Generation`: two `uint64_t` fields, `stream` and `format`. Both start nonzero.
  Every successful `configure`, including recovery, requires both to exceed the
  last configured/current tokens. `flush` requires a strictly newer stream token
  and the same format token. Tokens must never be reused within this instance.
- `Format`: `uint32_t sampleRate`, `uint32_t channels`,
  `std::array<Speaker, 8> speakers`. This describes rate and ordered labels, NOT
  storage encoding. Input is always interleaved native-endian signed 16-bit PCM;
  the device callback and `render` always write interleaved float32.
- `Speaker`: `Unknown`, `Mono`, `FrontLeft`, `FrontRight`, `FrontCenter`, `LFE`,
  `BackLeft`, `BackRight`, `FrontLeftCenter`, `FrontRightCenter`, `BackCenter`,
  `SideLeft`, `SideRight`. Unknown, duplicate, unsupported, and multichannel
  `Mono` labels are rejected. The negotiated output must also pass validation.
- `Options`: `output` (`Wasapi`/`Headless`), `headlessOutput` (`Format`),
  `queueFrames` (default 32768), `reserveMilliseconds` (default 5),
  `primeMilliseconds` (default 20), `musicUpmixHeadroom` (default 0.70710678).
- `Status`: `state`, `error`, accepted `generation`, acknowledged
  `renderedGeneration`, `outputMode`, negotiated `output`, `queuedFrames`,
  `reservedFrames`, `renderedFrames`, `starvationCount`, `rejectedBlocks`,
  `lastSourcePts`, `currentGain`, `deviceStarted`, `hardMuted`.
  Counters are individually atomic observations, not a transactionally coherent
  snapshot. `currentGain` is the gain for the next PCM sample, not a meter.
  `renderedFrames` excludes waiting/paused silence but includes the EOS filter
  tail. `queuedFrames` can include stale generations awaiting reclamation.
- `Result`: `Ok`, `NotConfigured`, `InvalidFormat`, `InvalidArgument`,
  `UnsupportedLayout`, `UnsupportedPlatform`, `QueueFull`, `ControlQueueFull`,
  `StaleGeneration`, `EndOfStream`, `Stopped`, `DeviceError`, `ResamplerError`,
  `PtsDiscontinuity`. Callers must act on these results, never silently drop them.
- `State`: `Closed`, `Priming`, `Playing`, `Starving`, `Paused`, `Drained`,
  `Stopped`, `Error`. Errors from device initialization/start or unexpected stop
  are explicit. Recovery is owner-driven reconfiguration, not a silent retry.

## Ownership and publication invariants

There are exactly three execution domains:

1. One serialized control/lifecycle thread owns every public method except
   `enqueuePCM` and the headless `render`. It owns configure/start/stop/close,
   commands and status polling. Do not call lifecycle operations from callbacks.
2. One serialized PCM producer calls `enqueuePCM`. The adapter must establish
   single-producer ownership for its VLC player. Concurrent VLC play callbacks
   cannot directly share this SPSC endpoint. No caller-owned PCM is retained
   after `enqueuePCM` returns.
3. One consumer is either miniaudio's device callback or a single headless
   render thread, never both. Do not concurrently call `render` on an instance.

Construction allocates the fixed implementation block. Lifecycle/configuration
requires the producer and headless consumer to be quiescent. `status` must not
race `configure`/`close`. While configured, producer, consumer, and control
commands may run concurrently; `stop` is allowed during producer activity.
The release/acquire indices publish complete PCM blocks and complete commands.
The consumer copies data before releasing queue slots. No blocking mutex,
allocation, device lifecycle call, IPC, logging, or viewer call occurs in the
engine render path. Atomics used there are compile-time required to be lock-free.

PCM enqueue is all-or-none. Ring capacity is at most 32768 source frames; blocks
larger than its configured capacity are rejected before any copy. An adapter
may split larger VLC blocks at frame boundaries, preserving adjusted PTS. On
`QueueFull`, it must use bounded, cancellation-aware non-RT backpressure or report
a discontinuity and explicitly flush. It must not silently discard part of a
block or retry a rejected block with a fabricated PTS.

There are 64 bounded control slots. `ControlQueueFull` means the command was not
published and must be retried/coalesced by the owner. Flush is transactional:
failed publication does not change the accepted token. Hard mute bypasses this
queue using an atomic latch with a sequence token; older queued gain commands
cannot undo a newer mute. Normal commands take effect at the next callback
boundary. Commands cannot retroactively alter a frame already handed to WASAPI.

Flush invalidates the retained output and resampler history when consumed, while
preserving the main gain envelope's current phase and pause state. Old PCM,
including an old producer copy already in progress, is discarded by its stream
token. Newer PCM waits rather than being misclassified as stale. A paused
callback still consumes commands and reclaims stale queue slots. Poll
`renderedGeneration.stream` for acknowledgement; stop or device loss can prevent
that acknowledgement until lifecycle recovery. Do not wait forever for it.

## Gain, starvation, pause and EOS

`gain` accepts finite targets in `[0, 1]` and finite durations in `[0, 60]` seconds.
Duration is rounded to nominal output frames. For duration N, the first actual
PCM sample uses the current envelope value, sample N reaches the target, and
subsequent samples hold it. A zero-duration command is immediate. Retargeting
starts at the current sample-index value. Waiting for initial PCM or reprime and
pause do not advance the main envelope. Commands carry full durations, not
frame-paced integer approximations. Quiet decoded PCM advances normally.

Identity routing and static gain apply no content analysis, auto-normalization,
RMS detection, or extra start fade. Authored fades therefore remain authored
fades. Request an initial fade explicitly, e.g. `gain(0.f, 0)` then
`gain(target, duration)` before the first PCM render.

Normally a small reserve of converted but unsubmitted PCM remains behind the
callback output. When it can no longer be replenished, the remaining reserve
ramps from its current continuity gain to zero, with the last retained sample at
zero. Only then does the engine emit silence. Newly arriving PCM waits during
this ramp. Recovery waits for the prime-plus-reserve threshold and ramps from
zero to unity over the reserve duration. Startup primes at unity unless an
explicit gain command says otherwise. This adds real buffering latency; it does
not pretend to fade samples already emitted or infer starvation from amplitude.

Hard mute zeros subsequently rendered frames promptly, latches independently of
normal fades, and keeps consuming PCM so unmute does not replay a backlog. Its
`target` and `durationSeconds` are validated but do not delay muting. A normal
`gain(..., false)` releases the matching latch on the consumer and ramps from
zero. Already submitted endpoint buffers cannot be revoked by this API.

Pause is immediate silence with frozen PCM and envelope phase, not an extra
fade. Resume continues at that phase. For a soft pause, the owner must complete
an explicit gain-to-zero command before pausing. Stop is immediate mute and
rejects new enqueue calls; it is not a graceful fade or restartable pause.

Call `drain` only after the final producer call is complete, with no new producer
calls possible for that generation. It permits short streams below the prime
threshold, releases the reserve without a synthetic fade, and rejects new PCM.
Equal-rate output bypasses the resampler and drains exactly the input frame
count. Unequal-rate output drains miniaudio's interpolation/anti-alias state by
zero-padding only at EOS, for `ceil(64 * outputRate / min(inputRate, outputRate))`
extra output frames (at most 8 ms). This is a bounded IIR tail policy, not a claim
of a mathematically infinite filter tail. No such padding occurs for starvation.
The resampler's latency query alone was insufficient: the last-impulse test
exposed premature truncation, and the bounded tail fixes that regression.

`Drained` means the engine has submitted all its PCM/tail to the callback, NOT
that the physical endpoint has finished playing it. The adapter must qualify
endpoint latency and drain completion before closing a real device or signalling
audible completion to VLC. A VLC drain callback must not deadlock shutdown while
waiting for an owner request: its non-RT wait needs explicit cancellation.

## Channel and spatial contracts

WASAPI initialization requests native channel count/rate, float callback output,
no automatic source-rate conversion, and no automatic endpoint rerouting. The
packaged backend obtains the mix format and derives its internal channel map
from `WAVEFORMATEXTENSIBLE::dwChannelMask` (`ma_channel_mask_to_channel_map__win32`).
It uses conventional mono/stereo labels only for zero-mask one/two-channel
formats. The engine validates every label and requires the client/internal maps,
counts and rates to match; it rejects unknown, duplicate, height/AUX, >8-channel,
or missing required source-speaker layouts. It never assumes count determines
an arbitrary multichannel map. The physical map remains hardware-unverified.

Music mono is copied to every configured main speaker. Labelled stereo maps L to
all left mains, R to all right mains, `(L+R)/2` to front/back centers and mono.
The default upmix headroom is -3.01 dB per main (0.70710678); stereo-to-stereo
and mono-to-mono identity retain unity. Matrix row sums do not amplify bounded
input into routing-induced clipping. Finite output is clamped to `[-1, 1]` as a
final guard against resampler overshoot; this is not an audible no-clipping
qualification for arbitrary source material. The channel-labelled tests include
reversed input stereo and permuted device output order.

LFE is not a main speaker. There is NO full-band mono/stereo duplication into
LFE and NO synthesized bass-management branch in this version. Authored LFE in
multichannel content is preserved at its labelled destination. Any future bass
management must separately low-pass redirected bass, define crossover, phase,
headroom and interaction with authored LFE, and be tested as such. Calling
blanket full-band channel duplication "bass management" is not acceptable.

Authored multichannel content maps by label at unity, with no automatic upmix or
downmix. Missing destination speakers are an explicit `UnsupportedLayout`.
VLC WG4 order is filtered by the actual mask, unlike Windows bit order:

| Layout with known mask | VLC WG4 input order | Windows output order |
| --- | --- | --- |
| 5.1 rear | FL FR BL BR FC LFE | FL FR FC LFE BL BR |
| 7.1 | FL FR SL SR BL BR FC LFE | FL FR FC LFE BL BR SL SR |

The public VLC setup callback exposes only count, rate and fourcc. A six-channel
callback alone does not prove rear versus side labels. Integration MUST establish
the exact amem output mask/order using the shipped VLC implementation and a
channel-impulse fixture, then pass those labels. Unknown layouts must fail audio
setup explicitly; do not invent labels from count or silently downgrade authored
multichannel material. These engine tests validate supplied maps, not the pending
VLC adapter's ability to discover them.

Object audio accepts mono or labelled stereo. The viewer supplies a normalized
listener-relative horizontal direction `(right, forward)`; positive right and
positive forward mean exactly those directions. The owner must project and
normalize any 3-D direction before calling; zero/NaN/nonunit vectors are rejected.
Speaker label azimuths are FC/Mono 0, FL/FR -/+30, FLC/FRC -/+15, SL/SR -/+90,
BL/BR -/+150, BC 180 degrees. Labels identify nominal azimuths, not measured room
coordinates. The two enclosing available main speakers receive constant-power
panning. Stereo uses two virtual sources at direction -/+15 degrees, each with
0.5 headroom so their sum cannot overload a main; mono uses unity. LFE is excluded.
No second distance attenuation or Doppler is applied. Viewer distance/category/
user attenuation is combined once into `gain`'s float target. Spatial commands
currently update the matrix at callback boundaries, without movement smoothing;
rapid movement still requires audible qualification.

## Clocks and limits

Rates: 8000-192000 Hz. Channels: 1-8. Reserve: 1-10 ms, prime: 1-100 ms, with
combined converted buffering strictly below 8192 frames. The configured source
queue must hold the corresponding source-frame prime/reserve budget plus 32
frames for resampling lookahead. Invalid combinations are rejected; defaults
work through 192 kHz. Device callback sizes are miniaudio-owned; headless render
accepts at most 32768 frames per call and requires storage for `frames * channels`
floats. Input pointer storage for `frames * source.channels` S16 values is the
caller's responsibility. Null, zero-sized enqueue, impossible rates/labels,
oversized blocks, nonfinite controls and overflowing PTS calculations reject
without partial publication.

PTS must be nonnegative microseconds in the libVLC playback clock. Within one
stream generation, block starts must stay within 2 ms of the first block PTS
plus the accepted source-frame count at its nominal rate. Discontinuities return
`PtsDiscontinuity`; the owner must flush with a new stream token before retrying.
Queue-full retries do not advance this PTS anchor. Sample timestamps are retained
through the reserve. `lastSourcePts` is diagnostic source provenance, not the
DAC position; with resampling it reports the most recently consumed source frame
for that output (initial filter samples can report -1).

The engine resamples at nominal rates, does not schedule output against the wall
clock, and does not implement adaptive drift correction or libVLC video-clock
feedback. Queue depth, startup prime, reserve, resampler group delay and WASAPI
buffering all contribute latency. Never claim automatic video synchronization
from PTS storage or from the headless tests. Video use needs a separately
qualified clock/scheduling policy; music/headless signal correctness does not
close that requirement. Existing VLC video and CEF paths must remain intact.

## Integration and teardown protocol

1. Add only `llvlcaudio.cpp` to the VLC plugin target, with the published package
   include directory and C++17. Do not compile another miniaudio implementation
   into that target or link `soloud.lib`. The standalone test links with no extra
   audio library; miniaudio's WASAPI backend loads the required Windows APIs.
2. Create the engine/adapter on the plugin control thread. Configure the actual
   source role, rate, validated channel labels and fresh generation pair. Check
   every result; publish unsupported/readiness/errors to the existing plugin
   owner outside the audio callback. Then start the device on that thread.
3. Negotiate exactly four bytes `S16N` in VLC's setup callback. Register audio
   callbacks before playback; retain the existing video/metadata callbacks.
   Marshal setup/lifecycle and pause/resume/flush/drain requests to the serialized
   owner. Do not call `configure` from an active device or play callback. A setup
   handshake must have a cancellation path and must not deadlock the thread
   performing `libvlc_media_player_stop`.
4. Copy play blocks through `enqueuePCM` with their original PTS and the token
   bound to that decoder generation. Do not substitute the newest token onto an
   old callback. Keep VLC's decoded amplitude at unity and remove the plugin's
   integer-volume and waveOut controls only in the later integration change;
   avoid double software gain. Forward float targets plus full fade duration,
   hard-mute intent, role and spatial direction through the future plugin API.
5. For normal EOS, stop producer publication, issue `drain`, and observe the
   engine state. Endpoint-tail completion is a separate integration obligation.
   For a configured fade-to-stop, first issue gain-to-zero, let actual PCM carry
   the fade, then stop. No PCM means there is nothing to audibly fade.
6. For shutdown/recovery, the control thread calls `stop` first. This rejects new
   PCM, mutes, and stops/joins the device callback through miniaudio. The owner
   then cancels any adapter waits, stops VLC, and proves ALL setup/play/pause/
   resume/flush/drain/cleanup callbacks have quiesced (including callbacks already
   entered before stop). Keep callback userdata and the engine alive throughout.
7. Only after VLC callback quiescence may the owner call `close`/destroy the
   engine. `close` calls device uninit after stop and releases resampler state.
   The destructor calls close but cannot prove foreign VLC callbacks stopped;
   violating the owner protocol is a use-after-free bug. A headless caller must
   also join its render thread before close/configure. Reconfigure recovery with
   strictly newer stream AND format tokens; unexpected device stop remains an
   error until this explicit recovery, including negotiation of a new map/rate.

## Verification and remaining gates

Run from this worktree with the baseline package path:

```powershell
& ./indra/media_plugins/libvlc/tests/build_audio_tests.ps1 `
    -PackageInclude C:/Dev/vulkanstorm/build-vc170-64/packages/include
```

The helper uses VS2022 Community x64 (override `VisualStudioRoot` for another
VS2022 edition), `/O2 /Zi /MD /DNDEBUG /W4 /WX /std:c++17` and linker `/DEBUG`:
an independent RelWithDebInfo-equivalent build, not a full viewer configuration.
Outputs are ignored beneath `.audio-test-build`. No device is opened by the
tests and no microphone capture code is requested.

Passed headless evidence on 2026-09-18:

- Exact encoded sample-fade preservation and sample-index envelopes for 2/6/8
  channels, targets 0.1 and 0.01, first-PCM start, 1/37/large callback partitions.
- Rapid continuous retarget, pause phase freeze, hard mute over queued commands,
  timed unmute, bounded command overflow and atomic flush failure behavior.
- Real reserved-PCM starvation ramp, reprime threshold and recovery fade;
  decoded silence never classified as starvation; partition-invariant output.
- Stream/format rejection, paused stale-frame reclamation, EOS below prime
  threshold, exact equal-rate drain, stop/close, PTS discontinuity/overflow limits.
- Labelled WG4 6/8 impulses including authored LFE, nonstandard output ordering,
  reversed stereo, mono/stereo all-main upmix and LFE exclusion.
- Mono/stereo object panning for 2/6/8 speakers, permuted output labels and one
  attenuation factor. Miniaudio 44.1->48, 96->48, 192->8 and 8->192 kHz conversion,
  variable input blocks, exact callback-partition equivalence and final EOS impulse.
   A resampled generation flush also matches a fresh engine sample-for-sample,
   proving old reserve/filter state is not carried into the next generation.
- Three-thread PCM/control/render stress with 200 concurrent generation flushes,
  bounded queue observations and exact final-generation content isolation.
- Fixed engine construction allocation 1,420,352 bytes in this x64 build, below
  the 2 MiB test ceiling. Preallocated rings: 32768 PCM frames, 8192 converted
  frames, 64 control commands. Tests detect zero C++ allocations in enqueue/render.
  This does not instrument arbitrary C `malloc`; miniaudio's linear processing
  and reset paths were separately inspected to use inline filter/sample state,
  not owned heap pointers. No thread sanitizer was run.

Not yet qualified: live libVLC amem adapter/channel-mask discovery, real WASAPI
device open/start/stop/loss/recovery, hardware speaker impulses, endpoint drain
latency, audible fade/pan quality, video synchronization, or CEF/process-session
volume interactions. Non-Windows device backends are intentionally unsupported.
These are explicit integration/acceptance gates, not silent fallbacks and not
claims that audio acceptance has passed.