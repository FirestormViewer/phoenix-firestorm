# UI image dependency investigation

Source: 3abd661f498329babaf87b49ffab910fcb5f0e6c, implementation baseline 90af5a7220.
Status: OPEN. These are inspected local bodies reached from
[UI font atlas construction](native-ui-dependencies.md), not a complete image
service or UI dependency trace. No runtime/parity validation or native implementation
is claimed. NV-00 requires resolving the named outgoing obligations before closure.

## UI-UIIMAGE-001: LLUIImage constructor

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L35).
**1.** Own name/image pointer; scale/clip regions(0,1,1,0), signal null, scale style
INNER, cached W/H=-1. Body calls getTextureWidth then getTextureHeight, eagerly
filling caches through virtual dimensions during base construction. Members actually
initialize in declaration order, not initializer-list order. No image null check.
**2.** CPU image declaration/region with versioned native sampled resource separately.
**3.** Preserve logical/clipped/natural dimensions as distinct metadata; wrapping a
GL LLTexture is not neutral construction. Check initial caches, null image and
later clip/texture-size change. Outgoing: LLRefCount/LLPointer, dimension getters,
virtual dispatch construction rules and texture lifetime.

## UI-UIIMAGE-002: LLUIImage destructor

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L49).
**1.** Delete loaded signal; members/base destruct afterward, releasing texture.
**2.** CPU subscription/asset identity lifetime plus independent GPU completed-use
retirement. **3.** Native image versions survive every prepared/retained draw until
completion even after declaration wrapper release. Check connected slots, texture
last reference and pending native users. Outgoing: signal/texture/refcount dtors.

## UI-UIIMAGE-003: LLUIImage::getWidth

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L54).
**1.** ll_round(float(image->getWidth(0))*clip.width). No cache used or null check.
**2.** CPU clipped natural dimension. **3.** Native layout needs exact source extent,
clip span and rounding, independent of texture availability. Check partial clip,
fractional result, negative/zero span, discard0 dimensions and delayed asset changes.
Outgoing: concrete LLTexture::getWidth, LLRectf::getWidth and ll_round.

## UI-UIIMAGE-004: LLUIImage::getHeight

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L60).
**1.** ll_round(float(image->getHeight(0))*clip.height). **2.** CPU clipped height.
**3.** Same explicit metadata model as width, separate vertical orientation from
dimension. Check clipping/rounding/asset resize. Outgoing: texture/rect/math helpers.

## UI-UIIMAGE-005: LLUIImage::getTextureWidth

Source: [lluiimage.inl](../../../indra/llrender/lluiimage.inl#L67).
**1.** If cachedW==-1 assign getWidth(), else keep cachedW; return it. Despite name
and comment, caches clipped width from getWidth at first resolution, not a direct
underlying texture extent query. Constructor resolves before later clip changes.
**2.** CPU cached extent policy. **3.** Determine consumer intent and invalidation
before sharing; native representation must not silently equate this to live width.
Check constructor resolution, later setClipRegion and texture changes, returned -1.
Outgoing: virtual getWidth, cached field and all consumers/invalidation (open).

## UI-UIIMAGE-006: LLUIImage::getTextureHeight

Source: [lluiimage.inl](../../../indra/llrender/lluiimage.inl#L73).
**1.** Same -1 sentinel cache of getHeight. **2.** CPU cache. **3.** Retain explicit
extent version only after consumer contract closes; no GPU dimension query needed.
Check first/later clip and source resize. Outgoing: getHeight/cache consumers.

## UI-UIIMAGE-007: LLUIImage::draw(x,y,color)

Source: [lluiimage.inl](../../../indra/llrender/lluiimage.inl#L27).
**1.** Calls width/height getters and sized draw. C++ argument evaluation does not
establish an order between width and height calls here. **2.** Native CPU natural-size
image preparation. **3.** Explicit layout extents before native submission, retaining
correct clipped dimensions. Check lazy dimension effects and fractional clip.
Outgoing: both virtual getters and sized draw.

## UI-UIIMAGE-008: LLUIImage::draw(x,y,w,h,color)

Source: [lluiimage.inl](../../../indra/llrender/lluiimage.inl#L32).
**1.** gl_draw_scaled_image_with_border with image, color, solid=false, clip/scale
regions, scale_inner iff style==INNER. Any other style value selects false.
**2.** Native CPU nine-slice or full-region geometry and typed sampled image/color.
**3.** Prepare geometry under exact scaling/rounding contract rather than invoke GL
helper. Check INNER/OUTER, full scale region and tint/coverage.
Outgoing: image pointer conversion, border helper and geometry/shader state.

## UI-UIIMAGE-009: LLUIImage::drawSolid(x,y,w,h,color)

Source: [lluiimage.inl](../../../indra/llrender/lluiimage.inl#L45).
**1.** Same helper/regions/style, solid=true. Still passes sampled image; not an
untextured rectangle. **2.** Native solid-color/texture-coverage shader contract.
**3.** Select dedicated native material only after solid shader/alpha semantics close.
Check image RGB versus alpha influence and shader restoration. Outgoing: border
helper, gSolidColorProgram and shader loader/ABI.

## UI-UIIMAGE-010: LLUIImage::drawBorder(x,y,w,h,color,border)

Source: [lluiimage.inl](../../../indra/llrender/lluiimage.inl#L58).
**1.** Set rect origin/size; stretch both axes by border; drawSolid entire enlarged
rect. Does not subtract inner area or draw an outline itself. **2.** Native image-
shaped solid decoration with enlarged geometry. **3.** Preserve image coverage,
not substitute a generic stroke. Check positive/negative border and overlap.
Outgoing: rect methods, drawSolid(rect) facade, solid helper.

## UI-UIIMAGE-011: LLUIImage::setClipRegion

Source: [lluiimage.h](../../../indra/llrender/lluiimage.h#L58).
**1.** Assign region only; no cached dimension invalidation or callback. **2.** CPU
image-view metadata. **3.** Prepared native snapshots need explicit metadata version;
preserve measured layout effects of cached versus live getters.
Check clip update after constructor and pending draws. Outgoing: region consumers.

## UI-UIIMAGE-012: LLUIImage::setScaleRegion

Source: [lluiimage.h](../../../indra/llrender/lluiimage.h#L63).
**1.** Assign region only. **2.** CPU nine-slice policy. **3.** Version prepared image
geometry when scale region changes; no automatic GPU sampler change.
Check asymmetric/invalid region and retained geometry. Outgoing: draw consumers.

## UI-UIIMAGE-013: LLUIImage::setScaleStyle

Source: [lluiimage.h](../../../indra/llrender/lluiimage.h#L68).
**1.** Assign enum only, no validation/notification. **2.** CPU scaling mode.
**3.** Explicit native preparation policy, not shader ambient state.
Check invalid enum and INNER/OUTER geometry. Outgoing: sized draw/style consumers.

## UI-UIIMAGE-014: LLUIImage::addLoadedCallback

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L111).
**1.** Lazy new signal if null, connect supplied slot and return connection. No
already-loaded immediate invocation in this body. **2.** CPU image availability
subscription. **3.** Separate logical load callback from GPU-ready publication;
native readiness cannot be inferred from registration success.
Check register after load, multiple subscribers, disconnect and callback ownership.
Outgoing: signal/slot lifetime, provider callers and onImageLoaded timing.

## UI-UIIMAGE-015: LLUIImage::onImageLoaded

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L121).
**1.** If signal exists invoke no-argument slots; no loaded flag, one-shot clear,
dimension-cache refresh or synchronization. **2.** CPU event delivery. **3.** Preserve
actual repeated-event policy; native GPU version publication needs separate evidence.
Check repeated call, slot mutation/deletion and no subscribers. Outgoing: every slot
and calling thread, image lifetime during dispatch.

## UI-UIIMAGE-016: ParamValue<LLUIImage*>::updateValueFromBlock

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L131).
**1.** Nonempty name not 'none' -> set vk_image_name(name,true). name=='none' ->
updateValue(null), return; does NOT clear prior vk_image_name in this body. Otherwise
Render2D singleton getUIImage(name); nonnull -> updateValue(image), null leaves prior
value unchanged. Native-name metadata capture does not remove provider lookup.
**2.** CPU semantic image resolution with explicit 'none', missing and available
outcomes; native resource publication separate. **3.** Neutral image identity/value
must preserve override semantics, not substitute vk_image_name as authoritative
without reconciling stale aliases and pointer fallback.
Check valid->none, valid->missing, empty name, late provider load and template default.
Outgoing: CustomParamValue/name methods, Render2D provider lookup, pointer ownership.

## UI-UIIMAGE-017: ParamValue<LLUIImage*>::updateBlockFromValue

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L155).
**1.** Null value -> name.set('none',make_block_authoritative), else name.set(image
name,flag). Does not assign vk_image_name here. **2.** CPU image value/schema
round-trip. **3.** Explicit authoritative direction prevents stale parallel metadata
from deciding native appearance. Check null/non-null, flag true/false and alias.
Outgoing: getValue/getName, scalar set/provenance and custom value synchronization.

## UI-UIIMAGE-018: ParamCompare<LLUIImage*>::equals

Source: [lluiimage.cpp](../../../indra/llrender/lluiimage.cpp#L168).
**1.** Both null -> false; otherwise pointer equality. Thus even two null values
are deliberately unequal for export/default comparison. **2.** CPU schema comparison,
not GPU identity equality. **3.** Keep export comparison distinct from native image
cache keys. Check null/null, same/different wrapper and equal names/different pointers.
Outgoing: serializer consumers and pointer lifetime.

## UI-PRIMITIVE-001: gl_draw_scaled_image_with_border(region overload)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L364).
**1.** stop_glerror; null image warns/returns before shader change. solid_color binds
gSolidColorProgram. Exactly full center region (L0,R1,B0,T1) uses gl_draw_scaled_image;
otherwise nine-slice path below. Finally solid_color binds gUIProgram, NOT previous
shader. No exception-safe shader restoration.

Nine-slice setup, in source order:

- Read UI scale; translation=(current UI translation+(x,y,0))*scale.
- uvWidth/Height=outer region spans; center UV edges = outer left/bottom plus each
   normalized center edge times corresponding span (top uses center.top).
- imageWidth/Height=image->getWidth/Height(0) as floats; naturalWidth/Height=ll_round
   (source extent*outer span). Initial draw center edges=center UV edges*source extent;
   no subtraction of outer UV origin from these center coordinates in this body.
- INNER: add output width-naturalWidth to right and height-naturalHeight to top.
   shrinkWidth=max(0,left-right); shrinkHeight=max(0,bottom-top). Axis shrink ratio=0
   if center span exactly1 else shrink/(naturalSize*(1-centerSpan)). borderShrinkScale=
   1-max(axis ratios). Multiply left/bottom by this factor; right=lerp(outputWidth,
   oldRight,factor), top=lerp(outputHeight,oldTop,factor). No final clamp of factor.
- OUTER: factor=min(outputWidth/drawCenter.width,outputHeight/drawCenter.height,1).
   Scale center width/height; set its center to (centerUV.centerX*outputWidth,
   centerUV.centerY*outputHeight). No zero-denominator/finite/region-validity guards.
- Round each draw center edge individually after translation+edge*UI scale. Outer
   draw edges are translation and translation+outputSize*scale, WITHOUT ll_round.
- Force-bind image at unit0, set color, use thread_local UV/position arrays54 elements;
   begin TRIANGLES, populate all vertices below, vertexBatchPreTransformed(pos,uv,54),
   end. Every position z=0; inherited color carried by batch overload.

Exact emitted vertices: define outer edges OL/OR/OB/OT and center edges CL/CR/CB/CT.
Each pair below refers to corresponding draw-position edge AND UV edge; order is
the actual 54-entry stream, not a claimed generic nine-patch equivalence.

| Quad | First triangle | Second triangle |
|---|---|---|
| bottom left | (OL,OB),(CL,OB),(CL,CB) | (OL,OB),(CL,CB),(OL,CB) |
| bottom middle | (CL,OB),(CR,OB),(CR,CB) | (CL,OB),(CR,CB),(CL,CB) |
| bottom right | (CR,OB),(OR,OB),(OR,CB) | (CR,OB),(OR,CB),(CR,CB) |
| middle left | (OL,CB),(CL,CB),(CL,CT) | (OL,CB),(CL,CT),(OL,CT) |
| middle | (CL,CB),(CR,CB),(CR,CT) | (CL,CB),(CR,CT),(CL,CT) |
| middle right | (CR,CB),(OR,CB),(OR,CT) | (CR,CB),(OR,CT),(CR,CT) |
| top left | (OL,CT),(CL,CT),(CL,OT) | (OL,CT),(CL,OT),(OL,OT) |
| top middle | (CL,CT),(CR,CT),(CR,OT) | (CL,CT),(CR,OT),(CL,OT) |
| top right | (CR,CT),(OR,CT),(OR,OT) | (CR,CT),(OR,OT),(CR,OT) |

**2.** CPU native image geometry from explicit region/extent/style/transform; native
GPU consumes vertices, typed image version and color/coverage material. **3.** Prefer
audited pure geometry preparation with explicit resource leases over either LLRender
translation or unverified library nine-patch defaults. Retain reference arithmetic
where defined; invalid sizes/NaNs require explicit policy, not undefined emulation.
Checks: full-region shortcut, asymmetric clipping, both styles, below-border sizes,
nonuniform/fractional UI scale and offset, center versus outer rounding, all54
positions/UVs, shader selection/restoration and missing image. Discrete stream order
can be exact; numeric/pixel tolerance remains to be established before comparison.
Outgoing: texture dimension virtuals, UI transform getters, rect/math/round helpers,
bind/color/begin/batch/end, gl_draw_scaled_image and both shader program contracts.

## UI-PRIMITIVE-002: gl_draw_scaled_image_with_border(pixel borders)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L348).
**1.** Null image warns/returns. Compute fractions borderWidth/imageWidth0 and
borderHeight/imageHeight0. scaleRect=(fx,1-fy,1-fx,fy). Delegate region overload
with input UV and style. No zero extent guard. **2.** CPU pixel-border to normalized
region conversion. **3.** Preserve source-natural units, not output/DPI pixel widths.
Check non-square source and excessive borders. Outgoing: dimension getters/helper.

## UI-PRIMITIVE-003: gl_draw_scaled_image

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L338).
**1.** Null image warns/returns; else gl_draw_scaled_rotated_image with degrees0 and
default target null. **2.** Native CPU image quad preparation. **3.** Close rotated
helper's zero-angle path before assuming generic rect vertices match rounding.
Check null and supplied output dimensions. Outgoing: rotated helper/diagnostics.

## UI-PRIMITIVE-004: gl_draw_image

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L323).
**1.** Null warns/returns; else rotated helper with live image width0/height0,
degrees0, supplied UV. Does not use LLUIImage clipped natural size. **2.** Native
full-source natural-size geometry. **3.** Preserve distinction from LLUIImage draw.
Check clipped UV with full source dimensions. Outgoing: virtual dimensions/helper.

## UI-PRIMITIVE-005: gl_rect_2d(coordinates,filled)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L118).
**1.** Unbind unit0 TT_TEXTURE. Filled -> TRIANGLES:
(left,top),(left,bottom),(right,bottom),(left,top),(right,bottom),(right,top).
Unfilled -> top--,right--; LINE_STRIP:
(left,top),(left,bottom),(right,bottom),(right,top),(left,top). end after either.
No color/shader/line-width change in body; no invalid/empty rect early return.
**2.** Native solid fill or outline geometry with explicit pipeline/material.
**3.** Preserve integer edge adjustment and inherited color; line rasterization
needs selected-device coverage decision rather than treating outline as filled quad.
Check zero/negative rect, filled flag, winding, UI transforms, bound texture effect
and current shader. Outgoing: LLTexUnit::unbind, begin/vertex2i/end, inherited state.

## UI-PRIMITIVE-006: gl_rect_2d(coordinates,color,filled)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L149).
**1.** color4fv then coordinate overload. **2.** Native explicit vertex color.
**3.** Preserve float-to-byte conversion before drawing, not arbitrary float tint ABI.
Check out-of-range alpha/color. Outgoing: color4fv and coordinate overload.

## UI-PRIMITIVE-007: gl_rect_2d(rect,color,filled)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L156).
**1.** color4fv then coordinate overload with L,T,R,B. **2.** CPU rectangle extraction
and native explicit color. **3.** Same primitive contract, no matrix or clipping
change implied by LLRect. Check extents/alpha. Outgoing: color and rectangle helper.

## UI-PRIMITIVE-008: gl_rect_2d_offset_local(coordinates,offset,filled)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L100).
**1.** Push gGL UI matrix; add integer font origin X to left/right and Y to bottom/top;
load UI identity. Call rect with floor(edge*global UI GL scale), subtract pixel_offset
from left/bottom and add to right/top. Pop UI matrix. Does not push/reset font origin
stack here. **2.** CPU device-pixel offset rectangle preparation. **3.** Distinguish
integer font origin/global display scale from floating UI transform; a single matrix
substitution would change rounding. Check fractional translations/scales, offset0,
negative offset and outline decrement. Outgoing: UI stack, font origin, llfloor,
rect helper and arithmetic overflow preconditions.

## UI-PRIMITIVE-009: gl_rect_2d_offset_local(coordinates,color,offset,filled)

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L94).
**1.** color4fv then offset overload. **2.** CPU explicit tint and pixel geometry.
**3.** Native prepared color must reflect reference byte conversion. Check inherited
color after draw and failed geometry. Outgoing: color helper/offset overload.

## UI-PROVIDER-001: LLRender2D::getUIImage

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1827).
**1.** Nonempty name AND provider -> provider->getUIImage(name,priority); otherwise
null. Priority default/caller matters. **2.** CPU skin image declaration/asset lookup
with native GPU publication separate. **3.** Audit selected provider and asynchronous
callbacks rather than reuse this facade as proof of neutral resources.
Check empty/missing provider, cache miss, alias and priority. Outgoing: concrete
provider method, returned LLPointer ownership and provider installation/removal.

## UI-PROVIDER-002: LLRender2D::getUIImageByID

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1815).
**1.** Provider present -> getUIImageByID(id,priority), otherwise null. No empty-ID
guard here. **2.** CPU asset identity resolution. **3.** Preserve ID lookup independent
of skin alias names and readiness. Check null UUID/missing provider.
Outgoing: provider concrete target and pointer/callback lifetime.

## UI-PROVIDER-003: LLRender2D::resetProvider

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1836).
**1.** If Render2D instance exists set provider pointer null; otherwise do nothing.
No deletion/cancellation in body. **2.** CPU detach from image service. **3.** Native
lifecycle must separately drain callbacks/retire resources; detachment is not teardown.
Check late callbacks and lookups after reset. Outgoing: caller/provider ownership.

## UI-PRIMITIVE-010: gl_draw_scaled_rotated_image

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L712).
**1.** Both image/target null warns/returns. If image nonnull force-bind image,
otherwise bind target. Set color. Degrees exactly0 uses thread_local arrays7 slots,
begin TRIANGLES, read scale/translation, add x/y then component scale translation;
scaledWidth/Height=ll_round(outputSize*axisScale) as S32. Emit six vertices/UV:
RT,LT,LB,RT,LB,RB, where position origin is scaled translation (not rounded), far
edges add rounded extents; z0. Pretransformed batch6, end. This is a different
rounding placement from nine-slice outer edges.

Nonzero degrees: push UI matrix; translate(x,y,0). offsetX=float(width/2) and
offsetY=float(height/2), with integer division before conversion. Translate offsets;
construct LLMatrix3(0,0,degrees*DEG_TO_RAD). Bind image/target and set color AGAIN.
begin TRIANGLES; form vectors (+ox,+oy),(-ox,+oy),(-ox,-oy),(+ox,+oy),(-ox,-oy),
(+ox,-oy), each multiplied by matrix. Set corresponding RT,LT,LB,RT,LB,RB UV before
vertex2f(rotatedXY); end; pop UI matrix. No local matrix/texture/uniform restoration
beyond UI pop, zero-size/finite validation or exception scope guard. Odd output sizes
lose one unit of extent in the nonzero-angle geometry because of integer half-size.
**2.** CPU native image quad preparation with explicit rotation pivot and rounding;
GPU consumes typed image/target snapshot and explicit transform/material.
**3.** Preserve defined zero/nonzero-angle distinction and target preference, not a
generic center-rotation formula silently replacing integer halves. Invalid input
policy must be explicit, and attachment target sampling needs NV-13 dependencies.
Checks:0 versus small nonzero angle, odd dimensions, nonuniform scale, fractional
translation, both image+target set, target-only, texture matrix and shader state.
Outgoing: image/target bind overloads, color, UI stack/getters/translate, ll_round,
LLMatrix3 Euler ctor/vector multiply, begin/texCoord2f/vertex2f/batch/end.

## UI-PRIMITIVE-011: gl_draw_scaled_target

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L333).
**1.** Delegate rotated helper degrees0, image null, supplied target. No target
guard in facade. **2.** Native prepared sample of a rendered product. **3.** Target
identity must include output version, layout and completion; GL target reuse is
not native resource dependency declaration. Check null/feedback/resize cases.
Outgoing: rotated helper and render-target producer/sample contract.

## UI-PRIMITIVE-012: gl_draw_rotated_image

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L707).
**1.** Delegate with image width0/height0 and degrees; dereferences image before
callee null check. **2.** Native CPU natural-size rotated quad. **3.** Explicit
metadata validity, not null image undefined behavior. Check natural odd dimensions
and null precondition. Outgoing: texture getters/rotated helper.

## UI-PRIMITIVE-013: LLTexUnit::unbind

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L415).
**1.** Debug check; index<0 returns. Otherwise always gGL.flush then activate.
If current texture type equals requested type: set cached current texture0; bind
sWhiteTexture for TT_TEXTURE, otherwise GL name0; debug check. Type mismatch still
flushes/activates but does not alter binding/cache. Does not clear current type.
**2.** Native solid primitives need explicit white sample or untextured material,
not 'no texture bound'. **3.** Pick a native material whose shader output equals
reference white-texture sampling under relevant UV/sampler state; close white image
creation first. Cached zero and actual GL white name are distinct.
Check type mismatch, dummy unit, pending geometry, white texture contents/sampler
and retained list capture of cached name0. Outgoing: flush/activate, white texture
owner/setup, type mapping and retained-list replay.

## UI-PRIMITIVE-014: LLRender::color4fv

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1978).
**1.** Pass four array values to color4f; no pointer/length check. **2.** CPU tint
capture. **3.** Native explicit color must preserve callee quantization.
Check input order/lifetime. Outgoing: color4f, source validity.

## UI-PRIMITIVE-015: LLRender::color4f

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1970).
**1.** Clamp each float0..1, multiply255, C-cast GLubyte, color4ub. Conversion
truncates nonnegative finite values; no rounding-to-nearest. NaN handling depends
on clamp/conversion and is not defined here as a compatibility mandate.
**2.** CPU color quantization before native vertex/uniform packing. **3.** Preserve
UNORM8 precision at producer boundary, not silently improve to arbitrary float tint.
Check0,0.5 ->127,1, out-of-range finite values and subsequent normalized shader input.
Outgoing: llclamp semantics, color4ub and invalid-input policy.

## UI-PRIMITIVE-016: LLRender::color4ub

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1954).
**1.** If no shader OR shader attribute mask includes COLOR, set current mColorsp
slot to bytes. Otherwise diffuseColor4ub, leaving vertex color slot unchanged.
No explicit flush in this body. **2.** Native explicit per-vertex versus draw-uniform
color material. **3.** Capture correct color domain for each shader; solid UI shader
uses uniform, ordinary UI uses attribute. Preserve state timing across queued draws.
Check no shader, COLOR mask, solid shader, shader swap and inherited vertex color.
Outgoing: shader attribute-mask producer, color copy, diffuseColor4ub/uniform flush.

## UI-PRIMITIVE-017: LLRender::diffuseColor4ub

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L2048).
**1.** Read shader pointer/assert nonnull; if pointer call uniform4f(DIFFUSE_COLOR,
r/255.f,g/255.f,b/255.f,a/255.f). No direct flush here; uniform helper owns any
pending-work ordering. **2.** Native draw uniform from normalized quantized bytes.
**3.** Keep uniform snapshot with draw/material identity, not mutable shared color.
Check null release-build branch, quantized midpoint and pending draw update.
Outgoing: uniform4f overload, reserved uniform map, shader lifetime and uniform cache.

## UI-SOLID-001: solidcolorV.glsl main

Source: [solidcolorV.glsl](../../../indra/newview/app_settings/shaders/class1/interface/solidcolorV.glsl#L34).
**1.** Position=MVP*vec4(position.xyz,1); UV=texcoord0 directly. No color attribute
or texture matrix. **2.** Native solid-image coverage vertex shader with explicit ABI.
**3.** Separate this material from ordinary UI texture-matrix behavior; geometry may
share buffer layout but shader semantics cannot be assumed identical.
Check nonidentity texture matrix has no effect, MVP/clipping and ABI attributes.
Outgoing: prelude/variant loader, uniform/attribute binding and raster state.

## UI-SOLID-002: solidcolorF.glsl main

Source: [solidcolorF.glsl](../../../indra/newview/app_settings/shaders/class1/interface/solidcolorF.glsl#L35).
**1.** alpha=texture(tex0,UV).a*color.a; frag_color=(color.rgb,alpha). Sample RGB
ignored, output RGB not multiplied by sampled alpha. No discard/gamma/depth logic.
**2.** Native coverage image plus uniform tint, with explicit downstream blending.
**3.** Preserve straight-color coverage contract; replacing with vertexColor*texture
or premultiplied output changes results. Check colored texture with same alpha,
transparent texel RGB, tint/alpha, blend and output-format encoding.
Outgoing: sampler/image swizzle, uniform mapping, blend factors/equations and target.

## UI-SOLID-003: gSolidColorProgram creation block

Source: [llviewershadermgr.cpp](../../../indra/newview/llviewershadermgr.cpp#L3347).
Local block only; owning loadShadersInterface function remains open.
**1.** Prior success gates block. Name Solid Color Shader; clear files, add solidcolorV
vertex and solidcolorF fragment, interface level, assign createShader result.
Success -> bind, uniform1i(sTex0,0), unbind. Failure skips sampler initialization.
**2.** Native reproducible shader ABI/material construction. **3.** Explicit sampler
binding and error propagation, not GL texture-unit number as native ABI.
Check compile/link failure, actual sTex0 mapping and interface-level variant.
Outgoing: createShader/bind/uniform1i/unbind, loader and owning function branches.

### Nine-slice static validation

A source-assignment comparison matched all54 position and UV edge pairs, in order,
against the UI-PRIMITIVE-001 table. The check parsed the assignments in the named
helper only and compared both streams independently. Initial documentation-row
matching omitted CRLF handling; rerun with CRLF-aware matching passed. This verifies
table transcription, not arithmetic, raster output, runtime reachability or parity.

## UI-COLOR-001: LLColor4::operator LLColor4U

Source: [v4color.cpp](../../../indra/llmath/v4color.cpp#L128).
**1.** For each RGBA float: multiply255, ll_round, llclampb, castU8; construct
LLColor4U from four resulting bytes. This is used by LLColor4U text_color(color)
in font rendering; differs from gGL.color4f's truncation after clamp. Finite normalized
0.5 maps128 here versus127 in gGL.color4f, subject to ll_round implementation audit.
**2.** CPU text/shadow color encoding. **3.** Native text and image primitive producers
must preserve their respective quantization; one universal byte conversion silently
changes a path. Numeric conversion is not sRGB transfer or premultiplication.
Check0,.5,1, halfway boundaries, finite out-of-range, exceptional inputs and glyph
alpha. Outgoing: ll_round, llclampb, U8 conversion range and byte-color constructor.

## UI-COLOR-002: LLColor4U(r,g,b,a)

Source: [v4coloru.h](../../../indra/llmath/v4coloru.h#L150).
**1.** Assign four bytes to RGBA indices; no arithmetic or callbacks. **2.** CPU
explicit packed color value. **3.** Suitable neutral value after ABI/index/type
layout verification; no renderer-owned color class required.
Check byte order/sizeof and normalized GPU vertex format. Outgoing: index constants,
array layout and later serialization/GPU consumers.

## UI-COLOR-003: LLColor4(const LLColor4U&)

Source: [v4color.cpp](../../../indra/llmath/v4color.cpp#L144).
**1.** Each byte times constexpr float(1/255) into corresponding float channel.
**2.** CPU normalized byte interpretation. **3.** Preserve explicit UNORM conversion,
not sRGB decode; numerical equivalence of shader UNORM depends on consumer precision.
Check0,127,128,255 and CPU/GPU normalized values. Outgoing: channel layout/consumers.

## UI-COLOR-004: operator%(color,scalar)

Source: [v4color.h](../../../indra/llmath/v4color.h#L503).
**1.** Return new LLColor4(original RGB,original alpha*scalar). No clamp or RGB
premultiplication. **2.** CPU inherited opacity composition for panel/image tint.
**3.** Keep straight RGB and alpha multiplication separate from blend state; native
premultiplied representation would require coordinated shader/blend changes.
Check RGB unchanged, alpha outside0..1 and later path-specific quantization.
Outgoing: float-color constructor, scalar inputs and color consumers.

## UI-COLOR-005: operator%(scalar,color)

Source: [v4color.h](../../../indra/llmath/v4color.h#L497).
**1.** Same new color with alpha*scalar and unchanged RGB. **2.** CPU opacity.
**3.** Same straight-alpha contract; operand order does not imply vector scaling.
Check exact channels/NaN policy. Outgoing: constructor and caller scalar provenance.

## UI-COLOR-006: LLColor4::calcHSL

Source: [v4color.cpp](../../../indra/llmath/v4color.cpp#L340).
**1.** Read RGB (alpha ignored); nested comparisons compute minimum/maximum, delta=
max-min; L=(max+min)/2,H=S=0. delta==0 ->H=S=0. Otherwise S=delta/(max+min) if L<.5,
else delta/(2-max-min). delR/G/B=((max-component)/6+delta/2)/delta. Red>=max ->
H=delB-delG; else green>=max ->1/3+delR-delB; else blue>=max ->2/3+delG-delR.
H<0 adds1; H>1 subtracts1 (two independent tests). Assign only nonnull hue,
saturation,luminance output pointers. All arithmetic still executes regardless of
which outputs requested. No clamp, finite check or transfer-function conversion.
Font shadow requests only luminance, which here is HSL lightness, not weighted
physical luminance. **2.** CPU shadow-strength policy input. **3.** Preserve actual
HSL lightness equation for reference shadow gating; substituting linear RGB luminance
would visibly change shadow policy. Share audited neutral math or isolate equivalent
calculation after numeric behavior review, not a shader lighting function.
Check greys, primaries, equal-max ties, .35/.6 font thresholds, optional outputs,
out-of-range colors and exceptional arithmetic. Outgoing: float precision/input
validity and font clamp_rescale helper; no GL calls in local body.

## UI-NATIVEIMAGE-001: checkpoint LLVKUIImage::drawSolid

Source: [llvkuiimage.cpp](../../../indra/llvulkan/llvkuiimage.cpp#L411).
**1.** Current native body ignores name, clears sink texture and emits full solid
rectangle with supplied color. Source GL contract UI-UIIMAGE-009/UI-SOLID-002 instead
uses image alpha, supplied scale/clip regions and uniform RGB. Native source is
inspected debt, not an approved reference or implementation closure.
**2.** Native coverage material needs sampled image alpha multiplied by tint alpha,
unchanged tint RGB, correct prepared image geometry, and explicit downstream blend.
**3.** Prefer immutable prepared material identifying sampled-color versus
sampled-coverage semantics and a reproducible shader ABI. Do not add GL-call-shaped
dispatch or retain raw mutable sink state as the architectural owner. R1 lifecycle,
R2 resource publication/completion and pipeline ABI are prerequisites for integrated
replacement. Check a texture with alpha0/1 and distinct RGB under identical tint;
current native rectangle cannot match its coverage. GPU execution remains unmeasured.
Outgoing: sink rect/setTexture, its pipeline and white image, every caller, uploaded
image versions and completion owners. No native implementation edit made here.

## UI-NATIVEIMAGE-002: checkpoint LLVKUIImage::drawBorder

Source: [llvkuiimage.cpp](../../../indra/llvulkan/llvkuiimage.cpp#L393).
**1.** Lookup name; absent/not-ok/nonpositive decoded dimensions -> untextured rect
at original bounds. Otherwise border/source-size fractions, construct centered scale
rect, copy ImageRec including native texture handles, replace scale, draw9Slice at
UNCHANGED outer bounds. No outer expansion and no coverage-only shader selection.
GL UI-UIIMAGE-010 expands bounds then invokes drawSolid using existing scale region.
**2.** CPU expanded decoration geometry plus native coverage image material.
**3.** Share one audited native image-geometry producer for coverage/color consumers,
with explicit region and expansion inputs; do not reinterpret requested expansion
as a new source slice. Texture handles in copied records require ownership audit.
Check alpha-shaped image, asymmetric regions, positive/negative border, natural-size
and scaled callers. Outgoing: lookup/ImageRec ownership, draw9Slice, failure policy,
sink and pipeline. Static mismatch only; no parity measurement.

## UI-NATIVEIMAGE-003: checkpoint LLVKUIImage::draw

Source: [llvkuiimage.cpp](../../../indra/llvulkan/llvkuiimage.cpp#L366).
**1.** Static environment debug toggle/counter; lookup name and ok flag; first12
debug-enabled calls log result/size/rect. Missing or not-ok -> clear texture and draw
full tinted rectangle, return. Otherwise draw9Slice. Existing arbitrary solid fill
does not establish reference missing-asset behavior at each widget caller.
**2.** Native image identity/residency with explicit unavailable policy and diagnostic.
**3.** Asset service must publish typed readiness/error; prepared controls select
documented fallback geometry/material from actual reference policy. No success-shaped
solid fill as substitute for failed resource publication (NV-15).
Check missing alias, failed decode/upload and reference null image versus placeholder
provider behavior. Outgoing: provider/init, debug logging, map/lifetimes, draw9Slice.

## UI-NATIVEIMAGE-004: checkpoint draw9Slice

Source: [llvkuiimage.cpp](../../../indra/llvulkan/llvkuiimage.cpp#L163).
**1.** Normalize both outer axes by swapping edges; width/height<=0 returns. Read
clip/scale/style, white*input RGBA modulation; set image descriptor in sink. Exactly
full scale region emits one quad using supplied outer extents, no rounded scaled
extent. Otherwise decoded source dimensions and clip spans determine UV center;
rounded natural source dimensions; drawCenter=UVcenter*source dims. INNER adds output
minus natural extent, computes shrink ratios/factor, scales left/bottom and lerps
right/top. OUTER fits center extent with factor<=1 and sets relative center. Convert
local y-up drawCenter to sink y-down by y0+height-gy, x by x0+gx. NO ll_round on
converted center edges and no separate UI scale/translation input. Emit bottom,
middle,top bands, each left/center/right quad, via emitQuad.
The current caller-provided target extents alone cannot recover GL's separation of
local dimensions, UI scale, fractional translation and center-only rounding. Reference
UI-PRIMITIVE-001/010 specifies those different rounding locations explicitly.
**2.** CPU geometry preparation from complete semantic inputs, then one coherent
view-coordinate conversion for native execution. **3.** Prefer prepared vertex data
with explicit local extent/source extent/clip/scale style/UI transform and target
view policy. Avoid patching one rounding operation into an interface missing required
inputs. Negative extents/edge normalization policy needs reviewed consumer evidence.
Check asymmetric clips, fractional/nonuniform UI scales and offsets, degenerate/full
regions and rotated/negative bounds. Outgoing: emitQuad, math/rect helpers, source
dimension/region initialization, callers' coordinate conversion and sink material.

## UI-NATIVEIMAGE-005: checkpoint emitQuad

Source: [llvkuiimage.cpp](../../../indra/llvulkan/llvkuiimage.cpp#L125).
**1.** Sort position x/y bounds independently without swapping UVs. Construct six
vertices TL,TR,BR,TL,BR,BL (sink y-down) with corresponding UV corners and identical
RGBA copied into stack arrays. Environment-gated first24 calls log coords/UVs using
static counter. Call process-wide sink texturedBatchPreTransformed6. No tint byte
quantization here. **2.** CPU explicit native quad generation with coordinate and
color ABI. **3.** Select winding/UV orientation from prepared view policy; do not
silently discard requested mirroring by normalizing position only. Per-path tint
precision must preserve reference quantization before submission.
Check reversed bounds with non-symmetric image, fractional tint and nontrivial
clip. Outgoing: sink batch data copying/lifetime, debug globals, vertex ABI/pipeline.

## UI-NATIVEIMAGE-006: checkpoint modulate

Source: [llvkuiimage.cpp](../../../indra/llvulkan/llvkuiimage.cpp#L112).
**1.** Multiply all four channels component-wise into output array. Comment calls
this LLColor4 operator%, but actual operator%(color,scalar) only scales alpha;
this operation corresponds to color*color. draw9Slice calls it with white, so finite
inputs unchanged there. **2.** CPU color multiplication when that material requires
it. **3.** Keep typed tint and opacity operations distinct; misleading comment is
not evidence for reference equivalence. Check RGBA values and actual callers.
Outgoing: input/output bounds, float semantics and consumers.

## UI-IMAGE-001: LLImageGL(const LLImageRaw*, bool, bool)

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L481),
[signature/defaults](../../../indra/llrender/llimagegl.h#L107).

**1.** Initialize save-data pointer=0 and external=false, call init(usemipmaps,
allow_compression), setSize(0,0,0), register this in sImageList, increment sCount,
then createGLTexture(0,raw) with default to_create=true. No return-value test.
Thus the font-cache call (raw,false,false) disables mipmaps/compression, not GL
creation. A failed create may still leave a registered wrapper. Implicit refcount,
member and allocation/destruction behavior remains an obligation.
**2.** Native image metadata registration and resource creation must produce typed
availability/error, not imply success by object existence. **3.** Validate the image
description and source bytes before GPU publication; retain a native resource owner
separately from the logical font page.
Checks: create failure after registration, null/invalid raw, first page creation.
Outgoing: init, setSize, static registry/counter lifetimes, createGLTexture(raw),
all constructor failure unwinding and intrusive pointer consumers. Edges open.

## UI-IMAGE-002: LLImageGL::init

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L523).

**1.** LL_IMAGEGL_THREAD_CHECK optionally stores current thread ID. Initialize
memory/bind time=0; pick-mask pointer/dimensions=0; supplied mip flag; explicit
format=false; mask=false; needs-alpha/pick=true; alpha stride/offset=0; created=false;
name/dimensions=0; discard=-1; supplied compression flag; target=2D, bind type=texture;
has-mips=false; mip levels=-1; resident=0; components=0; max-discard=MAX_DISCARD_LEVEL;
options-dirty=true; wrap addressing; anisotropic filter; internal format=-1, primary
format=0, type=unsigned byte, byte-swap=false; optional DEBUG_MISS=false; category=-1.
Finally resolve mainloop WorkQueue instance into mMainQueue. The queue lookup is a
non-graphics service dependency, not a scalar default.
**2.** Native descriptors and owner scheduling can represent these responsibilities
without GL enums or global queue lookup. **3.** Separate sampled-image interpretation,
CPU alpha/pick metadata, resource availability and publication destination. Inject
the owner queue only where asynchronous completion is needed.
Checks: initialization without mainloop service; thread-check configurations;
interpretation of default filter/address/alpha. Outgoing: LLThread identity,
WorkQueue instance/weak-handle behavior and member initialization; edges open.

## UI-IMAGE-003: LLImageGL::createGLTexture(raw overload)

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1532).

**1.** Check active thread. Disabled GL warns/returns false before assertion;
otherwise assert GL initialized and call stop_glerror. Null/invalid raw sets
created=false and returns false. Negative requested discard uses current level
after assertion; cap to MAX_DISCARD_LEVEL. Shift raw width/height by discard to
obtain full dimensions; setSize may destroy old GL texture. Failed setSize sets
created=false and returns false. Explicit RGBA with <4 components or RGB with <3
warns and clears explicit format. Without explicit format: components 1 ->
LUMINANCE8/LUMINANCE, 2 -> LUMINANCE8_ALPHA8/LUMINANCE_ALPHA, 3 -> RGB8/RGB, 4 ->
RGBA8/RGBA, all unsigned byte; other count logs fatal. Compute alpha stride/offset.
to_create=false destroys GL texture, assigns discard and bind time, created=false,
returns true. Otherwise set category, get raw pointer and forward to byte overload
with data_hasmips=false; return its result.
**2.** Native resource description and upload plan must separate actual/full extent,
discard, channel interpretation and intended usages. **3.** Validate checked extent
arithmetic and format compatibility; publish resource/data readiness explicitly.
The to_create=false path is metadata-only but still invokes GL destruction and is
not a reusable neutral helper.
Checks: disabled/uninitialized GL, invalid raw, negative/max discard, size changes,
each component count and explicit mismatch, metadata-only destruction. Outgoing:
thread/debug helpers, raw validation, setSize, alpha metadata, category and byte
overload. These remain open; no native format chosen merely by similar enum names.

## UI-IMAGE-004: LLImageGL::createGLTexture(byte overload)

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1631).

**1.** Check thread and record main-thread predicate. defer_copy makes input=null;
otherwise assert input. Negative discard resolves current after assertion, clamps
to [0,maxDiscard], then caps MAX_DISCARD_LEVEL. Main thread AND not deferred AND
existing name AND unchanged discard: optionally write output name and return
setImage(input,data_hasmips) immediately. Otherwise preserve old name; supplied
usename asserts main thread and selects it. Without supplied name, generate name,
bind with usename override, set base-level=0 and max-level=maxDiscard-discard.
Optionally expose new name to caller. Mip-enabled sets auto-generation true. Store
discard, call setImage with new name; false returns immediately without a local
cleanup of that new name. Set texture-unit mip state, address and filter, then
unbind. Not deferred: background calls syncToMainThread; main deletes differing
nonzero old name and assigns new. Deferred does neither publication branch.
Update memory accounting from getMipBytes(discard), update last-bind time, check
thread again and return true.
**2.** Native allocation, initialized image version, consumer publication and
retirement are separate dependencies. **3.** Completion-owned uploads can replace
same-name mutation and vendor-specific synchronization, while preserving when a
consumer observes the new dimensions/quality/data. Supplied/deferred publication
callers still require independent tracing.
Checks: same-size same-discard reuse, background/deferred creation, supplied name,
failed setImage cleanup, old/new users and output-name timing. Outgoing: thread
identity, generate/delete names, bind/texture parameters, setImage, sampler changes,
syncToMainThread and memory accounting. Edges open.

## UI-IMAGE-005: LLImageGL::setSubImage(raw overload)

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1233).

**1.** Unconditionally read raw data/width/height and forward all arguments to the
byte overload; no null raw guard. **2.** Native upload takes a bounded owned byte
view and explicit strides/extents. **3.** Require valid source ownership before
submitting work, not raw pointers whose byte lengths are absent from the ABI.
Check source-null and lifetime through deferred upload. Outgoing: raw accessors,
byte overload and source locking/lifetime; not closed.

## UI-IMAGE-006: LLImageGL::setSubImage(byte overload)

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1124).

**1.** Zero width OR height returns true before name/data validation. Choose supplied
name if nonzero else current; zero name or null data returns false. Not forced-fast,
zero x/y, region dimensions equal GL dimensions and source dimensions equal region:
call setImage(data,false,name), ignore its bool and finally return true. This is the
font-atlas whole-page update path.

Otherwise mip-enabled dumps/logs fatal; assert discard=0 and nonnegative x/y;
out-of-target or out-of-source region dumps/logs fatal. Set UNPACK_ROW_LENGTH to
source width; optional swap-byte flag=1. Compute pointer offset using region x/y
and components. bindManual(unit0,target,name), fatal on false. If stagger predicate
false, call glTexSubImage2D once; true calls sub_image_lines. Disable unit0 texture
type, optional swap-byte flag=0, row length=0, mark created=true, return true.
Pixel-store defaults are forced, not restored from saved incoming values. No local
exception/fatal rollback guarantees state restoration.

**2.** Native region updates require source offset/pitch and destination extent,
layout/access dependencies and validity checks. **3.** Use checked upload records
and versions or completion-gated mutation. Whole-page reallocation versus region
copy can differ as a mechanism only after consumer sampling/availability is preserved.
Check zero extent/null data ordering; full-update ignored failure; source/target
bounds; forced-fast; mip rejection; byte swap; context state after failure.
Outgoing: setImage, size/component accessors, dump/fatal/assert, GL pixel store,
bindManual/disable, stagger predicate, row helper and debug checks. Edges open.

## UI-IMAGE-007: LLImageGL::setImage(byte overload)

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L748).

**1.** Query compressed interpretation. Mip-enabled first unbinds, sets has-mips,
options-dirty and anisotropic filtering; otherwise clears has-mips. Bind unit0 with
optional name, ignoring bool. Branches:

1. Null data: setManualImage for level0 with current extent and supplied format/type;
   does not run alpha analysis or pick-mask update.
2. Mips + supplied mip data: iterate current..max discard inclusive, compute level,
   update mMipLevels=max, move pointer backward by each smaller mip's format byte
   count after the first level. Compressed uses glCompressedTexImage2D. Uncompressed
   optionally enables byte-swap, calls setManualImage with UNSIGNED_BYTE (not
   mFormatType), analyzes alpha at level0, updates pick mask on each level, then
   disables byte-swap.
3. Mips + no supplied mips + uncompressed + auto-generation: optional byte-swap;
   calculate mMipLevels via wpo2(max(width,height)); non-core sets GENERATE_MIPMAP;
   upload base, analyze alpha/update pick mask; reset byte-swap; core calls
   glGenerateMipmap. Non-core relies on the enabled legacy mode.
4. Mips + no supplied mips + uncompressed + manual generation: compute mip count;
   base uses input, later levels allocate nothrow width*height*components bytes;
   allocation failure deletes tracked buffers, marks created=false and returns
   false. Success calls LLImageBase::generateMip, uploads each level, analyzes alpha
   and updates pick mask only at base; frees previous allocated mip buffers,
   halves both dimensions each iteration, frees final allocated data.
5. Mips + compressed + no supplied mips logs fatal.
6. No mips: set mipLevels=0; compressed uploads one compressed level. Uncompressed
   optionally enables byte-swap, calls setManualImage, analyzes alpha, updates pick
   mask and resets byte-swap. Font atlas follows this uncompressed path.

After normal branches, stop_glerror, created=true and return true. Failure/assert
behavior, compressed detection and generated-mip edge dimensions require their
own contracts; this table does not assert their correctness.

**2.** Native subresources, source byte ranges, mip production and CPU alpha/pick
metadata must agree on format/discard identity. **3.** Separate CPU metadata from
GPU upload and use explicit image/subresource initialization. Do not reproduce
GL state bindings or auto-mipmap switches as a native API.
Checks: all six branches; reversed supplied-mip layout; odd/thin extents; allocation
failure; base/other-mip metadata side effects; native font whole-page preservation.
Outgoing: format/extent helpers, setManualImage, compressed GL upload, alpha analysis,
pick-mask update, wpo2, generateMip, texture-unit and error/profiling helpers. Open.

## UI-IMAGE-008: LLImageGL::setManualImage

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1319).

**1.** Core profile + GL version>=CONVERSION_SCRATCH_BUFFER_GL_VERSION:
ALPHA -> RED/R8 with swizzle (0,0,0,R); LUMINANCE -> RED/R8 with (R,R,R,1);
LUMINANCE_ALPHA -> RG/RG8 with (R,R,R,G). Swizzles target GL_TEXTURE_2D even though
target is an argument. Other formats unchanged. Core profile below threshold:
only UNSIGNED_BYTE formats transform. ALPHA expands (0,0,0,A), LUMINANCE_ALPHA
expands (L,L,L,A), LUMINANCE expands (L,L,L,255) into sManualScratch when pixels
nonnull; format/internal values change even for null input. No scratch bounds
check appears here. Non-core skips these conversions.

Compression is sCompressTextures AND allow_compression. If true, map RED/R8, RG/RG8,
RGB/RGB8, SRGB/SRGB8, RGBA/RGBA8, SRGB_ALPHA/SRGB8_ALPHA8, LUMINANCE/LUMINANCE8,
LUMINANCE_ALPHA/LUMINANCE8_ALPHA8, ALPHA/ALPHA8 to their corresponding compressed
internal formats; other internal formats warn and stay unchanged. Font-cache
allow_compression=false bypasses this mapping.

Call free_cur_tex_image bookkeeping. False stagger predicate: glTexImage2D once
with data. True: glTexImage2D with null data, then sub_image_lines only when data
nonnull. Call alloc_tex_image(width,height,internalFormat,1); debug checks surround
operations. The allocation counter call does not itself prove successful GL allocation.

**2.** Native sampled values must preserve luminance replication and alpha mapping;
pixel layout/encoding is distinct from upload strategy. **3.** Use a typed format
plus native view swizzle or explicit conversion, coordinated with shaders/samplers
and consumer tests. Do not inherit global scratch memory or implicit bound texture.
Checks: profile/version/format/type combinations; null source; exact sample values;
scratch capacity; compression on/off and unsupported mapping; upload failure.
Outgoing: scratch initialization, GL context/version state, format bookkeeping,
stagger/row helper, GL texture storage/parameters and debug failure policy. Open.

## UI-IMAGE-009: should_stagger_image_set

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1059).

**1.** Darwin returns !compressed && on_main_thread() && AMD. Other platforms
return !compressed && on_main_thread() && !Intel. Short circuits determine whether
thread/vendor predicates are read. **2.** Vulkan uploads need native command and
staging ownership, not reproduction of a GL-driver workaround. **3.** Keep equivalent
bytes/visibility and measure native batching separately; no vendor branch is
justified solely by this GL policy. Check resulting byte output in either GL upload
path before treating chunk sizes as unobservable. Outgoing: thread/vendor facts.

## UI-IMAGE-010: type_width_from_pixtype

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1036).

**1.** UNSIGNED_BYTE, BYTE and UNSIGNED_INT_8_8_8_8_REV return 1; UNSIGNED_SHORT/SHORT
return 2; UNSIGNED_INT/INT/FLOAT return 4. Default logs fatal; returns initial 0 if
execution resumes. Packed REV's returned 1 is interpreted together with component
count, not a general packed-element size. **2.** Native upload packing is CPU
format description. **3.** Use validated format-byte/block metadata rather than
reusing GL enums. Check each supported type with components and row length.
Outgoing: error policy; caller combination with dataFormatComponents.

## UI-IMAGE-011: sub_image_lines

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1072).

**1.** Compute components, type-width, row bytes=data_width*components*type-width,
and end-y=y_offset+height. Full width AND height divisible by32: batch=32, width>1024
selects8, else width>512 selects16; loop y from offset to end by batch, issue one
glTexSubImage2D(width,batch), advance pointer by rowBytes*batch. Otherwise loop each
row and issue height1 call, advance rowBytes. No source-length check. **2.** Native
buffer-image copy regions can encode row pitch and offsets explicitly. **3.** Bounded
staging uploads selected for Vulkan, not row-by-row translation. Check byte coverage,
pitch, threshold widths, partial width and unusual heights. Outgoing: components/type
helpers, arithmetic/input bounds, glTexSubImage2D and profiling scopes.

## UI-IMAGE-012: LLTexUnit::bind(LLImageGL*,bool,bool,S32)

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L284).

**1.** stop_glerror; negative unit returns false. Compute name from override or
texture->getTexName **before** null texture check. Null with zero override is
therefore not safely rejected by the later check. No name: recursively bind default
GL texture only if it and its name exist, else false. Changed name OR forceBind:
flush pending gGL work, activate unit, enable texture type, assign current name,
glBindTexture, update image bind stats, assign mip flag. Dirty options: clear dirty
flag then set address/filter. Unchanged name skips all of that even if options dirty.
Return true; for_rendering is unused in this overload.
**2.** Native recording binds an explicit descriptor/resource version whose use is
retained; no ambient texture unit or hidden geometry flush. **3.** Bindless or bound
descriptors are an implementation decision after shader/resource contracts, not
preserving LLTexUnit dispatch. Check cached same-name versus force bind, default
fallback, dirty options and pending geometry order. Outgoing: gGL.flush, activate,
enable, stats, sampler methods, default image lifecycle and GL call/error helpers.

## UI-IMAGE-013: LLImageGL::setFilteringOption

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1992).

**1.** Changed option sets dirty=true and stores option. Nonzero texture name AND
current active texture unit bound to that name: set unit filter immediately,
clear dirty and stop_glerror, even if the option value did not change. **2.** Native
sampler selection is explicit descriptor state. **3.** Version sampler interpretation
with the draw record; do not mutate descriptors still referenced by submitted work.
Check already-bound/unbound/current-unit mismatch and unchanged value. Outgoing:
getTexUnit/current-unit access, filter setter, pending draw flush and debug checks.

## UI-IMAGE-014: LLTexUnit::setTextureFilteringOption

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L484).

**1.** Negative unit OR zero current name OR multisample type returns. Otherwise
gGL.flush then setTextureFilteringOptionFast(option,currentType); it does not call
activate here. **2.** Native sampler parameters belong to explicit binding state.
**3.** Resolve sampler before immutable recording; no dependency on current GL unit.
Check guards and active-unit assumptions at every caller. Outgoing: flush/fast setter.

## UI-IMAGE-015: LLTexUnit::setTextureFilteringOptionFast

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L493).

**1.** MAG=NEAREST for POINT, LINEAR otherwise. MIN: option>=TRILINEAR and has-mips
uses LINEAR_MIPMAP_LINEAR; else option>=BILINEAR uses LINEAR_MIPMAP_NEAREST with mips
or LINEAR without; else uses NEAREST_MIPMAP_NEAREST with mips or NEAREST without.
If device anisotropy supported, global anisotropy enabled AND ANISOTROPIC option
sets device max, otherwise1. No local activation/validation of type/option.
**2.** Native sampler min/mag/mipmap modes and anisotropy negotiation. **3.** Cache
immutable sampler descriptions bounded by selected-device capabilities. Check each
filter/mip/anisotropy combination and effective font POINT behavior. Outgoing:
enum ordering, texture-type map, selected GL device/global setting producers.

## UI-IMAGE-016: LLImageGL::destroyGLTexture

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1943).

**1.** Check thread. Nonzero name: zero nonzero memory count, queue deleteTextures,
discard=-1, name=0, created=false. Zero name leaves those other fields unchanged.
**2.** Native logical unpublication is distinct from physical destruction.
**3.** Retire after completion of every resource use rather than frame-count delay.
Check zero/nonzero name, pending readers and deletion while GL disabled/uninitialized.
Outgoing: thread policy, deleteTextures and actual user lifetime tracking. Open.

## UI-IMAGE-017: LLImageGL::deleteTextures

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1305).

**1.** Only when GL manager initialized, append every supplied name to bucket
sFrameCount modulo4. No immediate GL delete. Uninitialized GL ignores the request.
**2.** Native retirement needs a completion token. **3.** A completed-use retire
queue, not copying the reference's three-frame-delay comment. Check bucket/duplicate
names, initialization state and callers during shutdown. Outgoing: updateClass,
free-list/thread ownership, vector allocation and global frame-count writers.

## UI-IMAGE-018: LLImageGL::updateClass

Source: [llimagegl.cpp](../../../indra/llrender/llimagegl.cpp#L1289).

**1.** Increment sFrameCount; choose (sFrameCount+3) modulo4. If bucket nonempty,
call free_tex_images bookkeeping, glDeleteTextures for all names, resize bucket0.
This code has no GPU fence check; OpenGL deletion semantics remain an external
contract. **2.** Native explicit completion-owned retirement. **3.** Release only
after all queues/users complete, with defined device-loss cleanup. Check actual
bucket age from caller sequence, frame wrap and shutdown drain. Outgoing: caller
schedule, free_tex_images, GL delete semantics and context lifetime.