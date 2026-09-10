# Native Vulkan development invariants

**Status:** Governing development contract, established 2026-09-09 from the
approved [OpenGL reverse-engineering report](reverse-engineering/opengl-pipeline-reverse-engineering.md).
**Reference revision:** `59108e15a1f8f94d2da7c674d937d19f5cf9450d` (`origin/master`
when inspected).

## Scope and authority

These rules govern the native Vulkan implementation in `indra/llvulkan` and
every supporting change to scene production, assets, shaders, UI, windowing,
startup, settings, packaging and tests. Scope follows behavior, not directory or
branch name. Zink is an OpenGL implementation on Vulkan, not the native renderer.

MUST/MUST NOT are requirements. SHOULD permits a justified, documented choice
that still satisfies the requirements. Reviewers must reject a violation unless
an explicit contract amendment is approved with the evidence described below.

This document takes precedence over conflicting native-renderer proposals in
historical design/phase documents, including the original `LLRenderBackend`
dispatch/RHI proposal. It operationalizes the result-equivalence principle in
[migration_strategy.md](migration_strategy.md); it does not require GL-shaped
code. It does **not** change backend defaults, platform support, or the scheduling
decision in [opengl_modernization_strategy.md](opengl_modernization_strategy.md).
Documenting native development rules does not itself resume or implement a
deferred roadmap phase.

The report is evidence about a pinned revision, not a specification of every
future OpenGL revision. Undefined, dormant or unmeasured behavior must not be
promoted to a compatibility requirement merely because it appears in source.

## Architecture and reference

### NV-00: Answer the three architectural questions first

Established 2026-09-10 after resetting implementation work to PR #41,
`90af5a7220f1fec28e3c909c051b6ee7e062f10b`. This is the foundational method for
all native Vulkan work, including startup, UI, helpers, assets, shaders, build
integration and shutdown, not only functions containing graphics API calls.

For every function and helper in the migration inventory, answer:

1. **What is the OpenGL function doing?** Recover its observable contract from
	source: inputs, outputs, units/encoding, state reads/writes, branches, early
	returns, failures, ordering, callbacks and ownership. Follow the actual helper,
	constructor/destructor, virtual and registered-callback targets. Do not infer
	purity from a name or a const method; identify configuration-dependent paths.
2. **How is this done in Vulkan?** Explain how to produce that result using native
	data and ownership. Identify responsibilities that stay CPU-only and suitable
	audited libraries within NV-01's sharing boundary. Separate CPU preparation, GPU execution, publication
	and retirement. State dependencies instead of translating API calls.
3. **What is the cleanest implementation of question 2 in terms of Vulkan?**
	Compare viable native designs, choose the smallest coherent ownership model,
	and explain its lifetime, synchronization, capability and failure contracts.
	Several GL functions may become one native operation, or one GL function may
	split across stages. A native counterpart with the same name is not required.

Before an implementation edit, the touched behavior MUST have a source-backed
answer to all three questions and a focused check that can disprove the proposed
contract/design. Existing contracts can be referenced rather than duplicated.
Unknown reachable behavior and unresolved transitive dependencies MUST remain
explicitly open; they cannot be silently treated as omissions or completed work.
Bounded probes may investigate an unknown, but MUST NOT be presented as finished
implementations. Global inventory work can proceed incrementally; this is not
permission to implement an unexamined slice while calling it complete.

Each record MUST identify source revision/configuration and function roots,
callee/callback obligations, the three answers, verification and current status.
Distinguish local-body inspection, transitive closure, implemented behavior,
runtime validation and measured parity. Counts of symbols, extracted AST entries,
successful builds or screenshots do not establish exhaustive understanding.

A failure diagnostic or disabled route may be temporary containment, but MUST
NOT replace the requested functionality or close its milestone. Likewise an
offscreen rendering experiment does not establish interactive login, native
widget construction, complete lifecycle routing or world rendering.

The [native viewer roadmap](native_viewer_roadmap.md) applies this method to the
restored checkpoint. Reverted experiments are evidence to reassess, not approved
architecture, reusable code by default, or current feature completion.

### NV-01: Preserve results, not OpenGL execution

Native Vulkan MUST implement the renderer's scene decisions, material equations,
composition and temporal contracts natively. It MUST NOT translate `gGL`,
`LLRender` or raw GL calls one-for-one; route GL draw callbacks into Vulkan;
introduce a common low-level GL/Vulkan RHI to preserve those callbacks; or render
the GL frame to a texture and present it as native Vulkan.

### NV-01 sharing boundary (user amendment, 2026-09-10)

Native Vulkan MUST NOT reuse the existing OpenGL-exclusive visual functions as its
implementation: rendering, UI construction/layout/input behavior, fonts/text
measurement and rasterization, textures/images, meshes/scene preparation, materials,
shaders, postprocessing, visual capture and related helpers. This applies to CPU
preparation as well as GPU execution wherever the actual dependency chain relies on
GL contexts, state, resources or execution semantics. Calling a function pure or
backend-neutral does not establish independence. Separate instances, wrappers or
indirect calls do not remove its GL dependencies.

Functionality separate from the visual aspect MAY be shared after audit. General file IO,
logging, allocation, generic configuration parsing or nonvisual application services
are candidates, not blanket approvals. Trace callees, callbacks and ownership under
NV-00: a nominally nonvisual service that constructs UI, resolves fonts, processes
textures/meshes or invokes rendering is not an approved shared dependency as-is.
Clarification, 2026-09-10: the incompatibility is in the existing functions as they
are, not in code reuse as an abstract property. This is not a blanket requirement
to reimplement independent third-party font rasterizers or other API-independent
libraries. Such a dependency must be audited independently under NV-00; using it
does not authorize reuse of the viewer's GL-coupled wrappers, globals or callbacks.
Neither library provenance nor a neutral name proves suitability. Reading the same
source asset or declaration does not authorize sharing its GL-exclusive consumers.

Native development MUST leave the OpenGL implementation untouched. Implement native
equivalents with their own visual functions and owners; do not extract shared visual
services from GL, alter its upload timing or add backend branches to its functions.
Keep existing GL/Zink operation intact. Vulkan-specific resource/command abstractions
are encouraged, but they MUST NOT translate GL calls. Equivalent results and the
three architectural questions remain required; independent code is not permission
to approximate behavior or reproduce undefined behavior.

**Example:** consume immutable material/mesh/view packets in a native pass;
do not implement `LLRender::begin/end` as Vulkan command recording.

### NV-02: Keep a pinned, independent reference

Each parity claim MUST name the GL revision, asset/scene inputs, settings,
effective shader variants, target sizes/formats, camera, timing/history state and
device/driver. The initial reference is the revision above.

A native implementation PR MUST NOT alter the GL oracle, golden data or
tolerances merely to make its comparison pass. Legitimate GL modernization is
independent work; adopting a newer reference requires an explicit reviewed
baseline update with before/after evidence and affected invariant IDs.

Historical report claims MUST remain dated to their original revision. Current
implementation status and newly measured evidence belong in separately dated
updates, not silently rewritten history.

### NV-03: One backend owns the process's rendering

Backend selection MUST occur before incompatible window/context initialization
and remain fixed until restart. A running native Vulkan session MUST NOT require
a GL context, GL object allocation, GL draw execution or GL readback for normal
rendering, assets, UI, text, previews or capture.

A startup fallback MUST be explicit and complete before rendering begins; after
a GL-free window has been created, failure MUST NOT silently continue into GL
initialization or present a success-shaped, permanently blank session. Surface
actionable errors or perform a deliberately implemented full recovery.

Existing links to GL-owning libraries/types are migration debt, not permission
to add new GL dependencies. Do not expand that coupling; new GPU interfaces MUST
use native handles or backend-neutral asset identities, never GL object names.

### NV-04: Separate scene policy from GPU submission

Native submission MUST consume stable/versioned data for geometry, LOD,
transforms/origin epoch, material family, coverage/cutoff, texture/sampler state,
skinning, light state and ordering. Recording MUST NOT consult an ambient mutable
"current material", GL matrix stack or framebuffer state later in execution.

Preserve routing and availability decisions: a face may contribute base, glow,
post-bump and exclusion draws; residency can change alpha classification;
rigged attachments and ordinary meshes have different transform/ordering rules.
Do not equate a mesh with one pass or vertex alpha with transparency.

### NV-05: Make every view explicit

Main, shadow, probe, mirror, impostor, preview, HUD and snapshot work MUST have
explicit camera/projection, viewport, clip, inclusion/LOD, target, output-encoding
and history policies. Mutable global camera/target/cull flags MUST NOT be the
native scheduling interface.

Auxiliary views MUST NOT overwrite main-camera occlusion/history or advance
simulation merely because another view is recorded. Preserve cube-capture
environment-time freezing while rebuilding its view-dependent uniforms.

## Visual and temporal contracts

### NV-06: Treat material data as a typed, versioned ABI

All producers and consumers MUST agree on field encoding, units, color space,
precision, sampling and alpha meaning. Legacy and SL PBR MUST remain distinct:
the reference G-buffer mixes legacy sRGB-valued and PBR linear payloads, ORM is
linear data, and category tags `0/.34/.67/1` use tolerance tests rather than bits.

Base-color factor application, UV/normal-map transforms, double-sidedness,
cutoffs and fallback textures MUST match their material family. Classic sky
behavior applies to both legacy and PBR, not just legacy surfaces.

Changing G-buffer packing, normal encoding or separating glow is allowed only
as an explicit ABI change with every producer/consumer updated and downstream
equivalence demonstrated. Blanket sRGB attachment conversion is not equivalent.

### NV-07: Preserve composition boundaries and lighting policy

Native passes MUST preserve the dependency order and blend/depth/channel
semantics recovered in report sections 5-7:

- deferred opaque/masked lighting followed by ordered forward composition;
- water exclusion and pre-water alpha, water and post-water alpha boundaries;
- haze placement changing above/below water and transmittance composition;
- glow carried separately from surface coverage, even when storage is shared;
- legacy/PBR light equations, selected light sets, probe fallback and shadows.

Deferred lighting's larger candidate set and direct colors MUST NOT be conflated
with the later six fade-scaled forward local-light slots. Water MUST NOT become
an ordinary transparent GGX material. SSAO MUST NOT become an indiscriminate
final-color multiply.

Clustered lights, reordered opaque work or merged passes are implementation
choices only after their consumer-visible results are shown equivalent.

### NV-08: Preserve the selected transparency algorithm

Sorted/interleaved alpha, PPLL and bounded peeling MUST be explicit policies with
view/capability eligibility, depth-write rules, limits and fallback behavior.
Avatar-ensemble ordering MUST NOT become an arbitrary global material sort.
Custom blends/particles, pre-water, HUD, shadows and probes MUST NOT be silently
forced through main-view post-water OIT.

For reference modes, preserve FP16 PPLL payloads, opaque-depth rejection,
bounded exact subset/traversal/approximate tail, allocation-overflow blending,
and peeling's exact layers plus legacy tail and cooldown. Only PPLL explicitly
restores ordinary rigged depth after its capture in the reference; do not
generalize that behavior to successful peeling.

Replacing these with weighted blended OIT or changing bounds is a quality/
behavior change requiring explicit approval and comparisons, not an automatic
optimization. CPU-budgeted peeling requires controlled budgets in parity tests.

### NV-09: Version history and preserve publication timing

SSR scene/depth and their camera transforms, previous glow feeding luminance,
exposure adaptation, probe scratch/resident products, spotlight ownership/fades
and occlusion state MUST have explicit validity, version and publication rules.

Preserve the reference's ordinary probe updates **after display** versus
hero/mirror work **before the main scene**, including snapshot rerenders that can
consume newly published probes. Ordinary irradiance/radiance cycles and staggered
hero faces are distinct policies, not a single "refresh all probes" operation.

New resources MUST NOT be sampled before initialization/completion. Resize,
teleport, origin shift, camera discontinuity and configuration changes MUST
have documented history invalidation or retention behavior. Where reference
initialization is undefined, define and review a deterministic policy rather
than claim parity with uninitialized memory.

### NV-10: Preserve postprocess order and output encodings

Native finalization MUST respect the reference's gated sequence: SSR history
copy; luminance/exposure; tone map plus optional CAS/gamma (or gamma-only);
display-space glow extraction/combine; DoF; FXAA or SMAA; optional RLVa/vignette/
snapshot effects; final noise/depth output; HUD/UI.

Do not apply tone mapping/gamma twice, move glow to conventional pre-tonemap
bloom, or include HUD/UI in world exposure/fog/DoF. Alpha MUST be typed by stage:
material data, glow, FXAA luma and DoF circle-of-confusion are different values.
Swapchain format and transfer function MUST agree with the produced color.

### NV-11: Change coordinate conventions coherently

Any change to clip depth, viewport Y, winding, origin space, matrix packing or
texture orientation MUST update and validate every affected consumer:
reconstruction, shadow bias/comparison, SSR, SSAO, water planes, OIT, HUD,
snapshots and CPU picking.

Texture-row orientation, screen orientation and material UV transforms MUST
remain separate concepts. Reverse-Z, octahedral normals and similar changes
SHOULD be isolated from initial parity bring-up to avoid changing multiple
unverified contracts simultaneously.

### NV-12: Preserve auxiliary outputs and UI semantics

Impostors, dynamic textures/bakes, previews, world labels, HUD attachments and
the widget tree MUST retain their own viewport, depth, coverage, ordering and
output contracts. A working login UI does not establish world or full-widget
coverage.

UI rendering MUST read prepared layout/state and use native primitives/text/
images, not invoke GL-coupled `draw()`. Existing draw-time layout side effects
MUST be handled explicitly in preparation. Preserve nested clips, inherited
alpha, painter order, font fallback and retained-buffer contents where used.

Ordinary picking MUST retain CPU intersection/selection/callback semantics;
do not add mandatory GPU ID readback merely to replace a misleadingly named
"GL pick" path. Screenshots are offscreen rerenders/readbacks with presentation
suppressed, not normal frame advancement.

## GPU correctness and capability

### NV-13: Declare resource dependencies, not just command order

Each native pass MUST declare/establish resource reads and writes, image aspect/
mip/layer, layout, load/store/clear behavior and queue ownership where applicable.
Uploads, shared depth, sampled scene copies, PPLL atomics/resolve and probe
filtering MUST have valid memory dependencies.

Sampling an image while writing it MUST use a deliberately supported and
validated arrangement, not accidental attachment feedback. Resource format,
extent, usage and sampler interpretation MUST be part of identity/validation.
Whole-resource assumptions MUST NOT hide independent probe face/mip hazards.

### NV-14: Retire resources by completed GPU work

CPU writes, image replacement, descriptor reuse and buffer/image destruction
MUST be gated by completion of **all** submissions using that version.
Use per-frame arenas or completion-tagged rings/retirement. A frame counter,
current-slot fence or "previous frame should be done" comment is not proof that
shared storage is safe.

Upload data MUST stay alive through consumption; publish ready image versions
without losing residency/discard callbacks or rebuild signals. Host-visible
noncoherent allocations MUST be flushed before GPU reads and invalidated before
CPU reads as required. Descriptor capacity MUST be bounded/reclaimable without
recycling live sets.

Per-upload/device-idle waits MUST NOT be the normal world-streaming architecture.
Bounded synchronous bootstrap/diagnostic work is permissible when explicitly
identified; it must not conceal missing lifetime rules.

### NV-15: Negotiate actual support and report failures

Feature/format/limit/extension checks MUST use the physical device selected for
the actual surface and queue topology. A successful enumeration probe, Vulkan
version string or enabled preference does not establish implementation support.
GPU facts and capability UI MUST describe that selected device and implemented
features.

Unsupported/incomplete features MUST be unavailable or use an explicitly
documented, tested fallback with diagnostics. No silent missing geometry,
success-shaped resource failures or claims of parity for a clear frame.

Acquire/present out-of-date and surface/device loss MUST have explicit recovery/
failure handling, including same-size invalidation and fence state after failed
submission. Readback MUST use valid usage flags and application-owned images,
with completion and row/format conversion; device-idle after present does not
reacquire a swapchain image.

### NV-16: Build complete, reproducible shader variants

Shader builds MUST include required prelude definitions, feature helpers,
permutations and fallback selection, not compile each raw reference GLSL file
in isolation. Vulkan bindings and vertex/uniform/storage layouts MUST be
explicit and validated against CPU packing and device limits.

Runtime SPIR-V MUST correspond to the reviewed source/configuration and be
reproducible through the documented build path. Updating only source while
shipping stale checked-in binaries does not complete a shader change. GL program
binary caches and texture-unit numbers MUST NOT be reused as native shader ABI.

## Evidence, scope and amendments

### NV-17: Prove parity at the consumer boundary

Implementation PRs MUST identify affected invariant IDs and include the smallest
relevant existing build/tests plus runtime evidence for changed rendering or GPU
lifetime behavior. Compare stage outputs and temporal sequences, not only a
final screenshot. Validation layers MUST be used for applicable GPU changes;
their availability and actual execution must be reported, not inferred from a
setting. Performance claims require repeatable measurements.

Use exact comparison for discrete identities/order/flags and outputs whose
byte-exact contract is established. Numeric/image tolerances MUST be documented
from repeated reference captures and approved **before** evaluating the change;
do not assume all floating-point attachments are cross-driver byte-exact or
adjust thresholds after seeing failures.

Record actual results, environment and limitations. Static tracing is not a GPU
test; compilation is not visual parity. Unavailable runtime evidence MUST be
marked unverified and MUST NOT be used to claim a feature's parity gate passed.
Documentation-only changes need link/source/hygiene checks, not a renderer build.

### NV-18: Separate parity from restoration and improvement

Do not emulate undefined shader values, inactive branches or accidental resource
hazards as requirements. Document reachable defined quirks; make corrections
explicit behavior changes with evidence. The separate native glTF scene's
compile-disabled draw-data production is not the active SL PBR material path;
enabling it requires its own feature scope, coverage and tests.

Improvements such as new light selection, different alpha algorithms, revised
tone mapping or newly supported platforms MUST be distinguishable from baseline
parity. They require an approved contract/baseline amendment and independently
reviewable implementation, not an unannounced substitution.

## Required PR review record

Native-renderer PR descriptions MUST contain the following, proportionate to the
change. Use `N/A` with a reason rather than omitting a category.

| Review field | Required content |
|---|---|
| Contract | Affected NV IDs, feature/view/material modes and intended behavior |
| Reference | Revision, scene/assets, settings/variants, dimensions, time/history and device/driver |
| Data flow | Producer/consumer/ABI changes; color, alpha, depth and coordinate semantics |
| GPU safety | Ownership, dependencies, publication and retirement proof |
| Validation | Commands, capture stages/sequences, predetermined tolerances and actual results |
| Limits | Unsupported modes, unverified claims and explicit failure/fallback behavior |
| Change class | Parity implementation, correction, restoration, optimization or contract amendment |

Before world rendering expands, affected foundational risks from the
[backend investigation](reverse-engineering/backend-contracts.md) MUST be
resolved/verified rather than inherited: shared mapped-buffer reuse,
noncoherent staging/readback, descriptor reclamation, valid capture ownership,
capability negotiation and WSI recovery.

An amendment MUST name the rule and old/new behavior, rationale, affected
consumers/platforms, reference evidence, migration/fallback plan and approval.
Update this contract and applicable instructions together. Absence of a test,
an old draft, or a performance aspiration is not an exception.

## Amendment record: visual implementation independence

- Approval: user directive on 2026-09-10, clarified as zero shared functions for
	rendering, UI, textures, meshes, postprocessing and related visual behavior;
	nonvisual functionality may be shared.
- Affected rules: NV-00 dependency classification and NV-01 sharing permission;
	NV-03 process exclusivity and all parity/lifetime requirements remain in force.
- Old permission: audited neutral scene/layout/asset helpers could be shared.
	New boundary: existing GL-exclusive visual functions must not implement the native
	path. Nonvisual functionality may be shared after audit. The user's subsequent
	clarification does not impose a blanket ban on independently audited API-independent
	third-party functionality; it does not permit refactoring GL into shared visual code.
- Rationale: build native equivalents without modifying or depending on the GL
	visual implementation. A shared CPU atlas/GL-mirror refactor was explicitly rejected.
- Evidence: the checkpoint native startup reaches GL font texture creation without
	a GL context; see the separately dated
	[startup investigation](reverse-engineering/native-ui-startup-dependencies.md).
	This supports dependency separation, not an assertion of native completion.
- Scope: all native visual consumers and supporting platform/startup/build paths.
	One viewer executable and one OS process remain required. Existing GL/Zink visual
	implementation remains unchanged; nonvisual selection/infrastructure may be shared
	only after its contract is established.
- Migration/verification: replace native dependencies on visual GL functions with
	independently owned native equivalents. Audit direct, indirect and third-party
	call dependencies; test native construction without GL owners and qualify output,
	lifetime and unchanged GL behavior. Older reports remain source evidence, but any
	earlier recommendation to share visual helpers is superseded. No fallback may
	silently invoke GL in a running native session. No runtime gate is waived.

## Evidence map

| Invariants | Approved evidence |
|---|---|
| NV-01 to NV-05 | [Main report, architecture and frame flow](reverse-engineering/opengl-pipeline-reverse-engineering.md); [scene submission](reverse-engineering/scene-submission.md) |
| NV-06 to NV-08 | [Shader/material contracts](reverse-engineering/shader-contracts.md); [scene submission and alpha](reverse-engineering/scene-submission.md) |
| NV-09 to NV-12 | [Frame history and postprocessing](reverse-engineering/opengl-pipeline-reverse-engineering.md); [auxiliary views](reverse-engineering/auxiliary-views.md) |
| NV-13 to NV-16 | [Resource/backend contracts](reverse-engineering/backend-contracts.md); [shader ABI](reverse-engineering/shader-contracts.md) |
| NV-17 to NV-18 | [Verification plan and limits](reverse-engineering/opengl-pipeline-reverse-engineering.md); dormant/undefined-path caveats in the companions |

These are review requirements, not a claim that existing code satisfies them or
that CI currently enforces them. Automatic agent instructions point to this
document; runtime qualification remains required.
