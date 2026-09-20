# Native login, progress and logout screens

Update 2026-09-20: the hidden-bar behavior described below is superseded by the
stage-based progress correction in [connected acceptance](native_connected_acceptance.md).
That record tracks current builds and remaining validation limits.

## Scope and evidence

2026-09-18. Implementation worktree: `native-sl-login`, unchanged HEAD
`005a4ae8693cf9711093a3c44783bff7af605f99`; native architectural
checkpoint: `90af5a7220f1fec28e3c909c051b6ee7e062f10b`. Read-only GL oracle:
`59108e15a1f8f94d2da7c674d937d19f5cf9450d`, verified in
`worktrees/notification-gl-reference`. No reverted visual implementations or
GL-owner bridge functions are reused. The user has explicitly resumed this UI
slice. Governing rules: NV-00, NV-01, NV-02, NV-03, NV-12, NV-17 and NV-18;
[invariants](native_vulkan_invariants.md) and
[roadmap](native_viewer_roadmap.md) remain authoritative.

This record precedes implementation. Evidence so far is source inspection only.
No build, fixture, viewer, screenshot or measured-parity result is asserted.

## NV-00: Lifecycle presentation family

### 1. What does OpenGL do?

Roots in the pinned oracle:

- `llstartup.cpp`: `idle_startup`, login cleanup, authentication and startup
  completion; `set_startup_status` publishes producer-owned fraction, description
  and message through `LLViewerWindow`. Login cleanup initializes account-local
  start textures, shows full or mini progress according to
  `FSDisableLoginScreens`, exposes **Quit**, and calls `revealIntroPanel`.
- `llviewerwindow.cpp`: `setShowProgress`, `setProgressString`,
  `setProgressMessage`, `setProgressPercent`, `setProgressCancelButtonVisible`,
  `setStartupComplete`. Fullscreen visibility starts a fade; mini visibility is
  immediate. Text/percentage and action updates reach both views; message is
  fullscreen-only. Mini `setPercent(100)` hides the mini view.
- `llprogressview.cpp`: constructor/postBuild, `setVisible`, `fade`, `draw`,
  `drawStartTexture`, `drawLogos`, `setText`, `setPercent`, `setMessage`,
  `setCancelButtonVisible`, `handleKeyHere`, `handleHover`,
  `onCancelButtonClicked`, `revealIntroPanel`, `setStartupComplete`, `onIdle`,
  `handleUpdate`, `onAlertModal`, `handleMediaEvent`, texture initialization and
  release, destructor. CPU-only visual responsibilities include XUI layout,
  focus, key interception, dynamic MOTD height, logo placement and fade clocks.
  These are GL-exclusive visual-owner methods, not reusable native helpers.
- `llappviewer.cpp`: `userQuit` goes directly to `requestQuit` when progress is
  visible, otherwise uses `ConfirmQuit`/`finish_quit`. Startup Quit may send a
  partial-session logout before exit. Connected `requestQuit` closes floaters
  and starts shutdown. `idleShutdown` first waits for modal decisions and
  floaters, saves histories/final snapshot, waits for uploads/metrics with a
  deadline, then sends logout and waits for reply/deadline. Progress is
  `SavingSettings` with a real upload fraction or `LoggingOut`; fullscreen
  selection uses `FSDisableLogoutScreens`. The UI does not invent those phases.

The neutral declarations are `panel_fs_nui_login.xml`, `panel_progress.xml`
and `panel_progress_mini.xml` in the selected skin/language. Fullscreen progress
has a black or aspect-fill account start-image background, a declared centered
layout, title, status, percent bar, MOTD, conditional vendor logos and a
bottom-right action. Startup fade-in and world fade-out are linear over one
second; `onIdle` closes the login panel after fade-in. Fullscreen input consumes
keys except Ctrl-Q; its background hover selects the wait cursor. Startup's
button requests Quit, not teleport cancellation. After `STATE_STARTED` the
same GL button instead cancels teleport; teleport is outside this slice.

Transitive/callback obligations: intro media navigation, close events, audible
media ownership and unload; external MOTD URL activation and modal channel
handling; texture decode/upload/release; registered idle callbacks and focus
release; login validation/account storage and shutdown service callbacks.
These are not closed by reading the local progress body. In particular
`getVkDrawState`, retained GL raw-image bridges, `LLPanel`, `LLMediaCtrl`,
`LLViewerTextureManager`, GL font measurement and GL layout/input callbacks
MUST NOT be invoked. Intro media, per-account start-image publication, logo
variants and GL shutdown services not represented by native services remain
explicit integration/parity obligations, not silent completed omissions.

### 2. How is the result produced natively?

`LLVKViewerUi` owns native XUI progress panels, CPU visibility/layout and input
policy. Existing independently native factory, fonts, images, widget tree and
paint packets are the consumers; no GL visual owner is shared. Existing login
controls, defaults/Enter behavior, password recovery, account selection and
credential preparation stay in place. A session snapshot selects login,
authentication/challenge/agreement, connecting, connected, disconnecting or
stopped presentation. It never drives transport state or claims world readiness.

The existing `refreshSession` callback path remains responsible for tagged MFA,
agreement and actionable failure/retry notices. Only `NativeSessionProgress`
creation is replaced. Screen actions capture owner and tag and revalidate them;
Quit requests the window's existing orderly shutdown path. Existing owner
cancellation, cleanup retry, reverse service retirement and credential clearing
must remain. Communications declarations and implementation belong to the other
agent and are not renamed or edited.

The current snapshot has status/cleanup operations, identity and generation but
no percentage, MOTD, start image or world-ready signal. Until real producers are
bound, no synthesized percentage, timed completion or `STATE_STARTED` is allowed.
An absent progress value must not be presented as a made-up fraction. Fade-out
to a world cannot be inferred from `Connected` alone. Full/compact declaration
selection can preserve settings without pretending those missing inputs exist.

### 3. What is the cleanest native implementation?

Use a viewer-owned presentation subtree and small screen-state methods in the
assigned viewer files, with minimal window input integration. Do not add another
service/state machine, modal queue protocol, GPU resource owner, GL wrapper or
shared widget modification. Keep the progress screen independent of the modal
notice panel so agreements and cleanup errors can remain above it and actionable.
Composition uses existing immutable native paint data and resource lifetimes;
no new GPU allocations/barriers/descriptor recycling are introduced here.

Reuse the layered neutral progress XUI through `mDialogFactory`. Screen layout,
focus and optional fade clocks are native CPU responsibilities. Screen changes
must clear transient menu/capture state and prevent covered login/communications
controls receiving keyboard, pointer, wheel, browser or shortcut actions. Leaving
progress must restore the appropriate login or connected consumer, not stale
account focus. Cleanup failure keeps its recovery dialog; changing screens must
not call or skip retirement. The Win32 pre-window `LLVKStartupStatus` splash is
separate and remains unchanged.

## Falsifiable checks, before implementation

1. Source/diagnostic check: `refreshSession` no longer enqueues ordinary lifecycle
   progress notices; MFA/agreement/error branches and tagged cleanup retry remain.
   Native screen construction uses only native factory/tree/paint and neutral XUI.
2. Controlled snapshot sequence (integrator test): prelogin -> authenticating ->
   challenge/agreement -> connecting -> connected -> disconnecting -> prelogin
   and stopping. Verify actual state selects presentation, no fabricated percent
   or readiness, hidden credentials cannot submit, and generation changes revoke
   previous screen actions. Failures and cleanup Retry remain actionable.
3. Input test: Enter still invokes the login default action; progress swallows
   underlying keys/mouse/wheel/IME; fullscreen Ctrl-Q and the displayed Quit reach
   orderly shutdown once; modal input wins over screen input. Returning from a
   cancelled attempt restores valid login focus and account isolation.
4. Capture/operator gate: selected skin/language, full/compact settings, resize,
   DPI, full fade sequences, optional start image, MOTD/link, vendor logos and
   intro media must match the pinned reference exactly in visual/effects/input
   behavior. Missing service/resource inputs are failures of this gate, not an
   allowance for approximate parity. No tolerances are relaxed.

## Validation and review record

| Field | Record |
|---|---|
| Contract | NV-00/01/02/03/12/17/18; lifecycle screen presentation only |
| Reference | Pinned revision above; runtime dimensions, settings, media, timing and device captures not yet supplied |
| Data flow | Session snapshots to native CPU screen state; existing tagged commands and notices retained |
| GPU safety | Existing native widget paint/resource path only; no GPU ownership changes |
| Validation | Focused source/XML assertions, editor diagnostics, documentation links/ASCII and diff hygiene passed; no compiled/runtime/parity evidence |
| Limits | Actual progress, world readiness, account images, logos, intro media and exact captures remain open |
| Change class | Focused parity implementation work in progress, not feature/parity completion |

## Implemented presentation slice

- `llvkviewerui.h/.cpp`: `refreshLifecycleScreen` owns detached native full/mini
   screen trees. It extracts only the progress panel declaration from layered
   `main_view.xml`, removes the GL class registration and lets the native factory
   consume the referenced progress XUI. Skin fonts, colors, images and placement
   remain declaration-driven. It binds localized actual-state status and a tagged,
   single-request Quit action. `appendLifecycleScreen` prepares native layout,
   black background and a one-second full-screen fade-in, then composes the screen
   above ordinary UI and below actionable notices. It hides login subtrees after
   full fade-in, or immediately in compact mode, preserving their original
   visibility for retry/disconnect. There is no fabricated progress percentage.
- `llvkdialogs.cpp`: `refreshSession` calls that presentation after updating real
   state/controls, removes the ordinary `NativeSessionProgress` producer, and
   retains MFA, TOS/critical agreement, error, retry-login and retry-cleanup paths.
   Existing password clearing at connection remains. Login visibility is cached
   once rather than overwritten when a progress-hidden login reaches Connected.
   Returning login focus is restored after controls are enabled. Owner replacement
   hides the screen and restores cached visibility without retaining credentials
   or account paths in screen state.
- `llvkwindowmgr.cpp`: the lifecycle-only input boundary routes pointer actions
   to the screen and blocks underlying browsers, menus, preferences, key capture,
   text/IME and wheel input. Ctrl-Q requests existing orderly shutdown; other
   lifecycle keys are consumed. Required modal input retains priority. The wait
   cursor becomes an arrow over actionable Quit. No transport or retirement loop
   is changed.
- `llvkstartupstatus.cpp/.h` are unchanged. The existing pre-window and final
   shutdown splash is not a lifecycle modal and remains owned by startup.

The new public presentation interface is exactly:

```cpp
LLVKWidgetTree::Id lifecycleScreen() const noexcept;
void quitLifecycle();
```

The former reports the active input/composition root (zero outside progress);
the latter validates the current owner tag and login-phase state before using
the already-bound quit request. They do not authenticate, cancel transport,
signal readiness, or run cleanup. `sessionSnapshot()` remains the state source.

Communications/in-world integrator: no communication members or methods were
renamed or removed, and no edits were made to `llvkcommunications.cpp`.
Existing `initializeCommunications`, `clearCommunications`, and
`refreshCommunications` behavior remains the connected consumer. Focus currently
returns to `local_composer` under `mCommunicationPanel`. A replacement in-world
owner must provide its own valid focus destination at that existing handoff;
do not interpret `Connected` or absence of a lifecycle screen as `STATE_STARTED`.

## Static validation actually performed

- Editor diagnostics reported no errors in the four edited C++ files after
   each focused edit. This is not compiler or linker evidence.
- Ten serialized read-only source/XML assertions passed: no ordinary progress
   notice producer; retained MFA, agreement, tagged cleanup retry and Enter-default
   bindings; native main-view placement; lifecycle routing before key capture;
   no named GL visual-owner dependency added to viewer UI; both base full/mini
   declarations resolve with status and action controls. These are lexical and
   XML structural checks, not execution of the native widget factory.
- Initial parallel terminal output interleaved and was discarded as evidence;
   the above assertions were rerun in one serialized invocation. Further terminal
   validation is serialized. The final `git diff --check` passed; relative
   documentation links and ASCII hygiene also passed.
- No builds/configures, CTest, window fixtures, live viewers, installs, commits,
   branch switches, network changes or shared-memory writes were performed.
   Concurrent changes in the communications implementation were left untouched.

## Exact open behavior and service gaps

1. **Progress and messages:** the bar is hidden, not animated or assigned a
    guessed fraction. Snapshot status does not distinguish upload-saving,
    metrics-wait, logout-request and logout-reply phases or provide a MOTD.
    `LoggingOut` currently describes Disconnecting as a whole. The integrator
    needs a generation-tagged producer record with `std::optional<float>` percent
    in [0,100], localized phase key and bounded public message, published from
    actual completed work. No raw login response, credential or token may become
    display text. Those fields/producer bindings are not added to the session owner
    or transport in this change.
2. **World transition:** Connected hands off immediately to the current native
    communications view. There is no fake readiness or fade-to-world timer. The
    in-world owner must supply an explicit generation-tagged viewer-ready event
    before a source-equivalent one-second fade-out can be implemented. A transport
    connection alone cannot publish it. Compact completion likewise needs a real
    percentage/completion producer, not a forced 100.
3. **Images and media:** fullscreen currently uses the source black/no-image
    path. Account-local last/home image selection (`UseStartScreen`, start
    location, grid and legacy BMP fallback), decode/publication, vendor logo
    variants and intro media/audio/close/unload are not implemented here. No
    account image from another profile or GL owner is accessed. Integration needs
    native-owned image publication for the active account and independently native
    browser/audio callbacks before claiming those configurations. MOTD link and
    dynamic-height behavior await the real message producer.
4. **Input and variants:** fullscreen input interception is implemented; compact
    mode currently also isolates lifecycle input. The reference mini panel is not
    a fullscreen key sink, so exact compact in-world interaction remains open
    pending the real in-world input owner. Default/native login is retained;
    legacy-login and additional skin/language/layout/font/effects combinations are
    not capture-qualified. The title currently uses the native `[APP_NAME]` label
    context, not a call to the GL application's title owner.
5. **Lifecycle timing:** instantaneous cleanup may reach Stopped without a
    presented logout frame. No artificial dwell is inserted. Existing owner
    cancellation, retry-cleanup and service retirement are unchanged; service
    parity with GL histories/final snapshot/uploads is not established by this UI.

## Integrator gates

Serialize the existing native UI/widget and window builds/tests, then the viewer
link gate. Add controlled snapshot/input assertions to the integrator-owned test
surface (no new target was created here): both progress modes, MFA/TOS ordering,
cleanup Pending/RetryCleanup, stale actions, rapid cancel/retry, owner replacement,
modal dismissal, login Enter, account isolation and resized/high-DPI placement.
Exercise the native factory against selected skin/language layers, not only XML
parsing. Verify focus, clips, alpha and modal painter order through captures.

All compiled/runtime/parity gates remain unverified. Exact image, effect and
interaction parity is not complete. Future viewer launches must be announced,
allow manual login, wait for actual readiness plus an uninterrupted 75-second
dwell, then close through WM_CLOSE and verify exit zero and `Goodbye!`. Never
force-stop or run a fixture concurrently with a live viewer.
