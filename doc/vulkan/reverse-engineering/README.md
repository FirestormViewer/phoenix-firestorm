# Approved OpenGL reverse engineering

**Approved for publication:** 2026-09-09.
**Inspected baseline:** local `origin/master` and `HEAD` at
[`59108e15a1f8f94d2da7c674d937d19f5cf9450d`](https://github.com/anne-skydancer/vulkanstorm/tree/59108e15a1f8f94d2da7c674d937d19f5cf9450d).

The objective is to recover the governing logic needed for equivalent native
Vulkan results, not to translate OpenGL functions. This is a source-based
investigation, not runtime validation or a claim of pixel parity.

## Reading order

1. [End-to-end report and native Vulkan contracts](opengl-pipeline-reverse-engineering.md):
   frame/data flow, render targets, composition/history, proposed architecture,
   requirements and validation matrix.
2. [Scene submission](scene-submission.md): visibility, geometry/LOD, material
   classification, draw pools, rigging, water and transparency.
3. [Shader contracts](shader-contracts.md): G-buffer encodings, material/lighting
   equations, coordinates, atmosphere, reflections and postprocessing.
4. [Backend/resource contracts](backend-contracts.md): GL resource/state
   assumptions, current native Vulkan coverage and foundational risks.
5. [Auxiliary views](auxiliary-views.md): probe scheduling/filtering, environment,
   screenshots, picking, UI and HUD.

## How to use the evidence

The [development invariants](../native_vulkan_invariants.md) are the normative
rules derived from this report. Proposed architecture and test plans in the
report are not assertions that those implementations/tests already exist.

Reports retain their original investigation scope, cross-investigation handoffs
and static-evidence caveats. Source line numbers refer to the pinned commit, not
necessarily the current checkout. The main report's source links are immutable
GitHub links; companion citations use repository-relative paths and defined
shorthand. Worktree/revision/cleanliness statements describe the investigation
before this documentation was committed.

Publication preserves the approved findings while replacing machine-local paths
with portable references and correcting publication-context wording. Subsequent
baseline updates must be explicit and reviewed; do not silently rewrite the
historical observations to describe newer code.

## Subsequent UI investigation

The [native UI coverage ledger](native-ui-coverage.md) tracks the separate
2026-09-10 investigation on branch native-vulkan-ui, based on implementation
checkpoint 90af5a7 and source revision 3abd661f. Its local contracts and open
dependencies do not amend the approved historical report or establish exhaustive
coverage, native implementation, runtime validation or measured parity.
