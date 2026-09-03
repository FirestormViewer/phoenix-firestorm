# llvktext — the parallel Vulkan text renderer (design + result contract)

**Status:** design. **Scope:** pulled into M0 (byte-exact chrome needs working
text layout; layout needs non-GL text metrics). **Branch:** `vulkanui`.
**Rule:** parallel implementation, NOT a shared abstraction over GL's font
classes. `llvktext` reproduces the RESULT of `LLFontGL` and is used EXCLUSIVELY
by the Vulkan path. It shares with GL only: FreeType rasterization (CPU,
backend-neutral) and this result contract. It never touches `LLFontGL`,
`LLFontBitmapCache`, `LLImageGL`, or `gGL`.

---

## Why parallel, not shared (decided 2026-09-02)

The GL text path fuses layout (neutral) with GL texture allocation in one lazy
path: `LLFontGL::render` interleaves layout with `gGL` submission, and the atlas
is a GL texture created on first glyph measure. There is no clean "reuse the
cache, gate the GL" — the cache IS GL's. So `llvktext` is its own renderer: own
FreeType raster driver, own atlas (Option-A RGBA8 → Vulkan textures → the sink),
own layout, own submission into `LLVKUI2D`.

The binding constraint that shapes it: **fonts initialize before the Vulkan
session/device exists** (window created GL-free → fonts init →
`LLVKSession::start()`). So glyph pages are rasterized to CPU and QUEUED;
upload to Vulkan textures happens once the device is up (flush on the first UI
frame). This mirrors the archived `llvkuicache` queue-and-flush precedent.

---

## The result contract (agent-verified; the spec llvktext is built against)

### Glyph quad (draw result)
Per glyph, a 6-vert quad (2 tris, CCW):
- screen rect (px, GL bottom-left origin, snapped with `ll_round` on left AND top
  independently):
  `left = round(cur_x + bearingX + x_offset)`, `top = round(cur_y + bearingY)`,
  `right = left + width`, `bottom = top - height`.
- UV into the atlas page: `u = (bitmapOffsetX .. +width) / pageW`;
  `v` spans `(bitmapOffsetY - 0.5) .. (bitmapOffsetY + height + 0.5)` over pageH
  (PAD_UVY = 0.5 half-pixel border, Y-inverted in UV).
- color: grayscale glyph → text RGBA; color/emoji → (255,255,255, textAlpha).
- blend: SRC_ALPHA / ONE_MINUS_SRC_ALPHA (BT_ALPHA).
- advance: `cur_x += xAdvance + kerning`, then `cur_x = ll_round(cur_x)`;
  kerning = FT_KERNING_UNFITTED + RSB/LSB subpixel correction (±1px thresholds).
- effects: BOLD = draw twice (offset +1px); DROP_SHADOW = shadow at (+1,-1) then
  main; DROP_SHADOW_SOFT = 5 shadow offsets then main.

### Layout (what callers consume)
- `render()` returns `chars_drawn` (a count, NOT width).
- `right_x` out-param = unscaled X of the text end: `(cur_x - originX)/sScaleX`.
- `getWidthF32()` = unscaled F32 width incl. kerning + overhang padding, with
  `ll_round` after each kerning; `getWidth()` = `llfloor` of that.
- `maxDrawableChars()` = max chars fitting a pixel budget (with wrap modes).
- These MUST work even when nothing is drawn (layout depends on them).

### Atlas
- Pages ≤ 512×512; 1px margins + gaps; new page when full.
- Grayscale page: 2-channel (L,A), only **alpha** holds coverage.
- Color/emoji page: 4-channel BGRA from FreeType.
- Cache generation increments per glyph alloc — Vulkan must watch it to
  invalidate cached glyph UVs.

### FreeType (identical bitmaps)
- `FT_Set_Char_Size(face, 0, point_size*64, horzDPI=96, vertDPI=96)`.
- Load flags: `FT_LOAD_FORCE_AUTOHINT` (+`FT_LOAD_COLOR` for emoji);
  render `FT_RENDER_MODE_NORMAL`.
- Metrics: `bitmap_left/top`, `advance.x/y` (/64), `lsb/rsb_delta`.

### Shared state the text path reads (llvktext must honor the same inputs)
`sCurOrigin`, `sScaleX/sScaleY`, `sCurDepth`, `sVertDPI/sHorizDPI`, the cache
generation, `sDisplayFont`. (These are viewer-neutral CPU state; llvktext reads
the same values the GL path would have, supplied via the neutral owners, never
via gGL.)

---

## Components of llvktext

1. **Raster driver** — FreeType → glyph bitmap + metrics, using the contract's
   parameters so bitmaps are identical to GL's. Reuses the FreeType library
   (backend-neutral); does NOT reuse LLFontFreetype the class.
2. **Atlas** — own Option-A pages: grayscale coverage expanded to RGBA
   (white + coverage-in-alpha) so the 2D shader's tint×texture multiply gives
   the right result with NO second shader; color pages as BGRA. Pages upload to
   Vulkan textures via the LLVKContext 2D texture path; queued until the device
   exists.
3. **Layout** — width/advance/kerning/wrap/ellipsis reproducing the contract.
4. **Submission** — glyph quads into the LLVKUI2D sink (texturedBatch / glyph
   quads), honoring the shared-state transform.

## Acceptance
- Text renders on the Vulkan login screen; layout matches GL (text extents).
- Byte-exact text regions vs GL (alpha tol 1 — text is alpha-blended).
- No GL context, no crash; the GL path is byte-identical (untouched classes).
