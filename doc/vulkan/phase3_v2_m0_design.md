# Phase 3 v2 — M0 Design: Skeleton Frame on `llvkrender`

**Status:** DRAFT for review.
**Branch:** `vulkanui`. **Parent plan:** [phase3_v2_ui_plan.md](phase3_v2_ui_plan.md).
**Milestone goal:** the Vulkan session runs the *real* `LLViewerWindow::draw()`
tree through `llvkrender` into the batched `LLVKUI2D` sink, with correct
ortho/scale/clip, solid fills + outlines + lines live, textures and text stubbed
to safe no-ops. **Accept:** solid-color regions of the login screen byte-exact
vs GL; no crash; clean `Goodbye!` + exit 0.

---

## 0. What M0 must prove

The seam and the coordinate/clip pipeline — not yet textures or text. M0 is
correct when the structural pixels (panel/background fills, borders, focus
lines) the login tree emits land exactly where GL puts them. Everything the sink
needs to do that is already built and byte-exact-verified; M0's work is the
**seam** (how tree calls reach `llvkrender`) and the **neutral state feed**
(transform/clip/color without reading `gGL`).

---

## 1. The neutral choke-point router (Design ii)

One thin header declares the 2D routing surface — it is a **router**, not a
renderer: it dispatches the tree's 2D calls to whichever backend is bound, and
owns no drawing logic itself. Exactly one implementation is bound per backend;
selection happens once at the frame seam. No runtime `if (backend)` inside
shared TUs.

New file: `indra/llrender/llui2drouter.h` — **declarations only, no GL, no Vulkan.**

```cpp
// Neutral 2D/UI drawing ROUTER. Routes the widget tree's 2D calls to the bound
// backend; owns no rendering itself. Backend-agnostic: one implementation is
// bound per render backend at the frame seam. The GL impl wraps the existing
// gl_* helpers (GL reference stays byte-stable); the Vulkan impl is llvkrender.
namespace LLUI2DRouter
{
    // --- transform traversal hooks (stack-based; the tree pushes/pops per
    //     widget via LLRender2D::translate/pushMatrix/popMatrix — see §3) -----
    void pushTransform();
    void popTransform();
    void translate(float x, float y);          // relative, accumulates
    void loadIdentityTransform();

    // --- production state (carried so the Vulkan impl never reads gGL) ------
    void setColor(float r, float g, float b, float a);
    void setBlend(int blend_type);            // LLRender::eBlendType values
    void setScissor(int x, int y, int w, int h);  // GL bottom-left device px
    void clearScissor();

    // --- primitives (vocabulary subset M0 needs; see plan §2.4) --------------
    void rect(float left, float top, float right, float bottom);      // filled
    void outlineRect(float left, float top, float right, float bottom);
    void line(float x1, float y1, float x2, float y2);

    // M0 stubs (no-op on the Vulkan path until M2/M3; live on GL):
    void image(/* LLTexture* + rects, per llrender2dutils signatures */ ...);
    void text(/* LLFontGL render args */ ...);
}
```

Note the transform surface is **stack-based relative** (`push/pop/translate`),
mirroring how the tree actually accumulates offsets through
`LLRender2D::translate/pushMatrix/popMatrix` — not a flat `setTransform`. Each
backend maintains its own transform stack: GL impl forwards to `gGL`'s UI matrix
stack; `llvkrender` keeps its own (§3).

**Binding:** a single function-table (struct of function pointers) or a thin
abstract base, bound once. `LLUI2DRouter::bindGL()` installs the GL impl;
`LLUI2DRouter::bindVulkan()` installs `llvkrender`. The frame seam calls the
appropriate bind when the backend is chosen (session start / first Vulkan
frame), not per call.

**Why a function-table over per-call virtuals:** one indirection at bind time,
none per primitive; keeps the hot path free of vtable chases. (The heavy cost is
GPU submission, which the sink batches — see the §5b invariant.)

---

## 2. The two implementations

### 2a. GL impl — zero-change pass-through (reference stays byte-stable)
`indra/llrender/llui2drouter_gl.cpp` (lives in `llrender`, may touch `gGL`):

- `rect` → `gl_rect_2d(left, top, right, bottom, color, /*filled*/true)`
- `outlineRect` → `gl_rect_2d(..., filled=false)` (the 1px outline)
- `line` → `gl_line_2d(x1, y1, x2, y2, color)`
- `setColor` → `gGL.color4f(...)`; `setBlend` → `gGL.setSceneBlendType(...)`
- `pushTransform/popTransform/translate/loadIdentityTransform` → the existing
  `gGL.pushUIMatrix/popUIMatrix/translateUI/loadUIIdentity` (i.e. forward to the
  same `LLRender2D` bodies used today — unchanged)
- `setScissor/clearScissor` → the existing `LLScreenClipRect`/`glScissor` path
- `image`/`text` → the existing `gl_draw_scaled_image*` / `LLFontGL::render`

**Invariant:** every GL impl body calls the *same* helper the tree calls today,
in the same order, with the same args. The GL path's pixels cannot change.

### 2b. Vulkan impl — `llvkrender` (independent, never reads `gGL`)
`indra/llvulkan/llvkrender.cpp` (+ `llvkrender.h`), built on `LLVKUI2D`:

- Holds its **own** production state: current color, its own transform
  (offset/scale) **stack**, current scissor, current blend. Set by the
  `LLUI2DRouter` calls; consumed at emit. **No `gGL` access.**
- `rect` → `LLVKUI2DSink::get().rect(...)` (applies its own transform at emit).
- `outlineRect` → `lineStrip` (5-pt closed strip, matching GL's inset winding).
- `line` → `lineStrip` (2 pts).
- `pushTransform/popTransform/translate/loadIdentityTransform` → maintain
  `llvkrender`'s own offset/scale stack (§3), then push the resulting flat
  off/scale to the sink via `sink.setTransform(off, scale)` at emit.
- `setScissor` → Y-flip then `sink.setScissor`; `clearScissor` → `sink.clearScissor`.
- `image`/`text` → **M0 no-op** (emit nothing) until M2/M3.

---

## 3. The critical piece: neutral transform/scissor feed (no `gGL` reads)

This is where the archived funnel went wrong (it read `gGL.getUITranslation()`).
M0 must source the transform and clip from **backend-neutral owners** the tree
already maintains. Verified against the baseline (2026-09-02):

- **The traversal hooks are the seam.** The widget tree accumulates per-widget
  offsets through `LLRender2D::pushMatrix() / translate() / popMatrix()`, called
  from `LLView::drawChildren()` ([llview.cpp:1315-1334]) and a few other spots.
  These hooks are the neutral *call sites* — the tree just says "translate by
  the child rect offset." Today their **bodies write to `gGL`**
  ([llrender2dutils.cpp:1765-1797]): `translate` → `gGL.translateUI`, `pushMatrix`
  → `gGL.pushUIMatrix`, etc. So the transform stack currently lives in `gGL`'s
  UI matrix stack.
- **The seam routes these hooks by backend.** `LLRender2D::translate/pushMatrix/
  popMatrix/loadIdentity` become choke points too:
  - **GL impl:** unchanged — drive `gGL`'s UI matrix stack exactly as today.
  - **Vulkan impl:** accumulate the same offset/scale into `llvkrender`'s own
    transform stack (never `gGL`).
  These hooks *also* maintain the backend-neutral font origin state
  (`LLFontGL::sCurOrigin`, `sOriginStack`) — that part is shared CPU state both
  backends read; it is NOT GL-coupled and stays common.
- **Scale:** `LLUI::getScaleFactor()` is neutral (not `gGL`). Combined with the
  window's aspect-correction (`mDisplayScale`). The seam reads this.
- **Scissor Y-flip:** the clip stack (`LLScreenClipRect`) computes GL
  bottom-left device rects. `llvkrender::setScissor` converts to the sink's
  top-left space: `vk_y = device_height - (y + h)` (device height from the
  swapchain extent), then `sink.setScissor(x, vk_y, w, h)`; empty →
  `clearScissor`. Driven by the neutral `LLScreenClipRect`, not `gGL`.
- **Color:** rects/lines carry explicit `LLColor4` at the choke point. For the
  rare ambient-color paths, `llvkrender` tracks its own current color via
  `LLUI2DRouter::setColor` (the tree sets color per-draw; we mirror the setter,
  we don't read `gGL`'s copy).

**`llvkrender` transform model:** a small offset/scale stack mirroring the
tree's push/pop, so that at emit time the accumulated `(off, scale)` equals what
`gGL`'s matrix stack would have produced. The contract result is known (plan
§2.1: widget px → logical px = Σ ancestors.mLeft/mBottom + local, × mDisplayScale);
`llvkrender` reproduces that result in its own stack, fed by the same
`LLRender2D` hook values, never by reading `gGL`.

---

## 4. The frame seams

- **Pre-login** [`display_startup()`](../../../indra/newview/llviewerdisplay.cpp):
  when `LLVKSession::isRunning()`, bind Vulkan impl, `LLVKSession::beginUIFrame()`,
  run `gViewerWindow->draw()`, `LLVKSession::endUIFrame()`. (Replaces the
  current teal-only `renderFrame()` branch.)
- **Post-login** [`render_ui_2d()`](../../../indra/newview/llviewerdisplay.cpp):
  same binding + bracket around `gViewerWindow->draw()`. **Full redraw; the
  `RenderUIBuffer` dirty-rect FBO path is skipped on Vulkan** (pure optimization,
  default off — plan §2.1).

On the GL backend these seams are untouched (the existing GL path runs).

---

## 5. Harvest + CMake

- Bring `llvkui2d.{h,cpp}` (sink), `llvkcontext` 2D-pipeline additions, and the
  compiled `ui2d.vert/frag.spv` from the archived `vulkan-ui` branch into
  `indra/llvulkan/`. The sink is contract-verified; import unchanged.
- New: `indra/llvulkan/llvkrender.{h,cpp}`, `indra/llrender/llui2drouter.h`,
  `indra/llrender/llui2drouter_gl.cpp`.
- CMake: add the new sources to `indra/llvulkan/CMakeLists.txt` and
  `indra/llrender/CMakeLists.txt`. New files → requires a reconfigure (drop
  `--no-configure` once).

## 6. M0 acceptance

1. Login screen solid regions (panel/background fills, borders, focus lines)
   byte-exact vs GL (opaque tol 0) via the capture harness.
2. Textured/text primitives no-op cleanly on Vulkan (no crash, no GL touch);
   they render normally on GL.
3. Clean shutdown (`Goodbye!` + exit 0) per the testing protocol.
4. **Benchmark (non-binding, §5b):** record draw-call count per frame (must be
   bounded / independent of widget count) + an environment-annotated FPS
   datapoint. No absolute FPS gate.
5. Harness scenes 0–4 stay byte-exact (regression gate).

## 7. Explicitly out of scope for M0

Textures/images (M2), text/fonts (M3), media (M4), the 5 custom-draw widgets
(tree-dispatched; GL-only for now), the capability probe (separate task).
