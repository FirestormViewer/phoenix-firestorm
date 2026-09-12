---
description: Governing invariants for native Vulkan rendering and its scene, asset, UI, window, shader and build integration
applyTo: "**"
---

# Native Vulkan development

These instructions apply only to work affecting native Vulkan; the broad file
pattern includes startup, build, packaging and tests outside the renderer tree.
For such changes, including integration outside `indra/llvulkan`,
read and follow [the native Vulkan invariants](../../doc/vulkan/native_vulkan_invariants.md).
They are normative; conflicting historical renderer drafts are not authority.
They do not change the separate OpenGL modernization roadmap.

- Before implementing or changing any Vulkan-related function or helper, answer:
  1. What is the OpenGL function doing?
  2. How is this done in Vulkan?
  3. What is the cleanest implementation of question 2 in terms of Vulkan?
  Follow NV-00 in the invariants: trace branches, hidden state, callbacks and
  transitive helpers; identify CPU-only responsibilities explicitly. Record the
  source contract, native design and discriminating check before editing code.
  One GL function need not map to one Vulkan function. Unknown behavior stays open;
  a scaffold, wrapper, disabled feature or successful screenshot is not closure.
- Follow the [native viewer roadmap](../../doc/vulkan/native_viewer_roadmap.md).
  Start from checkpoint 90af5a7, not the reverted post-checkpoint implementations.
- Implement equivalent results with native scene/view/material/resource data.
  Do not transpose GL calls, add an LLRender-dispatch/shared low-level RHI, invoke
  GL-coupled draw callbacks, or display a GL-produced frame as native Vulkan.
- Keep the selected backend GL-free and process-exclusive. Do not reuse existing
  GL-exclusive visual functions, including CPU UI/layout/input, fonts, textures,
  meshes, scene preparation and postprocessing. Audit transitive dependencies, not
  just direct GL calls. Nonvisual functionality may be shared after audit.
  Independently audited API-independent third-party functionality is not banned
  merely because both paths use it; this does not permit GL-coupled wrappers or
  shared extraction from the GL visual implementation. Follow clarified NV-01.
- Leave the OpenGL implementation untouched. Build independently owned native
  visual equivalents, not shared extractions, GL-owner wrappers or backend branches
  in GL functions. Do not expand existing GL-library/type coupling.
- Preserve material/color/alpha/depth contracts, water/transparency/post ordering,
  per-view policy and versioned history. Changing representation requires
  coordinated producer/consumer changes and evidence, not silent approximation.
- Prove upload/publication, barriers/layouts, in-flight ownership, descriptor reuse
  and completion-based retirement. Handle noncoherent memory and WSI failures.
- Negotiate capabilities on the selected device; incomplete features must be
  explicit. Compile/package the complete shader variants and matching ABI.
- Do not modify the GL reference or tolerances to disguise native mismatches.
  Cite affected NV IDs and use the contract's required PR review record.
- Distinguish static analysis, runtime verification and measured parity. Preserve
  defined behavior; do not reproduce undefined/dormant code as a feature mandate.

Example: replace an implicit FBO stack with explicit per-view image dependencies;
do not replace `glBindFramebuffer` with a Vulkan-shaped wrapper at every callsite.

The [approved report](../../doc/vulkan/reverse-engineering/README.md) supplies the
source-backed contracts. Its baseline is historical, not a claim of current
feature completion. Documentation-only changes require documentation validation.
