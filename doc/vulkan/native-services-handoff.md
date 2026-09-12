# Native services handoff

Updated 2026-09-12 after resumed WebRTC lifetime and voice configuration work.
This is a continuation checkpoint, not a completion or parity claim.

## Current objective

Engineer viewer-wide native equivalents for graphics-API-exposed portions of:
assets/texture residency, scene selection/picking, render configuration, and
audio/media/voice. Share audited API-independent code. Preferences is a client
of these services, not their lifetime owner. Full Preferences integration remains
the product goal; do not manufacture settings-only substitutes for missing services.

Latest user request resumes service integration, starting with shared WebRTC
shutdown and capture-disabled Window Validation. No commit or push was requested.

## Immediate next step

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
3. Continue viewer-wide services from the source contracts, not Preferences-only
  substitutes. Voice session transport remains open; a useful next bounded audit
  is session-independent enabled/muted/gain ownership versus tuning transitions,
  before connecting it to authenticated session transport. Asset delivery/residency,
  live scene producers/picking and renderer consumers also remain open.
4. Shutdown limits: active peer observers, driver hangs, partial-init failure
  injection and non-Windows hotplug are not runtime-verified. The synchronous
  observer-list lock and observer/log callback lifetime on the timeout path are
  not proven bounded by the worker timeout. Do not claim complete shutdown parity.

## Saved work and evidence

- Base/checkpoint: 9089558822, branch native-vulkan-ui (recheck HEAD on resume).
- Extensive subsequent work is uncommitted, including new untracked source files.
  Preserve it; do not reset, clean, checkout, or discard files.
- Preferences opens the original hierarchy through the real native menu. Earlier
  notes describing a provisional window are historical, not current status.
- Preferences/widget gate: 191/191 passed again after the resumed voice changes.
- Latest reported context gate: 10/10 passed after scene-selection component work.
- Current window gate: 4/4 passed with the live settings loop and real device engine;
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

## Other uncommitted Preferences work

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