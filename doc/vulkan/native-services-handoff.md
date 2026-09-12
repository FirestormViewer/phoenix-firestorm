# Native services handoff

Updated 2026-09-12 after read-only cache, startup policy and cache-backed previews.
This is a continuation checkpoint, not a completion or parity claim.

## Current objective

Engineer viewer-wide native equivalents for graphics-API-exposed portions of:
assets/texture residency, scene selection/picking, render configuration, and
audio/media/voice. Share audited API-independent code. Preferences is a client
of these services, not their lifetime owner. Full Preferences integration remains
the product goal; do not manufacture settings-only substitutes for missing services.

The service fixes were committed as 3a73d29462 at the user's request, without a
push. The user then requested implementation of shared nonvisual startup, native
visual initialization, and applicable parity with existing viewer shutdown.
The lifecycle changes below are uncommitted; no additional commit was requested.

## Lifecycle direction

This direction is partially implemented under NV-00/01/03/14/15/17, not full
lifecycle parity. The source survey used checkpoint 3a73d29462 on Windows. Its roots were
[Windows entry](../../indra/newview/llappviewerwin32.cpp),
[application startup and shutdown](../../indra/newview/llappviewer.cpp),
[login/world startup](../../indra/newview/llstartup.cpp), and
[view/GL destruction](../../indra/newview/llviewerwindow.cpp).

- Startup: share nonvisual services only after auditing constructors, callbacks,
  registered work and teardown. Independently initialize native UI services,
  fonts, images, notifications, floaters and tools. A native startup entry point
  should orchestrate explicit owners and initialization phases, not wrap the
  legacy LLUI/LLViewerWindow initialization or introduce a shared low-level RHI.
  Reuse existing native components and audited independent libraries.
- Shutdown: preserve applicable quit confirmation/editor resolution, account/history
  persistence, final snapshot, upload/metrics drain, logout/reply/deadline handling,
  service shutdown, GPU retirement and final process cleanup. Preserve settings
  save ordering, successful-login account guards and restore-without-overwrite.
  Rendering/capture and visual destruction require native equivalents; ordinary
  IO, protocol and other independent operations may be shared after audit.
- Applicability depends on actual lifecycle state: pre-login close, partial login,
  normal logout, initialization failure and crash termination are distinct paths.
  An absent native implementation is an open obligation, not a parity exemption.
  Never clean up a legacy singleton merely to mimic ordering if it was not started.
- Validation must distinguish component behavior, actual lifecycle wiring and
  end-to-end verification. Require initialization-failure unwind and shutdown-order
  checks, late-callback rejection, persistence guards and completion-based GPU
  retirement. Existing window/voice tests do not establish full viewer shutdown.

The current native entry returns before LLAppViewerWin32 construction. Its scoped
window/service cleanup does not yet replace the complete viewer startup,
authenticated quit/logout and final process cleanup contracts.

## Lifecycle implementation and evidence

- Native VisualServices in llvkwindowmgr.cpp now owns actual UI construction,
  renderer/GPU-cache creation and browser startup as separate phases. Existing
  native font/image/notification/floater/input implementations are reused, not
  legacy GL visual initialization. Settings, IO and voice continue using the
  previously audited shared nonvisual code.
- Teardown detaches native input and callbacks before file-picker destruction can
  pump messages, closes the browser, checks GPU idle, releases GPU/UI owners, then
  destroys context and HWND. Normal shutdown reports GPU retirement failure;
  partial startup and error exits use the same owner ordering. Driver-loss and
  browser-shutdown timeout behavior are not runtime-verified.
- WM_CLOSE, menu quit, restart requests and the test frame limit use orderly quit.
  Native queued/active modals delay closure. Preferences application quit preserves
  live bound values, matching LLFloaterPreference::onClose(true); ordinary close
  still cancels. Global, loaded-account and warning changes use existing persistence
  callbacks; successful Restore suppresses exit writes. This is not blanket Apply
  for unaccepted child-dialog drafts or a complete general exit-save implementation.
- Startup restores saved normal window geometry/maximization. Shutdown captures
  live normal placement before HWND destruction, without replacing normal size
  with maximized/minimized dimensions. Offscreen/multi-monitor/DPI/fullscreen and
  maximized/minimized runtime qualification remain open.
- Selected-native startup now catches failures through configuration and visual
  startup/shutdown, logs failure via the shared logging API, and emits Goodbye!
  only after the owned window/services return successfully. Preselection failures,
  complete application logging/markers/crash handling and update cleanup remain open.
- Window4/4 passed: rejected browser helper after native UI/device initialization,
  complete HWND unwind, subsequent successful startup, six presented frames, real
  WM_CLOSE, persisted normal client geometry and no GL parent module. No microphone
  capture was enabled. The interaction fixture consumes its informational notices;
  modal blocking is explicitly exercised by the widget shutdown test.
- Widget192/192 passed: ordinary-close rollback versus application-quit preservation,
  queued/active modal waits, save failure/retry, idempotence, warning persistence,
  pre-login account guard and Restore no-overwrite. Viewer link passed. The production
  entry and Goodbye! were compiled, not exercised through a full viewer launch.

Next lifecycle obligations include the remaining nonvisual process startup/termination
services, broader exit persistence, authenticated session ownership, upload/logout
coordination, native final-world capture and world-editor/tool closure. Missing
services remain open dependencies, not evidence of shutdown parity or exemptions.

## Immediate next step

Latest increment (uncommitted, Window6/6 and viewer link passed):
- Explicit readOnly cache configuration opens existing metadata and shared lock
  files without fast-cache creation, pruning, repair, validation updates or writes.
  Tests compare file bytes before/after and reject missing/incompatible/truncated
  indexes. Unique native worker names permit multiple readers. This is a component;
  production still starts writable, and readers cannot coexist with an active writer.
  Automatic secondary-viewer operation and cross-process/legacy concurrency stay open.
- planStartup now owns the source-backed capacity/version/encoder/purge/location
  policy and is used by actual startup. Tests cover signed bounds, current versus
  requested location, no read-only metadata writes, and retaining the global purge
  request. Old-cache removal and complete relocation/purge acceptance are unfinished.
- LLVKTexturePreview is wired into the native window loop. It reads cache/local
  encoded bytes, schedules at most two native decode jobs and generation-checks
  publication to native texture controls. Fixture evidence includes exact TGA RGBA,
  alpha, identity replacement, native paint, misses, incomplete/corrupt bytes and
  retry after cache writes. Existing native J2C/JPEG/TGA decoders are reused.
- Local UUID reads explicitly route through the shared file reader: the original
  UUID reader's LOCAL branch is disabled. Native file reads have a preallocation
  byte bound. No GL texture-fetch wrapper is invoked. The successful decode fixture
  is TGA; general progressive decoding or ready-image hot reload is not completed.

Next asset-delivery dependency: native authenticated region/capability ownership.
LLTextureFetch resolves ViewerAsset/texture URLs through the current LLViewerRegion
and distinguishes waiting for capabilities from disconnect. That native producer is
absent; do not use a generic downloader as a substitute for live asset integration.
HTTP cache-miss delivery, retry/cancellation across region changes, progressive decode,
world texture sampling/mips/residency, and bake/material previews remain open. No full
viewer, real profile cache, microphone capture or network asset request was exercised.

Persistent texture-cache integration is now implemented in the working tree:
LLVKTextureCache owns shared LLTextureCache through explicit path/watchdog/settings
dependencies, starts in llvkStartup before visual services, pumps from the native
window loop, and drains/stops after visual teardown. Window5/5 passed with real
isolated persistent write/read/reopen, misses, exclusive-owner rejection, purge,
version reset and fast-cache invalidation. Read/write futures return encoded bytes;
network fetch, progressive world decoding and Vulkan world-texture residency are not integrated.
Do not mistake UI GPU caching or these disk-cache tests for those consumers.

Production uses the existing cache paths, capacity clamp, version/encoder identity
and purge flags. A pending relocation selects the new directory without deleting
the old one. The global one-shot purge request is retained until the other caches
are integrated. Native secondary instances currently fail explicitly on ownership
conflict; automatic read-only secondary startup, full crash-marker handling, old renamed
cache deletion, path-race hardening and cross-profile legacy concurrency remain open.
No full viewer/profile-cache run was performed. The source contract records the
shared-library audit and timeout/IO limitations. Final viewer relink passed;
focused source diagnostics and patch hygiene checks passed.

1. Preserve the now-validated shared WebRTC shutdown correction. The source-confirmed
  defect was worker-to-signaling-to-worker deployment surviving the single worker
  barrier while thread members were moved away. Thread/resource owners now remain
  in LLWebRTCImpl, with atomic shutdown gates and worker/signaling/worker drains
  before signaling-thread connection transfer. Drains run inside the existing
  ten-second shutdown worker budget; the caller retains the object on timeout.
2. Window Validation passed 4/4 after the fix and after adjacent voice configuration
  wiring. Test4 now covers eight rapid restart cycles, queued device selection and
  refresh, never-started/idempotent stop, and processing configuration. Capture
  remains disabled; do not repeat these passed states without a new technical need.
3. Continue the remaining lifecycle obligations above before expanding session-dependent
  services. Continue from source contracts, not Preferences-only substitutes.
  Voice session transport remains open; a subsequent bounded audit
  is session-independent enabled/muted/gain ownership versus tuning transitions,
  before connecting it to authenticated session transport. Asset delivery/residency,
  live scene producers/picking and renderer consumers also remain open.
4. Shutdown limits: active peer observers, driver hangs, partial-init failure
  injection and non-Windows hotplug are not runtime-verified. The synchronous
  observer-list lock and observer/log callback lifetime on the timeout path are
  not proven bounded by the worker timeout. Do not claim complete shutdown parity.

## Saved work and evidence

- Current service checkpoint: 3a73d29462, based on 9089558822, branch native-vulkan-ui
  (recheck HEAD on resume). The accumulated native fixes are committed.
- Unrelated excluded changes remain uncommitted. Preserve them and any subsequent
  work; do not reset, clean, checkout, or discard files.
- Preferences opens the original hierarchy through the real native menu. Earlier
  notes describing a provisional window are historical, not current status.
- Preferences/widget gate: 192/192 passed after lifecycle changes.
- Latest reported context gate: 10/10 passed after scene-selection component work.
- Current window gate: 6/6 passed with cache policy/read-only access, previews and lifecycle coverage;
  no microphone capture, full viewer launch or acoustic/visual parity claim.
- Native core build and Viewer Link Validation passed. The first link exposed a
  stale llvulkan.lib missing the existing working-tree createSwapchain(...,bool)
  signature. Building only llvulkan fixed it; no VSync source changes were needed.
- Focused editor diagnostics and tracked patch whitespace checks passed.

## Newest API-service surfaces

- llvkimagepublication: invalidate() retires logical publication and discards an
  in-flight result only when completion allows it. GPU tests cover same-size
  replacement after invalidation. Preserve completion-based ownership.
- llvkbrowsersurface / llvkwidgetpaint / llvkwidgetgpu: streaming surface epochs
  invalidate obsolete media publications; ordinary frame updates may coalesce.
  Static skin-image caching remains separate.
- llvkcontext / llvkwindowmgr: RenderVSyncEnable selects FIFO or IMMEDIATE when
  available with explicit fallback; swapchain changes happen at frame boundaries.
  Requested/effective mode and recreation were reported GPU-tested.
- New llvksceneselection.{h,cpp}: CPU component with versioned triangles, origin
  epochs, separate HUD/world segments, shared math intersection routines, and hit/
  selection identities. It is NOT yet a complete live world producer, alpha-aware
  picker, or rendered selection-highlight service.
- New llvkvoice.{h,cpp}: UI-independent device/tuning owner over shared llwebrtc.
  Native window owns it directly; VoiceDevices only binds Preferences callbacks.
  configure() shares AudioConfig and the existing software APM, validates levels,
  retains requests without starting the engine, applies on startup/live changes,
  and restores defaults after overrides. The window synchronizes the authoritative
  settings group each frame, independently of Preferences visibility. State reports
  submitted configuration, not audio-effect measurements. Full voice-session
  transport is not done. Direct lifecycle test4 now passes after the shared fix.
- Existing native OpenAL work remains. The existing viewer audio engine reaches
  viewer-owned request_sound fetching, so wholesale reuse is not audited closed.
- RGBA UI uploads are NOT a complete world-texture residency implementation:
  world formats, sampling/mips, progressive asset delivery and consumers remain.

## Other checkpointed Preferences work

Source and tests contain beam color/shape editors, preset deletion, directory picker
generation guards, graphics preset Save/Load/Delete/Default, quality-mask policy,
global backup/restore, anti-spam reset injection, search highlighting and native
browser-cache clearing. The detailed source contracts and limitations live in
reverse-engineering/native-ui-construction-dependencies.md.

Known open items include inventory/local/bake/pipette texture picker modes, native
asset fetching/residency, authenticated account/history ownership, full renderer
consumers, RLVa enforcement, audio/media transport and cache relocation/purge.
Recommendations currently use explicit unavailable-bandwidth fallback classification.
The old Vulkan bandwidth probe was inspected but not approved as-is (unchecked
command results, repeated destination writes lacking dependency). Do not quietly
restore it or call recommendation policy measured parity.

## Methodology and constraints

- Read native_vulkan_invariants.md, native_viewer_roadmap.md and
  ../../indra/llvulkan/AGENTS.md before implementation. NV-00/NV-01 govern all
  supporting lifecycle, UI, build, media and shader work, not just Vulkan calls.
- Recover source contract, transitive callbacks/ownership, native design and a
  discriminating check before editing. Validate the touched slice immediately.
- Share audited nonvisual services/libraries. Independently own GL-coupled visual
  implementations; no shared low-level GL/Vulkan RHI, GL draw callbacks or GL frame
  presentation. Leave the GL implementation intact.
- The user approves careful control-by-control/tab-by-tab work. Explicitly label
  component implementation, live wiring, and runtime/end-to-end verification.
  Never equate fixture tests with full service integration or measured parity.
- Use dedicated VS Code build tasks with the canonical Cygwin environment. A prior
  reused command terminal interrupted a build and left orphaned cl.exe workers.
  Do not reuse a busy terminal, poll, sleep, or kill unknown processes.
- No subagents unless explicitly authorized. Manual edits via apply_patch.
- Do not request secrets via chat/tools; do not test on real profile data, capture
  microphone audio, or run destructive restore operations without authorization.

## Validation tasks

Use deferred run_task (load it once with tool_search), workspace C:/Dev/vulkanstorm:

- shell: Native Vulkan Window Validation (includes capture-disabled voice tests).
- shell: Native Vulkan GPU Validation (context tests and window library).
- shell: Native Vulkan Widget Validation (Preferences/widget tests).
- shell: Native Vulkan Core Build (llvulkan only; skips dependency traversal).
- shell: Native Viewer Link Validation (after dependencies are built).

Tasks are in .vscode/tasks.json, RelWithDebInfo, build-vc170-64. Keep LL_TESTS,
LL_VULKAN_GPU_TESTS and LL_VULKAN_BROWSER_TESTS enabled. Widget test uses /bigobj.
Do not substitute an all-target build; unrelated baseline tests remain outside scope.
The core task was added during this resume because context integration tests compile
their own context objects and do not refresh the viewer's llvulkan static library.

## Preserve exclusions

- indra/llui/tests/llmarkdown_test.cpp and llurlmatch_test.cpp
- indra/newview/tests/llworldmap_test.cpp and llworldmipmap_test.cpp
- old untracked indra/llvulkan/llvktextlayout.{h,cpp}
- untracked mcp-Vulkan/
- user-modified .vscode/settings.json (read before any edit; no edit needed here)

## Keep the next chat small

Use this handoff plus nearby source reads, not the full old transcript. Update this
file after meaningful validated increments. Keep build output summaries concise;
read only failure details when output is captured to a file. Old repository memory
contains chronological and superseded states: this disk-checked handoff takes
precedence where they conflict. Ask for clarification only for actual scope or
authority blockers, not routine continuation decisions.