# Native Vulkan viewer roadmap

Established 2026-09-10; work plan updated 2026-09-13 after PR #43 merged.
Status: replacement development roadmap following the requested rollback.
No implementation milestone below is marked complete by this document.

**Implementation checkpoint:** PR #41,
`90af5a7220f1fec28e3c909c051b6ee7e062f10b`.
**Historical GL oracle:** `59108e15a1f8f94d2da7c674d937d19f5cf9450d`, as recorded in
the [approved reverse engineering](reverse-engineering/README.md).
These are different roles; resetting the implementation does not silently change
the parity reference. The checkpoint contains earlier Vulkan work and its debt;
restoring it is not a claim that its native renderer works or is GL-free.

Authority: [native Vulkan invariants](native_vulkan_invariants.md), particularly
NV-00. This roadmap supersedes implementation sequencing in older native plans,
not the separate [OpenGL modernization strategy](opengl_modernization_strategy.md).

## Service-first sequencing (2026-09-13)

Following the native service survey and merged
[PR #43](https://github.com/anne-skydancer/vulkanstorm/pull/43), the user selected the
following execution order. This refines near-term sequencing; it does not mark the
R0-Rn milestones below complete, change the GL oracle or waive native invariants.
Current implementation/status evidence is in the
[native services handoff](native-services-handoff.md). The starting baseline is
merged `master` at `1b68d3afa1`, containing implementation checkpoint `a9e9ead2bd`
and documentation checkpoint `7e2aac6c6c`. The historical GL oracle is unchanged.

| Phase | Scope | Exit evidence |
|---|---|---|
| 1. Native session owner | Pre-login/authenticating/agreement/connecting/connected/disconnecting/stopped states; account/session identity, cancellation and owner teardown | Controlled state transitions, retry, stale-response rejection and partial-startup cleanup; not authentication merely because a state machine exists |
| 2. Nonvisual network and data services | Independently audit/reuse transport, protected credentials, login requests/replies, capabilities, event/message processing and asset/data foundations; wire native login/TOS UI | Authenticate, establish and maintain a region connection, then log out cleanly without entering GL lifecycle; this gate spans phases 1 and 2 |
| 3. Authentication-dependent services | Explicitly prioritize account persistence, agent/region state, inventory/assets, names/profiles, chat/IM, notifications and voice provisioning | Each selected live workflow and its disconnect/reconnect behavior; authentication alone does not qualify consumers |
| 4. Minimal in-world environment | Connected-region ground/terrain and materials, sky/environment, water, applicable postprocessing, camera/view ownership, in-world UI, snapshot floater and overlays | Region-derived scene, propagated environment changes, interactive native UI, correct snapshot view/resolution/overlay policy, preview and agreed output path |

Agree the required phase-3 service subset and phase-4 UI/snapshot outputs before
implementation: neither phrase silently authorizes the entire viewer. General scene
objects, avatars, vegetation, particles and world editing are outside the initial
minimal-environment milestone unless explicitly added. Real region terrain/water/sky
must not be replaced by placeholders and described as parity.

Proceed in vertical slices: source contract, owner, service, native consumer, focused
tests, runtime acceptance and measured parity. Selected scope remains subject to
exact UI/effects parity and the full material/color/alpha/depth/composition contracts.
CPU-only API-independent functionality may be shared after audit; GL-exclusive visual
functions, including coupled CPU preparation, remain forbidden. No calendar or
completion-percentage estimate is established by this sequencing decision.

## Detailed execution plan (2026-09-13)

This is planned work, not a record of implementation. Reuse the existing native UI,
browser, settings, cache and GPU foundations after checking their actual contracts
and consumers. Their presence and existing test counts do not establish readiness
for authentication, world services or world rendering.

### Preparation: native error messaging

Implement this on the dedicated `native-error-messaging` branch before adding
authentication failure paths. Keep session/network/world implementation on subsequent
focused branches; this planning document does not create those branches.

- Define a structured error contract: stable code, operation, severity, recoverability,
  localized user-message key, redacted technical details and request/session identity.
- Provide native presentation before UI startup, during normal UI operation and after
  renderer failure. Reporting must not depend on the subsystem that failed.
- Supply logging, duplicate suppression and explicit Retry/Cancel/Close behavior.
  Services report facts; the session owner decides whether and how to recover.
- Test missing resources, cache/permission errors, network/TLS failures, renderer
  failures and late errors after cancellation. Never put credentials, tokens or
  sensitive response bodies in diagnostics, copied reports or test artifacts.

Exit: actionable errors are delivered at each lifecycle stage, retries obey owner
policy, and reporting failure cannot recursively invoke the failed visual service.

### Phase 1: native session owner

1. Recover the applicable lifecycle from [LLStartUp](../../indra/newview/llstartup.cpp)
   and [LLAppViewer](../../indra/newview/llappviewer.cpp), including callbacks,
   agreements, deadlines, retry and teardown. Record the NV-00 three answers and
   unresolved dependencies before implementing each behavior family.
2. Own explicit pre-login, authenticating, awaiting-agreement, region-connecting,
   connected, disconnecting and stopped states. Track login-attempt generations and
   region epochs; publish account/session identity only at defined transitions.
3. Separate application-, account/session- and region-lifetime services. Serialize
   state transitions on the owner thread; workers return bounded, generation-tagged
   results rather than mutating UI or service lifetimes directly.
4. Wire the owner into native startup. UI submits commands and observes snapshots.
   Cancellation invalidates pending results. Shutdown rejects new work, retires
   callbacks and drains dependencies in reverse ownership order, including partial
   initialization and failure exits.

Exit: deterministic tests cover valid/invalid transitions, duplicate requests,
cancellation at every stage, reordered/stale responses, retry, partial cleanup and
idempotent shutdown. Structural state-machine tests are not proof of working login.

### Phase 2: nonvisual network and data services

1. Audit transport, protected credential storage, protocol/LLSD parsing, event queues
   and caches through constructors, globals, callbacks and teardown. Share independent
   code; do not wrap GL-coupled viewer owners to obtain nominally nonvisual functions.
2. Establish TLS verification, proxy policy, deadlines, bounded queues/payloads,
   cancellation, retry/backoff, error mapping and secret-safe logging.
3. Implement account/grid/start-location selection, login requests and responses,
   supported challenge/redirect flows and TOS/critical-message replies. Wire native
   login controls, progress, cancellation and actionable failure/retry UI.
4. Establish region session/circuit setup, seed capabilities, capability discovery,
   message dispatch, event queues, acknowledgements/keepalives and disconnection.
5. Provide common data-request infrastructure: asset identity and permission context,
   scheduling, cache lookup, cancellation and result publication. Feature-specific
   inventory, texture and audio consumers follow in phase 3.

Exit: controlled protocol tests and a live authenticate/connect/maintain/logout
workflow in the native process, without entering the GL lifecycle. Authentication
and region connection are a joint acceptance gate across phases 1 and 2; no world
image is required. Keep authenticated, region-connected, services-ready and
world-visible conditions distinct. Do not label a transport connection STATE_STARTED
merely to satisfy a test; define and verify the applicable viewer-readiness contract.

### Phase 3: authentication-dependent services

Each group requires a real session, a native consumer and a disconnect/reconnect
test. Complete the required subset explicitly rather than implying that a successful
login integrates all dependent services.

| Group | Work package | Workflow gate |
|---|---|---|
| Account and agent | Account directories/settings/persistence, identity, permissions, position, region state and crossings | Account isolation; region changes reject stale data and do not leak prior-session state |
| Notifications | Native template/channel processing, ignore rules, responses, expiry, persistence and presentation | Service events reach the correct native UI; response and disconnect cleanup execute once |
| Inventory and assets | Inventory/library acquisition and updates, permission checks, asset requests, cache/decode/publication | Browse and perform agreed mutations; fetch permitted content and handle misses, denial and cancellation |
| Names and profiles | Resolution, caches, profile requests and inspector data | Real identities populate native UI and refresh without stale-account publication |
| Chat and IM | Send/receive, session membership, unread state, history, mute/filter policy and persistence | Two-party workflows survive disconnect/reconnect without duplicate or misrouted messages |
| Voice | Authorization, provisioning, channel join/leave, device state and teardown | Actual selected voice-session modes work; microphone testing requires explicit operator consent |

Before implementation, agree inventory mutations, chat/IM modes, notification classes
and voice modes required for this milestone. Economy, purchases, uploads, scripting
and editing are not implicitly added by a button or incidental dependency. Follow
dependencies between these groups rather than treating table order as proof that
they can be implemented independently.

### Phase 4: minimal connected environment

1. Verify the selected-device/resource foundation for world use: capabilities,
   budgets, uploads, barriers/layouts, descriptors, all-use in-flight retirement,
   noncoherent memory, WSI recovery and reproducible shader/CPU ABI packaging.
2. Decode actual region terrain patches, ground-material inputs, water level and
   environment settings into versioned native CPU records. Handle region changes and
   origin shifts without reading GL visual owners.
3. Establish native camera and view policy: coordinate conventions, projection,
   movement/input, culling and explicit main/auxiliary views. Camera ownership must
   not require avatar rendering.
4. Implement terrain geometry/materials, sky/atmosphere and water, including lighting,
   above/below-water policy, transparency ordering and reflection/refraction passes
   required for the accepted result. Newly discovered passes are explicit dependencies,
   not permission to silently approximate or expand the accepted scene scope.
5. Implement the applicable postprocessing dependency graph, including exposure,
   tone mapping, glow, AA and any other agreed effects. Preserve history invalidation,
   material/color/alpha/depth contracts and keep UI outside world exposure.
6. Integrate the agreed in-world status/toolbars, camera/environment controls and
   phase-3 UI. Implement the snapshot floater: requested resolution/aspect/crop,
   selection overlays, inclusion/exclusion policy, frozen view/history, native
   capture, preview, encoding and output.

Exit: connected-region data renders correctly, environment and camera changes
propagate, native UI remains interactive, and snapshots preserve the requested view,
resolution, color/alpha and overlay policy. Compare stage outputs and temporal
sequences against the pinned reference, not just the final screenshot.

Agree the initial in-world UI set, postprocessing effects and snapshot destinations
before this phase. Local PNG is the recommended first output, not an approved limit
on the overall snapshot objective; other destinations require their own services.

### Cross-cutting objective: UI and menu integration

Integrate UI and menu functions as their services enter the viewer. Do not defer
service consumers to a separate final UI phase or call isolated component tests
workflow completion. Reuse and complete suitable existing native widgets/dialogs.

| Work phase | Accompanying UI/menu work |
|---|---|
| Error messaging | Startup errors, in-app failures, diagnostic details and Retry/Cancel/Close actions |
| Session/network | Login/account/grid/start-location controls, progress, agreements, cancellation and logout |
| Authenticated services | Inventory, chat/IM, notifications, profiles/inspectors, voice controls and applicable Preferences actions |
| Minimal world | Camera/environment controls, in-world menus/toolbars, snapshot floater and overlays |

Every increment's gate includes production bindings to the actual service; correct
visibility, enablement, checkmarks, loading and error states; mouse/keyboard input,
focus and dependent-dialog behavior; cancellation and disconnect/reconnect; and
teardown without stale callbacks. Require source-defined visual/effects/interaction
parity within the implemented scope. A component is not workflow-complete until its
relevant UI and menu actions work end to end.

### Cross-cutting requirement: service error reporting

Established 2026-09-13 following the existing-service reporting work. Every native
service increment MUST deliver its error reporting together with the service and
its UI/menu consumers. Reporting is part of the service's exit gate, not a later
cleanup phase. The existing reporting infrastructure is a foundation, not evidence
that a new producer's failure paths are covered.

- Recover the source-defined failure contract under NV-00, including asynchronous
  callbacks, partial initialization, cancellation, timeouts and teardown. Identify
  which failures are recoverable, fatal or expected user cancellation.
- Map actual producer failures to stable, cause-specific codes, operation and
  request/session identities. Supply actionable localized messages and safe
  diagnostic details; never expose credentials, tokens or sensitive response bodies.
- Wire reporting into the production service and native consumer. Preserve usable
  reporting before UI initialization, during operation and after renderer failure,
  using an independent fallback when the normal presentation path is unavailable.
- Let the operation/session owner decide Retry, Cancel and Close behavior. Implement
  applicable deadlines and retry/backoff policy in that owner, not in the dialog.
  Reject stale responses and actions; do not offer recovery that has no executable
  production binding or bypass security checks to make a retry succeed.
- Bound admission and duplicate handling, deliver responses once, and retain the
  resources needed to explain and recover from failed cleanup. Implement expiry,
  ignore and persistence rules when required by that service's source contract;
  never persist callbacks or replay recovery against an expired owner.
- Include discriminating failure-injection tests at the production boundary for
  relevant missing resources, permissions, network/TLS failures, cancellation,
  late results, retry and partial cleanup. Verify localized presentation and fallback
  behavior separately from successful-operation tests and measured UI parity.

Phase 2 transport must bring login/TLS/MFA, agreement and connection diagnostics
with its real protocol producers. Each selected Phase 3 service must bring its own
authenticated failures and notification policies. Phase 4 must cover its resource,
rendering and snapshot failures. A fixture-only error, generic placeholder message,
disabled route or successful build cannot close the corresponding reporting gate.
Record remaining failure paths and verification limits in each service's handoff.

### Delivery and validation

- Deliver focused branches/PRs per behavior package. The sequence is source contract,
  native design, discriminating test, implementation, production wiring, runtime
  acceptance and measured parity. Publish exact remaining gaps at each checkpoint.
- Reuse passed evidence unless relevant changes invalidate it. Use isolated profiles,
  fake clocks, controlled protocol responses and fault injection first; never test
  destructive operations against the operator's real profile.
- Use the smallest relevant existing widget/browser/window/GPU/link gates. Add
  session/protocol tests within repository conventions and GL/Zink regression checks
  where shared nonvisual code changes. Module imports alone do not prove GL execution
  or its absence; qualify selected-native runtime behavior separately.
- Notify the operator before full viewer launches and allow manual login time.
  Full-viewer acceptance requires actual STATE_STARTED readiness, a 75-second
  uninterrupted settle period, graceful WM_CLOSE, exit zero and Goodbye! Never
  force-stop the viewer. Pre-readiness failure/cancellation fixtures are separate
  tests, not a substitute for the completed full-viewer acceptance run.
- Keep exact UI acceptance and approved reference-derived numerical comparisons
  distinct. Do not change the oracle or tolerances to hide mismatches. A minimal
  milestone narrows features, not correctness within those features.

Critical path: error messaging, session lifecycle, authentication/region transport,
required account/data services, native environment scene, in-world UI/snapshots,
then workflow/parity qualification. Source audits, protocol fixtures and auxiliary
view design can begin earlier, but implementation must respect ownership dependencies.
Do not infer calendar estimates or completion percentages from lines of code or
test counts; estimate packages after the initial reuse/ownership audits.

## Foundation: three questions for every function and helper

1. **What is the OpenGL function doing?**
2. **How is this done in Vulkan?**
3. **What is the cleanest implementation of question 2 in terms of Vulkan?**

The first answer is a behavior and ownership contract, not a list of GL calls.
The second may be a CPU-only algorithm rather than a Vulkan command. The third
selects native representation and scheduling based on the contract, not a desire
to preserve GL class boundaries. Every responsibility must be accounted for;
there need not be one native symbol per reference symbol.

Follow direct helpers, constructors, destructors, virtual implementations,
registered callbacks, workers and failure paths. Configuration and platform
alternatives are separate coverage obligations. Describe event loops through
transitions and invariants; do not claim enumeration of every unbounded sequence.

### Required investigation record

Use one record per function or tightly coupled behavior family, with all constituent
functions individually identified. Keep it with the nearest domain report.

| Field | Required content |
|---|---|
| Identity | Record ID, source revision, configuration, owning functions and callers |
| Question 1 | Inputs/results, units, formats, state changes, branches, ordering, lifetime, failure and teardown |
| Outgoing obligations | Direct/virtual/helper/callback targets, invocation and thread conditions, closures or unresolved edges |
| Question 2 | Native result production, CPU-only responsibilities, native resources and publication dependencies |
| Question 3 | Alternatives considered, selected ownership/algorithm, capability constraints and rationale |
| Consumer checks | Exact observable assertions, negative cases, controlled reference inputs and approved numeric/image tolerances |
| Evidence/status | Local-inspected, edges-open, contract-closed, implemented, runtime-validated, parity-validated; each separately supported |

A source link plus a guessed native class name does not answer the three questions.
An AST/call index assists coverage accounting but does not resolve indirect calls,
feasible paths or callback lifetimes. Reconcile compiler configuration with the
actual build; earlier editor indexing omitted Windows-only paths and included a
nested source copy. Restrict investigation to the active source root.

## What is retained from the investigation

The five approved reports remain in the checkpoint and retain their historical
claims and references. Later experimental code, tests, launchers and completion
documents have been rolled back. Their source investigations inform the obligations
below, but do not carry implementation credit into this roadmap.

| Domain | Established evidence or later finding to recheck at the checkpoint | Consequence for the new work |
|---|---|---|
| Startup and provider | LLAppViewer startup constructs UI/services and installs callbacks around a late backend/window decision; GL and Zink share provider/lifecycle logic | Trace from OS entry, not merely display/initWindow. Design the selected lifecycle before changing dispatch |
| Fonts and parameters | UI Params can request default GL fonts during construction; nominal font measurement can allocate/rasterize glyph resources | Audit construction and measurement independently. CPU metrics and GPU glyph publication need explicit native owners |
| Settings | Commit callbacks can cause nested settings writes, font/UI construction and resource rebuilds; bool callback results need not veto writes | Recover validation, commit, saved/transient layers, ordering and shutdown subscriptions before sharing settings logic |
| Chat and input | Later tracing found history replay, unread/toast effects, clear/set draft mutation and draw-time scroll adjustment | Recheck exact checkpoint bodies; separate arrival/replay/acknowledgment and layout effects before native rendering |
| Image/asset service | Fetch/discard, raw/aux retention, creation and loaded callbacks have different readiness/lifetime meanings | A texture handle alone is not the asset service; preserve dependent geometry/UI invalidations and terminal callbacks |
| Callback details | Later tracing found aggregate aux demand, global versus per-entry pause differences and cancellation during retained-raw destruction | Treat as specific reinspection questions, not mandatory quirks or assumed intended behavior |
| GPU foundation | Approved backend report identifies shared mutable buffers, upload/publication, descriptor capacity and readback/WSI risks | Establish all-use completion and explicit dependencies before consumer migration |
| World rendering | Approved reports recover scene routing, material equations, water/transparency, post order and view history | Preserve these result contracts; do not begin with a convenient generic forward renderer |
| Auxiliary views | Ordinary probes publish after display; hero/mirror work, snapshots, impostors and picking have distinct policies | Model view purpose and history explicitly; a main-frame early return cannot own all rendering work |
| Teardown | Callbacks and destructors may release or reconstruct UI/image state; partial initialization differs from normal shutdown | Design cancellation and owner destruction together with startup, not as an afterthought |

Portable source anchors for reinspection:
[application](../../indra/newview/llappviewer.cpp),
[Windows entry](../../indra/newview/llappviewerwin32.cpp),
[window](../../indra/newview/llviewerwindow.cpp),
[control defaults](../../indra/llui/lluictrl.cpp),
[GL fonts](../../indra/llrender/llfontgl.cpp),
[settings](../../indra/llxml/llcontrol.cpp),
[settings subscribers](../../indra/newview/llviewercontrol.cpp),
[texture callbacks](../../indra/newview/llviewertexture.cpp),
[rich chat](../../indra/newview/fschathistory.cpp),
[pipeline](../../indra/newview/pipeline.cpp).
Later findings are investigation leads until matched to this revision and its
transitive paths. Do not restore reverted implementations merely because an
experimental test once passed.

## Architecture decisions and constraints

The user's confirmed runtime boundary is **one viewer executable with its 3D
renderer in the viewer process**, selecting an independent GL/Zink or native
Vulkan lifecycle before incompatible application initialization. As clarified
on 2026-09-11, Dullahan/CEF browser helpers and the slvoice helper are permitted; the one-process
rule is strictly for the viewer's 3D portion. Independent lifecycle does not mean a
separate 3D renderer executable/process, another viewer instance, or GL-owned constructors
followed by a late native display branch. Common process/static initialization
must be audited too; early branching alone does not prove absence of GL effects.

The following alternatives remain decision work, not silently adopted changes:

| Alternative | Feasibility question and required decision |
|---|---|
| Build-time GL/Zink versus Vulkan | Can CMake source/target closures and autobuild configurations select each lifecycle under the same executable target? Define settings/packaging behavior when a backend is compiled out. Do not just add a flag while linking GL owners |
| Dependency suitability | Existing GL-exclusive visual functions cannot serve the native path. Share audited nonvisual services; independently assess API-independent third-party functionality under clarified NV-01. Implement native visual equivalents without changing or extracting shared services from GL |
| Shared low-level RHI | Does it actually improve native ownership, or only preserve GL procedures? Present an explicit alternative design; current NV-01 prohibits GL-call translation/shared dispatch |
| GL/Vulkan interop | Requires shared external memory as well as semaphores, compatible devices/drivers, formats/layouts and both APIs' completion. A hybrid strategy would require explicit NV-01/NV-03 amendment; hardware feasibility is unverified |
| Compile GL out of native builds | Useful enforceable boundary after neutral dependencies are separated; it does not implement the missing viewer. Preserve the independently buildable GL reference, not delete it wholesale |

Keep native OpenGL and Mesa/Zink availability, provider selection and AMD
compatibility work intact. No new public renderer option, removal of a supported
backend, interop adoption or build-time-only product policy is approved by this
roadmap. Decide these explicitly in milestone R1, using the three questions.

## Milestones and exit gates

Each milestone includes investigation, design, implementation and verification.
Do not merge an implementation merely because its predecessor exists: its own
function/helper contracts must be closed. Independent domain analysis can proceed
in parallel; dependency gates below govern integration and completion claims.

### R0: Re-establish evidence and inventory

- Confirm the restored source, build environment and GL reference independently.
  Old binaries/captures under the build directory are not checkpoint evidence.
- Inventory application entry/init/loop/shutdown roots; settings/events; window
  input/DPI; UI factories/controls/fonts; assets/media/workers; scene/materials;
  shaders; auxiliary views/postprocessing; packaging and platform failures.
- Give every reachable function/helper an investigation record or explicit open
  obligation. Reconcile feature/compiler/platform conditions and indirect targets.
- Reproduce a controlled GL startup and capture at the reference without changing
  it. Establish CPU, GPU and temporal comparison methods before using tolerances.

Exit: a revision/configuration-bound coverage ledger and evidence plan. Global
coverage can remain open while a fully closed local slice advances; no exhaustive
completion claim until all inventory obligations are resolved.

### R1: Lifecycle, configuration and build decision

Dependencies: R0 startup/configuration/helper contracts.

Answer the three questions for entry, updater hooks, settings source precedence,
reset/first-run, renderer/provider policy, multiple-instance/URL handling, readiness,
partial failure and teardown. Determine which initialization is common and audited
neutral, and which belongs exclusively to the selected lifecycle. Evaluate the
architecture alternatives above before coding their selection mechanism.

Exit: reviewed lifecycle state diagram, owner graph and build decision; tests of
settings precedence and failure behavior against the reference; one viewer
executable with in-process 3D rendering (browser and slvoice helpers permitted), no GL
initialization on the native route, unchanged GL/Zink route.
A diagnostic rejecting the requested route is containment, not this exit gate.
Packaging/updater paths must not be left pointing to an incomplete scaffold.

### R2: Native resources and reproducible execution

Dependencies: R1 ownership boundaries; approved backend resource contracts.

Recover buffer/image allocation, updates, formats/alpha/origin, descriptors,
submission, mapped memory, capability selection and shader packaging responsibilities.
Choose native resource identities, immutable/versioned publication, completion
tracking and bounded allocation policy based on consumers. Reassess baseline Vulkan
code; fix or replace it only with a closed contract and discriminating check.

Exit: selected-device feature/format tests; upload/readback and in-flight lifetime
tests; noncoherent memory handling; descriptor reuse after completion; resize,
out-of-date, submit/acquire/present failure and device-loss behavior; reproducible
SPIR-V/CPU ABI; validation layers actually executed with recorded results.

### R3: Native construction, fonts and UI preparation

Dependencies: R1 CPU-service boundaries; R2 for GPU consumers.

Start at Params/factory/style/font resolution, not screenshots. Trace defaults,
provided/inherited values, image lookup, translation, font fallback/emoji, metrics,
layout, callbacks and constructor/destructor behavior. Separate CPU layout/input
from glyph/image upload and immutable UI recording. Preserve text-source indices,
clips, painter order, alpha and layout-time/draw-time effects explicitly.

Exit: native controls construct without GL owners; font/layout CPU fixtures;
glyph/image GPU lifetime checks; native action callbacks; focus/capture/clipboard/
IME/password/editing/undo cases; nested layouts and dynamic notification creation.
An offscreen login rendering is an intermediate output check, not a full widget
or interactive-login milestone. Use actual declaration contracts, not a hand-placed
visual approximation presented as migrated UI.

### R4: Interactive login and session startup

Dependencies: R1-R3 and closure of login/application-service callees.

Implement account selection and credential access through audited native/neutral
services, input validation, start location/grid/mode policy, login actions, progress,
errors, cancellation, notifications, network startup and service teardown. Classify
web/media login content separately and define explicit supported behavior.

Exit: the main executable enters a usable native login UI in its original process;
submit/cancel/failure/retry work; supported authentication/session transition is
verified; asynchronous callbacks during close cannot recreate destroyed owners.
No authentication secrets in traces or test artifacts. Offline tests alone do not
prove live login, and a clear world view after login does not prove scene support.

### R5: Streaming assets and scene production

Dependencies: R2/R4 service lifetimes and closed source families.

Migrate fetch/decode/cache/discard policy, raw/aux subscriptions, missing/error/
cancel/retry paths, media CPU frames, dynamic products and dependent rebuild events.
Then recover scene traversal, LOD/culling, geometry, skinning, material availability,
transforms/origin and per-face pass contributions into stable native records.

Exit: progressive availability and cancellation preserve logical identity;
subscribers see correct quality/final/error timing; old versions survive all users;
CPU scene routing/geometry contracts match discrete reference expectations.
Tests include avatars/attachments, terrain, volumes/sculpts, particles and HUD;
do not equate one mesh with one pass or resident fallback with asset success.

### R6: World materials, lighting and ordered composition

Dependencies: R2/R5; the scene/shader reports' function and shader-helper contracts.

Implement typed legacy/PBR material ABIs, opaque/masked G-buffer, sky/atmosphere,
lighting and shadows, followed by forward/water/transparency boundaries. Preserve
above/below-water rules, distinct light selection, glow versus coverage, sorted
avatar ensembles and eligible PPLL/peeling modes. Any representation change must
coordinate all producers and consumers with evidence.

Exit: controlled stage outputs and discrete ordering match the pinned contract;
approved numeric tolerances precede evaluation; missing capabilities and overflow/
fallback paths are explicit and tested. No altered GL oracle to hide differences.

### R7: Auxiliary views, history and final output

Dependencies: R6 for complete results; auxiliary contract/design work starts earlier
so R6 does not embed main-view-only assumptions.

Implement probes, mirrors, impostors, previews, HUD, dynamic textures and snapshots
as explicit view jobs. Preserve main versus auxiliary history, environment-time
freezing, pre/post-display publication and snapshot rerenders. Implement SSR history,
luminance/exposure, tone mapping, display-space glow, DoF, AA and final output order.
Preserve CPU picking and capture encoding rather than invent mandatory GPU picking.

Exit: temporal sequences, resize/teleport/origin changes, probe publication,
snapshot-without-normal-advance and per-view inclusion/encoding tests pass; UI
does not accidentally enter world exposure or postprocessing.

### R8: Product qualification and release

Dependencies: all promised feature contracts and prior exit gates.

Run normal and failure lifecycle tests through teardown, GL/Zink regression checks,
supported device/configuration matrices, installer/update/protocol launch and
asset/shader packaging. Audit GL API execution separately from static imports;
if a native-only build is approved, enforce its link/include exclusions. Measure
performance only with repeatable scenes, timing and driver provenance.

Exit: supported workflows are usable and documented; unsupported features are
explicit; required runtime/parity/product gates pass. A successful build, guard,
image capture or partially populated window cannot stand in for this gate.

## Working rules and immediate next task

Keep each change reviewable around one behavior contract. Implement and validate
the smallest native operation before expanding to adjacent consumers. Share audited
nonvisual functionality under clarified NV-01. Existing CPU visual helpers are not
neutral merely because their bodies contain no direct GL call. Independently audited
API-independent third-party functionality is not categorically forbidden. Leave the
OpenGL implementation untouched; independently implement native visual equivalents.
Check initialization, global state, callbacks and teardown for every dependency.
Do not reintroduce wrappers around GL owners or create disconnected test executables
as substitutes for the application's selected lifecycle.

The next task is **R0/R1 source investigation**, beginning at Windows entry and
LLAppViewer initialization: enumerate configuration and registered-service branches,
close their helper obligations, then write the three answers and lifecycle/build
decision. Do not restart the selector or font/resource implementation first.

The rollback removes the three post-checkpoint commits from the active development
line and discards their uncommitted continuation. The abandoned
copilot/reverse-engineer-rendering-process branch is removed locally and remotely.
An external recovery bundle and hash-verified file archive preserve those experiments
for forensic reference, not automatic reapplication. Unrelated branches and the
untracked mcp-Vulkan workspace content are not part of this rollback.