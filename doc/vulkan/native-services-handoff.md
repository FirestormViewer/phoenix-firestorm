# Native services handoff

Updated 2026-09-13 after PR #43 merged and the detailed work plan was agreed.
This is a continuation checkpoint, not a completion or parity claim.

## Current checkpoint and evidence

### Existing-service reporting (2026-09-13)

The user accepted finishing native reporting for existing services, then requiring
reporting as each transport/authenticated service is integrated. See
[the accepted reporting scope](native_error_messaging.md) for NV-00 contracts and
the distinction from complete GL notification/crash-service parity.

- Independent OS fallback now uses retained selected-skin translations with strict
  Unicode handling; unavailable/invalid catalogs retain English fallback.
- Scoped native fatal logging and missing-file/OOM warning hooks produce safe
  records and a fatal-state signal. Normal window work stops after a fatal warning.
  Warning callbacks are synchronized with retirement; previous shared handler and
  OOM strings are restored. No GL visual implementation or callback is reused.
- Existing runtime audio/voice/translation/preview failures have distinct stable
  causes. Six new messages are present in all 13 catalogs. Local notices are bounded
  and queued responses run once. Existing ignore preferences remain persistent;
  the 29 admitted local alert templates do not request persistence or expiry.
- Configured Widget209/209, Window7/7 plus error/cold-cache checks, standalone
  OS-dialog checks, shared llerror18/18 and Native Viewer Link Validation passed.
  No full viewer, credentials,
  microphone or operator-profile tests were run.

Live transport/authentication reporting remains with Phase 2; authenticated
notifications remain with their Phase 3 services. Exact UI parity, crash submission
and exhaustive legacy producer-detail classification are not established here.

### Production lifecycle follow-up (2026-09-13)

Supersedes the cache-only and no-agreement-consumer limitations in the historical
integration checkpoint below. See [session integration](native_session_owner.md)
for source contracts, ownership order and the reproduced recovery stall.

- Existing window services are now inside the adopted native application owner:
  browser views, audio, voice, joystick, translation verification, pickers and preview
  producers. Their callbacks detach before retirement; preview producers retire
  before startup's cache. Failed cleanup retains its dependencies and requires the
  exact-tag Retry Cleanup action. The presentation host remains alive until then.
- Resolved the observed nonresponsive component test. Retired browser consumers
  were still visible without frames, preventing any recovery frame from publishing.
  Hiding them during detach fixes that dependency. Modal buttons now preserve their
  action names, allowing the real cleanup retry control to execute.
- Agreements now publish bounded exact text with ID/revision and request identity.
  A read-only scrolling native modal provides explicit Accept/Decline, with Decline
  as default. Superseded callbacks cannot accept or reject a later request. Both
  actions are present in all 13 shipped catalogs; their XML checks passed.
- Configured owner checks, Widget209/209, Window7/7 with error/cold-cache regression,
  and native viewer link passed. The live window fixture observes three recovery
  frames without automatic retry, activates Retry Cleanup and verifies zero retained
  services and destroyed HWND. No component test or debugger is left running.

Live transport/authentication, server agreement retrieval and measured exact UI
parity remain separate unverified gates. No full viewer, real profile, credentials
or microphone run, commit, push or merge was performed. The earlier 382-check
source-worktree owner result predates the agreement payload extension; current
claims use the configured main owner and UI regressions, not that old count.

### Main integration follow-up (2026-09-13)

The user authorized integration in main `native-error-messaging` without commits
or branch merging. This supersedes the integration-open statements in the earlier
parallel-track checkpoint below; its historical test evidence is retained.

- Imported the tested `LLVKSessionOwner` implementation unchanged from
  `worktrees/native-session-owner`. The main copy passed the source worktree's
  382 deterministic checks with MSVC C++20 `/W4 /WX`. A main-tree standalone
  `INTEGRATION_TEST_llvksessionowner` target also passes its adoption, cancellation,
  stale-action, reverse-retirement and explicit-cleanup tests.
- Startup now adopts its actual texture cache via `LLVKApplicationCache` before
  acquisition and lends that owner to the window/UI. Preview producers detach
  before owner shutdown. Pending cleanup polls only when the owner returns Wait;
  failed cleanup requires a tagged UI retry. The cache service retains the native
  shutdown-status presenter through retirement. Fatal teardown failure retains the
  cache owner until process exit; that containment is not successful cleanup.
- Native error notices use the existing modal/widgets and selected-skin catalogs,
  with close, safe diagnostic copy, bounded error admission and deduplication.
  English and German contain 27 native-error keys; other locales currently fall
  back to English for these new keys. Login observes the real owner and reports
  unavailable authentication, never a fabricated connection. Retry Login,
  Retry Cleanup and Cancel capture exact owner tags. Generic legacy errors remain
  close-only because their producers do not carry recovery identities.
- Final isolated MSVC compilation passed for the touched startup, window, dialog,
  UI-construction, error, owner and test sources using configured project include
  paths/definitions, `/O2 /WX`. Isolated widget and window fixture links passed.
  Widget tests **208/209** passed for localization, modal delay/focus, deduplication,
  safe copy/copy failure, unavailable/installed transport distinction and stale
  recovery callbacks. Window test **7** passed for real cache adoption, queued-write
  drain, failed partial acquisition and shared status-presenter retirement.
  The expanded standalone error/Win32 fallback suite also passed; its recursive
  presenter negative test deliberately prints the fixed unavailable-presentation
  diagnostic. No test above authenticates, starts the full viewer or captures audio.

Validation limitation: these are isolated compiles/links and exact-test runs, not
a refreshed configured viewer build. A full isolated widget run initially hit
`0xC00000FD` (stack overflow); after matching fixture optimization it passed 1-49
and hit the same status at existing test 50. The final focused runs used matched
optimization for all replacement objects. The full-suite failure was not diagnosed
or hidden by modifying that test. Main must run the configured Widget/Window and
viewer-link gates for the combined changes. Earlier 7/7 window and viewer-link
evidence below predates this integration. GPU context test8 remains separately
owned by main and was not run or edited here.

Still open: authentication/region transport, full service adoption beyond the
cache, real producer-specific failures, network retry/backoff policy, localized
OS fallback, remaining locales, full startup fault injection, runtime shutdown and
measured exact UI/effects parity. Existing native modal scrolling, shadows and
keyboard/nesting behavior are not closed by these component tests. See
[session integration](native_session_owner.md) and the dated integration section in
[error messaging](native_error_messaging.md) for contracts, exact files and limits.

No real profile/credentials/microphone, full viewer, GPU-owned source, commits,
pushes or merges were used. Source worktree files and existing dirty main changes
were preserved. Isolated helper scripts, runner and compiler outputs are only in
the ignored `worktrees/native-session-owner/build-vc-session-owner` directory.

### Parallel tracks started (2026-09-13)

Following the user's parallel-work directive, both tracks start from `485967401a`
(the subsequent upstream merge on master). Work is uncommitted and unpushed:

- Main checkout, `native-error-messaging`: structured errors, safe catalog/diagnostic
  formatting, bounded generation-scoped duplicate handling, independent Win32 fallback
  and production startup/window failure boundaries. See
  [native error messaging](native_error_messaging.md) for contracts and limitations.
  Standalone MSVC/real OS-dialog tests, configured error target, Window7/7 plus cold
  startup and viewer link passed. GPU context test8 was intermittent in a broader
  build; it remains unresolved and is not hidden by the passing window evidence.
- Isolated local worktree `worktrees/native-session-owner`, branch
  `native-session-owner`: independent LLVKSessionOwner, not legacy LLVKSession.
  Seven states, request/generation/region tags, exact agreement identity, bounded
  thread-safe reply ingress, reverse resource retirement with retained failed cleanup,
  and tagged Cancel/RetryCleanup commands are implemented. Standalone MSVC C++20
  /W4 /WX validation passed 382 deterministic checks. Its local contract report is
  `doc/vulkan/native_session_owner.md` in that worktree, not yet in this branch.
  Production startup/window adoption, error adapter and session UI remain open.

The tracks deliberately do not share a partially defined header yet. The session
owner exposes typed status/action plus generation; a later integration adapter must
map those statuses to stable error codes and execute recovery only through tagged
owner commands. Error presentation currently acknowledges only; it does not invent
Retry/Cancel or authentication. Services-ready, region-connected and STATE_STARTED
remain different conditions. Neither complete error-roadmap acceptance nor the
Phase 1 production integration gate is claimed by this checkpoint.

No full viewer/profile launch, authentication, microphone capture, branch merge,
commit or push was performed for these tracks. Keep builds/worktree paths explicit;
parallel agents can change the persistent terminal cwd even with isolated source trees.

### Merged foundation

Merged baseline: `master` at `1b68d3afa1`, containing implementation checkpoint
`a9e9ead2bd` and documentation checkpoint `7e2aac6c6c` from
[PR #43](https://github.com/anne-skydancer/vulkanstorm/pull/43). The local
`native-vulkan-ui` branch was removed after merge; `native-error-messaging` was
created from this baseline for the error-reporting prerequisite. Creating that
branch did not implement error messaging. The detailed plan is documented on master;
subsequent implementation branches must incorporate it before continuing.

The production native path is a pre-login application with real local services and
Vulkan UI presentation, not the complete OpenGL viewer with a replacement renderer.
[Windows entry](../../indra/newview/llappviewerwin32.cpp) calls
[llvkStartup](../../indra/llvulkan/llvkstartup.cpp) and returns before constructing
LLAppViewerWin32. [LLVKWindowMgr](../../indra/llvulkan/llvkwindowmgr.cpp) runs its own
service/input/browser/2D loop. The native login button is constructed but has no
production authentication binding; no native authenticated STATE_STARTED or region
loop is established. Legacy LLVKSession paths and standalone components do not
count as production integrations merely because they compile or exist in this tree.

Last passed evidence, collected 2026-09-12 against the implementation now committed:

- Native Vulkan Widget Validation: 207/207, including original Widgets construction,
  cell aliases, checkbox colors, PNG roundtrip, XUI Preview and divider behavior.
- Native Vulkan Window Validation: 7/7 plus standalone cold-cache startup regression;
  temporary-profile and loopback fixtures exercise Win32 input, browser isolation,
  resize, Widgets menus, atlas PNG output and overlap presentation.
- Native Vulkan Browser Validation: earlier passed 1/1 lifecycle/navigation evidence,
  retained rather than rerun without a change invalidating it.
- RelWithDebInfo viewer link, source diagnostics and diff whitespace checks passed.
  Windows/MSVC, AVX2, non-OpenSim, no BugSplat configuration; this is not a portability
  or release-packaging qualification.
- No new full viewer/profile launch, authenticated session acceptance or measured
  exact GL visual/effects parity was performed. GitHub CI success is not asserted.
  Chromium GCM/WidgetHost messages appeared in passing fixtures; their wider
  significance is unqualified, not declared harmless.

The earlier inline-column failure and recursive factory stack overflow are fixed;
the retained historical failure records below are not current failures. Prior
uncommitted/no-push statements below describe their dated checkpoints, not HEAD.

## Production service inventory

Wired means reachable from production startup; it does not mean parity-qualified.
The source-backed survey is bounded to startup, service bindings, consumers and
shutdown, not an exhaustive transitive audit of the whole repository.

| Service | Live native integration | Remaining gap |
|---|---|---|
| Settings/profile | Defaults, user overrides, warning/crash preferences, reset, keybindings, backup/restore and exit writes | No authenticated account directory/loading/save callback; broader process policy remains incomplete |
| Window/input | Win32 window, keyboard/mouse, clipboard, asynchronous file/directory pickers, placement/resize and orderly close | Full DPI, multi-monitor, IME/input and window behavior qualification |
| Native visual UI | Independent font/text/image/skin pipeline, widgets, menus, Preferences and auxiliary floaters | Incomplete XUI coverage and exact rendering/interaction parity |
| Notifications | Selected templates, ignore preferences and queued modal alerts/responses | Full channel graph, persistence, tips/toasts and console |
| Texture cache | Shared audited storage with native runtime ownership, startup policy, worker IO and shutdown drain | Cache coordination/relocation/purge gaps; storage is not fetching or world residency |
| Texture previews | Bounded cache/local encoded reads, native decode, generation-checked publication | Network fetch, progressive textures, bake/material/world pipeline |
| Browsers | Login page, Guidebook, Media Browser, shared CEF lifetime, navigation and named targets | General media policy, authenticated URLs, parcel/object media, feedback and lifecycle parity |
| Audio | OpenAL device, master/UI gain/mute and already-decoded cached UI sounds | Sound fetching, spatial world sources, parcel audio and full streaming |
| Voice | WebRTC device enumeration/selection, processing configuration and tuning path | Authentication/provisioning, region/channels and conversational sessions; microphone capture not acceptance-tested |
| Joystick | Enumeration, selection, polling and configuration/preview | Agent/camera/world consumers |
| Text assistance | Native spelling/dictionaries and AutoReplace editing/persistence | End-to-end native chat/IM consumers |
| Translation/proxy | Real translation-key verification transport, proxy settings and protected SOCKS credentials | General viewer networking/translated chat; browser proxy capabilities differ |
| Diagnostics | About/device facts, settings/color editors, font registry/atlas dumps, partial XUI Preview | Complete diagnostic tools and live session-derived facts |
| Shutdown | Owned pre-login modal resolution, persistence, browser/audio/voice/GPU/cache retirement | Logout/reply handling, pending asset/metrics uploads, history drain, world snapshot and world editor closure |

Important interfaces that exist but are not production integrations:

- setGraphicsPreferenceHandler, setViewerPreferenceHandler and setPrivacyActionHandler
  are not bound by native startup. Editors/policy calculations may work while their
  downstream effects report an unavailable service. VSync is separately consumed by
  the live swapchain; that does not establish world graphics-setting integration.
- Production does not supply media-filter load/save data/callbacks or authenticated
  account persistence. Constructed Media Lists/Block List UI is not server integration.
- LLVKSceneSelection has a native scene/picking component but no production world
  producer publishing region geometry. Compiled GPU foundations are not a live world.
- Conditional Velopack entry handling precedes backend selection, but native bypasses
  much of normal LLAppViewer bootstrap. Crash preference persistence does not establish
  crash-handler, marker, logging, updater or multiple-instance lifecycle parity.
- The anti-spam purge callback is shared after audit; it is not an integrated incoming
  chat/message/anti-spam pipeline.

Reference comparison roots are [LLAppViewer](../../indra/newview/llappviewer.cpp)
(initialization, workers, network idle and shutdown) and
[LLStartUp](../../indra/newview/llstartup.cpp) (authentication, world/agent setup and
inventory stages). Native does not invoke those GL-coupled visual owners to obtain
their services. Reuse is permitted only for independently audited nonvisual code.

## UI completion inventory

This inventory describes the current login menu and associated UI. A hidden or
unbound entry is not counted as implemented, nor automatically a parity failure if
the same build policy intentionally hides it in the reference.

| UI family | Implemented checkpoint | Missing or unqualified |
|---|---|---|
| Login | Native original control hierarchy, browser, password visibility and settings-mode display | Authentication dispatch, credentials/account workflow, grid/session lifecycle and agreement handling |
| Viewer/Help actions | Preferences, quit/close handler, Window Size, wiki/troubleshooting URL route, About, Whitelist, Report Problem | External-site acceptance; complete current report facts/location and field parity |
| Guidebook | Independent embedded view, toggle/bring forward, F1 close, reopen, normalized placement/visibility persistence | URL substitutions, authenticated/custom-scheme actions, general media/failure/feedback policy, full-process restore and exact visual parity |
| Media Browser | Original chrome, address/history controls, Back/Forward/Reload/Stop, load state and named-target reuse | Full external/internal routing policy, shared persistent URL history, context menus, cursor/tooltips, script-window and error behavior qualification |
| Debug tools | Debug/Color Settings, logging actions, font test and registry/atlas dumps | Complete downstream settings effects; native atlas output intentionally differs in packing/filenames from GL |
| UI Tests | Textbox, Text Editor, Font Test and Widgets original XML, native controls and embedded menus | Exact skin/text/interaction parity; recognized live LLTrace stat bindings remain unsupported |
| XUI Preview | Catalog/languages, two preview owners, panel wrappers, Show/Hide/Refresh/default/double-click, overlap selection/copies | Editor/browse/diff handlers, live reload/fade, rectangle/diff/overlap highlighting, full bounds/tooltips and arbitrary-XUI support; language fallback/callback effects need qualification |
| Inspectors | No functional native authenticated inspector workflow | Object/agent selection, profile/name/region/media data and action services |
| TOS/Critical | No functional native login-linked dialogs | Login reply owner, page/probe readiness, agreement enablement, Continue/Cancel and retry behavior |
| Notifications Console | No native channel console; its panel can be previewed | Channel events/history, template catalog, add, inspect/respond and snapshot lifetime |
| Preferences | Broad native construction, editing, local persistence and selected real services | Unbound graphics/viewer/privacy actions; account-dependent actions and world effects; complete nested-dialog and visual acceptance |
| Menu/window mechanics | Predicates, nested keys/jump keys, generic floater dock state, embedded bars and divider dragging | Tear-off menus, Alt activation timing, full locale/accelerator semantics, generic saved floater state, mixed/animated/vertical divider and cursor parity |
| In-world UI/snapshots | Not integrated into the native production loop | HUD/toolbars/chat/inventory/world-dependent UI, snapshot floater/overlays, capture/preview/output workflow |

Reference policy distinctions:

- Close Window, Show Grid Picker and Show Debug Menu controls are hidden in original
  login XML; their handlers/tests do not imply changing that visibility policy.
- Notifications Console is also hidden in original login XML. Its missing service
  remains recorded without enabling a fake console.
- Non-OpenSim builds hide grid Help/About/separator; OpenSim dynamic-grid services
  have not been implemented or qualified by these tests.
- RegInClient is commented out in the menu, not an active requirement.
- XUI Preview Save/Save All do not serialize in the reference: they display a removal
  warning after preview operations. The matching native warning handlers remain open.
  Reference schema export is commented out and menu-file preview has an empty branch;
  do not invent exporters or count dormant code as a required feature.
- Explicit clip_partial is supported, but default text clipping/metrics parity remains
  open after the reference default hid a native login link. Do not relax acceptance.

## Agreed next sequence

The service-first sequence supersedes menu-by-menu work as the immediate priority;
the full menu remains a downstream objective. See the dated sequencing section in
the [native viewer roadmap](native_viewer_roadmap.md).

The roadmap's detailed execution plan adds native error messaging as the first
prerequisite, work packages and exit gates for all four phases, scope decisions and
validation/branch discipline. UI and menu integration is cross-cutting: every
service increment must include its relevant production UI bindings, state feedback,
input, cancellation/reconnect and teardown tests. Component completion is not
workflow completion. This requirement does not turn unrelated unfinished menu
actions into prerequisites for each individual service slice.

1. Native session owner: explicit state, lifetime, cancellation, stale-response guards,
   failure/retry and orderly disconnection. Structural tests are not working login.
2. Audited nonvisual network/data services: supply transport and login responses,
   capabilities/messages, account and asset foundations. End-to-end authentication
   and region connection are an acceptance gate across phases 1 and 2.
3. Authentication-dependent services: prioritize account state, agent/region data,
   inventory/assets, profiles/names, chat/IM, notifications and voice in testable
   workflows. Agree the required subset; this phase is not automatically the whole viewer.
4. Minimal connected environment: real region ground/terrain, sky, water, applicable
   postprocessing, native camera/view ownership, in-world UI and snapshot floater,
   overlays, capture/preview and an agreed output path. General objects/avatars,
   vegetation/particles and editing tools are not implied by this initial scene scope.

The gap is large: pre-login UI and local services are substantial, while authenticated
viewer behavior and production world rendering are largely unwired. No defensible
percentage or schedule follows from widget counts or lines of code. Minimal limits
scope, never correctness or exact visual/effects parity within the accepted scope.

## Preservation and resumption

- Keep `mcp-Vulkan/` untracked and untouched; retain ignored local VS Code changes.
- The four explicitly untracked legacy tests retain local copies and exact ignore
  entries: llui llmarkdown/llurlmatch and newview llworldmap/llworldmipmap tests.
- Do not restore unused llvktextlayout files or conflate native text with legacy
  LLFontGL-based llvklegacytext. No shared GL visual wrappers/extractions are allowed.
- Reuse passed evidence unless a relevant change invalidates it. Notify the operator
  before any full viewer launch and allow manual login time. Full-viewer acceptance
  requires STATE_STARTED, 75 seconds uninterrupted settling, graceful WM_CLOSE,
  exit zero and Goodbye!; never force-stop a viewer. Native STATE_STARTED is currently
  missing, so component/window fixtures do not satisfy that acceptance procedure.
- Keep implementation, live wiring, runtime verification and measured parity separate.
  Follow NV-00 source-contract/design/check records before further implementation.

## Historical implementation journal

The sections below retain the progression and debugging evidence from 2026-09-12.
Statements about uncommitted work, failing gates, missing progress controls or next
menu anchors are historical and superseded by the current inventory above.

### Widgets repair and initial XUI Preview

Latest resume checkpoint, 2026-09-12: the user authorized continued investigation.
Inline cells now use the canonical `column` parameter with `name` as its alias,
matching LLScrollListCell::Params. The Checkbox tab's dotted enabled/disabled label
colors are also resolved. The original Widgets sample now constructs and paints;
tests verify exact cell text, colors, flyout/menu children and dock-state behavior.
The window fixture opens Widgets and dispatches embedded-menu input through Win32.

Current gates: Widget207/207; Window7/7 plus standalone cold-cache regression;
RelWithDebInfo viewer link; source diagnostics passed. The window sequence includes
font PNG output and a presented XUI Preview overlap stage. No full viewer/profile
launch or measured GL parity. No commit or push; all exclusions remain intact.

Fonts menu: native registry diagnostic snapshot logs size/family/file declarations
and resolved instances without populating glyphs. Dump Font Textures writes actual
retained CPU atlas pages from LLVKWidgetGpu into a unique directory under logs.
The PNG encoder has an asymmetric byte-exact RGBA/orientation roundtrip test; the
window fixture decodes nonempty 256x256 dumped atlas pages. Native packing and names
differ from legacy font atlases by design; no GPU readback or GL font calls occur.

XUI Preview is partial, not a completed menu entry. Implemented: original tool XML,
language/file catalog, independent primary/secondary floater previews, panel wrappers,
Show/Hide/Refresh/default/double-click routes, language-scoped skin loading, dependent
close, overlap panel selection and native diagnostic copies. The reference menu-file
preview branch is empty; Save only warns that saving was removed, and schema export
is commented out. Native handlers for Save warning, editor/diff file operations,
live reload/fade, rectangle/diff/overlap highlighting, complete inspector geometry
and tooltip behavior still need implementation/qualification. Arbitrary preview
files remain constrained by native factory support; no exhaustive XUI coverage claim.

User-resizable layout panel dividers now capture drags, clamp adjacent visible pairs
and update resize fractions; the old reject-only test was replaced with drag/minimum/
release/hidden-panel checks. Complete vertical/animated/mixed-panel and resize-cursor
parity are still unqualified. The notification-channel panel can now be previewed;
this does not implement notification channels themselves.

Diagnostic correction: adding a stack-local LLVKControl::Params in recursive factory
build caused test50 to overflow at depth63 (0xc00000fd, LLDB __chkstk/Parser::attribute).
Moving that temporary to heap storage restored the existing recursion-limit test.
Only the crashed widget test was stopped in LLDB; no viewer process was killed.

Service blockers to full menu functionality: the notification console requires the
ten-channel notification graph and response/snapshot lifecycle. LLNotifications is
not reusable unchanged: constructor/ignore/template paths access LLUI setting groups,
register UI callbacks and use LLTrans. Native alerts currently provide only a bounded
template subset and queued modal path. Inspectors and TOS/Critical additionally require
native region/object/profile and login-reply services. Implementing those owners is
service work beyond merely wiring menus. Tear-off, Alt-trigger timing and exact UI
parity also remain open. Do not mark the overall menu task complete.

Earlier in this resume: the pending Media Browser history test passed Window7/7
plus cold-cache regression. Named popup targets now reuse visible native dialogs;
blank and closed targets allocate fresh views (Widget203). Native menu jump keys
use sibling words and explicit keys; test204 covers disabled/hidden states and
keyboard underlines. WM_CHAR/WM_SYSCHAR route to the open menu, and the window
fixture passed opening original Help/Guidebook via its jump key.

Widgets dependencies added: native flyout split action/list behavior and shipped
skin defaults; embedded menu subtree parsing, widget-owned menu state/draw identity,
bar painting in widget order and dropdown popup overlays, focused-menu keyboard
routing. Component test205 passed before full sample activation. LLFlyoutButton
registration is in llui.cpp. Generic dock behavior updates state and button
visibility without specialized positioning. The sample's missing stat="stat"
retains zero/no-graph behavior; recognized named LLTrace counters are explicitly
unsupported pending native recording semantics. Dynamic columns use the existing
equal remaining-width distribution. The former inline-cell blocker is resolved by
the latest checkpoint above. Existing uncommitted work, exclusions and local tasks preserved.

User directive: proceed incrementally in logical groups without stopping at partial
menu milestones. Full menu functionality is still the objective; do not call this
task complete. Current uncommitted work now also includes:

- Window Size: original XML, editable resolution parsing, overflow/capability bounds,
  actual native Win32 client resize, saved width/height, Cancel/default action.
  Window fixture verifies 1100x800 client dimensions after key-capture modal closes.
- Close Window: frontmost closable native floater, with notice/key-capture guards.
- ToggleControl/CheckControl and live UseDebugMenus visibility. RegInClient is inside
  an XML comment and is NOT an active menu requirement. LLVKMenu now evaluates
  visibility predicates in paint/keyboard/accelerators and rejects stale hidden hits.
- Debug Settings: original floater over retained shared control metadata; name/comment
  search, changed-only policy, scalar/bool/string/vector/rect/quaternion/color controls,
  reset/copy/native sanity notices, live selected-value updates, hidden-write guard,
  and current-value exit persistence with account guards. Scoped factory callbacks
  avoid UpdateFilter/CommitSettings collisions with Preferences or Color Settings.
- Color Settings: native palette catalog, original search/list/swatch/alpha/reset
  controls, live palette mutation and existing user-color persistence. Selected rows
  survive unfiltered edits. Native color catalog excludes runtime-only overrides.
- Textbox, Text Editor and Font Test original generic XML floaters. Text Editor
  prevalidators use native validators before text/history mutation. Explicit
  clip_partial is implemented in native painting and tested; native default clipping
  remains unchanged because applying reference default=true hid the existing login
  password link. Default text metric/clipping parity is therefore still open.
  Font Test's font.style. spelling follows reference dot tokenization; missing Script
  font falls back at the UI boundary to SansSerif Medium, as LLUI font parameters do.

Latest gates: Widget201/201 passed after the unfiltered Color Settings selection fix;
Window7/7 and cold-cache regression passed with resize, Debug Settings and Color
Settings integrated (before that last selection-only fix). Viewer has NOT yet been
relinked for these groups. No full viewer/profile launch and no parity claim.
No commit/push; ignored test sources, mcp-Vulkan and edited local tasks preserved.

Next local anchor: Media Browser / Widgets need native progress_bar. Source
LLProgressBar::draw uses optional ProgressTrack/ProgressBar images, clamps percentage
0..100, rounds fill width, sets track alpha to draw alpha, and modulates fill alpha
by 0.75+0.25*sin(3*time). Original defaults are widgets/progress_bar.xml. Native
factory does not support this type yet. Build the actual control, not a dummy panel.
Then complete browser chrome/navigation/popup policies, UI Preview, remaining Widgets
types, font dumps, notifications console, menu jump keys/tear-off as applicable.

Service dependencies remain explicit: LLInspectObject::onOpen promotes gObjectList
objects into LLSelectMgr network-backed family selection and resolves media; avatar
inspectors query region/profile services. LLFloaterTOS Continue/Cancel posts to a
login reply pump; Cancel also calls login_alert_done through a notification. TOS
page readiness uses loading-page completion plus asynchronous HTTP probe before
enabling agreement. Full native authenticated login/region/world services remain
absent. Those actions cannot be declared functional by merely showing a dialog.
Critical XML is floater_critical.xml, not floater_message_critical.xml. Textbox and
Text Editor have no dedicated source classes; registration uses LLFloater directly.

## Latest increment: embedded Guidebook

Guidebook now opens from the original Help menu in an independently owned native
browser floater. It no longer depends on a second process-wide CEF initialization
or replacement of the login page. Native-only generated Dullahan sources retain the
initial CEF application owner until the last browser closes; view starts reject
conflicting process settings, cross-thread use and restart after final shutdown.
The legacy media plugin and GL implementation are unchanged.

The native dialog composes the original web-content and Guidebook XML, explicitly
applies the differently named Guidebook root attributes (310x525), and prepares its
300x505 stack before constructing children. Navigation/debug/status panels are
hidden and their inactive children are not constructed. Native per-widget browser
input, immutable frame publication, asynchronous close and immediate reopen are
wired into the window owner. F1 closes the foreground Guidebook before login Help
shortcuts. Normal close records hidden state; application quit preserves open state.
Normalized position and visibility use the reference guidebook setting names and
the shared settings writer; startup restores saved visibility.

Evidence: Browser1/1 tests real independent pages, initial-owner close, surviving
input, another view joining the runtime, conflicting settings and final retirement.
Widget195/195 tests original dialog dimensions, chrome policy, toggle/reopen,
position retention and quit-save semantics. Window7/7 plus the cold-cache regression
passed. The window test uses a loopback HTTP fixture and actual Win32 input to check
Guidebook page pixels after click, key and wheel, F1 close, immediate reopen, unchanged
login pixels and saved-state disk reload. RelWithDebInfo viewer link and source
diagnostics passed. No full viewer/profile launch was performed.

This is an implemented embedded-page increment, not full Guidebook or menu parity.
Public-site behavior, URL-template substitutions from LLWeb, popup/custom-scheme
dispatch, authenticated world actions, full media policy/failure UI, browser cursor
and tooltip feedback, restored visibility in a fresh full viewer process, and exact
GL visual/effects parity remain unverified or unimplemented. Chromium emitted GCM
deprecated-endpoint and WidgetHost messages in the passing integration run; they
were not classified as Vulkan validation failures or proven harmless globally.
Remaining menu work should proceed by shared capability families (browser dialogs,
settings/debug tools, session-dependent actions), then per-action acceptance checks.
Existing uncommitted menu work and exclusions remain intact; no commit or push.

## Latest checkpoint: top-menu actions

HEAD is a1628b9922. Earlier lifecycle/cache work was committed in c7523c3efc,
text consolidation in 88baf039b5, and cold startup/status presenters in a1628b9922.
The uncommitted descriptions below are historical checkpoints, superseded by this
commit status. Current menu changes remain uncommitted; no push was performed.

The current user objective is full top-menu functionality. This increment implements
Whitelist adviser from the unchanged floater_whitelist.xml with explicit runtime
folder/executable paths; Report Problem with escaped native diagnostic facts and the
configured report URL through the existing external-browser confirmation callback;
and logging-level actions/checkmarks through audited shared LLError services.
Native menu predicates now support live check/enable state, and nested keyboard
Right/Left navigation enters/exits submenus. Existing non-OpenSim grid Help/About
visibility policy is retained and tested, not newly introduced.

Verification: Widget194/194, Window7/7, standalone cold-cache startup regression,
and RelWithDebInfo viewer link passed. Source diagnostics and git diff --check passed.
Tests exercise original menu dispatch, Whitelist values/reopen/paint, report URL
escaping/rejection, real logging changes/checkmarks with restored global state, and
synthetic nested navigation/live enable predicates. No full viewer launch, external
report submission, microphone capture or measured visual parity was performed.

Next: implement Guidebook as its original embedded browser floater. The current
Dullahan adapter owns process CEF initialization/shutdown per browser and permits
only one initialization; it needs a correctly owned shared runtime and independent
views first. Do not initialize CEF twice, navigate the login browser away, or replace
Guidebook with a generic external URL. Remaining Debug dialogs/tools, visibility
predicates, tear-off/jump-key behavior and dynamic grid services remain open. Report
facts currently follow the About snapshot, with unavailable native session/location
fields explicit; legacy report field parity and live refresh are not established.
The source/native contract is recorded in
[native UI construction dependencies](reverse-engineering/native-ui-construction-dependencies.md).

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

Startup/shutdown presenter added after the operator's visual check (uncommitted):
LLVKStartupStatus uses the existing SPLASHSCREEN resource and native localized string
loading without invoking LLSplashScreen/LLWindow visual wrappers. Cache initialization
now has a visible status (clearing for explicit purge); final shutdown reopens it with
ShuttingDown after LLVKWindowMgr returns and before persistent-cache drain/stop. It
hides before Goodbye! and on scope exit. No artificial delay; fast cleanup may show
only briefly. In-cache purge-to-initialize subphase reporting remains open.
Window7/7, including real resource visibility/text/hide/reopen and pending cache write
retirement under the shutdown dialog, plus the cold-start regression and viewer link
passed. No full viewer/profile relaunch or screenshot parity measurement was performed.
The top-menu observation remains open: Guidebook, Whitelist adviser, Report Problem
and grid Help/About need native handlers and underlying services. Do not substitute
generic URLs for original dialog/system-info/location behavior or mark these complete.

Cold-start crash correction after 88baf039b5 (2026-09-12, uncommitted): the operator's
13:24:33 launch exited before creating a window. Windows Event 1000 reported
0xc0000005 at executable RVA 0x404e15e, resolved with the matching PDB to
LLMutex::isSelfLocked. A separate cold-process cache regression reproduced the crash;
LLDB showed LLThread::threadRun constructing a child ThreadRecorder with a null master
and crashing in addChildRecorder's mutex. Prior window tests initialized LLCommon
through their harness and therefore did not cover the missing startup prerequisite.

The native cache's reference-counted runtime now creates a shared LLTrace master
recorder when absent, borrows an existing recorder otherwise, and retires an owned
recorder only after cache workers stop. Timeout retention keeps this dependency alive
with retained workers. No GL visual initialization was added. The new standalone
INTEGRATION_TEST_llvktexturecache_startup executable is built/run as a dependency of
Window Validation and intentionally has no LLCommon-initializing test harness.
It failed with the original access violation before the fix, then passed cold startup,
pending write drain and owned-recorder teardown. Window6/6 also passed afterward.

Deployment correction: the actual viewer directory contained a September 9 WebRTC
DLL, unlike the tested sharedlibs/build DLL. The viewer POST_BUILD step now copies
the DLL from the llwebrtc target. Viewer relink passed, deployed/built SHA256 matched,
and vulkanstorm-bin.exe was last written 2026-09-12 13:34:53 local time. The full
viewer/profile has NOT been relaunched by the agent; visible-window confirmation is
still pending. No commit or push was made. Source diagnostics and whitespace passed.

Native text module consolidation (uncommitted after c7523c3efc): llvktext.h now owns
the current native plain layout, web-text, styled-segment and styled-document
declarations. The old llvkplaintextlayout.h/llvkstyledtext.h forward to it; algorithms
and public type names are unchanged. GL-font atlas declarations moved to
llvklegacytext.h, with all existing legacy consumers retargeted. llvktext.cpp now
combines the native plain and styled implementation bodies, compiled by llvkwidgets.
The superseded llvkplaintextlayout.cpp and llvkstyledtext.cpp were removed; their
compatibility headers remain. llvklegacytext.cpp preserves the former LLVKText atlas
implementation unchanged and is compiled separately by llvulkan. Exact native body
comparisons and the renamed legacy source SHA256 check passed. No legacy text
renderer is newly invoked by native startup. Widget192/192 with a header dependency
guard, Window6/6, native core build and viewer link passed. New shaping/bidi/grapheme
support and algorithm unification are not implemented or claimed. No full viewer run.

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