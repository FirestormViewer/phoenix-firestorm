# UI frame and traversal dependencies

Source revision: 3abd661f498329babaf87b49ffab910fcb5f0e6c (implementation 90af5a7220).
Status: OPEN. No complete UI root set, dynamic-dispatch closure, runtime validation
or native implementation is claimed here. These records supplement
[construction/font dependencies](native-ui-dependencies.md),
[font lifecycle](native-ui-font-lifecycle.md) and
[image dependencies](native-ui-image-dependencies.md).

Each virtual call is an obligation to identify every applicable implementation and
its actual branches; listing the dispatch point does not stand in for those reads.
World/HUD/preview/media contributions reachable from UI rendering remain in scope.

## UI-FRAME-001: render_ui

Source: [llviewerdisplay.cpp](../../../indra/newview/llviewerdisplay.cpp#L2609).

**1.** Enter scene-time/CPU/GPU profiling and check GL states. Save current modelview.
If not snapshot, push GL matrix, load last modelview and publish it as current CPU
modelview. SceneMonitor needsUpdate: push matrix, setup2DRender, compare, setup3DRender,
pop. Always renderFinalize before HUD/UI composition. Draw HUD elements; if RLVa
SETOVERLAY behavior exists, run RlvOverlay visual effect; draw HUD attachments.
Create default and UI-default GL-state guards, disable pipeline lights. Query UI
debug-feature mask. Enabled: connected calls render_ui_3d; disconnected calls
render_disconnected_background. Disabled: renderAllForTimer. Enabled again:
LLHUDObject::renderAll then render_ui_2d. Disabled again: renderAllForTimer a second
time. Always setup2DRender, updateDebugText and drawDebugText inside guard scope.
Not snapshot: restore saved CPU modelview and pop matrix. Arguments zoom_factor
and subfield are not read in this body. No local exception-based stack restoration.

**2.** Native UI is an explicit ordered consumer of final world output and selected
HUD/overlay products; CPU debug/model updates must not hide in draw submission.
**3.** Separate preparation effects from view-pass scheduling with explicit matrix,
target, state and history inputs. Do not collapse the two timer calls or reinterpret
snapshot behavior without evaluating defined effects. No GL draw callbacks reused.
Checks: snapshot versus normal, monitor update, connected/disconnected, UI disabled,
RLVa overlay and effect ordering; exceptions/early failures in callees.
Outgoing: profiling, state checks/guards, current matrices, SceneMonitor methods,
setup2D/3D, renderFinalize, HUD functions, RLVa predicate/effect, disableLights,
UI mask, HUDObject methods, 2D/3D/disconnected roots, debug update/draw. All need
individual closure; a final screenshot cannot establish their temporal behavior.

## UI-FRAME-002: render_ui_3d

Source: [llviewerdisplay.cpp](../../../indra/newview/llviewerdisplay.cpp#L2835).

**1.** Enter pipeline GL-state guard/profiling. Debug check; bind gUIProgram, set
white color. Cached ShowAxes true -> draw_axes. Cached FSShowChatRangeSpheres true
-> drawChatRangeSpheres. Always renderSelections(false,false,true). UI debug mask
true -> render object beacons, reset beacons, add sun/moon beacons in order. False
-> renderAllForTimer. Final GL error check. The body does not unbind gUIProgram.
The comment about depth cleared by a prior HUD pool remains a caller dependency,
not independently verified here.
**2.** Native world-space UI contributions have explicit depth/view policy and
simulation/preparation effects. **3.** Prepared selections/beacons/axes cannot
consult ambient GL state; reset/add operations remain ordered CPU effects.
Checks: both settings, UI mask changes between caller and callee, depth policy and
beacon update order. Outgoing: GL-state guard, shader bind, cached controls,
draw_axes/range spheres, renderSelections, beacon/sky/HUD timers and error checks.

## UI-FRAME-003: render_ui_2d

Source: [llviewerdisplay.cpp](../../../indra/newview/llviewerdisplay.cpp#L2893).

**1.** Enter UI-default GL-state guard/profiling. Set polygon fill, setup2DRender,
read camera zoom/subregion and UI scale. Zoom>1: derive tile x/y with ceil(zoom),
subtract rounded scaled-window tile offsets from LLFontGL origin. Valid avatar AND
HUD zoom<0.98: bind UI shader, push, scale by UI scale, translate to half world-view
extent, scale HUD zoom, white color, draw unfilled outline, pop/unbind.

RenderUIBuffer enabled:

1. If sIsRectDirty: clear flag; bind mUIScreen; enable RGBA write; pad dirty rectangle
   by8 each side. Enable scissor state. Function-static last_rect initializes from
   this first padded rectangle. Union last_rect with current dirty, save current
   into t_rect, set global dirty to union, replace last_rect with t_rect.
2. Divide last_rect coordinates by UI scale and cast to coordinate type; construct
   local clip_rect from it. **No glScissor or LLScreenClipRect construction appears
   in this block.** glClear(COLOR), viewer draw, then exit scissor guard.
3. Flush UI target, disable alpha writes (RGB true), restore global dirty=t_rect.
   If initial dirty flag false, skip all update steps.
4. Disable cull/blend via guards; bind UI target; emit a four-vertex TRIANGLE_STRIP
   over scaled window extent with texture coordinates 0..width/height, white color.

RenderUIBuffer disabled: call viewer draw directly. Finally reset font origin to
(0,0), not its incoming value. Failure in called methods has no local recovery.

**2.** Native retained UI needs explicit initialized attachment contents, dirty
regions, framebuffer/sample coordinates, load/store and final composition. **3.**
A full redraw or incremental scheme is a design choice only after preserving
visible results, draw-time side effects and history. Do not copy the assumed scissor
behavior from the comment; trace incoming state and actual target type first.
Checks: buffer on/off, dirty first/subsequent/false, zoom tiles and origin reset,
avatar outline, alpha writes, target sampling coordinates, scissor state at clear.
Outgoing: state guards, polygon state, setup2DRender, camera/scale/rect conversion,
font origin, HUD zoom predicates, gl_rect_2d, retained target bind/flush/bind sampler,
gGL geometry/shader and viewer draw. Each remains an explicit unresolved dependency.

## UI-FRAME-004: LLViewerWindow::draw

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L2985).

**1. Ordered local behavior:**

1. Set LLView::sIsDrawing=true, debug check, set line width1, select modelview and
   load identity. If not RenderUIBuffer set dirty rectangle to scaled window rect.
2. Cached DisplayTimecode: load identity, format gFrameTime, obtain SansSerif,
   render centered-window-minus100/top-minus60 text with Left/Top white style.
3. Bind UI shader, white color; push GL matrix and LLUI matrix. Obtain camera;
   scaleUI by display scale; save LLUI scale. Zoom>1 computes tile coordinates,
   applies GL translation and zoom scaling, multiplies LLUI scale by zoom.
4. Always call current tool's virtual draw before the root widget tree.
5. Mouselook AND FSMouselookCombatFeatures: read world extent, translated unknown
   name, palette and cached IFF settings/offset/alignment; get global camera position,
   conjugate camera quaternion by negating xyz; fetch avatar IDs/positions in range.
6. Nonempty avatars: iterate, skip self ID, skip null position. Obtain contact-set
   color, apply friend-color policy, override with netmap mark color when available.
   If renderIFF true, draw tracker marker.
7. Mouselook AND no crosshair label yet AND no RLVa SHOWNAMES: transform relative
   target position by quaternion, remap components (-y,z,x). Strict bounds x within
   (-0.75,0.75), z>0, y within(-1.5,1.5) select target. Resolve cached name or unknown;
   render name/distance via SansSerifBold with configured alignment/offset, Top,
   Bold/soft shadow. Mark crosshair rendered. If IFF disabled and label rendered,
   break avatar loop; otherwise keep iterating.
8. FSShowMouselookInstructions AND (mouselook OR free-camera mode) invokes instruction
   draw and debug check. Draw mRootView unconditionally next.
9. Debug rects -> sticky tooltip rect. Visible nonnull top control -> local origin
   to screen, select modelview, push/translate, call its virtual draw, pop. This
   draw does not use drawChildren's root/dirty-overlap gates.
10. Global overlay-title enabled AND nonempty title -> SansSerifBig centered/top
    text at height-20 with white alpha0.4.
11. Restore LLUI scale, pop UI and GL matrices, unbind shader, set sIsDrawing=false.

There is no local exception unwind restoring the drawing flag/scales/matrix stacks.
Root/top/tool virtual targets and avatar policy effects are not transitively closed.

**2.** Native UI preparation includes tool overlays, world-derived labels, root
widgets and topmost controls in that order. Transforms, scale, font requests and
opacity belong to immutable prepared data, while name/contact/model queries remain
CPU service responsibilities. **3.** One native preparation owner composes the
distinct contributions; it must not call this method or its GL-coupled virtual
targets during recording. Do not omit non-widget contributions to obtain a clean
login screenshot.
Checks: every listed predicate, empty/root/top/tool variation, avatar threshold
boundaries, RLVa hiding, contact colors, tiled snapshots, failure state restoration.
Outgoing: all named methods/settings/signals, math helpers, GL matrix/UI stack,
every virtual tool/root/top draw implementation, fonts, translation/name services,
tracker/netmap/contact sets and debug helpers. All unresolved targets remain open.

## UI-FRAME-005: LLViewerWindow::drawDebugText

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L2967).

**1.** Bind UI shader/white, push GL and UI matrices, scaleUI by display scale,
call mDebugText->draw, pop both, flush gGL, unbind shader. No debug-text-null guard.
**2.** Native prepared debug overlay with explicit scale and order. **3.** Keep its
format/update state out of GPU recording; retain the same placement and eligibility.
Checks: display scale, pending geometry order and debug overlay empty/content.
Outgoing: DebugText::draw, shader/matrix/flush owners; edges open.

## UI-FRAME-006: LLView::draw

Source: [llview.cpp](../../../indra/llui/llview.cpp#L1288).

**1.** Call drawChildren, no other local operation. **2.** Native child traversal
produces prepared contributions. **3.** The default container can be represented
without a GL draw callback once traversal and subclass overrides are resolved.
Check dispatch actually selects this base versus override. Outgoing: drawChildren
and all virtual call sites; no closed claim for derived widgets.

## UI-FRAME-007: LLView::drawChildren

Source: [llview.cpp](../../../indra/llui/llview.cpp#L1293).

**1.** Empty child list -> no work. Otherwise obtain root, increment static depth;
iterate child list in reverse order, advance iterator before drawing. Null child
continue. Visible AND valid rect -> calculate screen rect; root local rectangle
overlap AND global dirty overlap -> push LLUI matrix, translate child left/bottom,
set child's in-draw flag, call virtual draw, clear flag. Debug rects true -> child
drawDebugRect, then test **this parent's** rectangle validity and warn if invalid.
Pop matrix. After loop decrement depth. No exception cleanup or proof that arbitrary
child-list mutation is safe from preincrement alone.
**2.** CPU native traversal and dirty/visibility policy. **3.** Prepare stable child
membership/order and transforms before recording; mutation effects require explicit
timing rather than silently discarded callbacks. Checks: reverse order, null,
invisible/invalid/offroot/offdirty children, debug branch and draw-time removal.
Outgoing: root lookup, rect math/access, calcScreenRect, UI matrices, child virtual
draw/debug and all child-list/dirty-state producers. Edges open.

## UI-FRAME-008: LLView::drawChild

Source: [llview.cpp](../../../indra/llui/llview.cpp#L1450).

**1.** Nonnull child AND parent==this -> increment depth. Visible/valid OR force_draw
-> modelview mode, push UI matrix, translate child origin plus supplied offsets,
virtual draw, pop. Decrement depth. No root/dirty overlap, in-draw flag assignment
or debug draw in this method. **2.** Native explicit forced-child contribution.
**3.** Preserve this distinction from normal traversal in prepared eligibility,
not one generic draw gate for every child. Checks: wrong parent, force invisible/
invalid child, offset and exception. Outgoing: parent/rect/visibility, UI matrices,
virtual draw and ownership; unresolved.

## UI-FRAME-009: LLView::dirtyRect

Source: [llview.cpp](../../../indra/llui/llview.cpp#L1340).

**1.** Start child=parent, parent=grandparent, cur=this. While child,parent and
parent's parent exist: cur=child, advance child/parent upward. If no prior dirty
flag, set dirty=cur screen rect and mark flag; else union cur screen rect into
dirty. This broadens invalidation through ancestors, not just changed leaf bounds.
**2.** Native CPU invalidation policy. **3.** Explicit per-document dirty state and
layout/version changes, with eligibility checked against retained-image rules.
Checks: parent depths0/1/2/3+, first/repeated invalidation and screen transforms.
Outgoing: parent chain, calcScreenRect, rect union, all callers and static state.

## UI-CLIP-001: LLScreenClipRect constructor

Source: [lllocalcliprect.cpp](../../../indra/llui/lllocalcliprect.cpp#L35).

**1.** Construct mScissorState(GL_SCISSOR_TEST) and store enabled. If enabled, push
clip rect, set scissor state to stack-nonempty, update region. Disabled still
constructs the GL-state member; do not infer no graphics dependency. **2.** Native
CPU clip scope with explicit eventual scissor. **3.** Clip-stack evaluation during
preparation; native command state changes only when recording prepared draws.
Checks: enabled/disabled, nested scopes and allocation failure. Outgoing: GL-state
constructor/setEnabled/destructor, push/update; open.

## UI-CLIP-002: LLScreenClipRect destructor

Source: [lllocalcliprect.cpp](../../../indra/llui/lllocalcliprect.cpp#L47).

**1.** Enabled -> pop stack then update region. Implicit mScissorState destruction
occurs after body and can restore GL enable state. Disabled skips body actions.
**2.** Native CPU scope exit; next prepared draw uses parent clip. **3.** Scope/stack
ownership independent from GPU command emission, with empty-stack semantics explicit.
Checks: pop-to-empty versus parent remains; failure unwind. Outgoing: pop/update
and GL-state destructor.

## UI-CLIP-003: LLScreenClipRect::pushClipRect

Source: [lllocalcliprect.cpp](../../../indra/llui/lllocalcliprect.cpp#L57).

**1.** Copy incoming rect. Nonempty stack -> intersect with top; empty intersection
becomes LLRect::null. Push resulting rect. Incoming empty rect with empty stack
does not take the canonicalization branch. **2.** CPU intersection. **3.** Native
clip math preserves integer conventions and empty behavior, or explicitly corrects
it based on consumer tests. Checks: nested/disjoint/degenerate rectangles and first
empty scope. Outgoing: rectangle methods, stack allocation/lifetime.

## UI-CLIP-004: LLScreenClipRect::popClipRect

Source: [lllocalcliprect.cpp](../../../indra/llui/lllocalcliprect.cpp#L75).

**1.** Unconditionally pop; caller must establish nonempty stack. **2.** CPU scope
bookkeeping. **3.** Native balanced scopes with checked ownership. Check mismatched
scope teardown; undefined empty-pop behavior is not a requirement. Outgoing: stack
contract and all callers.

## UI-CLIP-005: LLScreenClipRect::updateScissorRegion

Source: [lllocalcliprect.cpp](../../../indra/llui/lllocalcliprect.cpp#L81).

**1.** Empty stack returns without flushing or changing rectangle. Otherwise flush
pending gGL draws; take top rect; x/y=floor(left/bottom*corresponding UI scale).
Width/height=max(0,ceil(rect dimension*scale))+1; call glScissor and debug checks.
Even canonical null rect yields1x1 by this formula, despite pushClipRect's comment
about avoiding zero-area lines. Actual artifact/reachability is unverified.
**2.** Native scissor derived from explicit UI-to-framebuffer transformation.
**3.** Preserve known edge coverage after establishing reference pixel behavior;
empty clipping versus one-pixel inclusion needs an explicit decision. Check negative
origin, fractional scale, empty/disjoint rectangles and prior queued geometry.
Outgoing: gGL.flush, rect/scale math, rounding, GL scissor and guards. Open.

## UI-CLIP-006: LLLocalClipRect constructor/destructor

Source: [lllocalcliprect.cpp](../../../indra/llui/lllocalcliprect.cpp#L102).

**1. Constructor:** add LLFontGL current origin x/y to all local rect coordinates,
then invoke LLScreenClipRect with enabled. **1. Destructor:** empty body followed
by base destruction. **2.** Native CPU local-to-screen clip conversion. **3.**
Explicit transform stack independent from the font renderer's global origin.
Checks: translation, nested UI matrices, snapshot zoom and disabled flag.
Outgoing: font-origin writers, rect arithmetic and base clip lifetime.

## UI-ALPHA-001: LLViewDrawContext constructor

Source: [llview.h](../../../indra/llui/llview.h#L75).

**1.** Set alpha to argument(default1). If stack nonempty, multiply by top context's
alpha. Push this pointer. No clamp or finite check. **2.** Native inherited opacity
is CPU preparation state. **3.** Explicit inherited alpha per prepared contribution,
with lifetime independent from raw stack pointers. Check nested products and
nonfinite/out-of-range values before deciding validation policy. Outgoing: callers,
stack allocation and alpha consumers.

## UI-ALPHA-002: LLViewDrawContext destructor

Source: [llview.h](../../../indra/llui/llview.h#L87).

**1.** Pop stack without testing pointer identity/emptiness. **2.** CPU scope exit.
**3.** Native scoped preparation state with provable LIFO behavior. Check exception
unwind, non-LIFO lifetimes and persistent default context. Outgoing: constructor/
getCurrentContext/caller lifetimes.

## UI-ALPHA-003: LLViewDrawContext::getCurrentContext

Source: [llview.cpp](../../../indra/llui/llview.cpp#L2931).

**1.** Function-static default_context default-constructs on first call; its
constructor itself pushes onto the same stack and multiplies by existing top if
present. Then if stack empty return default, otherwise return current back. This
is not a side-effect-free fallback accessor. No local mechanism removes that
function-static entry before process teardown.
**2.** Native current-opacity query should be pure over explicit preparation state.
**3.** Base opacity1 plus scoped factors avoids static insertion; observable reference
behavior depends on first-call order and must be characterized before claiming parity.
Checks: first call with empty/nonempty stack, nested scopes then destruction and
subsequent query. Outgoing: context constructor/destructor and all accessor callers.

## UI-ALPHA-004: LLView::getDrawContext

Source: [llview.cpp](../../../indra/llui/llview.cpp#L2926).

**1.** Forward to getCurrentContext and return reference. **2.** CPU opacity query.
**3.** Read prepared inherited opacity, not a mutable font/GL global. Check callers
retaining references after scopes. Outgoing: getCurrentContext and consumers.

## UI-COORD-001: LLViewerWindow::setup2DRender

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L6904).
**1.** Call gl_state_for_2d with raw window width/height, then setup2DViewport with
default offsets. No local scale/zoom application. **2.** Native view parameters use
raw framebuffer extent separately from logical UI scale. **3.** Prepare a projection
and viewport explicitly; do not route a native call through this GL-owning function.
Checks: raw versus scaled dimensions, zero extent and caller scale stacks. Outgoing:
gl_state_for_2d, setup2DViewport, raw rect accessors. Status: body-inspected/edges-open.

## UI-COORD-002: LLViewerWindow::setup2DViewport

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L6911).
**1.** Write gGLViewport x/y=raw window left/bottom plus offsets, width/height=raw
rect dimensions; glViewport with those four values. **2.** Native explicit viewport
plus CPU screen conversion data. **3.** One versioned view description consumed by
rendering and input/capture calculations; changing origin requires coordinated
consumers. Checks: nonzero origin/offset, resize and negative/zero extents. Outgoing:
rect methods, global viewport readers and GL viewport behavior. Edges open.

## UI-COORD-003: LLViewerWindow::setup3DRender

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L6921).
**1.** Get camera and call setPerspective(NOT_FOR_SELECTION, world raw left/bottom/
width/height, false, camera near, MAX_FAR_CLIP*2), then setup3DViewport. **2.** Native
world-view policy distinct from 2D overlay. **3.** Explicit camera/view inputs per
pass rather than restoring global state by calling this method. Checks: world view
smaller than window, near/far and selection flags. Outgoing: camera singleton/getNear/
setPerspective, constants, viewport and matrix publication. Edges open.

## UI-COORD-004: LLViewerWindow::setup3DViewport

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L6928).
**1.** Profile; write gGLViewport from raw world-view rectangle with supplied x/y
offsets; glViewport. **2.** Native explicit world viewport. **3.** Share neutral view
data between input and rendering after auditing its producers/consumers, not GL
window owner calls. Checks: world/window extent differences and offsets. Outgoing:
rect methods, viewport readers, profiling and GL API. Edges open.

## UI-COORD-005: gl_state_for_2d

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L64).
**1.** GL error check; cast integer extent to floats. Select projection, identity,
ortho(0,max(width,1),0,max(height,1),-1,1); select modelview and identity; error check.
No lighting toggle occurs here despite introductory comment. **2.** Native CPU
orthographic projection with coordinated viewport/depth convention. **3.** Store
projection in prepared pass constants; avoid implicit matrix-mode mutation. Checks:
zero/negative width or height clamp, clip-depth conversion and pixel orientation.
Outgoing: matrixMode/loadIdentity/ortho, llmax and debug checks. Edges open.

## UI-COORD-006: LLUI::pushMatrix

Source: [llui.h](../../../indra/llui/llui.h#L323).
**1.** Inline forward to LLRender2D::pushMatrix. **2.** CPU transform-scope operation
when represented natively. **3.** Native preparation scope, not invoking this facade
and assuming its callee is neutral. Check nested scope ordering. Outgoing: Render2D
push and all scope callers. Body inspected; callee recorded below.

## UI-COORD-007: LLUI::popMatrix

Source: [llui.h](../../../indra/llui/llui.h#L324).
**1.** Inline forward to LLRender2D::popMatrix. **2.** CPU scope restoration.
**3.** Native balanced preparation scopes with no GL owner dependency. Check paired
push/pop and exception paths. Outgoing: Render2D pop and caller ownership.

## UI-COORD-008: LLUI::translate

Source: [llui.h](../../../indra/llui/llui.h#L326).
**1.** Forward x,y,z (default z=0) to LLRender2D::translate. **2.** CPU translation
inputs. **3.** Preserve separate geometric/text coordinate contracts, not silently
replace them with one float matrix. Check fractional/negative translation.
Outgoing: Render2D translate and all consuming vertex/text/clip paths.

## UI-COORD-009: LLUI::setLineWidth

Source: [llui.h](../../../indra/llui/llui.h#L330).
**1.** Forward width to LLRender2D::setLineWidth. **2.** Native primitive width
policy. **3.** Explicit prepared stroke representation after source geometry rules
are closed; no presumption that vkCmdSetLineWidth is equivalent or supported.
Check scaled widths and primitive families. Outgoing: Render2D width setter.

## UI-COORD-010: LLRender2D::translate

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1765).
**1.** Add float x/y/z through gGL.translateUI; separately add S32(x), S32(y) to font
origin and add float z to font depth. Per-call truncation differs from truncating
the sum. **2.** Native CPU geometry/text transform policy. **3.** Explicitly represent
the two accumulated conventions until consumer parity proves they can unify.
Checks: repeated subpixel and negative offsets, text versus image/scissor placement.
Outgoing: translateUI, font-origin/depth readers and float-to-int behavior. Edges open.

## UI-COORD-011: LLRender2D::pushMatrix

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1774).
**1.** Push gGL UI matrix, then append current font origin/depth pair to font stack.
Failure during second allocation has no local rollback of first stack. **2.** CPU
scope state. **3.** Native single owned preparation frame with explicit geometry
and text fields can make scope lifetime coherent without GL calls. Checks: nesting
and allocation failure. Outgoing: gGL pushUIMatrix, vector allocation and origin.

## UI-COORD-012: LLRender2D::popMatrix

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1781).
**1.** Pop GL UI state first; restore origin/depth from font stack back, then pop
that stack. No local emptiness check on font stack. **2.** CPU scope restore.
**3.** Native coherent balanced state frame rather than two independently mutated
global stacks. Check invalid pairing, restored depth and fractional transforms.
Outgoing: popUIMatrix, font-stack lifetime and all callers.

## UI-COORD-013: LLRender2D::loadIdentity

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1790).
**1.** gGL.loadUIIdentity, then set font x/y=0, depth0. **2.** CPU reset of local
coordinate state. **3.** Native prepared state reset with defined parent relation;
do not assume this is the modelview identity used by setup2DRender. Checks: nested
UI frame and empty stacks. Outgoing: loadUIIdentity and all origin/depth users.

## UI-COORD-014: LLRender2D::setLineWidth

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1799).
**1.** Call gGL.setLineWidth(width*lerp(UI scale x,UI scale y,0.5)). The older
glGetFloatv range-query/glLineWidth path is commented out, not active. **2.** Native
stroke width comes from logical width and average axis scale. **3.** Choose native
geometry or supported wide-line state based on actual line rendering contract;
do not restore dormant GL code. Checks: anisotropic UI scale and width boundaries.
Outgoing: lerp, static scale producers, gGL width/line-geometry consumers. Open.

## UI-COORD-015: LLRender::translateUI

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1298).
**1.** Empty offset stack logs fatal; then add LLVector4a(x,y,z) to back. No scale
application at this step. **2.** CPU offset accumulation. **3.** Native offset/scale
composition must match eventual vertex consumers, not conventional matrix order
assumed from method name. Check translation after scale and empty-stack failure.
Outgoing: vector math, fatal policy and vertex consumers.

## UI-COORD-016: LLRender::scaleUI

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1308).
**1.** Empty scale stack logs fatal; multiply back component-wise by (x,y,z).
No translation change. **2.** CPU scale accumulation. **3.** Native prepared transform
with explicit application order. Check nested nonuniform scaling and translation.
Outgoing: vector math, error policy and vertex consumers.

## UI-COORD-017: LLRender::pushUIMatrix

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1318).
**1.** Offset empty -> append zero vector, else copy back; scale empty -> append
ones vector, else copy back. Separate allocations, no local rollback. **2.** CPU
transform-stack push. **3.** Native scoped frame containing offset and scale together.
Check initially empty or inconsistent stacks and allocation failure. Outgoing:
vector construction/copies and allocation, callers establishing balanced lifetimes.

## UI-COORD-018: LLRender::popUIMatrix

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1339).
**1.** Empty offset logs fatal; pop offset then scale without an independent scale
check. **2.** CPU stack pop. **3.** Native balanced scope rather than ambient stacks.
Check stack mismatch and failure continuation. Outgoing: vector/fatal and callers.

## UI-RECT-001: LLRectBase::overlaps

Source: [llrect.h](../../../indra/llmath/llrect.h#L137).
**1.** Return NOT(left>other.right OR right<other.left OR bottom>other.top OR
top<other.bottom), short-circuited in that order. Touching edges count as overlap.
No validity test. **2.** CPU culling relation. **3.** Reuse this audited neutral
relation or test equivalent native math, not half-open rectangle assumptions.
Checks: edge/corner touching, disjoint, contained and invalid input rectangles.
Outgoing: scalar comparisons only; application input validity remains caller-owned.

## UI-RECT-002: LLRectBase::isValid

Source: [llrect.h](../../../indra/llmath/llrect.h#L224).
**1.** left<=right AND bottom<=top. Zero width/height is valid. **2.** CPU geometry
predicate. **3.** Retain distinction from nonempty/visible/native viewport validity.
Checks: reversed/zero/positive extents. Outgoing: scalar comparisons only.

## UI-RECT-003: LLRectBase::isEmpty

Source: [llrect.h](../../../indra/llmath/llrect.h#L229).
**1.** left==right OR bottom==top. Negative extent alone is not empty. **2.** CPU
predicate. **3.** Use with separate validity checks where required by native APIs.
Checks: zero one/both axes, negative extents. Outgoing: scalar comparisons only.

## UI-RECT-004: LLRectBase::unionWith

Source: [llrect.h](../../../indra/llmath/llrect.h#L239).
**1.** Left/bottom=min of operands, right/top=max, assign in left/right/bottom/top
order. No empty/invalid special case. **2.** CPU dirty-bound union. **3.** Preserve
the relation for authored integer rectangles, with explicit caller validity policy.
Checks: null rectangle union, containment, disjoint, invalid. Outgoing: llmin/llmax.

## UI-RECT-005: LLRectBase::intersectWith

Source: [llrect.h](../../../indra/llmath/llrect.h#L247).
**1.** Left/bottom=max, right/top=min; if left>right, set left=right; if bottom>top,
set bottom=top. Disjoint results collapse to zero dimensions rather than retaining
negative dimensions. **2.** CPU clip intersection. **3.** Native clipping retains
these integer endpoints independently from framebuffer rounding. Checks: disjoint
each axis, touching, null, nested rectangles. Outgoing: llmin/llmax only.

## UI-STATE-001: LLGLState constructor

Source: [llgl.cpp](../../../indra/llrender/llgl.cpp#L2552).
**1.** Initialize state, prior=false/current=false. Nonzero state reads/inserts
sStateMap[state] into prior, then setEnabled(requested); zero state no work.
**2.** Native pipeline/dynamic state is explicit per pass/draw. **3.** Prepared state
does not query or mutate an ambient GL enable map. Checks: absent map entry, state0,
CURRENT_STATE. Outgoing: map initialization, setEnabled, profiling; edges open.

## UI-STATE-002: LLGLState::setEnabled

Source: [llgl.cpp](../../../indra/llrender/llgl.cpp#L2564).
**1.** State0 returns. CURRENT_STATE translates cached bool to enabled/disabled;
else enabled request with cached not true flushes/enables/sets map true; else disabled
with cached not false flushes/disables/sets map false. Store mIsEnabled=requested
or resolved value even for otherwise-unrecognized request. **2.** Native explicit
state selection. **3.** Pipeline identity/dynamic commands should only represent
required state, not transcribe enable/disable wrappers. Checks: equal/different/
CURRENT/invalid enum and pending geometry. Outgoing: gGL.flush, GL APIs, state map.

## UI-STATE-003: LLGLState destructor

Source: [llgl.cpp](../../../indra/llrender/llgl.cpp#L2589).
**1.** Nonzero state: if debugGL, non-debugSession asserts cached equals glIsEnabled;
debugSession instead calls ll_fail on mismatch. If current differs from saved prior,
flush; enable/map true if prior true else disable/map false. **2.** Native state
does not need implicit restoration when each contribution declares requirements.
**3.** Resolve scope semantics into prepared state while retaining ordering; never
inherit GL guard objects in native code. Checks: nested guards, debug mismatch,
pending draws before restore, exception paths. Outgoing: profiling/debug/failure,
flush and GL enable-state API.

## UI-STATE-004: LLGLDepthTest constructor

Source: [llgl.cpp](../../../indra/llrender/llgl.cpp#L2822).
**1.** Save static enabled/function/write states; check errors and debug state.
Depth disabled forces requested writes=false regardless of input. For changed
enable, flush then enable/disable depth and update cache. Changed function flushes,
glDepthFunc and cache update. Changed writes flushes, glDepthMask and cache update.
**2.** Native pipeline depth test/write/compare requirements. **3.** Model all three
explicitly; UI-default guard resolves to no depth test and no writes even though
its constructor passes write=true. Checks: every change/no-change combination,
disabled test+requested write and prior-state restore. Outgoing: checkState, flush,
GL depth APIs and static initial-state assumptions.

## UI-STATE-005: LLGLDepthTest destructor

Source: [llgl.cpp](../../../indra/llrender/llgl.cpp#L2857).
**1.** checkState then independently restore changed enable, function, writes in
that order, each with flush and corresponding GL call/cache mutation. **2.** Native
subsequent pass declares its own state. **3.** No GPU-state stack required, but
prepared scope requirements must preserve ordering. Checks: nested depth guards,
pending geometry at each restoration, debug mismatch. Outgoing: checkState/flush/API.

## UI-STATE-006: LLGLDepthTest::checkState

Source: [llgl.cpp](../../../indra/llrender/llgl.cpp#L2882).
**1.** Only debugGL queries DEPTH_FUNC, DEPTH_WRITEMASK and glIsEnabled(DEPTH_TEST).
Any mismatch with cached values: debugSession writes failure log; otherwise
LL_GL_ERRS diagnostic. **2.** Native validation checks declared state and API use.
**3.** CPU contract assertions plus enabled Vulkan validation, not GL polling.
Checks: debug flags and each mismatched field. Outgoing: GL queries, diagnostic
macro/stream behavior and cache writers.

## UI-STATE-007: LLGLSDefault constructor

Source: [llglstates.h](../../../indra/llrender/llglstates.h#L56).
**1.** Construct disable-blend then disable-cull guards, empty body; destruction
implicitly reverses member order. **2.** Native pass blend/cull requirements.
**3.** Explicit pipeline state; do not infer depth/texture state from this class.
Checks: entry/exit prior-state combinations. Outgoing: LLGLDisable/LLGLState.

## UI-STATE-008: LLGLSUIDefault constructor

Source: [llglstates.h](../../../indra/llrender/llglstates.h#L83).
**1.** Enable blend, disable cull, construct depth test(false,true,LEQUAL). Effective
writes become false inside depth owner. This class does not set blend factors.
**2.** Native UI pass state with explicit blend equation from its actual producer.
**3.** Pipeline selection must include blend factors, target encoding and depth
contract; this guard alone is insufficient evidence. Checks: prior blend factors,
depth writes and member destruction order. Outgoing: enable/disable/depth guards.

## UI-STATE-009: LLGLSPipeline constructor

Source: [llglstates.h](../../../indra/llrender/llglstates.h#L99).
**1.** Enable cull then depth test(true,true,LEQUAL); no blend setup here.
**2.** Native world-space UI pipeline policy. **3.** Separate from 2D overlay state,
not one universal UI pipeline. Checks: 3D selection/beacon consumers and depth target.
Outgoing: enable/depth guards and caller blend/target selection.