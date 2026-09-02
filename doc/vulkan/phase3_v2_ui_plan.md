# Phase 3 (v2) — Independent Vulkan UI Pipeline

**Status:** DRAFT for review — no code written against this plan yet.
**Branch:** `vulkanui` (cut from master @ `8d0e03293d`, the PR #17 merge).
**Supersedes:** the `vulkan-ui` funnel/interception Phase 3 (draft PR #18, closed unmerged, retained as archive).

---

## 0. Governing rule

> **Follow the results, not the logic.**

GL's UI pipeline is the **reference for results only**. We reproduce the pixels it
delivers, with our own functions, contexts, and workflows. We do **not** enter
GL-coupled functions and skip the GL calls inside them; we do **not** reuse
GL-presuming machinery (`gTextureList`, `LLViewerFetchedTexture` GL upload,
`LLImageGL`, `LLGLState`, `gGL` submission, discard levels, `mTexName`). If a
function's body presumes a GL context, it is off-limits — not "call with guards."

The only GL touchpoints in the entire UI result are **texture upload** and
**draw submission**. Everything upstream of those two (decode, FreeType raster,
layout, transform, clip computation, batching math) is backend-neutral CPU work
that we may reuse. The plan below builds a Vulkan pipeline that owns exactly
those two touchpoints and reproduces every result contract the UI depends on.

This plan is written to be **two-executable-ready**: even while the runtime
`RenderBackend` selector exists, the Vulkan code path is written as if the GL
UI internals did not exist. Backend divergence is resolved at **boundary seams**
(choose GL path *or* Vulkan path once), never via guards inside GL machinery.

---

## 1. The holistic UI process

The UI is one interconnected process. The GL implementation routes every UI
pixel through a single funnel; understanding that funnel is what lets us replace
the *submission* end without disturbing the *production* end.

```mermaid
flowchart TD
    subgraph SEAM["Boundary seams (choose backend ONCE)"]
        DS["display_startup()<br/>pre-login"]
        RU["render_ui_2d()<br/>post-login"]
    end
    DS --> WD
    RU --> WD
    WD["LLViewerWindow::draw()<br/>SINGLE convergence point<br/>for the whole widget tree"]
    WD --> SETUP["setup2DRender()<br/>ortho 0,0 bottom-left,<br/>+X right +Y up, × mDisplayScale"]
    SETUP --> TRAV["Tree traversal<br/>painter's order, reverse-LIFO<br/>+ scissor-rect stack"]
    TRAV --> PRIMS["Closed primitive vocabulary"]
    PRIMS --> SUB["PRODUCTION (backend-neutral CPU)<br/>decode · FreeType raster · layout ·<br/>transform · clip · batch math"]
    SUB --> UP["UPLOAD  ← Vulkan owns this"]
    SUB --> DRAW["SUBMISSION  ← Vulkan owns this"]
```

**Two frame entry points funnel into one tree draw:**
- Pre-login: [`display_startup()`](../../indra/newview/llviewerdisplay.cpp#L164) → `setup2DRender()` → `gViewerWindow->draw()`.
- Post-login: [`render_ui_2d()`](../../indra/newview/llviewerdisplay.cpp#L1949) → `setup2DRender()` → `gViewerWindow->draw()` (direct) *or* the `RenderUIBuffer` dirty-rect FBO path.

**Seam strategy:** at both entry points, when the Vulkan session owns the frame,
we run the *same* `LLViewerWindow::draw()` production traversal, but the
backend-neutral primitives it emits are submitted to the Vulkan sink instead of
GL. This is one boundary decision per frame, not per-call interception.

---

## 2. Result contracts (agent-verified)

Each contract below is what the rest of the system **consumes** — the pixels and
semantics — stated independently of GL. These are the acceptance specs.

### 2.1 Frame orchestration
- **Transform:** widget-space pixel `(x,y)` at a widget whose accumulated tree
  offset is `(Σ ancestors.mLeft, Σ ancestors.mBottom)` lands at logical pixel
  `(ΣmLeft + x, ΣmBottom + y)`, then scaled by `mDisplayScale` (aspect-correction
  × `LLUI::getScaleFactor()`), then ortho `[0,w]×[0,h] → [-1,1]`. Origin
  bottom-left, +X right, +Y up.
- **Order:** strict painter's algorithm — children traversed in reverse
  (`rbegin()→rend()`), so last-added draws last = on top. No depth buffer.
- **Clip:** pure screen-space rectangle **intersection** stack
  (`LLScreenClipRect`); nested clips only get more restrictive; empty stack = no
  clip. Scissor rect in GL bottom-left coords, `w+1,h+1`.
- **Dirty-rect FBO (`RenderUIBuffer`):** **pure performance optimization, default
  OFF, result identical to a full redraw.** → *Decision: Vulkan does a full clean
  redraw each frame; we do not port `mUIScreen`, the dirty-union, or the FBO
  blit. (Revisit only if fill-rate becomes a measured bottleneck.)*
- **Inputs read:** `LLUI::getScaleFactor()`, `LLFontGL::sCurOrigin`, camera
  zoom/sub-region, HUD zoom, `gFocusMgr.getTopCtrl()`, debug flags, skin colors.
  These are cosmetic/optimization inputs; none change ordering or geometry.
- **State persisted:** `sDirtyRect`/`sIsRectDirty` (only meaningful to the FBO
  path we are not porting); `sCurOrigin` reset per frame. Nothing the Vulkan path
  must carry frame-to-frame for correctness.

### 2.2 Images / textures
- **Per-draw hand-off:** destination rect `(x,y,w,h)` screen px + UV sub-rect
  `LLRectf` ∈ [0,1] + straight-alpha tint RGBA + blend mode + clip. Pixel result:
  `frag = tint × sample(uv)` blended over framebuffer. Shader multiply, **no
  premultiply**.
- **Blend:** `BT_ALPHA` (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`) ~always; glow uses
  `BT_ADD_WITH_ALPHA` (`SRC_ALPHA, ONE`).
- **Content-vs-pad:** GL pads NPOT images to power-of-2 and compensates with
  `mClipRegion = original_dim / padded_dim`. **Vulkan has no NPOT restriction →
  upload at exact content dims, UVs 0..1, the warp-bug class disappears.** The
  widget's *content rect* semantics are preserved as a result; the padded-texture
  logic is not.
- **9-slice:** `mScaleRegion` (normalized content space) splits into 9 quads (54
  verts): fixed-size corners, axis-stretched edges, center scales both axes.
- **Decode format:** RGBA8, **linear** (never sRGB), straight alpha, byte order
  RGBA. Source PNG/TGA/J2C.
- **Placeholder:** widget checks `imagep.notNull()`; if the image isn't loaded it
  simply **does not draw** (no explicit placeholder texture). Critical art is
  preloaded; dynamic art appears on the next draw after load.
- **Load key (pre-GL):** image **name** (string) or **UUID** — both known at
  widget construction, before any texture exists. Cache is keyed by name/UUID.

### 2.3 Text / fonts
- **Per-glyph result:** a screen-aligned quad (6 verts) at
  `origin + advance + bearing`, UV sub-rect into a glyph atlas page, colored by
  `tint × glyphAlpha`, `BT_ALPHA` blend. Pixel-snapped (`ll_round`).
- **Atlas:** packed 512×512 pages, 1px border, per-glyph
  `(offsetX, offsetY, w, h, bearing, advance)`. Grayscale glyphs = luminance+alpha
  (coverage in A); color emoji = BGRA premultiplied. Y-flip on copy into atlas.
- **Coordinate model:** baseline origin; `sCurOrigin` global offset; advance
  accumulates per glyph with kerning, rounded to whole px; vertical align
  (BASELINE/TOP/BOTTOM/VCENTER), horizontal align (LEFT/RIGHT/HCENTER).
- **Layout features actually used:** LTR only, word-wrap, ellipsis truncation,
  per-segment color (not mid-word), underline drawn as a separate line primitive,
  bold = double-draw at +1px, drop shadow = 1–5 offset copies. No RTL, no
  ligature shaping, no sub/superscript.
- **Backend-neutral:** FreeType rasterization is 100% CPU (FT_RENDER_MODE_NORMAL),
  no GL. Identical bitmaps under any backend. We reuse FreeType; we own the atlas
  texture (Vulkan) and the quad submission.

### 2.4 Chrome / primitives
**Exhaustive closed vocabulary** the sink must cover, all screen-space 2D,
pre-transformed CPU-side, per-vertex RGBA, straight-alpha blend:
solid/gradient rect · outline rect (1px) · line (1px) · triangle ·
textured quad (simple 6v, rotated, 9-slice 54v) · segmented 9-slice rect ·
drop shadow (10 tris, per-vertex alpha gradient, extends `lines` px right/down,
inner=start_color→outer=alpha 0) · checkerboard (tiled texture) ·
circle/arc/washer (rare, TRIANGLE_FAN/STRIP) · corner brackets · glyph quad.
- **Layering:** painter's order only; modals sit atop via a separate stack that
  still resolves to draw order. Opacity via `getCurrentTransparency()` modulating
  all primitives (`color % alpha`).
- **Blend:** only `BT_ALPHA` and `BT_ADD_WITH_ALPHA` are used. **No stencil, no
  XOR, no multiplicative, no depth, no custom shaders.** Rounded corners come
  from 9-slice texture art, not SDF/geometry shaders. → *Every effect maps to a
  standard Vulkan alpha-blend pipeline.*

---

## 3. Architecture — what we build

All new code is Vulkan-native, lives under the established `llvulkan` module and
a viewer-side Vulkan UI layer, and is reached through boundary seams only.

```mermaid
flowchart LR
    subgraph PROD["Production (reused, backend-neutral)"]
      W["widget tree draw()"] --> P1["transform/clip math"]
      IMGDEC["image decode<br/>(LLImage)"] 
      FT["FreeType raster"]
    end
    PROD --> SINK["LLVKUI2D sink<br/>(harvested from archive:<br/>rect, texturedQuad, 9-slice,<br/>lineStrip, dropShadow, glyph,<br/>per-topology pipelines)"]
    subgraph VKOWN["Vulkan-owned (new)"]
      LOADER["VK image loader<br/>fetch+decode → RGBA8<br/>exact content dims, no pad"]
      STORE["VK pixel store / cache<br/>keyed by name/UUID"]
      ATLAS["VK font atlas<br/>512² pages, LA + BGRA"]
      TEXC["LLVKUITextureCache<br/>staging + descriptors"]
    end
    LOADER --> STORE --> TEXC
    FT --> ATLAS --> TEXC
    TEXC --> SINK
    SINK --> CTX["LLVKContext 2D pipelines<br/>dynamic viewport/scissor,<br/>BT_ALPHA + ADD blend"]
    CTX --> SES["LLVKSession frame<br/>acquire → render → present"]
```

**Components:**
1. **`LLVKUI2D` sink** — *harvest from `vulkan-ui` archive* (it is already
   byte-exact for the harness primitives). Extend to the full vocabulary: 9-slice
   textured quad, drop-shadow (per-vertex alpha), glyph quad (mask×tint),
   gradient rect, circle/arc/washer. Per-topology pipelines; append-offset
   batching; deferred buffer growth.
2. **VK image loader + pixel store** — *new.* Owns fetch (curl/file) → decode
   (LLImage, backend-neutral) → RGBA8 at exact content dims → keyed store.
   No `gTextureList`, no `LLViewerFetchedTexture` GL path, no discard levels,
   no `mTexName`, no `onUIImageLoaded` GL callback.
3. **`LLVKUITextureCache`** — staging upload + descriptor per resolved texture.
   (Archive version exists; re-key it by name/UUID instead of opaque pointer.)
4. **VK font atlas** — *new upload path around reused FreeType raster.* Own
   512² LA/BGRA pages as Vulkan textures; own descriptor-per-page.
5. **Frame seams** — `display_startup()` and `render_ui_2d()` choose the Vulkan
   submission once per frame; the production traversal runs unchanged but emits
   to the sink. **Full redraw; no `mUIScreen` FBO.**

**Explicitly not built:** `mUIScreen` dirty-rect FBO; any reuse of GL texture
objects; any guard-inside-GL-function; GL matrix stack (transforms are already
CPU-computed pre-transform — we consume the results).

---

## 4. Milestones

Each milestone ends in a **byte-exact (or policy-tolerance) diff vs. GL** on the
capture harness, per the parity policy: opaque = tol 0, alpha-blended = tol 1.

- **M0 — Skeleton frame.** Vulkan session runs `LLViewerWindow::draw()` into the
  sink with the correct ortho/scale/clip; solid backgrounds only. *Accept:*
  solid-color regions byte-exact vs GL on the login screen.
- **M1 — Chrome primitives.** rect/line/outline/gradient/drop-shadow through the
  sink. *Accept:* login window chrome + focus rings byte-exact.
- **M2 — Images.** VK loader + store + cache live; named chrome art and
  UUID-addressed dialog art render. *Accept:* icon/panel/dialog regions
  byte-exact; no padding warp.
- **M3 — Text.** VK font atlas + glyph quads; wrapping/ellipsis/segments/
  underline/bold/shadow. *Accept:* all login text byte-exact (tol 1, alpha).
- **M4 — Media/dynamic.** login_html and other dynamic textures via the loader.
  *Accept:* media regions match GL.
- **M5 — Sweep & full-frame.** every widget class; ungated full-frame byte-exact
  diff vs GL across login + a post-login scene.
- **(Parallel) Capability probe** — Vulkan populates device/feature facts from
  `VkPhysicalDeviceProperties` + memory properties before `LLFeatureManager`
  first reads them; bandwidth benchmark gets a Vulkan replacement or static
  fallback. *(Separate task; not on the UI critical path.)*

---

## 5. Testing protocol (unchanged)

- Notify before launching a viewer; 75 s settle; close via `WM_CLOSE`; verify
  `Goodbye!` + exit 0; never kill a healthy run.
- Capture via the existing harness env toggles; diff via
  `fsutils/vulkan_frame_diff.py` (`--mode opaque|alpha`).
- Harness scenes must stay byte-exact as each milestone lands (regression gate).
- Do not overstate parity — verify shapes visually; diff PNGs can mislead on
  narrow sample regions.

---

## 6. What changed vs. the superseded plan

| Superseded (`vulkan-ui`, funnel) | This plan (`vulkanui`, independent) |
|---|---|
| Intercept GL call sites (funnel hooks) | One boundary seam per frame; production traversal emits to a Vulkan sink |
| Repair GL texture pipeline to run GL-free | Own VK loader/store/cache keyed by name/UUID |
| Guard GL calls inside shared functions | GL-presuming functions are off-limits, not guarded |
| Port GL's NPOT padding + clip UVs | Exact-dims upload; UVs 0..1 (warp class removed) |
| (implicit) port dirty-rect FBO | Full clean redraw (FBO is opt-in perf only, default off) |
| Discard levels / `mTexName` fakery | Absent — GL residency logic is not a result |
