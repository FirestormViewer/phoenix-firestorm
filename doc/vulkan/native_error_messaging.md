# Native error messaging

## CEF hyperlink fixture resolved (2026-09-17)

The previously deferred stage0 timeout was fixture horizontal overflow, not a
production Guidebook rendering failure. Read-only CDB observations showed a
300x500 browser with a15-pixel vertical scrollbar: x284 remained yellow while
x285 was scrollbar gray. The added link at left100 with width190 ended at290,
five pixels beyond the285-pixel content width, creating a horizontal scrollbar.
At y484 the left edge remained yellow; y485 was gray. The bottom-left readiness
pixel was RGB252,252,252 rather than expected255,255,0, so the click was never
reached. Native browserFrame correctly flips that bottom row into pixel zero.

NV-00/01/12/17: the test link now has width120, retaining its x120 click target,
vertical scrolling, original readiness colors and all production rendering.
No GL reference, tolerance, browser sizing or production URL policy changed.
Window7/7 now passes, including the actual CEF internal Preferences hyperlink,
Privacy tab and nested autoresponse selection, and normal lifecycle cleanup.
The first successful run logged a CEF browser-info response timeout despite
completing all assertions; it is not silently treated as a clean CEF log.

The secondary abort was LLVKSessionOwner's deliberate destructor guard against
still-owned services during assertion unwinding. The fixture now records a
deadline failure, posts WM_CLOSE, allows its existing cleanup/retry path to run,
and asserts only after session retirement and HWND destruction checks. The
test-only LL_VK_TEST_GUIDEBOOK_TIMEOUT environment switch forces that path.
Fault injection verifies the preserved stage0 message and ordinary assertion
exit1 (6passed/1expected failure), rather than fail-fast0xc0000409. Normal window
validation remains7/7. Editor diagnostics pass. Changes are test/documentation
only; no full-viewer run or production relink is needed. This closes this CEF
click verification, not complete SLURL support or unrelated destinations.

## Text movement cache optimization (2026-09-16)

The user deferred further investigation of the Guidebook fixture timeout and
directed work to continue with UI latency. Live Guidebook display is explicitly
operator-accepted. The real CEF internal-link regression remains unverified:
the fixture times out at stage0 before the link click, then aborts during session
cleanup. That failure is not evidence that Guidebook fails in live use.

NV-00/01/12/13/14/17, optimization: the GL reference contract is
LLFontGL::render -> LLFontFreetype::getGlyphInfo at the pinned reference revision
59108e15a1f8f94d2da7c674d937d19f5cf9450d. Cached glyph bitmap coordinates are
independent of the screen rectangles generated for each draw; cache misses use
addGlyph. Native LLVKWidgetGpu previously included screen positions in atlas
identity, rebuilding and uploading unchanged pixels whenever text moved.

The native implementation now asks LLVKGlyphAtlas::updateLayout to replace only
CPU placements when the complete ordered glyph-identity sequence matches. Count
or identity mismatch rejects reuse before mutation and retains the existing
rebuild/publication path. No GL function is reused or changed. Pixel pages, UVs,
sampling, shader ABI, color conversion, shadow order and clips are unchanged.
This avoids offset arithmetic that could change fractional rounding: the exact
new device-space rectangles are copied into placements.

GPU safety: placement changes do not touch Vulkan images or descriptors. Existing
uploads retain their completion gate. Packets and recorded draws own copied
vertices and shared immutable image references, so later CPU placement changes
cannot modify in-flight geometry. Glyph-identity replacement still uses new
resources, with existing completion-based consumer retention. No new barriers,
device-idle waits or descriptor recycling are introduced.

Measured on AMD Radeon RX9070XT, Windows, RelWithDebInfo: the same32-update
fractional movement probe took56.5941ms with31 upload/publication cycles before
the change. Afterward it took0.0506ms with zero cycles; subsequent runs measured
0.0532,0.1055,0.0521 and0.0503ms, all with zero cycles. These are fixture CPU
preparation timings including explicit completion waits in the old path, not
end-to-end input latency or a full-viewer speedup. First-open Preferences stalls
and browser scheduling are not claimed fixed. The earlier shutdown task remains
backlog, outside this continuation's three-feature request.

Validation: GPU10/10, Widget210/210 and offscreen glyph6/6 pass. The GPU fixture
checks exact fresh-versus-reused vertices, UVs, colors, draw order and clipping at
75/100/125/150 percent, unchanged image identities, new-glyph publication and
retained old packets. The offscreen fixture checks every target pixel against
unchanged blending, clipping, hard/soft-shadow, bold and depth expectations after
repositioning; it also mutates placements after recording and checks failed reuse
is nonmutating. Khronos and synchronization validation execute in the offscreen
fixture with zero validation errors. Editor diagnostics, native core build and
RelWithDebInfo viewer link pass. No full-viewer run or new commit was performed.

Prior bounded effects evidence is retained: GLgl-controlled-tooltip-bottom-99
versus native-controlled-partial-216 matched all138 full-frame samples exactly
before this cache optimization. Native216 SHA256:
DE92C916C9D3883FB30043C96E314A323B10633050758CEA2CD575C5A9B0C7FF.
That expanded matrix includes bottom-edge tooltip placement and partial floater
dragging; modal tooltip suppression also passed widget tests. It is historical
full-window evidence, not a rerun of that matrix on the optimized binary.

Native Preferences URL tab/subtab/search routing and external forwarding passed
direct widget tests before the deferral. Explicit CEF custom-scheme clicks are
queued for dispatch after browser iteration; non-gesture or redirect events are
not treated as clicks. Other internal destinations remain unsupported rather
than silently forwarded to the system browser. None of this closes the deferred
real-CEF click verification or establishes complete SLURL support.

## Tooltip skin layering resolved (2026-09-16)

The timeout-override blocker below was an incorrect test expectation, not a failed
numeric parse. NV-00/01/02/12/17 source tracing:
LLUICtrlFactory::loadWidgetTemplate calls LLXMLNode::getLayeredXMLNode, whose
LLXMLNode::updateNode only updates attributes already present in the base node.
LLVKXmlLayers implements the same rule. The base widgets/tool_tip.xml declares
max_width and padding but not visible_time_near. Therefore a partial skin overlay
can change width/padding but cannot introduce that timeout attribute. Keeping the
configured10-second timeout in that case matches GL; forcing0.25seconds would not.
Neither the GL implementation nor the native shared XML merger was changed.

The regression now verifies inherited Tooltip background styling, width80,
padding9, and unchanged tooltip identity when the unsupported new timeout
attribute is ignored. It separately changes ToolTipVisibleTimeNear to0.25 and
verifies timeout-driven tooltip renewal. Widget210/210 passes. This corrects the
assertion against the inspected source contract, not a visual tolerance.

The skin-backed native template also passes the retained default-skin controlled
pixel regression: GLgl-controlled-tooltip-edge-98 versus native-tooltip-skin-214,
all122 full-frame samples exact. Native214 SHA256:
CB8CC8EE1F85B0A04A0155EFB8DB86A3193D30CFD8AEA62225C48ED574BAE6AE.
The request and GL binary identities are unchanged from the continuation below;
native exits zero with Window7. This is default-skin pixel evidence plus focused
custom-overlay state/geometry tests, not a claim of paired screenshots for every
custom skin. Broader controlled-effects obligations remain separately tracked.

Window7, GPU10, native core build, RelWithDebInfo viewer link and editor diagnostics
pass. The in-place viewer contains the validated tooltip changes. No full-session
operator run, new commit or push was performed for this correction.

## Controlled tooltip continuation (2026-09-16)

After checkpoint ff29de9e70, the user requested sequential completion of controlled
effects, hyperlink activation and UI latency. This continuation remains on the
first feature; the latter two have not been changed in this pass.

The expanded controlled replay now records tooltip initial/fast delays, replacement,
near/far timeout, keyboard fade, focus loss/regain, a dormant Win32 leave message,
and right-edge positioning. GLgl-controlled-tooltip-edge-98 and
native-controlled-tooltip-edge-213 match all122 full-frame samples exactly.
Native213 SHA256:3AB4CC6D6AE74607DA421A6037D3627663BE8D38D361C40D25F2FE2D91CB329F.
The GL diagnostic binary remains1DFD3ACCFAB4472D957FA240E0CD139B7D7473A72252FC8D545AB7275787CE8B.
Request SHA256:0FBD03AA884CEAFFE3FD7C5963B835F08F33D7F2B6A0605E835CE742E25DF411;
the recorded timeline identifies the expanded sequence. Both runs exit zero;
GL records Goodbye and native passes Window7. No image tolerance or transform is
used. The original54 checkpoints are included unchanged.

NV-00/01/09/12/17: source LLView::getTooltipTimeout uses the fast delay while a
tooltip remains visible. LLToolTipMgr selects over/near/far timeouts; LLToolTip::draw
uses F32 timer values for fade thresholds. Native now matches those policies and
the cursor-exclusion placement in LLUI::positionViewNearMouse and
LLView::translateIntoRectWithExclusion. LLViewerWindow::handleFocusLost does not
fade existing tooltips: it suppresses new requests via application focus. An
initial native WM_MOUSELEAVE addition was removed after tracing showed the GL
Win32 handler is commented out; dormant behavior is not a parity mandate.

Native210 failed after replay with Invalid native notice clock: controlled time
advanced beyond wall time, and releasing replay moved notice time backwards.
The diagnostic-to-ordinary clock handoff now rebases to the last accepted time;
211-213 pass graceful fixture completion. Native211 had one delay checkpoint
mismatch from double versus F32 fade-boundary evaluation;212 closes all106 earlier
samples and213 adds16 exact edge/drag/fade samples. Failed evidence is retained.

A subsequent tooltip skin-default implementation is NOT yet validated. It merges
widgets/tool_tip.xml layers into an independently owned native panel/text template,
with width, padding, font, wrapping, background and timeout attributes. Its new
test203 passes inherited image, width and padding checks, but fails
"XML near timeout renews eligible tooltip". A temporary probe showed timeout=10
instead of the declared0.25 at times1.3 and1.4. Switching to all skin layers fixed
the missing background; explicit locale-independent parsing did not fix the
remaining timeout failure. The temporary logging has been removed.

Latest widget result is209/210, with that one regression unresolved. The required
retry limit has been reached; further correction awaits user direction. Do not
mark the controlled-effects gate complete or infer current-template parity from
the earlier122-frame pass. Current source includes the unverified skin-template
changes; the window fixture binary and production viewer have not been rebuilt
with that later slice. No new commit or push was made.

## Native hyperlink hover cursor (2026-09-16)

The user reported that Vulkan hyperlinks did not select a hand cursor. Two native
routing gaps were found: ordinary widget cursor requests had no default sink
(only specific About controls installed one), and both login and auxiliary CEF
event drains discarded cursor-change events. Windows client cursor messages also
had no retained native cursor policy.

NV-00/01/03/12/17 source contract: native plain-text link hit testing already
requests a hand over a link; the GL MediaPluginCEF::onCursorChangedCallback maps
Dullahan CT_HAND to UI_CURSOR_HAND. The equivalent native implementation is CPU-only
cursor selection, independent of GL rendering: the native tree now has a fallback
cursor handler, preserving per-control overrides. LLVKBrowser publishes semantic
hand/ibeam/arrow cursor names; WindowState caches them by browser identity and
applies asynchronous changes only for the eligible browser under the pointer.
Client cursor selection survives WM_SETCURSOR, resets on pointer movement, and
auxiliary browser retirement erases its cached cursor. Other CEF cursor shapes
remain arrow fallbacks; this is not complete CEF cursor-shape parity.

The existing real-window test now checks native signup-link hand selection,
WM_SETCURSOR persistence, departure back to arrow, asynchronous CEF login-link
hand selection, movement within the link, departure, and reentry before actual
navigation. Window7, Widget210, GPU10, native core build and RelWithDebInfo viewer
link pass. The rebuilt in-place binary contains the cursor fix. No new full-viewer
operator run or pixel-parity closure is inferred from these cursor tests.

## Operator verification outside the harness (2026-09-16)

The user reports that the RelWithDebInfo binary, run in place outside the test
harness, opens Help links in the system browser and opens Guidebook without issue.
The user clarifies that most Help links are external URLs. Accept this as operator
verification of the tested external-link launches and Guidebook opening; do not
require those successful checks to be repeated. This report does not supply a
separately recorded executable hash or exhaustive URL/confirmation-state matrix.

The earlier harness forced an internal Help browser with a local test page and
exercised resizing. Its black GL browser frames remain evidence about that specific
embedded-browser test, not evidence that ordinary external Help links or Guidebook
opening fail. Investigating that discrepancy must not block acceptance of the
operator-verified workflows or justify changing native behavior to display black
content. Exact embedded-resize parity, untested confirmation policies and other
previously declared coverage gaps remain distinct from these successful checks.

## Post-checkpoint fractional coverage (2026-09-16)

Requested commit/push completed:4349b481f849e14ba0fcd4496352447f3a1848c9 on
origin/native-error-messaging. The following work is subsequent and uncommitted.
The request to close every open gate remains incomplete.

### Measured fractional interactions

All30 corresponding full-frame samples in each listed nested-dialog matrix match
exactly, including the initial notification, menu, Preferences, Colors, picker
open/close/reopen, parent press/drag/release and parent close. No masks, transforms,
rescaling or tolerance changes were used.

| UI scale | Pinned GL capture | Native capture | Request SHA256 |
|---|---|---|---|
|75%|gl-fractional-dialog-92-075|native-fractional-run-203-075|DC58BD5BFC70FB5AF6D98B3D0B62EA9E15C3CDDB2D88B80AD57387F850506A78|
|125%|gl-fractional-dialog-91-125|native-fractional-regression-209-125|8856F385AD69C2A661B8098A6BA5BDE0FA57C2612BB5BC6CEA6C34879C8244EF|
|150%|gl-fractional-dialog-93-150|native-fractional-origin-206-150|62A947ED257977DCCF3CD9E0CAB381ADFFA5136DECE185CB1D688443D4DB9B23|

Native203 SHA256:B593BAB216693A52ECB0941C8A46C33008165DB2CA2E643561249C218E410DED.
Native206/209 SHA256:AEF9AD7705B17DA2A10D4EA7262018A733709C8BAA6EB85C6500C76838A24BF0.
The later width/origin changes have retained75-percent evidence, not a new75-percent
run of the206 binary. The final100-percent controlled regression208 still matches
all54 checkpoints against GL90. Every native run completed Window7 with exit zero;
the pinned GL runs exited zero and recorded Goodbye. This closes the listed
fractional nested-dialog matrix, not all fractional Help/tear-off/resize workflows.

NV-00/01/02/11/12/17: LLViewBorder::drawOnePixelLines, LLColorSwatchCtrl::draw,
LLFloaterColorPicker::draw/drawPalette and LLMenuItemSeparatorGL/LLMenuItemTearOffGL
use directed line coverage, including distinct endpoint order and inherited physical
line width. The standalone tools/vulkan/line_coverage_probe.cpp creates its own
hidden diagnostic context, measures quarter-pixel endpoints in both directions,
and destroys it normally. It does not link into native rendering or modify the
pinned GL source/executable. All64 one-pixel cases match its explicit prediction;
the1.5-pixel-width measurements show two-pixel cross-axis coverage. Measurements
are local AMD Radeon RX9070XT driver26.9.1 evidence, not a universal driver claim.
Native paint emits explicit triangles for the recovered coverage. The speculative
epsilon rules were replaced; the12-pixel corner gap is closed in actual captures.

LLVKFont::metrics already returns logical metrics. Native menu row height no longer
divides them by scale twice. Menu text now snaps its row origin separately, matching
LLFontGL's origin/local-offset order. LLDragHandleTop::reshapeTitleBox uses separately
ceiled ascender/descender for title height; native does likewise. Hyperlink underline
positions now use physical raster bearings/advances and the returned pen endpoint.
Linked text preserves style boundaries in document layout and rendering, following
LLNormalTextSegment::drawClippedSegment. A bounded diagnostic showed78 of80
characters emitted in the old whole-line font run; splitting at link boundaries
restored the final period at75%. The temporary text diagnostic was removed.

### Help resize reference blocker

Follow-up no-input observation: the user suggested delayed GL repaint rather than
an inherent defect. GLgl-help-resize-dwell-96 used the unchanged pinned executable
and inserted three consecutive10-second capture streams after resize release,
before any restore gesture. The measured interval was30.42seconds; its1811 WGC
callbacks all reported foreground=1. Each stream retained one unique image, and
all three images are byte-identical to the released frame and the earlier black
GL95 reference. No recovery was observed during this interval. This does not prove
a permanent failure or identify the cause; delayed recovery beyond30seconds and
stalled repaint/publication remain possible.

The observation data and timeline were saved, but the runner then failed while
counting states: Select-Object property expansion does not handle its live ordered
dictionaries as intended. Explicit dictionary access fixes that metadata operation;
the saved23-state dictionary timeline and script syntax pass the focused check.
The run logged Goodbye during graceful cleanup, but the usual final exit-code
manifest was not produced, so this run is diagnostic evidence, not a full runner
acceptance pass. Captures were not repeated or overwritten. The opt-in
LL_DIAGNOSTIC_HELP_RESIZE_DWELL=1 path injects no input during the three streams.

The shared Help workflow now adds browser field input and separated title/resize
gestures before restoring geometry and closing/reopening. GLgl-help-workflow-94 and
GLgl-help-workflow-recheck-95 use the unchanged pinned executable. Both produce the
same black embedded browser after resize, including byte-identical resized,
restored and later reopen checkpoint frames. Native-help-workflow-207 retains
browser content at the same resized floater geometry. The resized sample differs
442800pixels. The viewer log does not identify a media-process crash; the root
cause and whether this is a defined failure or a reference defect remain unknown.

Request SHA256:D5651ACF167E861E73E297277B916B74995C7DA9B1B2345B63441FC74782D23E.
Both GL runs exited zero with Goodbye; native exited zero with Window7. Initial
Help, field-input and movement samples have matching evidence before resize;
ordinary caret-phase differences remain in unrelated preparation frames. Matching
samples alone do not prove every input produced the intended DOM state.
The sequence completion count is now derived from the timeline (20Help states);
older94/95/207 fixed-count metadata is superseded by their recorded timelines.

The pinned reference has not been patched, replaced or given different tolerances.
Native has not been changed to discard valid browser content to manufacture equality.
Further reference-specific diagnosis or an explicitly approved diagnostic reference
is required before closing this embedded-resize workflow. Subsequent operator
verification above confirms tested external launches and Guidebook opening outside
the harness. Untested confirmation policies, additional Help navigation/error cases,
and remaining declared tooltip obligations are still open. The agent did not launch
an external system browser during this capture continuation.

### Build state

Latest Widget210, Window7, GPU10, native core build and RelWithDebInfo viewer link
pass. The production executable now includes these changes, but no new full-viewer
login/settle/shutdown acceptance run was performed. Prior CTD acceptance remains
valid for the reproduced startup defect, not blanket acceptance of new UI work.
UI/shutdown performance follow-ups remain open as previously measured. Unrelated
mcp-Vulkan files were not staged or modified.

## Gates 1-3 continuation (2026-09-16)

The user requested closure of controlled effects, fractional interactions and
external/Help workflows. This request is NOT complete. The following supersedes
older pending statements only for the explicitly measured states.

### Controlled effects evidence

GLgl-controlled-effects-90 versus native-controlled-shadow-188 matches all54
full-frame samples, two at each of27 controlled checkpoints. These cover tear-off,
menu activation feedback, first Preferences paint, Help-button hover rise/decay,
search-editor focus and blink, tooltip appearance, typed search and deletion,
button press and release outside. Inputs, update timestamps and frame deltas are
shared, with no image transformation, masking or tolerance. The sequence records
29 states including its two ordinary preparation states. This is a bounded matrix,
not proof of every animation family or of fractional interactions.

- GL diagnostic SHA256:1DFD3ACCFAB4472D957FA240E0CD139B7D7473A72252FC8D545AB7275787CE8B.
- Native188 SHA256:02E9D013FA8965FEB50F763BE44084DD1F913502E1DF1AD99F36F899011F926B.
- Request SHA256:0FBD03AA884CEAFFE3FD7C5963B835F08F33D7F2B6A0605E835CE742E25DF411.
- Matrix:glref-build/captures/native-controlled-shadow-188/controlled-comparison.json.
- Both fixtures exited zero; GL recorded Goodbye, native passed Window7.

NV-00/01/02/09/11/12/17 source/design record: LLMenuHolderGL::draw and
setActivatedItem retain the live selected row for0.3seconds. Native LLVKMenu
retains its item identity, geometry and activation time in the shared native model,
then emits native paint commands with live checked/visible state. LLTabContainer
draw positions buttons AFTER child rendering; selection/visibility update state
without publishing next-frame positions early. Initial vertical positions retain
the last configuration-excluded declaration slot, matching ordered source removal.
LLFloaterPreference::postBuild installs a search keystroke callback in addition to
commit; native now does likewise and searches outer controls/tab buttons as well
as content. Text highlights use document bounds and encoded solid colors.
LLFontGL::render derives soft-shadow strength from HSL lightness; native widget GPU
text now does so, leaving atlas publication/retirement unchanged. Outside button
release clears stale captured hover. Focused Widget210 and GPU10 tests passed.

LLView::handleToolTip/childrenHandleToolTip, LLToolTipMgr::show/createToolTip/
updateToolTipVisibility, LLToolTip::draw and LLUI::positionViewNearMouse define
ordinary tooltip selection, delay, geometry and fade. Native tooltip selection
permits disabled controls and respects child precedence/opaque overlap. A separate
native widget subtree uses the Tooltip skin image and native text layout. Its
captured visible and keyboard-fade states match the GL matrix. Callback/media
tooltips, all edge-exclusion cases, skin-default overrides, timeout variants,
mouse-leave and full activation/modal policy are not exhaustively qualified by
this plain-text implementation. These remain explicit obligations, not closure.

### Fractional interaction blocker

Shared dialog input now accepts the request's UI scale and converts input positions
around the menu/centered-dialog anchors. RGBA captures are never rescaled.
GLgl-fractional-dialog-91-125 is the retained125-percent pinned reference; request
SHA256:8856F385AD69C2A661B8098A6BA5BDE0FA57C2612BB5BC6CEA6C34879C8244EF.
Native189 failed an obsolete100-percent fixture assertion. Dialog-only fractional
requests are now allowed; the browser-only restriction remains.

Native190 Colors differed7285pixels. Nearest-integer tab widths, matching
LLFontGL::getWidth, plus native physical-pixel border meshes reduced this to2349
in191 and17 in192. Directed endpoint coverage in193 leaves12pixels different;
an alternative endpoint tie probe194 leaves20 and was reverted. Native193 SHA256:
7AB9471C1276EA1DBE3D1E10FB55FDE24C8DFE988ACFA8053BDA85B6B555FB47.
All failed captures/reports are retained. The restored directed mesh is still
an unaccepted implementation probe: the endpoint contract is not closed.
The required three-attempt limit was reached for this corner-coverage slice.

The125-percent picker192 still differs4062pixels, concentrated in its outlines,
palette and crosshair. Other fractional menu/Preferences/movement states remain
unqualified. Initial modal and final parent-closed frames matched, but do not
close the interaction gate. Other scales and Help/external workflow captures have
not been completed in this continuation. No external browser was launched.

### Integration and prior feedback

The real CEF login-popup and same-window hyperlink sequence passes Window7 after
extending its overall bounded deadline from60 to90seconds; its preceding stages
already consumed most of the earlier deadline. Native login signup/recovery
callbacks and login-popup event dispatch are implemented; custom schemes and
OpenSim-specific URLs remain outside that evidence.

Earlier opt-in timing captures178-180 measured the session-refresh guard change
from2.37939ms to about0.0013ms per update, with checked dialog frames unchanged.
Prelogin shutdown measured about689ms: browser567ms and audio122ms. No shutdown
optimization or full-session performance acceptance is claimed.

Latest Widget210 passes after removing the failed tie probe. Window7 passed with
the preceding probe binary; the restored source still needs its final integration
build. GPU10 passed the color-dependent shadow regression. Production viewer has
NOT been relinked with this continuation; the user-accepted CTD-fixed binary is
not replaced or requalified by these fixtures. No new commit or push was made.

## RelWithDebInfo startup CTD (2026-09-16)

The user reprioritized investigation of the full-viewer Vulkan crash ahead of
the remaining parity gates. CDB reproduced an unhandled access violation
(0xC0000005) during LLVKContext::createSwapchain, in vkGetSwapchainImagesKHR.
The stack passes through VkLayer_api_dump, Khronos validation and amdvlk64.
Evidence is retained locally in logs/native-rwdi-ctd-01.log. The older normal
profile logs ended with Goodbye and were not evidence of this native startup crash.
An initial LLDB attempt crashed inside the debugger's LLVM/PDB handling; that
debugger failure is separate from the reproduced viewer fault.

NV-00/03/15/17: the relevant owner is LLVKContext::createInstance. This is native
Vulkan instance configuration, with no OpenGL visual counterpart to reuse or alter.
The validation flag previously requested both VK_LAYER_KHRONOS_validation and,
whenever installed, VK_LAYER_LUNARG_api_dump. API dump is optional call tracing,
not validation. Automatically enabling it made ordinary startup depend on an
installed tracing layer. Existing GPU/window test tasks suppressed this layer,
so their passes did not exercise the failing full-viewer configuration.

The fix removes automatic API-dump selection while retaining Khronos validation
and debug-utils reporting. Explicit loader-based tracing remains possible; no
global environment or installed layer was disabled. The GPU regression asserts
that Khronos validation is loaded and API dump is not. A local Default Layer
Validation task clears layer enable/disable overrides and passes all10 GPU tests.
The production llvulkan library was rebuilt and the RelWithDebInfo viewer relinked.
No shader, swapchain format, resource ownership or OpenGL implementation changed.

Full-viewer verification under CDB used the same explicit RenderBackend=Vulkan
launch without API-dump suppression. It passed swapchain creation and reached
LLVKContext::end2DFrame. The loaded-layer list contains Khronos validation and
does not contain API dump. The debugger detached without terminating the viewer;
the Vulkan window remained responsive for manual interaction. Evidence is in
logs/native-rwdi-ctd-fixed-02.log. Corrected executable SHA256:
F7A9472DE79CBF5F7A50BC200127F3FEB9DAAA1D58F62DF67F637C10C386101E.

This verifies removal of the reproduced startup CTD. It does not identify the
internal defect within the API-dump/driver dispatch chain, nor qualify explicitly
forced API tracing. Post-login settling and graceful full-session shutdown remain
operator verification; no STATE_STARTED or successful full-session exit is claimed.

## Approved animation acceptance (2026-09-16)

### Controlled tear-off parity achieved

GLgl-controlled-tearoff-89 versus native-controlled-monotonic-176 matches all14
full-frame samples at0,50,100,150,200,250,300ms, two samples per checkpoint.
Frames are2560x1369 RGBA8,100-percent UI scale, default skin, English, anisotropy
off, with the recorded local rich browser page. Input is queued before the next
controlled update. No frame alignment, crop, rescaling or tolerance was applied.
Shared request SHA256:
0FBD03AA884CEAFFE3FD7C5963B835F08F33D7F2B6A0605E835CE742E25DF411.
Diagnostic GL executable SHA256:
1DFD3ACCFAB4472D957FA240E0CD139B7D7473A72252FC8D545AB7275787CE8B.
Native executable SHA256:
64F501BA517F80444F87BAB3DABA4CF552312A38B4DB965D1B59BBCDF54AEA6B.
Both runs released replay and exited zero; GL recorded Goodbye and native passed
Window7. The pinned GL executable remains unchanged at its recorded hash.

Ordinary-rendering qualification: diagnostic GLgl-timing-passthrough-87 matches
the pinned modal and detached frames exactly; its menu image matches the pinned
alternate caret phase. The CEF DLL matches the pinned139 runtime exactly.
After the native corrections, native-tearoff-framehover-177 matches all four
ordinary detached/departed samples of pinned GLgl-menu-tearoff-79.

Controlled capture exposed source ordering omitted by settled-only checks.
LLFloaterView constrains geometry before LLTearOffMenu::draw grows its height.
Native now follows that order. The source initial two-axis fit clears hover;
pre-clamping x had skipped this transition. Menu hover is sampled during frame
update, with popup-local movement history retained across detachment, rather than
selecting a row on every raw WM_MOUSEMOVE. Intermediate and ordinary captures
both discriminate these corrections. Tests check transient bounds and stationary
selection; Widget210 and Window7 pass.

Replay protocol corrections are confined to test scheduling: immutable numbered
acknowledgements avoid Windows file-replacement contention; completion is
idempotent; native entry uses the last accepted widget time to remain monotonic.
A requested native paint packet is retained through GPU readiness retries so one
requested update cannot accidentally apply layout repeatedly before presentation.
Standalone protocol tests and five interprocess exchanges passed. Earlier native
165-175 diagnostic failures remain retained. Native172 had controlled equality
but an ordinary hover regression;176/177 supersede that incomplete result.

This closes the controlled tear-off intermediate-pixel comparison, not every UI
animation or scale. Other animation families still have their recorded state tests
and settled/runtime coverage, not a blanket controlled-pixel parity claim. Broader
fractional interaction and external-browser acceptance obligations remain separately
tracked; real-world stalls and latency remain performance measurements.

The user approved stateful elapsed-time animation with controlled update steps.
For NV-00/02/12/17, compare identical initial state, ordered input events and
per-update clock values/frame deltas, then require exact state and pixels at the
corresponding checkpoints. Matching only total elapsed time is insufficient.
Do not match captures by unrelated frame indices, transform images, or relax the
zero-tolerance visual criterion. The pinned GL implementation remains unchanged.

The source contracts are LLSmoothInterpolation::calcInterpolant/updateInterpolants,
LLButton::draw, LLTearOffMenu::draw and LLLineEditor::draw/focus/input resets at
the pinned reference revision. Smoothing uses 1-pow(2,-delta/halfLife); tear-off
height rounds upward on every update. Caret phase is relative to its own editor's
reset event. Native already accepts frameDelta and explicit editor time, so this
change adds controlled tests without changing production clocks or animation laws.

The existing widget tests now check:

- Tear-off growth at successive50ms steps:13,19,22,24,25,25 pixels toward a25-pixel
   header expansion, retaining child origin and screen constraints.
- Zero-delta redraw does not advance the tested animation state.
- Five10ms updates produce4,7,10,12,14 pixels of growth, demonstrating why update
   partitioning is part of the contract rather than just total elapsed time.
- Button glow rises0.25,0.375,0.4375 toward0.5, then decays0.21875,0.109375,
   0.0546875 after pointer departure, at matching50ms steps. Encoded draw alpha is
   checked exactly as well as the retained float state.
- Existing caret checks cover the initial one-second delay, half-second phase
   boundaries, accepted input resets and independence from other editor clocks.

Validation: Widget210/210 passes with these assertions. Expected values are
derived from the inspected pinned source; this is not a newly executed GL GPU
animation replay. Existing exact settled-frame evidence remains valid for its
recorded states. Controlled paired GL/Vulkan intermediate pixel capture remains
unverified and must not be inferred from arithmetic or paint-state tests alone.

Wall-clock input latency, frame pacing and stalls are a separate performance
track. The retained independent-run caret/animation differences remain evidence
of their original schedules, not failures of a controlled replay they did not
perform. No performance pass threshold or new performance pass is asserted here.
This approval resolves the comparison-model question; it does not automatically
close other outstanding verification gates or advance the queued crash task.

### Controlled replay prerequisite

The user approved an isolated timing-instrumented diagnostic fixture on2026-09-16.
This is a narrow exception for test clock/input scheduling in a separately generated
copy of the pinned source, not permission to edit the pinned worktree or production
GL implementation. It must use its own build directory and executable identity.
Uninstrumented execution must first match retained ordinary captures; only then may
controlled checkpoints be evaluated. Rendering logic, assets and tolerances remain
unchanged. Fixture construction and qualification are in progress, not completed.

Inspection of the pinned reference's LLFrameTimer::updateFrameTime shows that it
samples totalTime internally; its public API does not accept an injected frame
timestamp. LLSmoothInterpolation owns a protected LLFrameTimer and cached delta.
LLLineEditor owns and resets a separate caret timer. The existing notification
capture driver sends input and observes mainloop events but does not control those
clocks. Suspending a process or spacing external input cannot establish identical
per-update timestamps across both renderers.

The approved diagnostic copy is worktrees/notification-gl-timing, built separately
in gltiming-build. Its only tracked source differences are the clock substitution
in LLFrameTimer::updateFrameTime and a frame-entry/exit gate in LLAppViewer::doFrame.
Both use tools/vulkan/diagnostic_replay_clock.h. The pinned reference's tracked
sources and executable remain unchanged. The diagnostic viewer's ordinary-rendering
comparison is still pending; its controlled frames are not qualified evidence yet.

The test protocol uses atomic numbered timestamp requests, a named wake event and
matching acknowledgements. No request means ordinary timing. A release request
restores ordinary execution; timeout records failure and releases the gate without
terminating the viewer. The standalone protocol test and the interprocess
step_diagnostic_replay.ps1 exchange pass, including two frames and release with
exit zero. Testing exposed a Windows file-sharing defect: the reader must close
request.txt before waiting so that the next request can replace it atomically.
That defect is fixed. The C++20 UTF-8 path constructor and parenthesized numeric
limits call keep the header compatible with the viewer's warning policy and
Windows macros without suppressing those diagnostics.

Native LLVKWindowMgr now has an optional diagnosticFrameTime callback. It preserves
the previous frame time during input dispatch, applies the requested time during
paint preparation, and rejects nonfinite/backward timestamps. Without the callback,
ordinary timing is unchanged; service shutdown deadlines retain their real clock.
Only the isolated capture test reads LL_DIAGNOSTIC_REPLAY_DIR. Its callback retains
the same timestamp through resource-readiness retries and acknowledges presentation
before allowing a new step. Window7 passes with ordinary execution after this hook.

The shared capture runner has an explicit controlledReplay request flag, requires
queued tear-off input and a diagnostic GL binary identity, and records acknowledged
steps at0,50,100,150,200,250,300ms. It releases replay before ordinary graceful
shutdown. Full-frame captures are not aligned, transformed or tolerance-adjusted.
Scripts pass syntax validation. Paired execution and pixel equality remain pending.

## Text, caret and menu continuation (2026-09-16)

### Latest measured states

Help lifecycle: GLgl-help-lifecycle-queued-84/native-help-lifecycle-queued-157
has10 exact full-frame samples across opening, departure, closure and reopening.
Request SHA256:4B239049D98118EAB2CEA51A7B2A4319A4A385FB8F1468A767A79F92A9E89EFC.
Native fixture SHA256:9756A552C62C0CF79DAE054FB4B989F0C13ACC3D47E9C5FDE5E63E82E0493D31.
Independent Help close now uses an explicit native close-focus policy, returning
to pre-login default focus rather than the previously focused Preferences control.
LLFloater::closeFloater restores a dependee only for dependent floaters. Source
LLFloater::setForeground releases focus only on a foreground transition; native
control-opacity state is therefore distinct from foreground image selection.
The synchronous GL83/native156 reopen comparison remains1371 pixels different:
native can draw between SendMessage press/release while GL queues the burst.
Queued input on both backends exercises the corresponding batching contract and
matches; it does not erase or close the separate synchronous temporal discrepancy.

Nested picker lifecycle: GLgl-picker-lifecycle-85/native-picker-parent-close-159
matches picker opening, closure, reopening, departure and parent closure in both
samples. Native previously left the picker visible when Preferences closed.
Preferences now closes its owned pickers before restoring its settings snapshot,
through an explicit dependent-close hook. The regression test checks cancellation
and hidden parent/child state. Request SHA256:
773D52677F5187E0F96BA47412169291F00AD3005E5A792C55445062D3EF4376.
Native159 fixture SHA256:F41ABD08717BC81388FE128AAC17559EC741AAFEC6B3E0699A65CFB2845A3F93.

Parent movement: GLgl-picker-movement-86/native-picker-snap-162 has8 exact samples
for parent title press, drag, release and parent closure. Native focus-root history
now retains the valid last focused descendant, matching LLFloater::setFocus instead
of selecting the first editor on reactivation. The epoch guard protects reentrant
focus callbacks; erased groups discard their history and stale targets are rejected.
Positioned native pickers retain their parent snap target and follow parent drag
deltas, matching LLFloater::addDependentFloater/translate. Independent picker movement
clears that attachment. Tests cover parent focus restoration, translation and closure.
Request SHA256:43129521D9F31B3F6EAB5E02EB3ABFA013944B752CC57DBE40AFDDA0AE18C04D.
Native162 fixture SHA256:920E1F988B6CB12E8009B7ADA26E9BBEE0E29D6B14239C02FDEE5CB9B0750161.
Earlier160/161 failures remain retained. All these runs were isolated pre-login
fixtures, exited zero, and required no credentials; GL recorded Goodbye.

Latest implementation gates: Widget210/210, Window7/7, GPU10/10 and the
RelWithDebInfo viewer link pass. Diff whitespace validation passes. Editor diagnostics
show no new native-code errors; the untouched notification_background.html has
existing missing viewport/lang metadata warnings. LNK4020 debugger-metadata warnings
remain. These results do not establish full temporal or fractional-scale interaction
parity, external-browser operator acceptance, or authenticated Help behavior. The
reported full-viewer Vulkan crash remains queued after the current verification TODO;
successful fixture runs and linking do not diagnose that crash.

Initial fractional-scale regressions on the latest native162 binary remain exact:
GLgl-browser-scale-59-075/native-scale-regression-163-075 and
GLgl-browser-scale-65-150/native-scale-regression-164-150, both samples at each
scale. Each native fixture exited zero with Window7. These are initial-modal
regressions, not fractional-scale nested-dialog interaction qualification.
The remaining temporal gate must distinguish equal elapsed-time animation states
from independent wall-clock presentation schedules. Current captures demonstrate
differences in the latter and do not provide a controlled shared elapsed-time oracle;
neither automatic alignment nor relaxed tolerances are authorized by this record.

GLgl-menu-lifecycle-separated-81 versus native-menu-lifecycle-fixed-149 has
16 byte-identical settled samples across detached, pointer-departed, title-press,
drag, release, close, popup reattachment and fresh-detachment states. The initial
menu samples differ only in the username caret phase. The source keeps tear-off
selection on title press, clears it on two-axis translation, and creates a fresh
presentation after close; native now follows those rules. The shared lifecycle
request is revision2, SHA256
26BEA5CD32CFBAAE7613E6EBE9CDCEB429A8908A40AB344B1189694EA13E14C3.
GL80/native147's combined drag burst is retained but invalid for movement acceptance:
GL coalesced the move before the press. The revised sequence separates press,
movement and release. Native149 fixture SHA256 is
B60C97E7A9529AA7A66CB2C5173D68E9B158557704DD6E149790A9BED10E356F.
Later tests cover live visibility-driven size changes, menu background opacity,
title-focus keyboard routing and global accelerators. Those later changes have
widget/window coverage but do not expand the captured menu state matrix.

Continuous tear-off streams are not temporally equivalent: GL81 retained two
intermediate images; native149 had a roughly180ms presentation gap and one
different intermediate. Foreground-window state also differs in these streams.
Neither settled equality nor the deterministic half-life test closes this gate.

GLgl-help-browser-82 versus native-help-encoded-153 has four byte-identical full
frames for Help opening and pointer departure, including Preferences behind it.
The request uses the same local rich page and internal-browser policy, SHA256
F0CE6482A946B174334D0B0DF73BE492EE6275DA4CEA17888C18CF0556798F83.
Native153 fixture SHA256 is
0DA9EE5E9C1B0A6FB0F26B90FB571413CB883E59326F414EDD36E49D5A7464E8.
Both processes exited zero; GL recorded Goodbye and native passed Window7.

The Help corrections follow LLFloater's centering region below the19-pixel menu
strip, LLResizeHandle's11x11 natural image geometry, and LLFloater::updateTransparency
through LLUICtrl::getCurrentTransparency. Native caches paint inputs per floater
with ActiveFloaterTransparency/InactiveFloaterTransparency while preserving the
separate draw-context alpha input. The final five search-field pixels were caused
by unencoded0.95 line-editor background alpha; LLLineEditor::drawBackground uses
the UNORM8 UI vertex-color path. Native now encodes that image tint, with direct
regression assertions. Native150/151/152 retain77844/1521/5-pixel failures.
No pixel-specific compensation or tolerance change was used.

Startup Help restoration is in LLStartUp's post-login path, not the pre-login UI;
it remains deferred with authenticated context/history verification. Browser
interactions, resize/move/close/reopen effects and external-confirmation acceptance
still need their own captures. The user also requested investigation of the
RelWithDebInfo executable's Vulkan crash after the current TODO list is complete;
that investigation is queued, not performed or diagnosed here.

The requested Help/tab checkpoint was committed and pushed as 1374ad9906.
This continuation is uncommitted; overall interaction/effects parity remains open.
The independent GL revision and zero-tolerance acceptance rule are unchanged.

NV-00/01/02/11/12/17: LLNormalTextSegment::getDimensionsF32 passes
no_padding=true through LLFontWidthBuffer to LLFontGL. The native measureRun
argument is includePadding, the opposite meaning. Plain-text layout now passes
false. Its advance-only clip removes the final two picker-title shadow fragments;
no shadow alpha, sampler or triangle-order compensation was retained. Widget210
tests printable glyph widths, and GLgl-dialog-separated-78 versus
native-text-bounds-139 has byte-identical picker click/departure samples.

LLLineEditor resets its own caret clock on focus and accepted editing/selection
operations, not on unrelated window input. Native line editors now own that reset
time. Native UI paint opts into the editor clock while low-level tests retain an
explicit elapsed-time input. Tests cover the one-second delay, half-second phase
boundaries, accepted key/Unicode input and independent editor focus. Widget210 and
Window7 pass. Native-caret-owned-140 retains eight exact Colors/picker samples.
Independent real-time capture phases and the multiline editor clock remain open;
deterministic timer tests do not claim identical end-to-end presentation latency.

Menu source roots are LLMenuItemGL nominal dimensions/draw/onCommit,
LLMenuItemSeparatorGL, LLMenuItemTearOffGL, LLMenuGL::arrange/draw/setTornOff,
LLMenuItemBranchGL activation/highlight, and LLTearOffMenu construction, draw,
focus, updateSize and closeTearOff. Supporting contracts include native equivalents
of LLFloaterView::refresh/adjustToFitScreen, LLView's getNeededTranslation and
LLSmoothInterpolation::calcInterpolant. These are CPU layout/input responsibilities;
the existing native paint packet and completion-owned GPU path remain the consumer.

The native menu uses a shared command/predicate/item model with independent popup
and detached view state. It retains live bindings, check/enabled/visible predicates
and leaf callbacks; it does not copy a stale command list or call GL visual owners.
Declared tear-off rows route to native owned floaters, preserve keyboard handling,
and support close/reopen. Explicit activation of an already detached branch focuses
its owner; hover does not create another popup. Detached base content is painted in
floater order, with only transient submenus in the late popup pass. Tests193/194
cover these model/lifecycle boundaries, outside-click routing and composition.

Popup layout now uses rounded ascender/descender row height, 40 units of plain item
padding and declared shortcut padding only for accelerators. All four vertical
padding units are at the bottom. Accelerator text reserves 22 units on the right;
branch markers use U+25B8. Tear-off lines use the declared disabled color, and
separators use six-unit endpoints. Popup shadows reuse the existing native
alert/floater mesh builder; no GL utility is shared. Detached header growth uses
the source 0.05-second half-life and ceil rounding, with a 16-pixel partial-overlap
constraint and the pre-login floater region below the 19-pixel menu strip.

GLgl-dialog-separated-78/native-menu-shadow-143: the menu region is byte-identical,
but the full frame fails with 1538 username-control pixels. FSPanelLogin::giveFocus
explains this: native had no initial focus to restore after modal dismissal. Native
startup now focuses password only for a nonempty username and empty password,
otherwise the username editor. It does not repeatedly refocus during paint.

The shared capture harness has an opt-in serialized tearOff boolean, preserving
older sequences. GLgl-menu-tearoff-79 records menu-open, menu-detached and
menu-detached-away; its request SHA256 is
1595AABE76B679755D721737B0BD0A572821A514E8119313AA0760A1642D9362.
It exited zero with Goodbye. Native-menu-tearoff-144 exited zero with Window7;
fixture SHA256 is 3A0D3754528DB90DC6A29BA735411702A7B62B15552F7A04543F68F9D7608721.
Menu-open sample0 differs58 pixels; detached samples differ15390 pixels.
These captures predate the screen-constraint correction, which passes Widget210
but has not yet been recaptured. Native141 is retained as an invalid nonmaximized
sequence attempt;142/143 used the required maximized client. No evidence was erased.

Open menu obligations include exact detached visual/temporal qualification,
dynamic size updates, focus/hover details, submenu ownership and opacity settings,
and extended move/close/reattach sequences. Full Help-browser visuals, nested
lifecycle transitions and the previously recorded temporal/fractional-scale gates
also remain open. Login-dependent verification remains deferred.

## Help service continuation (2026-09-16)

The requested Copy/topic checkpoint was committed and pushed as c9df46aa11.
The following Help implementation and tab-clip correction are uncommitted work;
neither checkpoint claims complete native parity.

NV-00/01/02/03/11/12/14/17: the source roots are LLFloater::onClickHelp and
title-button creation, LLUICtrl::findHelpTopic, LLViewerHelp::getURL/showTopic,
LLViewerHelpUtil::buildHelpURL, LLWeb::useExternalBrowser/loadURLExternal,
LLFloaterHelpBrowser's open/close/media-event handlers, and LLURLHistory.
Native title buttons resolve topics at click time over native panels. They honor
FSHideHelpButtons, use Help foreground/pressed skin images and source hover glow,
and resolve the tooltip through the native localized BUTTON_HELP catalog entry.
Minimization uses existing native child visibility retention. Alternate floater
Help image declarations and all dock/tear-off combinations remain unqualified.

Native Help URL construction accepts an explicit metadata map, percent-encodes
topics using the source unreserved set (excluding tilde), substitutes the format,
and escapes URL spaces/backslashes. Empty topics use the reference fallback;
f1_help performs focus-topic lookup and pre-login fallback. The window provides
build/version, OS, language, pre-login null session/region IDs, first-login false,
parcel0 and the source GRID substitution policy. GRID_LOWERCASE is provided only
when present in the login URL. An authenticated window requires Configuration's
Help-context provider instead of inventing session/grid state. Unresolved uppercase
substitution tokens fail explicitly. A complete authenticated provider remains open.

External policy follows PreferredBrowserBehavior and the pinned domain rule.
DisableExternalBrowser suppresses launch. LLWeb's confirmation is an original
WebLaunchExternalTarget notice with okcancelignore, not the unrelated generic
native Windows URL prompt. Approval invokes a dedicated native ShellExecute
callback; Cancel never launches. The unit fixture captures callbacks and never
opens the system browser. Actual external-browser launch and ignored-confirmation
parity have not been operator-qualified.

The internal path constructs floater_help_browser.xml with its own LLVKFloater
and independently owned CEF view, not the general web-content floater. It reuses
the live singleton for navigation, updates localized loading/done status, and
records simplified URL history (query/fragment removed, newest first, limit10).
History remains in the native UI instance; account-file persistence and sharing
with other browser histories are still open. The reference init_history message
has no CEF handler and is not treated as an implemented feature mandate.
Load errors are forwarded from the native browser owner and may navigate the
configured GenericErrorPageURL once per requested Help navigation; the bounded
fallback avoids a failed-error-page loop, not a claim of identical repeated-failure
policy. The generic failure warning remains diagnostic.

Close detaches input/frame publication and requests browser shutdown; the floater
is destroyed on the next paint-preparation boundary or before reopen, outside its
own callback. Reopen has a new widget/browser identity and ignores old close events.
Partial startup failure retires the attempted browser and discards its widget.
Normal close clears HelpFloaterOpen; application quit preserves it for persistence.
Startup restoration from that setting and authenticated history persistence remain
open. Existing frame-slot/upload owners retain GPU versions through completion.

The Help XML's intentionally empty done_text exposed a parser restriction:
native panel-string declarations now accept empty contents while still rejecting
unnamed/nested declarations. No skin/reference XML was changed.

Validation: Widget210/210 includes URL escaping, fallback and all three browser
preferences; confirmation approval/Cancel/disable; missing metadata; partial-open
failure cleanup; dedicated browser widgets; title-button invocation; load/status/
fallback; singleton reuse; normal close/reopen; late-event isolation; and quit
preference persistence. Window7/7 includes two added real-CEF stages using the
existing loopback page, checks the presented browser pixels, closes/reopens Help
with a new identity, and completes the existing graceful shutdown/recovery path.
The final expected stage count is11, not the previous9. These are runtime/lifetime
checks, not a full-frame visual comparison of the Help browser itself.

An attempted generic ShowHelp menu binding was removed after tracing the actual
LLShowHelp callback: OpenSim grid_help/grid_about use grid-provided URLs and
LLWeb::loadURLInternal, not ordinary Help topics. Native grid-menu dispatch is
still open; title-bar Help and direct native Help service calls are connected.

### Tab clip evidence

LLTabContainer::draw clips panel children three units inside its sides.
LLLocalClipRect adds one device pixel to both width and height; native exclusive
clip representation lacked that allowance. Its tab-content producer now adds the
same endpoint allowance, and the existing overflow test checks the encoded bound.
This corrected the missing panel-border endpoints at screen1613,460 and1613,938;
no border-coordinate patch or tolerance adjustment was used.

Retained GLgl-dialog-separated-78/native-help-parity-136,2560x1369,100%,en,
AnisotropyOff: Colors click and departed states are byte-identical in both samples.
Preferences departed sample0 is also byte-identical; its other sample and the
click samples differ by30 caret pixels. Picker click/departed states differ by
two title-shadow pixels at1701,419 and1701,421, four channel levels each, in both
samples. They remain failures under the exact criterion. Native136 fixture SHA256:
6DEF1A8C874A63470385F4E2631D7743D9AD1B3DDEB8146B4B7929C2DF7B7224.
Native135 retains the preceding Help-icon pass with the two panel-border failures.
No reference or capture transformations were changed. Menu tear-off, full Help
browser visual/effects parity, remaining shadow/caret timing and broader nested
lifecycle/temporal checks remain open. Login-dependent verification stays deferred.

## Post-checkpoint continuation (2026-09-15)

The browser/dialog checkpoint was committed and pushed as 4154e1e879 on
native-error-messaging. That checkpoint records partial parity, not completion.
The following continuation is separate, uncommitted work.

NV-00/01/02/11/12/17: Copy's remaining mismatch was caused by the native button
overlay default, not texture sampling. Pinned LLButton::Params defaults
image_overlay_alignment to center; LLVKButton::Params incorrectly used Left.
Native now defaults to Center; explicit left/right overrides are unchanged.
Tests in the existing button group verify centered and explicit-left rectangles.

Bounded native132/133 diagnostics confirmed identical occupied source pixels,
18x18 logical size,32x32 padded storage, UV0..0.5625, and anisotropy disabled.
The actual native quad was x1588..1606, while the source-centered GL overlay was
x1589..1607. No source pixels, UVs, sampler, shader, capture transform or tolerance
were changed. Temporary pixel/quad instrumentation was removed after diagnosis;
its outputs remain in native-copy-diagnostic-132/133 for provenance.

gl-dialog-separated-78 versus native-overlay-parity-134: Copy has zero differing
pixels in both settled samples of Preferences, Colors and the nested picker.
Complete frames still differ52 pixels for Preferences/Colors and104 for the
picker, in both samples, chiefly missing Help icons plus isolated edge pixels.
Native fixture SHA256:
C1BC6564195B9A8FFA92B2207F3134464A471614C0C2B3E74ED8603868DC3923.
The fixture exited zero with Window7/7. The overlay regression passes Widget210/210.

Help implementation has begun at its native data owner:
LLVKWidgetTree::findHelpTopic follows the pinned LLUICtrl::findHelpTopic search
precedence over visible descendant panels, selected descendant tabs, then
panel/ancestor topics. As in the reference, descendant searches stay rooted at
the originally queried control while ancestor panels are considered. Tests cover
hidden topics, tab changes, button-to-parent fallback and invalid controls.
This is CPU-only lookup over native nodes, not reuse of GL UI functions.
It does not yet connect a visible Help control or complete the Help service.

The next Help boundary remains URL metadata and dispatch, including configured
external/internal browser policy, dedicated Help-browser lifecycle, URL history
and error-page behavior. Source tracing found no CEF handler for the reference
init_history plugin message; do not turn that no-op into a feature mandate.
LLURLHistory's actual add/remove/limit behavior and LLMediaCtrl's configured
error-page redirect remain relevant. Live session/grid substitution ownership
is unresolved; guessed values and an inert Help icon are not acceptable closure.
Menu tear-off, remaining edge pixels and temporal/nested lifecycle checks remain
open. Login-dependent verification remains deferred.

## Interaction and nested-dialog work in progress (2026-09-15)

The preceding scale slice was committed and pushed as c869492616 before this
work. This section is explicitly incomplete: browser interaction corrections have
measured settled-state passes, but menu, Preferences, nested-picker and temporal
parity are not closed. New code and evidence remain uncommitted.

### Browser publication and scheduling follow-up

The browser-focused follow-up retains three production corrections under
NV-00/01/02/03/09/11/12/13/14/15/17:

- LLVKGlyphUpload::submittedFor permits consumption only on the exact originating
   logical device, allocator, graphics queue and family. Browser streams opt in;
   published() and default LLVKImagePublication remain completion-observed APIs.
   The upload's TRANSFER_WRITE to SHADER_READ barrier and final shader-read layout
   precede the consumer submission on that externally synchronized owner-thread
   queue. Staging, command pool and upload fence remain owned until completion;
   consuming frame slots independently retain the image and descriptors until
   their own completion. No per-frame CPU upload wait or device-idle wait was added.
   The GL source contract is LLViewerMediaImpl::updateMediaImage/preMediaTexUpdate/
   doMediaTexUpdate, including the LLImageGLThread::sEnabledMedia worker branch;
   its effective reference worker setting remains unqualified, so this is not a
   claim that all GL uploads complete synchronously in the same frame.
- Pinned LLWindowWin32::LLWindowWin32Thread::run requests the device-supported
   multimedia timer period nearest 1 ms. Native LLVKWindowMgr::run now independently
   acquires that CPU-only Windows facility and releases it on every exit. Failed
   capability/acquisition is explicit. Diagnostic native113 observed mean work
   2.22 ms and wait 26.47 ms (195 of 221 waits over 20 ms), despite requesting
   16 ms. Native114 with the timer request observed work 2.27 ms, wait 14.97 ms
   (zero of 290 waits over 20 ms). Bounded instrumentation was removed; both logs
   are retained as frame-timing.log in their capture directories.
- Pinned slplugin.cpp subtracts elapsed work from its selected plugin sleep
   interval. Native now accounts for work within its declared 60 Hz browser/frame
   budget instead of adding a full 16 ms after rendering. Plugin normal/high
   priority rates differ; this is the elapsed-budget policy, not a claim that all
   effective GL/native pump frequencies are identical. No animation setting or
   synthetic input delay was changed.

GPU10/10 passes with validation enabled, including a real consumer submission
before CPU upload completion observation; all four compatibility rejections;
default completion-only publication; cancellation/epoch replacement; retained
consumer ownership; and retirement after final consumer release. Window7/7 passes.
These checks establish the tested queue/lifetime path, not other-device coverage.

Continuous captures native112 (queue publication), native114 (timer request),
native115 (work-accounted pacing) retain the progression against GL72. All ten
settled-0 states match. Observed scroll settlement changed from native103 259.3 ms
to native115 193.3 ms, versus GL72 188.1 ms; these single runs are not a timing
tolerance or performance average. Native116 tested a queued browser-input owner
and modifier snapshot API. That probe did not remove the long synchronous edit
delivery (129.85 ms); both changes and their generated headers were removed.

The shared fixture now has opt-in QueuedInput, recorded in the request, manifests
and completion record. It uses PostMessage instead of waiting for each synchronous
SendMessage handler. Neither is physical hardware input. Native's window handler
shares its render thread, whereas GL forwards from its separate window thread;
synchronous delivery therefore stretched the same key burst over many native
frames. Queued delivery accepts the burst in 1.49 ms native versus 1.06 ms GL and
removes the partial-edit frames without changing production input semantics.

GL74 (gl-browser-queued-74) and native117 (native-browser-queued-117) have full-byte
equality for both samples of all ten states: 20/20, no masks, transforms or relaxed
tolerances. The native request was generated by GL73 with anisotropy unspecified;
GL73 was invalid for this comparison because the qualified reference uses Off.
GL74 corrects that setting; native browser sampling is independently fixed to
BrowserLinearRepeat, unaffected by the skin-anisotropy setting, and native117 is
retained rather than rerun. GL74 exited zero with Goodbye; native117 exited zero
with Window7/7. Native executable SHA256:
5A8E3FEA88D3DE34A386D142F07F5D79E910699D7950173F132C9BFFC569DA02.

Hover, press, release, focus/caret, editing, popup opening (including intermediate
hash eed0eb2fa633), and selection have identical ordered distinct image identities
in those queued captures. Their timestamps are not identical. Scrolling still has
different sampled intermediate images. GL72 versus GL74 also has different sampled
scroll/reset sequences; that control includes different input delivery modes and
is evidence against assuming deterministic sampling, not proof of equivalent
animation laws. Exact temporal parity remains OPEN: independent real-time WGC
samples cannot certify common-clock animation output. No GL oracle, CEF baseline,
pixel tolerance or animation policy was changed to obtain a pass. Login-dependent
verification remains deferred and unrelated dialog failures below remain open.

### Remaining-dialog correction pass

The subsequent no-login pass is still incomplete. Native127 compared to GL70
has 252 differing Preferences pixels, 3162 Colors pixels, and 496 nested-picker
pixels in each of the two settled samples. No tolerance or reference image was
changed. The initial notification is byte-identical. Native fixture SHA256:
09742678E271624FBE2B1980337A66299699C4C6C8E155C68458A415E87C5BA5.
The runs closed normally, native Window7/7 and GL76/77 exit zero with Goodbye.

NV-00/01/02/11/12/17 source and implementation record:

- LLSpinCtrl's constructor supplies its declared font to the child editor.
   Native resolveSpinner now propagates both the font request and object before
   resolving the editor. Existing composite-control tests assert font identity.
   Native118 reduced Preferences 1015 to 949 pixels and picker 37160 to 36463.
- The factory retained radio-group control defaults but dropped its view defaults,
   notably follows=left|top. Native radioView now retains those defaults. GL76
   getInfo observations showed the radio group at screen bottom/top 728/748,
   versus native119 730/750; native121 matches the GL owner and label rectangles.
   LLCheckBoxCtrl::reshape expands the button through the fitted label top.
   Native radio finalization now runs that existing reshape path, matching the
   measured GL button bounds. Native122's entire General radio/spinner row is
   exact. Tests cover inherited follow flags, button bounds and pure-parent moves.
- LLButton image-overlay and glow paths reach LLRender::color4fv through
   LLUIImage::draw/drawSolid. Native now truncates their tint channels to UNORM8
   before blending, as already done for base images. Copy remains non-exact;
   its 18x18 source, padded32x32 representation, clipping, decode and button
   screen rectangle were inspected and are not a justification for arbitrary
   resampling. Full-frame reports retain this residual.
- LLFloaterColorPicker::draw/drawPalette and LLColorSwatchCtrl::draw use encoded
   solid colors. Native picker palette, luminance, selected fill and marker plus
   swatch fill now use UNORM8 channels; stored control colors remain unchanged.
   Swatch border alpha is independent of fill alpha. Native rectangle primitives
   reproduce the inspected integer outline coverage, including the picker's
   inverted vertical endpoints and ordinary palette rectangles. Crosshair lines
   occupy the negative side of integer coordinates. No hue texture modification
   was needed. Native123 removed 7778 picker differences from color encoding;
   native124 reduced the full picker mismatch to 5396 after swatch/edge changes.
   Fractional-scale coverage of these new primitives remains unqualified.
- LLTabContainer::addTabPanel explicitly gives horizontal tab buttons right
   padding2. Native now preserves that override, eliminating the displaced
   horizontal labels. Together with glow encoding, native125 Colors was416 pixels.
- LLViewerWindow's hover-set reconciliation calls onMouseLeave for departed
   views. Native had a mouseLeave implementation but no routed invocation, leaving
   button highlights active after departure. Native updatePointerHover maintains
   membership over visible hit bounds, top/capture ownership and front-to-back
   opaque views on routed hover events. It dispatches membership-change callbacks
   and uses checked coordinate subtraction. Tests cover departure, covering-view
   occlusion and invalid roots. Native126 removed2746 nested-picker pixels, but
   changed the earlier Colors capture: both retained GL70 and GL76 keep that tab
   highlight after the fixture parks the cursor, clearing it after the swatch
   click. Native now clears it at departure. This is an OPEN interaction mismatch,
   not a claimed pass. Per-frame stationary-pointer and window/menu interception
   cases are not qualified by the routed-event tests.

Read-only diagnostics: the native fixture emits native-widget-geometry.txt at
sequence completion. The LEAP driver requests bounded Preferences child geometry
after its settled capture and writes gl-dialog-geometry.xml. GL75 lacked that
file because the driver exited after the notification response; GL76 corrected
that lifetime and produced the decisive radio/Copy/editor bounds. A second-stage
picker probe in GL77 did not produce its file and was removed; do not treat it as
evidence. The restored one-stage driver builds and passes its framing self-test.
No GL source or reference executable was edited. Diagnostic Copy crops are not
acceptance images; all reported differences use complete untransformed frames.

Open work includes functional Help service and title buttons, menu tear-off,
Copy edge coverage, Apply-now checkbox geometry, remaining picker endpoints,
the Colors hover discrepancy, and nested close/reopen/move/focus transitions.
Help was traced through LLFloater::onClickHelp, LLUICtrl::findHelpTopic,
LLViewerHelp::getURL/showTopic, LLViewerHelpUtil and LLWeb browser policy.
The internal branch requires the dedicated Help browser's lifecycle/history/
error-page/status behavior; no inert Help icon or general-browser substitute was
added. Exact browser temporal parity and login-dependent checks remain open.

Later observations in the same pass supersede the open hover/checkbox hypotheses
above, without removing their failed intermediate evidence:

- LLWindowWin32::gatherInput processes one coalesced mouse move before queued
   button events. Parking the cursor in the same fixture action as clicking a tab
   therefore leaves GL at the click position in that frame. The shared dialog
   runner now captures preferences-click, colors-click and color-picker-click
   separately, then captures each pointer departure. GL78/native128 demonstrate
   exact Colors-tab pixels in all click/departure states. Native hover cleanup is
   retained; the earlier apparent regression was an input-order mismatch, not a
   reason to preserve stale hover. Full-frame parity still fails elsewhere.
- LLUICtrlFactory::createWidgetImpl calls initFromParams after child construction;
   LLView::initFromParams unconditionally invokes virtual reshape. Native factory
   checkbox finalization now performs that step, including equal-size reshapes.
   Direct createCheckBox remains a constructor-level API: putting this step there
   broke the intentional construction-padding test83 and was repaired at the
   factory boundary. Radio child finalization follows the same policy. Native129
   removed all222 Apply-now checkbox differences. A test asserts the factory
   button reaches the fitted label top.
- LLViewBorder::drawOnePixelLines uses identical endpoints with and without focus.
   Native settled one-pixel focus now uses the verified integer-line coverage and
   UNORM8 focus color. Native130 removes the194 Colors swatch-border differences.
   The test advances its clock beyond focus flash before checking this settled
   contract. Animated-width/fractional focus borders remain unqualified.

Current retained comparison: gl-dialog-separated-78/native-dialog-separated-130,
maximized2560x1369,100-percent,en,AnisotropyOff. Preferences sample0 differs222
pixels and sample1 differs252 (caret phase); Colors differs222 in both samples;
picker differs274 in both samples. Residuals include missing functional Help,
Copy image coverage, caret timing and isolated edge pixels. The menu still differs
8013 pixels. No remaining frame is accepted by subtracting those pixels. Native
fixture SHA256 B28D474A41E3E8A75E959D218B7EAF0D77414667625FF4170A958113E06661A5.
Copy source and both deployed PNGs have identical SHA256
75E1F2CF517567AE074C5D20826727DFC3648FBA9AA4113E632C53D18B70150C;
asset drift is ruled out for that file, not the rendering mismatch.

Because hover routing is shared, native-browser-regression-131 repeated the
queued browser sequence against retained GL74: all20 settled samples remain
byte-identical. This does not close the separate temporal gate. All new runs use
isolated pre-login fixtures and close normally; no authentication was attempted.

### Original shared fixture record

run_browser_sequence.ps1 sends the same physical Win32 pointer/key/wheel sequence
to each backend in isolated maximized 2560x1369, 100-percent, default/en fixtures.
The local browser_parity.html bytes and pinned GL revision
59108e15a1f8f94d2da7c674d937d19f5cf9450d remain unchanged. The native owner dismisses
the initial notification, signals readiness, continues pumping frames, asserts
PreLogin with disabled Log In, and closes after external sequence completion.
The runner restores the cursor/releases held input and records action/capture
intervals. No DOM mutation or real credentials are used. Dialog navigation uses
the real Viewer menu, Preferences, Colors tab and My text swatch.

The first key fixture omitted scan/extended bits, so GL ignored Home/Delete and
appended text while native replaced it. Valid Win32 key encoding fixed that input
mismatch; no native key semantics were changed. GL66 retains the malformed-key
failure; GL67 failed because a persistent PowerShell helper type lacked the new
method. The separately named key mapper fixes that fixture issue. GL68 is the
valid browser sequence. State names and sequence-complete markers alone are not
acceptance: raw images must show the intended outcome and compare exactly.

### Browser corrections and results

NV-00/01/02/03/11/12/13/14/17: reference LLMediaCtrl::handleScrollWheel passes clicks
through LLViewerMediaImpl::scrollWheel and LLPluginClassMedia to MediaPluginCEF's
scroll_event handler, which multiplies by -40 before Dullahan::mouseWheel. Native
had passed raw Windows wheel deltas (120 per click), scrolling three times too
far. Its window input now converts click counts to the same 40-unit CEF policy.

The reference CEF plugin uses flip_pixels_y=true. Dullahan::copyPopupIntoView
starts at height-popup.y for that orientation, producing a popup one row above
the nonflipped branch used by native. Native now composites the same in-bounds
placement into its own CPU buffer through LLVKBrowserSurface::compositePopup.
Clipping handles negative/top/right/bottom positions without reproducing unsafe
upstream edge copies. Generated native Dullahan also reallocates popup storage on
size changes and rejects mismatched paint sizes; the fetched source and GL plugin
remain untouched. Existing immutable frame publication and Vulkan upload/retirement
are unchanged. Widget210/210 includes popup offset, clipping and malformed-size
checks. Native Window7/7 passes after integration.

GL gl-browser-sequence-68 versus native-browser-sequence-92 has zero differing
pixels for ready, pressed, released, edited, select-open, select-changed, scrolled
and scroll-reset. Reports are browser-sequence-parity-68-92-<state> under
glref-build/captures. Native92 hover showed the unhovered state (1381 pixels
different); it remains an unexplained input/state observation, not erased by a
retry. Focused native-browser-hover-93 matches GL68 hover exactly, with added cursor
observations, report browser-hover-parity-68-93. Earlier native91 had exact hover
but incorrect popup placement (905 pixels) and wheel travel (775367 pixels).
Editing's valid reference hash equals the retained native91 edited frame.
These results qualify the stated settled states, not a reliably passing complete
sequence or every possible browser control and input device.

### Dialog corrections and remaining failures

The real menu comparison (GL69/native94) differs in 8013 pixels. Native lacks the
reference tear-off row and differs in width/shadow; tear-off behavior is not
implemented by a decorative substitute. The original pointer y=38 hit native's
separator; y=34 lies inside both actual Preferences command rectangles and opens
both parents. The menu mismatch remains open.

Reference LLFloater::initFloaterXML expands the outer top by positive
header_height-legacy_header_height after constructing children, without moving
them. Native parsed legacy_header_height but never applied it. Native now parses
header_height and performs an idempotent, bounded, non-reshaping header expansion
before creating chrome. Guidebook's source outer height is consequently 532 rather
than 525; move/reopen tests preserve that size. Reference LLFloater::center uses
the floater view below the status bar. Preferences now supplies that placement
region to the native floater owner instead of centering in the full login root.
Other placement callers are not thereby qualified. Radio items now apply inherited
checkbox label/button rectangles rather than leaving those defaults unresolved;
tests verify their 20/2-pixel left offsets.

Reference LLFloaterView::findNeighboringPosition places dependent floaters right,
left, below, then above, considering nearby dependents and bounded margin growth.
Native color pickers now use that owner-relative policy and initial Select-button
focus, matching LLFloaterColorPicker::postBuild rather than selecting Red.
This is independent CPU placement over native owners, not GL visual code reuse.
It does not yet qualify every dependent-close, movement or focus-restoration path.

Retained failures (full-frame, tolerance zero):

| Comparison | Differing pixels | Status |
|---|---|---|
| Preferences GL69/native95 | 172697 | Before header/placement correction |
| Preferences GL69/native97 | 24859 | After header/placement correction |
| Preferences GL70/native98 | 24829 | After radio layout correction; still fails |
| Colors GL70/native98 | 38600 | Still fails |
| Picker GL70/native98 | 326870 | Centered over parent; fails |
| Picker GL70/native99 | 59984 | Neighbor placement/focus corrected; still fails |

The nested picker genuinely opens on both backends. Remaining differences include
floater title/control chrome and missing help/shadow treatment, tab labels/borders,
disabled-control styling, time-format state, picker control alignment and parent
focus presentation. No nested-dialog parity claim or tolerance relaxation is made.

Further local corrections: LLFloater::updateTitleButtons uses UIFloaterCloseBoxSize
(16), UICloseBoxFromTop (5), a one-unit border and size+1 spacing, instead of native's
hardcoded 18-unit buttons. Existing native close/minimize/restore/dock actions now
use those dimensions. LLDragHandleTop::reshapeTitleBox supplies a 14-unit left
offset, measured line-height rectangle and five-unit top inset; native title text
now uses its independent factory-resolved SansSerif font and those bounds. Native
plain text does not yet support the reference font_shadow declaration; an attempted
attribute was rejected by tests and removed, so title-shadow parity remains open.
LLTabContainer explicitly defaults label_shadow=false, unlike ordinary buttons;
native TabDefaults now preserves that default and XML override. This reduced the
Preferences comparison to 15919 pixels and the picker comparison to 49995 pixels
(GL70/native-dialog-sequence-101, reports dialog-sequence-parity-70-101-preferences
and dialog-sequence-parity-70-101-color-picker). Both still FAIL.

Subsequent source-backed corrections reduced, but did not close, these failures:

- LLFloaterPreference initializes time_format_combobox from Use24HourClock and
   its commit callback sets that preference and calls the once-only ChangeLanguage
   notice. Native now implements that selection and callback, includes the setting
   in its Preferences snapshot, and tests Cancel rollback and notice suppression.
- LLFloaterView::highlightFocusedFloater treats a floater and its dependents as
   one foreground group. Native painting now receives an explicit focus group for
   color-picker owners/dependents, with Window_Background available for other
   floaters. The first topmost-only probe (native105) was wrong for dependent
   groups and produced 384814 nested differences; native106 corrects that grouping.
   Other dependent-floater families still need equivalent relationship coverage.
- LLFloater::drawShadow supplies one border-inset shadow before the image, using
   the full offset for foreground and one-fifth rounded offset/half alpha for
   background. Native uses its existing independent shadow triangles with explicit
   floater parameters and preserves separate alert passes.
- Combo subcontrol resolution no longer resets the generic button-image baseline
   to customized images. The reference LLButton constructor compares against the
   generic defaults to derive disabled-selected fallback and global disabled fade.
   This removed 4512 pixels from the disabled maturity combo comparison.
- Plain native text now supports explicitly requested soft shadows using the
   existing GPU text path, including linked/selected runs; floater titles request
   that treatment. The earlier rejected font_shadow attempt is superseded.
- Nonfocused one-pixel borders now reproduce integer GL line coverage on the
   left/bottom edges and UNORM8-truncated RGBA. Focused/two-pixel branches were not
   changed. Tests cover the packet coordinates and quantized alpha; fractional
   border coverage beyond the measured 100-percent state remains unqualified.
- Radio items resolve inherited checkbox label/button rectangles, and use the
   radio-group font unless the item explicitly supplies a font. Generic native
   parent reshape no longer recursively reshapes children on a pure translation,
   matching LLView::reshape's size-delta gate while keeping directly requested
   checkbox reshapes. This removes an unintended radio-button height change when
   opening a floater. Residual radio/spinner text differences remain.

Latest full-frame reports: dialog-parent-parity-70-111 has 1015 differing pixels
(Preferences); dialog-picker-parity-70-111 has 37160 (nested picker). These remain
FAILURES. dialog-initial-regression-111 remains an exact match to the retained
100-percent rich-page modal. Remaining parent differences include the missing
Help button/service, copy-button treatment, radio/spinner labels, and a caret-phase
sample. The native menu still lacks tear-off behavior and measured menu parity.
Nested dismissal/reopen, parent-close propagation and movement are not accepted
by the open-only sequence. Help cannot be closed by adding an inert icon: its
reference service resolves visible child topics and browser policy/URL expansion,
which native has not yet implemented equivalently.

### Transition evidence

Action and capture start/end timestamps are recorded, but each WGC helper launch
takes roughly 114..207 ms before returning its first frame. GL68/native92 mostly
contain only the settled appearance; scroll and reset contain two unique frames,
with unmatched intermediate hashes. Independent frame indices are not matching
animation times. Full temporal parity therefore remains unverified. It requires a
persistent capture session armed before input, frame presentation timestamps and
matched event-relative timing; the current sparse samples must not be relabeled
as a transition pass. No previously passed initial-state captures were overwritten.
That measurement limitation was subsequently addressed with notification_capture
--stream: a bounded WGC session signals READY after its first frame, stays active
before/during input, and records every delivered frame's SystemRelativeTime and
QPC observation. Unchanged pixel buffers reuse the prior raw file, not the prior
timestamp. The shared runner records action-start/end QPC and frequency on the
same machine clock. It preserves the one-shot capture path; the known-white-window
self-test still passes. Continuous capture is currently opt-in with -Continuous.

GL71/native102 exposed the editing-focus race again. Splitting field-focus into a
separate observed state gave GL72 the intended edited value, matching GL68. The
matched-focus continuous runs gl-browser-continuous-72 and
native-browser-continuous-103 have exact settled-0 hashes for all ten states,
including field-focus. This does not imply both caret-phase samples always match,
nor erase the prior hover observation. The continuously observed caret images
match, but timing differs. For example, first changed hover frames were 37.3ms GL
versus 86.2ms native after action end; press was 39.5ms versus 76.8ms; scrolling
reached its settled image at 188.1ms versus 259.3ms. Intermediate scroll hashes
also differ. These are single-run observations, not averaged performance claims
or approved timing tolerances, and transition parity remains a FAILURE.

A bounded pacing probe replaced the unconditional native 16ms post-frame wait
with the remaining frame budget. Native-browser-continuous-104 improved some
events but worsened or left others unchanged and still differed temporally. That
probe was reverted rather than retained as an unproven fix. Browser publication,
CEF scheduling, event batching, capture cadence and reference variability still
need disambiguation. No frame-time alignment, resampling or tolerance widening has
been used to turn these results into a pass.
Final current-code gates: Widget210/210, Window7/7, GPU10/10 with the existing
Khronos-layer assertion, viewer link, PowerShell parser and git diff --check pass.
These gates do not override the open visual failures above. No follow-up commit or
push has been made beyond the requested initial c869492616 checkpoint.

## Continued no-login scale checks (2026-09-15)

The prior verified browser/locale slice was committed and pushed as 1f95de3506
on native-error-messaging before this continuation. The following new work is a
separate slice. The broader no-login TODO remains open; no authentication was run.

NV-00/01/02/11/12/17, unchanged reference revision
59108e15a1f8f94d2da7c674d937d19f5cf9450d: the 75-percent rich-page comparison first
differed in 3911 pixels. The browser content matched; 1351 pixels belonged to the
modal and 2560 to one full-width header row. LLNotificationAlertHandler initializes
its centered channel using getWorldViewRectScaled(), whose layout derives from
the ceil-rounded outer root. Native had centered on the separately rounded login
panel. At 2560 pixels / 0.75, those widths are 3414 and 3413 respectively. Supplying
the explicit outer viewport to native notice positioning removed all modal
differences without changing the login root or calling GL visual helpers.

LLLayoutStack::draw clips each child panel, and LLScreenClipRect::updateScissorRegion
uses floor(origin * scale) + ceil(extent * scale) + 1 for the upper boundary.
The world panel's inclusive clip overwrites the lower edge of the status/menu
region. Native's synthetic full-width 18-unit backing did not preserve that
boundary at fractional scale. The native viewer now supplies the world-clip
boundary to menu painting. Independent triangle geometry represents its fractional
logical position; menu fills intersect that boundary with their original bounds,
while the backing follows the boundary itself. Layout/hit rectangles remain
unchanged. At 75 percent the boundary is device y=1357; at 150 percent it is 1342,
while the menu's own lower edge is 1342.5. No generic rectangle rounding, shader,
sampler, upload, descriptor or retirement policy changed.

Read-only LEAP layout diagnostics now include the status container, menu holder,
login menu, login holder/root and browser. GL runs 60/61 supplied missing geometry;
run 60 reproduced run 59's exact image. Intermediate layout snapshots can precede
settled placement and are not themselves pixel acceptance. Removing native header
backing was a rejected probe (native85); the backing was restored. Native84 isolated
the remaining header row, native86 left 88 menu-edge pixels, and native87 closed
the 75-percent comparison. A 150-percent menu-fill extension was corrected to an
intersection; native88/89 remain retained failures.

The first 150-percent GL capture (62) contained an extra WarnForceLoginURL modal
and was invalid for comparison. A direct LoginPage probe (64) was overridden by
startup and failed readiness; it was reverted and closed gracefully. The actual
cause was LLWindowListener::mouseEvent using a path's logical center as physical
input. The driver now supplies scaled physical coordinates for warning dismissal
and verifies that its control is no longer visible before submitting the fixture
notification. ForceLoginURL still supplies the same loopback HTML. No warning
template, GL implementation, reference image or tolerance was modified.

All passing comparisons are maximized 2560x1369, default skin/en, anisotropy off,
the unchanged browser_parity.html, MediaPluginFailed, and full-frame tolerance zero.
Capture/report directories below are under glref-build/captures.

| Scale | GL | Native | Report | Differing pixels |
|---|---|---|---|---|
| 75 percent | gl-browser-scale-59-075 | native-browser-scale-87-075 | browser-scale-parity-87-075 | 0 |
| 100 percent regression | gl-browser-content-53 | native-browser-scale-87-100 | browser-scale-parity-87-100 | 0 |
| 125 percent regression | gl-browser-content-52 | native-browser-scale-87-125 | browser-scale-parity-87-125 | 0 |
| 150 percent | gl-browser-scale-65-150 | native-browser-scale-90-150 | browser-scale-parity-65-90-150 | 0 |

75-percent SHA256: 91EEFB88840F40B4F62D0021575C48E609EA958D7549450B905672D35ADA79F8.
150-percent SHA256: C6E6356821942343DF35F1388B4DAF9A886A692728B0A6C317FA89AD752983B6.
The final 150-percent intersection leaves the previously verified 75/100/125
geometry unchanged. Widget210/210 covers outer modal centering, fractional backing
geometry and non-expanding menu fills; Window7/7 and LEAP framing self-test pass.
Final GPU10/10 passes with the existing explicit Khronos validation-layer check,
and the native viewer link succeeds. Both settled samples on both backends were
hash-verified for every passing row above. This continuation remains uncommitted.
The capture runs retain D3D11/no-opengl32 child-module checks. Existing PDB warnings
remain. These results qualify initial presentation at these scales, not browser
interaction sequences, nested dialogs, transition timing, other themes, physical
DPI changes or login-dependent cases.

## Rich browser visual differences traced and corrected (2026-09-15)

The controlled rich local page now matches the unchanged pinned GL captures
exactly at both 100 and 125 percent UI scale. This supersedes the browser-blocker
status below for these two initial presentation states, not the whole no-login
TODO or arbitrary browser workflows.

### Source contract and discriminating checks

NV-00/01/02/03/11/12/13/14/15/17 apply. Pinned GL source remains
59108e15a1f8f94d2da7c674d937d19f5cf9450d. The traced roots are MediaPluginCEF
construction/initialization, Dullahan's OnBeforeCommandLineProcessing,
LLViewerMediaImpl::updateMediaImage/preMediaTexUpdate/doMediaTexUpdate,
LLViewerMediaTexture construction, LLImageGL initialization and LLMediaCtrl::draw.

1. Reference MediaPluginCEF initializes mDisableGPU=false, passes it into Dullahan,
   and registers browser callbacks. The gpu_disabled receive branch has no sender
   in the inspected reference tree. Native had instead forced disable_gpu=true,
   adding disable-gpu and disable-gpu-compositing switches. No font-raster override
   was found in these paths. The coherent CEF 152 native build still differed in
   exactly 4101 pixels at 100 percent (native-browser-content-76), with the same
   image hash as before dependency alignment. A separately identified current GL
   CEF 152 diagnostic (gl-current-cef152-browser-58) was byte-identical to pinned
   GL capture 53. Thus CEF version was not the cause in this case. Enabling native
   CEF GPU rasterization with an explicit D3D11 ANGLE backend removed all 4101
   differences (native-browser-content-78), without changing page bytes or colors.
2. Reference media pixels occupy the lower-left region of a power-of-two texture,
   initialized opaque white; RGB upload ignores browser alpha. LLMediaCtrl samples
   through media-size/texture-size UVs and applies fractional UI transforms directly.
   Native now owns equivalent padded bottom-up RGBA pixels, preserves logical
   dimensions, and supplies the occupied UV region. Padding alone left the
   unscaled image unchanged (native-browser-content-77); it is not claimed as the
   cause of the text mismatch. The earlier direct fractional browser quad remains.
3. LLImageGL defaults to TAM_WRAP, and the media constructor does not override it.
   Native streaming publication incorrectly used the skin linear-clamp sampler.
   After the raster correction, 125 percent differed in only 1156 pixels, at the
   right browser edge (native-browser-content-79). An explicit BrowserLinearRepeat
   sampler removed all 1156 differences (native-browser-content-80). Skin clamp
   and glyph nearest-repeat policies remain unchanged.

### Native design and safety

CEF is independently built third-party functionality, not a viewer GL visual
wrapper. Native's Dullahan command-line setup explicitly selects use-gl=angle and
use-angle=d3d11. Its CEF child uses D3D11 for browser rasterization; native still
receives CPU BGRA callbacks, copies immutable frames, uploads through Vulkan and
presents only the Vulkan-composed UI. No desktop OpenGL context or viewer GL draw
path is requested. Native WebGL remains disabled and is not qualified here.
CEF's existing error, callback, multiple-view lifetime and final shutdown owners
are unchanged. EGL/GLES runtime files required by ANGLE are staged from the same
CEF 152 package; their presence is not desktop OpenGL rendering.

The image producer retains logical dimensions separately from padded allocation
dimensions. Padded allocations are bounded to 16 million pixels, alpha stays
opaque, and publication pairs each immutable source with its matching uploaded
resource. Existing upload fences, image barriers, noncoherent handling, descriptor
ownership and completion-based retirement are unchanged. Sampler selection uses
the existing device linear-filter capability check and no new shader ABI.

Capture 82 observed five live Dullahan children, one loading d3d11.dll, and none
loading opengl32.dll; the existing parent no-OpenGL-module assertion also passes.
The module snapshots are runtime evidence at capture time, not exhaustive API
tracing on every platform. An initial probe accidentally inspected the transient
WGC helper and failed; capture 81 is retained as a failed fixture run. The corrected
probe restricts inspection to Dullahan children and flushes diagnostics before
assertions. Unsupported D3D11 environments and CEF failure/fallback behavior on
other devices remain unverified; no OpenGL fallback was added.

### Exact measured results

All comparisons below use unchanged browser_parity.html bytes, default skin/en,
anisotropy off, maximized 2560x1369 WGC RGBA8 client captures and tolerance zero.
Paths are under glref-build/captures. Existing reference captures were reused.

| UI scale | Retained GL | Final native | Report | Differing pixels |
|---|---|---|---|---|
| 100 percent | gl-browser-content-53 | native-browser-content-82 | browser-content-parity-53-82 | 0 |
| 125 percent | gl-browser-content-52 | native-browser-content-80 | browser-content-parity-52-80 | 0 |

100-percent SHA256:
B5D2EC4FFB3BB12ED5BE998A10314E0D6DF78D92F29A59C0943F662E8B8054B7.
125-percent SHA256:
0674BC37F666859EA705D7D3E95B0D4B995A9F1347093FCA933633DD74E8BCEB.
The final 100-percent run includes the repeat sampler and CEF module checks;
125 percent uses the same production changes before adding that diagnostic check.
Widget210/210 verifies padded browser stride/UVs and immutable copies. GPU10/10
passes on AMD Radeon RX 9070 XT with the new sampler and existing publication,
retirement and fractional-quad checks. Window7/7 passes, including native browser
mouse, keyboard, wheel, close and reopen; these functional tests do not establish
visual parity for the interaction sequence. Existing LNK4020 PDB warnings persist.
The Windows GPU test now asserts that VkLayer_khronos_validation.dll is loaded;
GPU10/10 passes with that assertion, rather than merely requesting validation.
The viewer was relinked successfully with the final production browser changes.

The current-GL diagnostic has source base ab4ed0923c61f594329b3bb8341ca4b6129fece8
plus working-tree changes, not the historical reference revision. Its mislabeled
sidecar was corrected without changing captured pixels. The GL runner now rejects
non-oracle binaries unless an explicit diagnostic ReferenceRevision is supplied.
No oracle baseline, tolerance, GL implementation or page styling was changed.
No authentication or commit/push. Broader browser interaction/scroll/navigation,
animation, nested dialogs, WebGL, alternate drivers and display policies remain
open rather than being inferred from these two exact initial-state comparisons.

## Upstream CEF 152 alignment (2026-09-15)

At the user's direction, the current build now consistently targets upstream
Dullahan 1.44.0 / CEF 152.0.6+g708dc14+chromium-152.0.7977.83. The earlier
headers/runtime mismatch below is historical and is resolved for the active
build, not by downgrading the package or changing the pinned GL oracle.

NV-00/01/02/03/17: the current GL media plugin already consumes the upstream
autobuild package's Dullahan facade, wrapper, helper and runtime. Native retains
its independently built Dullahan owner and CPU-rendered browser policy, with
unchanged explicit initialization/reference-counting/shutdown patches. Its CEF
header archive now matches that package (SHA1
e5e3020627f4528bd43e22f4c4970000b0458e99), and Dullahan source is pinned to release
v1.44.0-CEF_152.0.6.83, commit f75972f4cba3a01a23007ed79b6c204ececf352c.
The new upstream dullahan_embed_scheme.cpp is included to satisfy the updated
implementation's resource-handler factory. This is third-party CPU URL/resource
handling, not a viewer GL helper; native does not configure an embed root.
Embed application workflows are not newly qualified. No GPU upload, shader,
descriptor or retirement behavior changes in this dependency update.

Fetched source contents are copied with configure_file(COPYONLY) before compilation
so older archive timestamps cannot leave stale Visual Studio objects after a pin
update. The first build exposed the missing embed source and stale callback-manager
object; the corrected build passes Window7/7, including native login/Guidebook
mouse, keyboard, wheel, close and reopen. Existing LNK4020 PDB warnings remain.
The current media_plugin_cef and copy_w_viewer_manifest targets also build, and
the native viewer link passes. Viewer llplugin and native fixture runtime/helper,
common resources and deployed locales were hash-checked against the CEF 152 package.
The fixture copies all package locales; the viewer keeps its existing manifest
selection. No GL implementation or packaging policy was modified.

The historical GL oracle and all prior captures remain unchanged. Its CEF 139
pixels are not a matched-CEF browser oracle, and the earlier rich-content failures
remain failures. This update is build/runtime validation, not a new exact-parity
claim or completion of the no-login sweep. No authentication, commit or push.

## No-login locale sweep and browser blocker (2026-09-15)

The second TODO remains open. This slice extends initial local-notification
presentation coverage; it does not qualify all no-login dialogs or browser
interactions. Authentication-dependent tests remain deferred. The pinned GL
revision, binary, assets and zero-tolerance comparison are unchanged.

### Locale contract, correction and evidence

NV-00/01/02/12/17: at reference revision
59108e15a1f8f94d2da7c674d937d19f5cf9450d, LLAppViewer initialization supplies
LLUI::getLanguage() to the skin owner. LLUI::getUILanguage(false) resolves
Language, InstallLanguage and SystemLanguage in that order, skipping empty and
default entries. It chooses English if none applies. FSEnabledLanguages then
filters that result: a disabled language yields English and writes Language=default.
The bypass for agent-language reporting is not the visual startup contract.
These are CPU settings/resource-selection responsibilities; no GL visual helper
is shared with native. Existing native LLControlGroup/settings bindings remain
the persistence and callback owners.

LLVKViewerUi::uiLanguage independently resolves the settings snapshot and records
the reset in that snapshot. Startup and the isolated capture fixture apply the
result before loading visual resources and propagate the reset through native
settings. Startup status, main UI, browser language and login-page language use
the same resolved value. Explicit low-level skin selection for translation
previews is unchanged. There are no GPU synchronization or lifetime changes.
Widget210/210 covers precedence, enabled-language preservation, disabled-language
fallback/reset and an empty allowlist; Window7/7 and viewer link validation pass.
Actual OS-language discovery and all settings-change callback combinations are
not newly qualified by these tests.

All following settled-0 comparisons are full-frame 2560x1369 RGBA8, maximized,
default skin, UI scale 1.0, anisotropy off, MediaPluginFailed with
PLUGIN=media_plugin_cef, and the unchanged local gray page. Every row has zero
differing pixels and zero channel error. Directories are under glref-build/captures.

| Requested locale | GL capture | Native capture | Comparison directory |
|---|---|---|---|
| az | gl-locale-54-az | native-locale-72-az | locale-parity-54-72-az |
| da (English fallback) | gl-locale-54-da | native-locale-73-da | locale-parity-54-73-da |
| pl | gl-locale-55-pl | native-locale-74-pl | locale-parity-55-74-pl |
| pt | gl-locale-56-pt | native-locale-74-pt | locale-parity-56-74-pt |
| ru | gl-locale-57-ru | native-locale-75-ru | locale-parity-57-75-ru |
| tr (English fallback) | gl-locale-57-tr | native-locale-75-tr | locale-parity-57-75-tr |
| zh | gl-locale-57-zh | native-locale-75-zh | locale-parity-57-75-zh |

The Danish failure (native-locale-72-da, 7344 differing pixels) remains retained;
only the native side was rerun after correction. The original Portuguese GL
capture gl-locale-55-pt had a black, not-yet-painted browser surface and was not
a matched-input comparison. The GL harness now gates settled acquisition on a
bounded WGC sample of the fixture's gray background. This readiness sample is not
parity acceptance: every pixel still participates in the final comparison.
Only that invalid GL run was repeated; the passing native Portuguese capture
was reused. Both fixture pages must retain the expected gray readiness sample.
All successful GL runs exited zero with Goodbye! and a recorded response; native
runs passed 7/7 and exited zero. Existing LNK4020 debug-symbol warnings remain.

### Rich browser content: partial correction, not parity

The runners accept a local page path, record its SHA256 in the shared request
and manifest, and reject mismatched native fixture bytes. The deterministic
browser_parity.html uses local text, form controls and canvas colors without
remote assets. Its SHA256 is
D8638DBB532386F0AA3BE3B2B4B163990A85120807045ECC566369AED528D68C.

NV-00/01/02/11/12/13/14/17: reference LLMediaCtrl::draw emits a direct media quad;
LLRender::vertex3f applies UI offset/scale without rounding the resulting extent.
Native streaming images previously used the skin-image preparation path, which
rounds stretched image extents. LLVKUiPacket's browserImage path now preserves
the fractional device rectangle and source clip region. It uses the existing
immutable image publication, append, descriptors and completion-based retention;
skin-image preparation and the shader ABI are unchanged. GPU10/10 includes an
exact 13.75 by 16.25 device-extent assertion at scale 1.25.

The correction reduced the 125-percent rich-page mismatch from 18889 pixels
(gl-browser-content-52 versus native-browser-content-69) to 10914 pixels
(native-browser-content-70). The 100-percent control still differs in 4101 pixels
(gl-browser-content-53 versus native-browser-content-71). Reports are
browser-content-parity-52-69, browser-content-parity-52-70 and
browser-content-parity-53-71. These are failures, not approximate passes.
Browser raster-source, padded-edge filtering and effective CEF rendering policy
remain unqualified; the fractional-quad test alone does not close them.

Dependency inspection found a material input mismatch:

- Pinned GL libcef.dll: 139.0.40+g465474a+chromium-139.0.7258.139,
   SHA256 CB01DBD9620DE4FBB469BCF85E418A0774E2C5F888FC3C5EDD2C30F17E52D83D.
- Native fixture libcef.dll: 152.0.6+g708dc14+chromium-152.0.7977.83,
   SHA256 C08E16D5F1BC62102529B891ADC2B8B8997655EC6A78E9ADC031CE63A317C0F9.
- native_dullahan.cmake still explicitly pins CEF 139 headers. The current
   autobuild package declares CEF 152 and stages that runtime into sharedlibs.
   The separate viewer llplugin deployment inspected before link still had 139.
- Native explicitly disables CEF GPU rendering. The reference plugin defaults
   to GPU enabled, but its effective incoming disable setting remains unverified;
   absence of a saved override is not proof of the effective policy.

Version drift can affect browser pixels but is not yet proven to explain every
remaining difference. No package downgrade, runtime DLL substitution, GL oracle
update, GPU-policy change or tolerance adjustment was performed. A coherent
headers/wrapper/helper/runtime configuration and a controlled comparison are
required before attributing residual browser pixels to Vulkan or claiming parity.
Browser interaction/scroll/navigation, nested-dialog workflows, time-aligned
transitions and broader display/theme combinations remain open. No commit or
push was made for this slice.

## UI scaling implemented and 125-percent parity verified (2026-09-15)

Today's scope is the UI-scale failure, not the remainder of the no-login parity
sweep. The previously inert UIScaleFactor now drives native font rasterization,
logical layout, device-space drawing/scissors, pointer coordinates and embedded
browser surface size/page zoom. Startup at 125 percent and live 100-to-125 percent
changes match the preserved pinned GL reference exactly; a live 125-to-100 percent
reset returns to the preserved 100-percent reference. No GL code, capture image,
mask, tolerance or reference setting was changed.

### Source contract and native design

NV-00/01/02/11/12/13/14/15/17 apply. Pinned source revision remains
59108e15a1f8f94d2da7c674d937d19f5cf9450d. Controlling reference paths:
LLViewerWindow::calcDisplayScale, reshape and initFonts; FSPanelLogin::show;
LLFontGL::initClass/render/getWidth; LLScreenClipRect::updateScissorRegion;
LLNormalTextSegment::drawClippedSegment; LLMenuItemBranchDownGL::draw; and
LLMediaCtrl::reshape, calcOffsetsAndSize, convertInputCoords and draw.

GL combines user scale and system UI size, clamps to 0.75..7.0, rasterizes fonts
at floor(base DPI times scale), and measures controls in logical units. Its
outer root is ceil-rounded, but the login panel is created from the separately
rounded scaled-window rectangle. Glyph drawing floors the scaled widget origin
before local placement and honors line vertical alignment. Scissors floor the
origin and ceil the extent with an inclusive boundary pixel. Media textures use
physical dimensions and page zoom; pointer coordinates cross the inverse scale
boundary before widget routing. These are CPU layout/window responsibilities,
not GL operations to be shared or translated.

Native implementation:

- LLVKFont owns explicit display scale, logical metrics and physical-resolution
   glyph rasters. Its existing device-pixel layout/measurement code remains the
   raster authority; public UI measurements and placements are logical units.
   LLVKFontRegistry prepares replacement fonts before committing a live scale
   change, preserving font-object identities held by controls. Retained old
   glyph objects remain valid for existing atlases and submitted draws.
- LLVKViewerUi derives scaled registry DPI from FontScreenDPI and effective
   scale. The native window queries system DPI under the reference's process-
   awareness policy, refreshes it on moves/DPI changes, and observes the live
   authoritative UIScaleFactor. Missing platform DPI support follows the source
   fallback. No GL window, font or UI wrapper is called.
- The login panel uses rounded logical dimensions; the menu receives the
   ceil-rounded outer viewport. Native paint preserves floored glyph origins,
   text vertical alignment and menu-item local origins. Notification button
   sizing uses the reference's integer font-width rounding.
- LLVKWidgetGpu converts logical image/solid/triangle geometry and scissors to
   physical pixels. Glyph atlases contain newly rasterized physical pixels,
   never a stretched 100-percent bitmap. The existing shader/vertex ABI and
   immutable image ownership remain unchanged. Old submitted resources stay
   retained by frame ownership; a scale-only change does not recreate the
   swapchain or introduce a device-idle wait into the UI loop.
- Pointer and wheel coordinates are converted to logical UI units, then browser
   input is converted to physical browser coordinates. Browser resize and
   Dullahan setPageZoom follow effective scale; Dullahan retains the requested
   zoom while its asynchronous CEF host becomes ready.
- Live changes retain the widget tree and entered values. Open notification
   presentation is rebuilt with its callback, text, cursor/selection, ignore
   value and response-delay state preserved; changing scale never submits it.

### Verification

| State | Native capture | Retained GL | Exact full-frame result |
|---|---|---|---|
| Startup 125 percent | native-scale-125-65 | gl-display-scale-45 | PASS, zero differing pixels |
| Live 100 to 125 percent | native-scale-live-67 | gl-display-scale-45 | PASS, zero differing pixels |
| Live 125 to 100 percent | native-scale-reset-68 | gl-anisotropy-off-17 | PASS, zero differing pixels |

All images are maximized 2560x1369, default skin/en, anisotropy off, with the
unchanged controlled local browser page. Reports are
`glref-build/captures/scale-125-parity-45-65`, `scale-live-parity-45-67`, and
`scale-reset-parity-17-68`. The 125-percent SHA256 is
B54CC6F286E5B23564918AC8A028AD31774E3E0E2399269226BBD512C08F63B1;
reset SHA256 is
383FCC103AF0CEC2AE8F8796CC5E40BD2122448E9AFC1F80B724B1FEDF20CD5F.
Earlier attempts 61..64 remain retained as failures that isolated origin,
alignment, rounding and outer/login viewport ownership mismatches.

Font18/18 verifies 120-DPI raster ownership at 125 percent, logical measurement,
live font identity preservation, old raster retention and invalid-scale rollback.
Widget209/209 verifies live up/down scaling preserves edited notification text,
selection and response behavior. GPU10/10 on RX 9070 XT covers physical geometry
at scales 1.25, 1.0 and 0.75, invalid-scale rejection, and existing image/font
publication and submission retention. Window7/7 passes. The live 125-percent
capture also clicks a field using scaled physical coordinates, checks native
keyboard focus and text entry, and asserts the session remains PreLogin.

A temporary capture-fixture regression created an undefined LLSD display entry
when live mode was disabled; the optional lookup is now guarded and ordinary
Window7/7 passes again. This was a fixture issue, not a renderer failure.
Existing LNK4020 PDB warnings remain unrelated and are not debugger certification.

Limits: exact pixels are verified for these captured states, not all percentages,
locales, themes, rich-text cases, browser pages or physical monitor configurations.
System-DPI moves and browser-content zoom need their own measured parity cases;
the controlled page here is static. Time-resolved scale transitions and broader
no-login UI workflows remain in the second TODO. No authentication, full logged-in
viewer run, commit or push was performed.

## GL-conformant focus cleanup and normal-window parity (2026-09-14)

NV-00/01/02/12/17: traced pinned GL WM_KILLFOCUS through
LLViewerWindow::handleFocusLost, LLFocusMgr::setAppHasFocus(false),
LLUI::clearPopups and its registered LLPopupView::clearPopups callback. Popup
entries are removed before onTopLost notification; pointer capture is released.
LLButton uses the focus manager's alpha (reduced to 0.4 when unfocused), while
focus regain restarts the existing flash without selecting a different control.
Native already implements the focus tint/flash contract but omitted top-popup
cleanup in its window handler. It now calls its independently owned
setTopControl(0), closing combo popups and dispatching topLost before capture
release. No GL helper, DWM color override or GPU representation is introduced.
Window7/7 includes actual Win32-message assertions for top-popup removal,
pointer-capture release, preserved keyboard focus and flash restart on regain.

The reference runner's optional InactiveFocus input explicitly dispatches
WM_KILLFOCUS and requires nonforeground state. The same shared request makes the
native fixture transfer foreground to its isolated offscreen focus owner and
dispatch WM_KILLFOCUS after modal construction. The fixture checks that painting
receives applicationFocused=false. The common capture sidecars verify inactive
state before/after both samples. This is a controlled focus-event test, not an
exhaustive real Alt-Tab/focus-history acceptance sequence.

New reference `gl-normal-inactive-focus-51` and native
`native-normal-inactive-focus-60` PASS full-frame zero-tolerance comparison at
1024x738, English/default skin, UI scale 1.0, anisotropy off and the unchanged
local browser page. ALL four raw images have SHA256
54E07A5ADBB64F02F5B81A48375E860E584BA7652B65A372283450885598976E.
Report: `glref-build/captures/normal-inactive-focus-parity-51-60`.
Both the earlier 36 DWM-corner pixels and the 1551 modal-focus-border differences
are absent with matching window and keyboard-focus state. The original GL
executable/source, tolerances, borders and images remain unchanged. Earlier
unmatched-input failures remain retained and are not rewritten as passes.

GL exited zero with Goodbye; native Window7/7 passed and exited zero. This closes
the tested settled inactive normal-window state, not all active/inactive
transitions, popup visuals or other no-login workflow gates. The 125-percent
UI-scale failure remains separate and open. No commit or push was performed.

## Corner mismatch explained by activation state (2026-09-14)

GL remains the unchanged presentation reference. The remaining 36 corner pixels
were compared under different actual window-activation states, not equivalent
DWM inputs. The GL runner requested SetForegroundWindow but did not verify that
Windows granted it. Common capture-boundary observations in
`gl-corner-foreground-50` show foregroundBefore=0/foregroundAfter=0; native
`native-corner-foreground-58` shows 1/1. Both reproduce the earlier respective
RGBA hashes exactly. The inactive GL frame has neutral rounded edges; the active
native frame has the configured DWM accent border and different edge alpha.

Controlled native probe `native-corner-inactive-59` transferred actual foreground
focus to a temporary offscreen fixture window. The capture helper verified 0/0.
Against the unchanged inactive GL reference, ALL 36 lower-corner pixels now match
exactly, including alpha. This resolves the cause of the original residual:
unmatched activation selected different DWM border/rounded-edge composition.
The prior inference of an unexplained Vulkan-versus-GL corner-alpha defect is
superseded by this measured result. No border suppression, recoloring, masks,
rescaling or GL changes are required to explain these pixels.

The full inactive-probe comparison `corner-inactive-parity-50-59` is NOT a pass:
1551 pixels differ at top-origin x=478..540, y=382..406, confined to the modal
button's focus border. The real native focus transfer also changed keyboard/UI
focus state; matching foreground alone did not reproduce the reference's full
input history. That is a separate qualification requirement, not permission to
force native focus colors or mark the normal-window workflow complete. The
captured frame/state history must be matched for full-frame acceptance.

Earlier controlled probes in this investigation were ineffective and removed:
`native-corner-class-55` matched the GL Win32 class flags; `native-corner-styles-56`
matched its window/extended styles; `native-corner-redirection-57` disabled the
redirection bitmap. Each was byte-identical to the original active native frame.
Thus none explained the observed corner difference. The temporary focus-owner
probe was also removed after observation. Production window/presentation source
is restored; only capture metadata and this record remain from this investigation.

NV-00/01/02/12/17: the common Windows capture helper now writes a separate
`.rgba.window.txt` sidecar with actual foreground state before/after capture and
window styles. Raw RGBA bytes and the GL oracle are unchanged. Future fixtures
must verify activation and UI focus/history rather than trusting an activation
request. Diagnostics used isolated pre-login sessions and clean shutdown, with
Window7/7 passes. Login-dependent tests remain deferred. No commit or push made.

## Rounded-corner source isolated (2026-09-14)

Investigation of the 36 normal-window residual pixels identifies a DWM accent
border contribution after Vulkan rendering, not magenta emitted by native UI.
All experiments used the retained fresh GL normal49 reference and isolated
1024x738 native fixtures. They are diagnostics, not amended parity baselines.

| Controlled experiment | Evidence | Result |
|---|---|---|
| Disable swapchain obscured-pixel clipping | native-corner-unclipped-49 | Byte-identical to unmodified native; not the cause in this run |
| Fill HWND GDI backing surface green | native-corner-backing-green-50 | Byte-identical to unmodified native; no observed backing-fill contribution |
| Copy acquired Vulkan frame before presentation | corner-prepresent-52.rgba and native-corner-prepresent-52 | Rendered corner is neutral gray; composed corner contains accent color |
| Set only this HWND's DWM border to green | native-corner-border-green-53 | 30 colored corner pixels change; no pixel outside the corner regions changes |
| Suppress only this HWND's DWM border | native-corner-border-none-54 | Magenta removed; 36 residuals remain, maximum RGBA errors 7/7/7/128 |

Windows has accent coloring enabled with RGB (194,57,179), matching the magenta
fringe. At top-origin (0,734), the actual pre-present Vulkan image is
(40,40,40,255); ordinary Windows capture is (190,57,175,255); changing only
DWMWA_BORDER_COLOR to green produces (1,249,1,255). The green intervention changes
30 colored corner pixels; six other residual pixels have black RGB and differing
edge alpha. This establishes the accent-border contribution causally, rather
than inferring it from color similarity. No global Windows settings changed.

The readiness-triggered pre-present copy and Windows capture have identical RGB
everywhere outside the 36 corner pixels. Their alpha differs at 102232 pixels:
raw render-target alpha is not the same contract as the opaque composed window
surface. An initial 102254-pixel raw RGBA difference was therefore not evidence
of a timing or orientation error; separate RGB/alpha analysis resolves it.
The pre-present raw hash is
6316F4F52229B24BE176AF59819D16B43302EC8CB970BB1FAA86CCC39F0BDAA0.
`corner-rendered-vs-composed-52` retains images for this diagnostic distinction,
not an acceptance comparison. `corner-border-none-diagnostic-49-54` retains the
border-suppressed result; it still fails zero-tolerance parity.

NV-00/01/02/11/12/13/14/15/17: the temporary readback negotiated supported
TRANSFER_SRC swapchain usage and disabled obscured-pixel clipping. After dynamic
rendering it transitioned the still-acquired image from color attachment to
transfer source, copied to host-visible staging, applied transfer-to-host memory
dependency, transitioned to present layout, submitted, waited for the frame
fence and invalidated the allocation before reading. Readback happened before
vkQueuePresentKHR, not via the invalid legacy post-present helper. The diagnostic
waits and staging allocation were temporary, not added to the normal frame loop.

The remaining presentation difference is not fully isolated: GL and native use
different Win32 class/extended styles and graphics presentation paths. GL's
dark-frame policy does not explain the current configuration (AppsUseLightTheme
is 1). The evidence does not establish which driver/DWM/WGC interaction causes
different rounded-edge alpha, nor justify removing the application's border as
a production fix. Rendering colors, glyphs, skin assets and UI clipping must not
be changed to compensate for these pixels. Normal-window exact parity remains
open, although the source boundary and accent-color contribution are now known.

All temporary renderer, backing-fill and DWM probes were removed after the runs;
normal swapchain settings were restored and Window7/7 rebuilt/revalidated. The
existing scale metadata from the earlier investigation remains. Captures are
preserved, no login was performed, and no commit or push was made.

## Fresh normal-window retry and scale diagnosis (2026-09-14)

User request: retry 1024x738 with fresh captures and investigate the 125-percent
UI-scale failure. No production rendering code was changed for this investigation.

Fresh `gl-normal-fresh-49` and `native-normal-fresh-47` both captured a 1024x738
nonmaximized client, English/default skin, anisotropy off, UI scale 1.0 and the
same local page. Both settled pairs are independently byte-identical. GL exited
zero with Goodbye; native Window7/7 passed and exited zero. Full-frame report
`normal-fresh-parity-49-47` reproduces the previous 36-pixel FAIL, maximum RGBA
errors 143/10/128/128. The new GL hash is
FD8E1C3B73C4645B5D7E35EBBB9B94F1A056FE4005AA65F321D6DD8F1B6069CD;
the new native hash is
A999B7DF42E063674BD917439E2A5940416992CE81716A292DAAF2DA07F48422.
These equal the earlier normal-size hashes, so the rounded lower-corner residual
is reproducible, not removed by fresh captures. The cause of its compositor-alpha
difference remains unresolved. No masks, rescaling or tolerance changes were used.

NV-00/01/02/11/12/17 scale investigation: capture-only metadata now records the
effective UI scale, physical client, UI-root bounds and configured font-registry
DPI. `native-scale-diagnostic-48` consumes the retained 125-percent request and
reports `uiScaleSetting=1.25`, `physicalClient=2560,1369`,
`uiRoot=0,0,2560,1369`, `fontRegistryDpi=96,96`. Its image remains exactly the
100-percent baseline (SHA256 383FCC103AF0CEC2AE8F8796CC5E40BD2122448E9AFC1F80B724B1FEDF20CD5F).
Against retained GL scale45, `scale-diagnostic-parity-45-48` reproduces 232000
differing pixels. This discriminates failed setting loading from missing scale
consumption: the value is present but does not affect native visuals.

Source-backed controlling paths:

- Native startup supplies font search paths/descriptor, but does not derive
   LLVKFontRegistry DPI from UIScaleFactor or FontScreenDPI. LLVKViewerUi::create
   forwards the configuration to the registry, whose DPI defaults are 96/96.
- LLVKWindowMgr's resize branch reshapes the root directly to swapchain extent;
   there is no separate logical UI extent. Its named UIScaleFactor use populates
   About information, not layout or rendering.
- LLVKWidgetGpu::prepare converts clips directly to framebuffer scissors, passes
   identity image transforms, and submits existing text/solid/triangle positions
   without a display-scale transform.
- WindowState pointer and wheel routes use physical client coordinates (with Y
   inversion) directly for widgets and browser hit-testing. Browser resizing uses
   those same widget dimensions. These consumers need coordinated conversion.
- Pinned GL LLViewerWindow::calcDisplayScale combines/clamps user scale and system
   UI size, with pixel-aspect policy. reshape derives rounded scaled-window bounds
   and separately ceil-rounded root extents; at scale 1.25 and 2560x1369 these are
   2048x1095 and 2048x1096, respectively. Its UI draw path scales geometry and input
   callbacks divide coordinates by display scale. initFonts passes scale to
   LLFontGL::initClass, which floors screen-DPI times scale for rasterization
   (120 DPI for a 96-DPI base at 1.25).

Root cause: the native viewer lacks an integrated logical-to-device display-scale
contract, not a bad saved preference, stale swapchain, anisotropy setting or
single shader constant. A correct fix needs native-owned scale state, logical
layout, scale-specific glyph rasterization/measurement, geometry/scissor conversion,
inverse input conversion and coordinated browser sizing/input. Scale changes must
retain old GPU resources until submissions complete. Enlarging an already rendered
frame, changing only font DPI, or only shrinking the root cannot close parity.
This investigation does not claim implementation or live scale-change validation.
Window7/7 validates the metadata addition; existing LNK4020 warnings remain.

## Japanese retry and document-width correction (2026-09-14)

The requested retry `native-locale-ja-44` reproduced the eight-pixel residual
against retained `gl-locale-48-ja`. The earlier missing-glyph failure was a
fixture defect: production startup searches Windows/Fonts, but the component
fixture did not. Adding the production fallback directory restored Japanese
glyphs; no reference font or asset was changed.

NV-00/01/02/11/12/17: pinned LLTextBase::drawText clamps each text run's right
edge to its document extent. LLNormalTextSegment passes that rectangle to
LLFontGL::render, which uses its width to reject glyphs that do not fit. Native
plain-text paint previously left maxPixels unlimited unless ellipses were on.
It now supplies the document's remaining draw width in both cases, using the
existing native font glyph-fit implementation. This is CPU text preparation;
GPU formats, shaders, publication and retirement are unchanged.

The Japanese Mode label has a 90-pixel measured line in a 75-pixel widget. The
missing glyph-fit limit emitted eight extra foreground pixels over the adjacent
dropdown. `native-locale-ja-45` now matches the entire maximized 2560x1369 GL
frame with zero differing pixels, anisotropy off, UI scale 1.0. Report:
`glref-build/captures/locale-ja-parity-48-45`; identical SHA256:
F679D922CF22ED89882D67860473EEA9C87D24AE8691581C5971B9B82EE3398D.
Widget209/209 includes a non-ellipsized overwide-glyph regression. Window7/7,
viewer relink and edited diagnostics pass; the existing LNK4020 warning remains.
The shared-paint change also passed a focused English baseline comparison in
`text-bound-en-parity-17-46`, reusing the prior GL oracle rather than recapturing
it. No login, commit or push was performed.

Additional retained display results: French, Spanish and Italian pass in
`locale-parity-48-42-*`. The 1024x738 normal-window comparison
`display-normal-parity-47-41` still differs at 36 rounded lower-corner pixels
with compositor alpha differences; it is not a pass and no masking was used.
GL normal-window attempt46 remained maximized and is invalid for that case;
the runner now explicitly restores/sizes and verifies its client dimensions.
125-percent UI scaling remains a confirmed open native failure. These results
do not close the broader no-login sweep, nested workflows or temporal gates.

## Checked-ignore and display sweep continuation (2026-09-14)

Direct Win32 input resolves the checked-ignore fixture blocker. The reference
LEAP driver identifies its parent viewer window, places the physical pointer,
sends mouse move/down/up, then verifies the checkbox value using getInfo before
permitting capture. The pointer is restored and mouse released on driver exit.
GL source remains untouched; prior failed LEAP click attempts are not parity
evidence. `gl-ignore-checked-42` and `native-ignore-checked-37` both have stable
settled pairs and identical full-frame SHA256
9C7B2A3614CD0AE04CEFB89C3DF1D841B0317823732D0148B27542F80AC1C5A0.
`ignore-checked-parity-42-37` reports zero differing pixels. GL exited zero with
Goodbye; native Window7/7 passed and closed. This proves checked appearance, not
yet restart persistence or nested-dialog behavior.

Selected editable-form text also passes in retained `form-selected-parity-38-33`:
native selection-background tint now follows source UNORM8 truncation. The
reference input helper emits Unicode for ASCII key events even with modifiers;
the qualified selection action uses Shift+Home, not Ctrl+A. Invalid attempts
remain retained. The text and selection are synthetic and no list is created.

The shared capture request now carries matched isolated display overrides.
Native applies them to the private settings group before creating the window.
`display-aniso-parity-43-38` (anisotropy on) and `display-de-parity-44-39`
(German, anisotropy off) both pass exact full-frame parity at 2560x1369.
These do not establish all settings combinations or all locales.

125-percent scale FAILS: `display-scale-parity-45-40` reports 232000 differing
pixels. The native image is byte-identical to its 100-percent baseline, while
the reference honors UIScaleFactor=1.25. Native LLVKWindowMgr currently reshapes
the UI to physical swapchain pixels, uses unscaled pointer coordinates and does
not derive font DPI or GPU transforms from UI scale. Pinned GL
LLViewerWindow::calcDisplayScale/calcScaledRect combines system UI size with
UIScaleFactor, rounds logical bounds and reloads scaled fonts. A coordinated
native logical/device coordinate, font and browser/input implementation is
required; scaling an already rendered frame is not an acceptable correction.
This remains an open confirmed failure. No tolerance or reference changed.

## Initial local-template matrix and trusted links (2026-09-14)

All 30 allowlisted local notification templates now have exact captured initial
presentation evidence in the default skin/en, maximized 2560x1369, anisotropy-off
configuration. This is not closure of edited/selected/scrolled/ignored/reopened
states, nested workflows, animation timing, display variants or live side effects.
Login-dependent tests remain deferred entirely.

Preserved MediaPluginFailed evidence plus `local-add-phase-parity-29-21-*`,
`local-parity-30-22-*`, passed `local-parity-31-23-*`,
`local-secondary-parity-31-26`, `local-parity-32-27-*`,
`local-parity-33-28-*`, `local-parity-34-29-*` and `local-parity-35-30-*`
cover the matrix. Failed attempts remain under their original paths. Each new
request/manifest records the synthetic substitutions and immutable source hashes.
Every qualified GL run exited zero with Goodbye; native component runs passed
Window7/7 and closed. The harness requests notifications with a test-owned reply,
not the real reset/backup/restore/quit callback. No actual preference resets,
cache clears, file imports, restores, credentials or authentication were used.

NV-00/01/02/11/12/13/14/17 trusted-link correction: pinned
LLUrlEntrySecondlifeURL/FirestormURL classify official hosts, LLTextUtil appends
their icon, and LLImageTextSegment contributes image width/height plus three
pixels to layout. Native LLVKWebText now provides independently classified icon
metadata; LLVKPlainControl uses its existing native styled-image segments for
reflow, and native paint emits retained skin images between glyph runs. Sizing
and display use the same icon-aware document. Native hit-testing accounts for
image advances and does not treat the icon itself as URL text. Glyphs align to
the top of image-enlarged lines; inline images stay vertically centered. Link
underlines reproduce the reference integer horizontal-line raster row. No GL
URL/UI helper is called. Existing immutable image publication, descriptor and
submission retention remain unchanged.

Widget209/209 covers trusted-host boundaries (including a lookalike-host
rejection), image/text hit ranges, plus existing text and image-segment tests.
Window7/7 passes. SpellingDictIsSecondary now matches the full reference frame
exactly, SHA256
31DC85184D155689DD1D78BCB447DD01638A987CF7DF84C323C717CA3CDCCD65.
The initial 15961-pixel failure, intermediate 3165-pixel glyph alignment failure
and 708-pixel underline-row failure were corrected, not masked or tolerated.
Other rich text forms, selected icon-bearing text and scale variants still need
their own capture checks. The reference driver's occasional large getPaths
response parse failure is retained as fixture history; modal queries now use
the existing API's Floater View subtree limit instead of the entire viewer.

## Local input-form corrections (2026-09-14)

NV-00/01/02/11/12/17: pinned LLToastAlertPanel sizes from
LLTextBox::getTextPixelWidth (text bounds), not reshapeToFitText's extra fitting
pixel. Native notice sizing now uses its document bounds, leaving generic text
fitting unchanged. AddAutoReplaceList is 203 pixels wide, not 204. Pinned
LLLineEditor::drawBackground replaces focus alpha with draw transparency and
passes its tint through byte-color image drawing; native now clamps/truncates
that tint to UNORM8. LLLineEditor::mBorderThickness is initialized to zero and
never changed; its content bounds are independent of its decorative border.
Native caret/selection/preedit preparation now preserves that zero content inset.
No shaders, uploads, descriptors or resource lifetimes change.

Widget209/209 passes with input width, exact border channels, caret endpoints
and existing input/response regressions. Window7/7 and isolated capture runs
pass. The reference-only URL warning must be dismissed by visible control-path
inspection before submitting a measured notification: the pinned notification
list API alone can return an empty list while the warning is visible.
`gl-local-add-27` is invalid for single-modal parity (warning behind the form).
`gl-local-add-28` failed binary LLSD decoding before submission and closed via
WM_CLOSE with Goodbye; the intermittent parser failure remains a fixture risk.
The driver now reports only packet size/parse position on failure and the runner
detects driver failure promptly. No packet contents or credentials are logged.

Qualified `gl-local-add-29` versus `native-local-add-21` has zero differing
pixels for both hidden and visible caret phases, across the entire 2560x1369
frame. Visible-caret SHA256 is
16FFA50E75A0B3E005B95997D21BF8B8817D1B2CCE7F6046ABCF82C2F2DECBBB;
hidden-caret SHA256 is
D758A3747A1318EC700E20C3960FC34C8F44EF73338BFC76456AAC43103B90AE.
Reports are under `local-add-phase-parity-29-21-*`. These are matched visible
states from preserved sequences, not proof of synchronized blink timing.

Four more templates have exact full-frame captured-state passes in
`glref-build/captures/local-parity-30-22-*`: RenameAutoReplaceList,
RemoveAutoReplaceList, InvalidAutoReplaceEntry and AddToMediaList. Inputs and
executable/page hashes are in each manifest and shared capture-request.xml.
All runs were maximized, default skin/en, anisotropy off, synthetic local data,
no authentication and clean exits. Form input editing/selection and nested
workflow capture coverage remain open; initial-form pixels do not close them.

## Login-independent verification sweep (2026-09-14)

User scope: verify and correct login-independent behavior; defer every test that
partially or fully depends on login. Prior passes remain valid and retained.
This is an ongoing sweep, not completion of the local UI or later service gates.

NV-00/01/02/11/12/17 focus correction: pinned GL LLWindowWin32 dispatches
WM_KILLFOCUS/WM_SETFOCUS to LLViewerWindow::handleFocusLost/handleFocus, then
LLFocusMgr::setAppHasFocus. Loss dims focus alpha and releases pointer capture;
regain restarts the 0.3-second flash without changing the focused control.
WM_ACTIVATEAPP instead reaches the joystick reset callback. Native now handles
the keyboard-focus messages using its own input state, capture owner and widget
clock. The CPU-only correction does not alter GPU data or resource lifetimes.
Other callback obligations, including popup and modal restoration, remain open
for their own local interaction cases; login-dependent agent/tool behavior is
not exercised or claimed here.

Widget209/209 and Window7/7 pass. The existing LNK4020 PDB warning remains.
Widget test106 verifies flash start, 150ms midpoint, 300ms expiry, preserved
keyboard focus and absence of a second tab-entry callback. New settled focus
captures use the same pinned GL executable, static page, default skin/en,
maximized 2560x1369, anisotropy off and zero tolerance as prior passes:

| State | Reference | Native | Full-frame result |
|---|---|---|---|
| Focus lost | gl-login-focus-26 | native-login-focus-17 | PASS, zero differing pixels |
| Focus regained | gl-login-focus-26 | native-login-focus-17 | PASS, zero differing pixels |

Reports are `glref-build/captures/login-focus-lost-parity-26-17` and
`login-focus-regained-parity-26-17`. These inject the actual keyboard-focus
messages; real external-window activation and time-aligned animation pixels are
not certified by settled frames. GL exited zero with Goodbye; native passed
PreLogin/no-authentication assertions and closed its window.

The existing capture harness now accepts a shared bounded LLSD request file for
local notification names/substitutions. This exercises the original reference
notification presentation and independent native queue, without service side
effects. Input forms, nested dialogs, local persistence workflows, display
configurations and offline browser cases remain pending until individually
recorded. Authentication/TLS login tests, account workflows, reconnect and world
composition are deferred entirely, including synthetic partial substitutes.

## Held-press clipping correction verified (2026-09-14)

The user confirmed the corrected reference/native images agree with their
standalone observations. Those observations and the passed enabled/hover states
were retained; only the affected native held-press capture was rerun.

NV-00/01/02/11/12/14/17: pinned GL LLLayoutStack::draw establishes panel clipping
through LLScreenClipRect::updateScissorRegion, whose rounded scissor dimensions
include the right/top boundary pixel. Native layout-panel paint now represents
that coverage by extending nonempty panel clips one pixel right/top where the
parent permits it, before intersection. The parent/framebuffer bounds remain
authoritative; empty or collapsed panel clips do not become visible strips.
This CPU clip correction leaves the Vulkan half-open scissor API, image/vertex
formats, shader code, uploads, descriptor ownership and retirement unchanged.
No GL visual helper or screenshot-specific coordinate adjustment is used.

Widget209/209 passes, including a layout edge-decoration regression checking the
included right boundary and parent/framebuffer containment. Window7/7, viewer
relink and edited-file diagnostics pass. The existing LNK4020 PDB warning remains.
The isolated maximized native pressed-only run `native-login-pressed-16` passes
pointer-capture, pointer-inside, PreLogin and release-outside assertions and exits
zero; both settled frames are byte-identical. No authentication was submitted.

Against preserved corrected GL `gl-login-pressed-25`, the complete 2560x1369
frame PASSES with zero differing pixels and maximum RGBA errors 0/0/0/0.
Both raw captures have SHA256
864860977A131238BB8865D5E372A99654B1D72D42312206644D4112B5FF16ED.
The report and PNGs are in
`glref-build/captures/login-pressed-parity-25-16`. All 52 formerly missing
right-edge focus-border pixels now match. Anisotropy remains off; the existing
local page, synthetic input and pinned reference are unchanged. Enabled, hover
and held-pressed settled states now have exact comparison evidence. Other
timed transitions, display configurations and broader UI workflows are not
certified by these captures. No tolerances, masks or alignment were changed.

## Held-press failure source correction (2026-09-14)

The earlier 7595-pixel held-press finding combined a reference-fixture state
mismatch with a real native clipping defect. GL `gl-login-states-23` predates
actual cursor placement: its nominal pressed capture renders the hover-style
fill, differing from the qualified hover image at only 22 pixels inside the
button. It is not a qualified held-pressed visual oracle. The earlier conclusion
that the whole fill mismatch was a confirmed native renderer failure is withdrawn.

The targeted GL-only rerun `glref-build/captures/gl-login-pressed-25` uses the
existing corrected cursor-placement runner. Its settled pair is byte-identical,
it stays in STATE_LOGIN_WAIT, and exits zero with Goodbye. Compared with the
preserved, stable native held-press capture `native-login-pressed-15`, the fill
and label match; only 52 pixels differ, all at top-origin x=1765, y=1242..1293.
Maximum channel error is 102. No native renderer code changed during this trace.

Source: LLLayoutStack::draw clips the login container through LLLocalClipRect.
LLScreenClipRect::updateScissorRegion adds one pixel to the rounded width and
height. Thus the GL scissor includes the right boundary at x=1765. Native
LLVKWidgetPaint intersects with the unexpanded layout-panel rectangle, and
LLVKWidgetGpu converts that to a Vulkan scissor with width right-left and height
top-bottom. Its exclusive right edge removes that focus-border column. At
(1765,1250), GL is (142,91,63,255), native is the background (40,40,40,255).
This clipping-convention mismatch is the remaining confirmed held-press defect;
it has been traced but not repaired here. Enabled and hover passes are retained.

## Log In interaction-state test results (2026-09-14)

Completed the requested enabled, hovered and held-pressed checks at 2560x1369,
maximized, anisotropy off, with a working local-page browser and fixed synthetic
credentials. The existing empty-credential/modal pass remains preserved. No
production renderer changes were made for this test sequence.

| State | GL evidence | Native evidence | Full-frame zero-tolerance result |
|---|---|---|---|
| Enabled, pointer away | gl-login-states-23 | native-login-states-14 | PASS, zero differing pixels |
| Hover | gl-login-hover-24 | native-login-states-14 | PASS, zero differing pixels |
| Held pressed, pointer inside | gl-login-states-23 | native-login-pressed-15 | FAIL, 7595 pixels; maximum RGBA errors 152/189/236/0 |

All qualified settled pairs are individually byte-identical. Reports and original
PNGs are under `glref-build/captures/login-enabled-parity-23-14`,
`login-hover-parity-24-14`, and `login-pressed-parity-23-15`. Pressed differences
are confined to Log In and its border, top-origin x=1625..1765, y=1241..1294
(inclusive). Native input asserts enabled state, pointer capture and pointer
inside the button. The pressed rendering mismatch is real; its production cause
has not been isolated or corrected in this test-only work.

Fixture changes: optional LoginButtonStates in the GL runner, selective ButtonStates
retries, and LLVK_CAPTURE_LOGIN_BUTTON_STATES in the native component. The GL
driver dismisses obstructing fixture warnings, enters username/password through
acknowledged LLWindow pasteText calls, then verifies connect_btn is enabled. The
runner applies hover/held-press input. Native uses Win32 input and asserts the
session remains PreLogin. Mouse release occurs outside Log In, and temporary
cursor movement is restored. Qualified GL logs remain at STATE_LOGIN_WAIT until
shutdown; none enter authentication or STATE_STARTED. GL runs exit zero with
Goodbye, native capture runs pass Window7/7 and close their windows. No real
credentials or user-profile settings are used. The reference paste API temporarily
uses the clipboard and restores it by its existing implementation.

Earlier attempts are retained but invalid: 18 exhausted an overly broad optional
layout probe; 19 encountered a stale PowerShell helper type; 20 retained a blocking
URL warning; 21 typed into the wrong field due queued focus handling; 22 rejected
a binary acknowledgement through the generic LLSD deserializer. The driver now
uses a bounded explicit header-prefixed binary parser, with framing and minimal
acknowledgement self-tests. The probe selects owner rectangles only. Synthetic
WM_MOUSEMOVE alone did not qualify GL hover; actual cursor positioning did.
Native pressed attempt 14 was unstable; retry 15 with physical cursor placement
and pointer-inside assertions is stable and retains the mismatch. Enabled and
hover passes were reused rather than recaptured. Runner syntax, driver self-test,
Window7/7 and edited-file diagnostics pass; LNK4020 debug-symbol warnings remain.

## Matched anisotropy-off full-frame parity (2026-09-14)

Rechecked Log In against the pinned GL reference with the user's current
anisotropy-off policy. The external runner now accepts Anisotropy=Off/On/Unchanged.
For an explicit policy the LEAP driver sets RenderAnisotropic through the existing
LLViewerControl API at STATE_LOGIN_WAIT and validates the returned value before
submitting the modal. This changes only the isolated reference profile, not GL
source or the user's preferences. Native records its live setting in capture
metadata and does not force it on. NV-00/02/17 apply to matching these inputs.

`glref-build/captures/gl-anisotropy-off-17/gl-anisotropy.xml` confirms value 0;
`native-anisotropy-off-13` records anisotropy=off. Both clients are maximized at
2560x1369 and use the same unchanged local HTML page with working browsers.
Both settled pairs are byte-identical. The complete unmasked frame comparison
in `glref-build/captures/full-login-parity-off-17-13/comparison.json` PASSES:
zero differing pixels, maximum RGBA errors 0/0/0/0, and identical raw-image SHA256
383FCC103AF0CEC2AE8F8796CC5E40BD2122448E9AFC1F80B724B1FEDF20CD5F.
Log In, Mode, link text, menu, browser, background and modal all match in this state.

The previous Log In edge residual compared GL anisotropy on against native off;
no hard-coded color/UV correction is needed. Mode's inherited four-pixel button
padding and visible-line text clipping (using each aligned line's actual right
edge) are now capture-verified. The former nine "underline" pixels were clipped
glyph descenders and now match. No resize, alignment, masks or relaxed tolerances
were applied. Earlier captures remain intact under their original settings.

Driver build/framing self-test and runner preflight passed. Native Window7/7
passed and capture exited zero; GL recorded the response, exit zero and Goodbye.
Earlier Widget209/209, GPU10/10 and viewer-link results cover the current native
code. Existing Window LNK4020 debug-symbol warnings remain. This proves the
captured settled pre-login state with empty credentials, the tested skin/locale,
viewport and anisotropy off. Enabled/hovered/pressed Log In, other browser content,
notification states, locales/DPI and anisotropy-on pixel parity remain separate
verification gates. No full logged-in viewer run was performed.

## Live anisotropy preference and pending detail checks (2026-09-14)

The user confirmed RenderAnisotropic was enabled when the preserved GL reference
was captured and is now disabled in Preferences. The reference's saved settings
also record true; its declaration default of false is not its effective state.
Do not force native on or alter the old evidence. New off-state visual comparisons
require a matched off-state GL capture. The attempted hard-coded true override
in the native capture fixture was removed before this update.

NV-00/01/02/13/14/15/17: native startup binds its loaded global settings group;
bindSettings publishes changes from that group into the widget tree. preparePaint
reads RenderAnisotropic each frame. The renderer negotiates samplerAnisotropy on
the selected physical device; enabled capability alone does not select its use.
Off uses the existing linear/clamp skin sampler. On uses an anisotropic sampler
at that device's maxSamplerAnisotropy limit. Requests without enabled capability
fail explicitly. Glyph and streaming-browser sampling remain unchanged.

Sampling policy is part of the skin-image cache key, so toggles publish distinct
immutable image/sampler versions instead of mutating in-flight descriptors.
Existing upload completion and submission-held resource lifetimes apply. This
does not introduce GL visual helpers or change shaders/texture bytes.

Widget209/209 verifies an authoritative off preference overrides a stale on
startup copy and live off/on/off changes reach painting. GPU10/10 verifies
unsupported-capability rejection, anisotropic publication/rendering, and switching
back to a distinct bilinear resource. Window7/7 and viewer relink pass; diagnostics
are clean. Existing LNK4020 debugger-symbol warnings remain. No user settings were
written and no new matching-settings visual parity claim is made here.

Related pending detail corrections: base button padding is now 4 pixels, matching
LLButton::Params and fixing the Mode dropdown's displaced label. Non-scrolling
text clipping now uses visible line rectangles (including aligned right edges),
the reference top adjustment and inclusive scissor extent. The nine former
"underline" residuals were glyph descenders clipped at the widget edge.
`native-login-details-12` eliminated the dropdown and descender discrepancies but
exposed a right-aligned Mode-label clip error, subsequently corrected to use each
line's right edge. That final clip change needs a fresh matched-settings capture.
The 30-pixel Log In edge difference was measured with mismatched anisotropy states;
its closure remains unverified until both backends use the same preference.

## Login defects and working-browser comparison (2026-09-14)

Requested scope: five traced login defects and a working-browser comparison.
NV-00/01/02/06/11/12/14/15/17; reference remains
`59108e15a1f8f94d2da7c674d937d19f5cf9450d`, with no GL source edits.

Source contracts and native changes:

- LLPanel and LLButton image drawing pass final colors through color4f's clamped,
   truncated UNORM8 representation. Native panel tints and button image tints now
   use that encoding after transparency/fade multiplication. The login background
   0.16 becomes 40/255; disabled-image alpha 0.5 becomes 127/255. Source assets,
   shader layouts and resource lifetime contracts are unchanged.
- LLMenuBarGL::arrange sizes the login menu to visible entries. Native's standalone
   login bar now fits its entries over black header backing; supplied embedded bar
   rectangles remain authoritative. Solid menu tints use UNORM8 precision too.
- Native initializes the start-location combo from command-line, next-login and
   saved login settings, choosing the existing last/home items or preserving
   explicit location text. This fixes the observed default placeholder mismatch;
   full grid-qualified SLURL validation/normalization remains part of login services.
- Remove-user enablement requires a selected saved combo item whose label still
   matches the editor, plus PreLogin. Typed or edited usernames do not enable it.
   This does not add a credential-store implementation or certify deletion itself.
- LLPluginClassMedia caps requested dimensions at 2048. LLMediaCtrl defaults to
   stretch-to-fill with aspect preservation; calcOffsetsAndSize centers the result.
   Native browser start/resize now cap surfaces at 2048 and skip redundant resizes.
   Native displayRect owns the aspect-preserving geometry used by paint and pointer,
   hover and wheel offset mapping. At 2560x1199 with 2048x1199 media it yields
   x=256..2304. Surface epoch invalidation, borrowed-pixel copying, opaque RGB upload,
   completion-based publication and GPU resource retirement remain unchanged.
- The resulting capture exposed a one-pixel line-editor baseline error. Native now
   uses the reference ceil(ascender)+ceil(descender) line height for vertical padding.

The existing window fixture has an opt-in LLVK_NOTIFICATION_CAPTURE_PAGE mode:
it starts a real native browser, queues the same synthetic MediaPluginFailed alert
as the GL driver, and waits for a published expected page color and capped width.
The original missing-helper regression stays unchanged outside that mode. The
isolated mode explicitly shuts down its separate cache owner before returning.
Early captures 8/9 terminated after image creation because that teardown was
initially omitted; they are not accepted lifecycle evidence. A debugger attempt
also failed while reading the existing damaged PDB; no backtrace was relied on.

Verification: Widget209/209 includes menu extent, native browser paint geometry,
wide/tall aspect calculations, last-location initialization, and saved/typed/edited
username enablement. Window7/7 passes normal browser lifecycle, mouse/key/wheel and
auxiliary-window workflows. The working-browser capture mode also exits zero with
7/7 checks. Viewer relink and editor diagnostics pass. LNK4020 PDB warnings remain.

Preserved GL `layout-probe-16` versus `native-working-browser-11`, both maximized
2560x1369 and using the unchanged loopback notification_background.html, yields
856 full-frame differing pixels (formerly 1025739). Native's settled pair is
byte-identical. The browser area, exposed panel background, menu, logo, trash
button, username/password/location fields and complete modal now match exactly.
Remaining differences: 817 pixels in Mode-selector text, 30 at the Log In image's
right edge (maximum channel error 3), and 9 in a link underline. Results and PNGs
are in `glref-build/captures/full-login-parity-11`. Full-login exact parity is not
claimed. Working browser parity is established for this static page/viewport;
arbitrary web pages, other DPI/zoom/decoupled-size modes, popup dialogs and browser
features require their own matched workflows. No tolerance, alignment or masking
was used to make the full-frame result pass.

## Focus encoding and exact captured modal match (2026-09-14)

NV-00/01/02/06/12/14/17: the pinned GL button focus-border path uses
LLButton::drawBorder, LLUIImage::drawSolid and LLRender::color4f. Final tint
channels are clamped and truncated to UNORM8 after focus/transparency are applied.
Native button preparation now independently encodes that final border tint in
the same way, retaining float storage of the normalized byte values. The skin
focus RGB (0.56,0.36,0.25) becomes (142,91,63)/255. Nonfinite channels are rejected;
unrelated image/text colors, shader ABI, uploads and resource retirement are
unchanged. No GL implementation or fixture background was modified.

Widget209/209 tests the focus RGB conversion and alpha multiplication before
encoding, while checking image alpha is unaffected. Window7/7, viewer relink and
editor diagnostics pass. The known Window LNK4020 debug-symbol warning remains.
New maximized 2560x1369 native evidence is in
`glref-build/captures/native-focus-unorm-7`; both settled frames are byte-identical.
Against preserved `layout-probe-16`, the unchanged top-origin region
[1043,1518) x [559,747), containing the entire dialog and shadow, has ZERO
differing pixels out of 89300 and maximum RGBA errors 0/0/0/0. All 632 remaining
Close-button/focus-border discrepancies are eliminated. Geometry remains
(1053,632)-(1503,800) in bottom-origin coordinates.

This establishes exact visual parity for the captured settled MediaPluginFailed
state, not all notification templates, input/focus transitions or the complete
login screen. The full-frame comparison still has 1025739 differing pixels
outside this region. No alignment, rescaling, masking within the tested region,
or tolerance changes were used; all previous captures remain intact.

## Shadow vertex-color encoding correction (2026-09-14)

NV-00/01/02/06/12/17: pinned GL `gl_drop_shadow` supplies float colors through
`LLRender::color4fv`/`color4f`. The latter clamps each channel to [0,1], multiplies
by 255 and truncates to GLubyte before vertex interpolation. Thus shadow alpha
0.5 is represented as 127/255, not 0.5. Native's full-precision shadow alpha made
the captured shadow one RGB level too dark. Native shadow paint now performs
the same independently owned CPU encoding after transparency multiplication and
before interpolation, retaining native float vertex storage and existing shaders.
No GL helper, texture modification, screenshot correction or tolerance is used.
Finite-value checks precede encoding; resource ownership and retirement are unchanged.

Widget209/209 verifies 127/255 at the native shadow producer, Window7/7 and viewer
relink pass, and editor diagnostics are clean. New evidence in
`glref-build/captures/native-shadow-unorm-6` is maximized 2560x1369 and byte-stable
between settled captures. Against preserved `layout-probe-16`, every outer-shadow
pixel and the three former panel-corner discrepancies now match exactly. The
top-origin sample (1504,572) is (17,17,17,255) on both backends, and the panel
background sample remains (32,32,32,255). Within the unchanged dialog/shadow region
[1043,1518) x [559,747), only 632 Close-button/focus-border pixels differ, each by
at most one RGB level. Message, checkbox, other panel pixels and outer shadow have
zero differences. Full parity remains open for that button and separate login UI
differences. The existing Window LNK4020 debug-symbol warning remains.

## Modal background composition correction (2026-09-14)

NV-00/01/02/12/14/17, pinned GL revision
`59108e15a1f8f94d2da7c674d937d19f5cf9450d`: `LLView::drawChildren`
does not exclude registered popups. The visible modal toast is painted under
LLFloaterView, then `LLPopupView::draw` paints that same toast again at its screen
position. Registration through LLModalDialog/LLFloaterView does not reparent it.
This defined two-pass composition was missing in native, which painted it once.
The PNG bytes match between the staged GL reference and native source;
Toast_Over's center is RGBA (32,32,32,204), and GL explicitly disables UI texture
compression. One blend over RGB 41 yields 34, whereas the reference's repeated
composition yields 32. Changing the texture or hard-coding RGB 32 is not the fix.

Native now appends the active modal subtree's already-prepared commands after
normal UI/menu preparation, preserving texture, tint, text, shadows, clipping
and order. CPU layout, callbacks and animation are not executed again. This is
native prepared-data composition, not a GL draw callback or shared visual helper.
The 65536-command budget is checked before append; image ownership and the
existing packet/upload/completion lifetimes remain unchanged. General popup
parity outside the native active notice is not claimed by this scoped change.

Widget209/209 verifies both background passes and identical resource, tint,
geometry and shadow colors. Window7/7, viewer relink and editor diagnostics pass.
New maximized native evidence is in `glref-build/captures/native-modal-composition-5`;
both settled frames are byte-identical, and panel geometry is unchanged at
(1053,632)-(1503,800). Against preserved `layout-probe-16`, the fixed top-origin
region [1043,1518) x [559,747), including the whole shadow, improves from 78137
differing pixels to 1237 of 89300. All residual channel errors are at most
1/255 RGB, alpha exact. The background sample at (1100,575) now matches exactly
at (32,32,32,255), and the nearby background at (1000,600) matches (41,41,41,255).
Residuals include 635 pixels inside the panel bounds and 602 outside. Full exact
pixel/effects parity remains open; no masks, alignment or tolerance changes were
used. The pre-existing Window LNK4020 debug-symbol warning remains.

## Login state and measured placement correction (2026-09-14)

NV-00/01/02/12/17: the pinned `FSPanelLogin::updateLocationSelectorsVisibility`
sets grid-panel visibility from ForceShowGrid; `updateLoginButtons` requires
nonempty username and password. Native now derives those states from its own
settings and editable controls, additionally requiring the session owner's
PreLogin state. Creation, paint preparation and session refresh apply the rule;
the login callback checks it again. No credentials are logged or authentication
implementation added. Tests use synthetic text in the combo editor, not a
nonexistent selected combo item.

The external LEAP driver now records bounded read-only LLWindow getInfo results.
`glref-build/captures/layout-probe-16/gl-layout.xml` measures the GL visible toast
wrapper at (1053,632)-(1503,800), outer toast 455x175, snap region y=63..1350,
and notification channel height 1252. The GL pre-login hidden toolbar retains its
60-pixel bottom panel plus 3-pixel layout spacing; the channel adds its 35-pixel
margin and excludes the 19-pixel menu bar. `showToastsCentre` uses this channel
height, ToastGap and outer-toast integer rounding, not full-window centering.
GL startup also copies ShowGroupNoticesTopRight into its internal session flag;
using the internal declaration default directly was incorrect.

Native independently reads the relevant layered skin dimensions and snapshots
the saved notices-position preference. Its channel calculation uses the live
ToastGap and ChannelBottomPanelMargin settings and preserves bounded tall-dialog
placement. This is the pre-login single-alert contract, not in-world toolbar,
teleport progress-view or multiple-toast composition certification. All layout
work remains CPU-only; GPU ownership, shaders and publication are unchanged.

Widget209/209 and Window7/7 pass, including grid visibility toggles, partial and
cleared credentials, session-state enablement, maximize/restore and tall agreement
containment. Viewer relink and editor diagnostics pass. The known Window PDB
LNK4020 warning remains. Native capture `native-login-layout-4` measures exactly
(1053,632)-(1503,800), matching the GL wrapper; its two 2560x1369 maximized frames
are byte-identical. Image inspection confirms the hidden grid row and disabled
empty-credential Log In button. GL probe exited zero with response and Goodbye.
This verifies panel geometry and the two login behaviors, not full pixel/effects
parity. The location placeholder and remaining decoration differences remain open.

## Maximized parity rerun (2026-09-14)

Result: FAIL. New GL evidence is in
`glref-build/captures/attempt-15-maximized-http`; rebuilt native evidence is in
`glref-build/captures/native-maximized-3`. Both clients are maximized at 2560x1369,
and each backend's settled pair has zero differing pixels. The full-client
zero-tolerance comparison in `glref-build/captures/maximized-parity-15-native-3`
reports 1121198 differing pixels, maximum RGBA errors 229/229/229/0. This count
includes differing login controls and is not a notification-only score.

The GL driver now enumerates already-visible notifications at STATE_LOGIN_WAIT,
responds to WarnForceLoginURL through the existing notification API without
resetting the fixture URL, and verifies its absence before submitting the target.
The saved preflight notification list is empty; image inspection confirms the
URL warning is absent. Driver build/framing self-test and editor diagnostics pass.
GL records the target response, exit zero and Goodbye. Native Window7/7 passes.

Native's recorded bottom-origin panel rectangle is 1052,597,1502,765. The new
shadow and corrected controls are visible, but the native alert remains lower
than the GL alert and its shadow appearance differs. The prior focused-centering
hypothesis therefore does not establish placement parity. No images were aligned,
resized or masked, no tolerances changed, and previous evidence is preserved.
Further renderer correction is separate from this requested capture rerun.

## Native alert decoration correction (2026-09-13)

Requested scope: correct native decoration and placement, then repeat the
maximized parity capture separately. No new parity claim or reference capture is
made by this change. Previous evidence remains intact.

NV-00 source contract, pinned GL revision
`59108e15a1f8f94d2da7c674d937d19f5cf9450d`, Windows/default skin:

- `LLToastAlertPanel` sizes buttons with measured label plus `OO` plus two
   4-pixel pads, with 8 pixels between buttons. `LLCheckBoxToastPanel::setCheckBox`
   adds label line count times line height plus half a line, and places the check
   above the 23-pixel buttons and 16-pixel bottom padding. Font line height is
   `ceil(ascender) + ceil(descender)`, not the font's baseline advance.
- `LLToast` owns an invisible outer rectangle, 5 pixels wider and 7 pixels taller
   than its visible wrapper. `LLModalDialog::onAppFocusGained` centers that outer
   rectangle through `centerOnScreen`/`LLView::centerWithin`. Native opening and
   resize use these outer dimensions for the focused capture state. The separate
   initial `LLScreenChannel::showToastsCentre` stacking/progress-view policy is not
   certified by this focused single-alert placement correction.
- Wrapper background precedes `LLToastAlertPanel::draw`'s shadow and controls;
   `LLToast::draw` then adds the wrapper shadow with a one-pixel inset. Each shadow
   has interpolated alpha on right/bottom edges and corner triangles, a one-pixel
   overlap, and five-pixel outward reach. The first uses `ColorDropShadow` directly;
   the second applies current transparency. No uniform ring approximation is used.
- `LLFocusMgr` interpolates `FocusColor` toward white during the focus flash,
   rounds border width from 1 to 2 pixels, and multiplies alpha by 0.4 when the
   application is unfocused. The pinned `LLPanel::updateDefaultBtn` is empty:
   default-button activation after 0.5 seconds is keyboard state, not an extra
   visual border. Actual focus drives the button border.

Native design (NV-01/02/03/12/14): native font/layout owns the CPU dimensions;
the native alert panel opts into two ordered shadow meshes in widget paint.
Optional per-vertex colors flow through `LLVKWidgetGpu` into `LLVKUiPacket`.
The existing UiVertex color attributes and alpha-blended shader interpolate them;
there is no shader ABI change, new GPU resource, GL callback, or GL visual reuse.
Uniform triangle callers retain their interface. Packet checks reject nonfinite
colors/coordinates, invalid scissors and geometry-budget overflow before append.
Existing frame submission, resource publication and completion retirement remain
unchanged. The GL reference worktree has no source diff.

Discriminating checks passed: Widget209/209 covers font-derived check geometry,
maximize/restore with outer-toast padding, twenty gradient triangles, alpha
endpoints and shadow reach, and delayed default-button state. GPU10/10 covers
per-vertex alpha through widget GPU preparation, invalid-color rollback, and
native recording/presentation. Window7/7 passes the actual alert lifecycle and
native browser/input integration. Editor diagnostics and diff whitespace checks
are clean. The existing Window target LNK4020 PDB warning remains; debugger symbol
integrity is not certified. Exact pixels, focus transitions and broader alert
states still require the next matched maximized parity capture.

## Both-backend maximized captures (2026-09-13)

The requested maximized capture set now includes both backends at 2560x1369.
The passed GL pair in `glref-build/captures/attempt-11-maximized` was preserved;
native was recaptured in `glref-build/captures/native-maximized-1` with
LLVK_NOTIFICATION_CAPTURE_MAXIMIZED enabled and IsZoomed asserted at capture time.
Both pairs have zero differing pixels within their own backend. Native Window7/7
passed and the capture-enabled fixture exited zero; no capture processes remained.

The full-client cross-backend comparison is retained in
`glref-build/captures/maximized-gl-native-comparison`: 3421375 differing pixels,
maximum channel differences 229/229/229/0. This is NOT a notification-only score:
GL has live browser content while the missing-helper native fixture has none.
Image inspection additionally shows the native modal retaining its pre-maximize
rectangle (295,287,728,480 in bottom-origin UI coordinates) instead of recentering,
plus differing panel/button/text appearance and the missing reference shadow.
Thus capture rerun and per-backend repeatability are complete, but parity fails;
no image scaling, masking or tolerance changes were used. Previous captures remain
untouched. The build still reports the existing LNK4020 debug-symbol warning;
these results do not certify debugger symbol integrity. No commit was made.

## GL fixture verification result (2026-09-13)

Pinned reference `59108e15a1f8f94d2da7c674d937d19f5cf9450d` built without source
modifications in the isolated `glref-build` directory. Capture attempt 9 successfully
launched the external LEAP driver, submitted MediaPluginFailed with media_plugin_cef,
captured the visible reference alert through Windows Graphics Capture, recorded
`close=true, ignore=false`, and exited zero with Goodbye. No viewer/driver remained.
Evidence is retained in `glref-build/captures/attempt-9`, including the executable
hash manifest, request/response XML, raw images and repeatability comparison.

Qualification FAILED: the two GL frames differ at 166075 pixels (maximum RGB errors
242/242/242, alpha error 0). Visual inspection shows changing live login-page content
behind the alert. Actual GL client extent is 2560x1369, not the requested 1024x738
or the retained native capture extent. The user confirmed that they manually
maximized the GL client during this run. This explains the extent mismatch and is
not evidence that the fixture failed to apply its initial window-size settings.
Do not resize/mask these images or count them
as a matched reference. The fixture's submission, capture and graceful-close path
works, but captures must use matching actual client dimensions and a stable background
before reference repeatability and cross-backend parity can pass. Phase 1 stays gated.

Driver corrections verified offline: derive setting types/encodings from the pinned
declarations; load nonpersistent LeapCommand through the profile-local default file;
use forward slashes because LLLeap's tokenizer treats backslashes as escapes; send
notation LLSD to the viewer while accepting its binary LLSD output. The pinned
llleap.cpp explicitly requires notation from children. Earlier failed attempts are
retained; they produced no accepted reference captures. Pending quit confirmations
were acknowledged normally, never force-stopped. Completed native repeatability
evidence remains unchanged.

## Notification capture acceptance protocol (2026-09-13)

Scope update: the user explicitly deferred world-dependent notification validation
to the corresponding stages. Phase 2 owns real authentication/TLS/connection error
captures; Phase 3 owns account-linked offers, IM/chat/inventory/voice notifications,
their persistence and reconnect behavior; Phase 4 owns composition over world,
water/postprocessing and in-world overlays. Local alert/template/input/suppression
captures remain in the current gate and do not require world.

Fixture implementation in progress: `notification_capture` uses external Windows
Graphics Capture and D3D11 staging-map completion, not the legacy Vulkan readback.
Its self-test passed physical client cropping and BGRA-to-RGBA conversion on a known
window. Output has an 8-byte little-endian width/height header and TOP-origin RGBA8
(unlike historical bottom-origin files). Existing files are never overwritten.
The native window regression accepts `LLVK_NOTIFICATION_CAPTURE_DIR` to capture two
settled MediaPluginFailed states with the production renderer and separate capture
processes. Capture evidence is supplementary until paired with the pinned GL oracle.

Sequence requested by the user: define capture validation, validate existing
notifications if feasible, then proceed to Phase 1 only after the notification gate
passes. Existing component passes are reused; they are not capture acceptance.
NV-00/02/03/12/14/15/17 apply. This protocol does not require authentication for
local notification fixtures and does not waive the separate full-viewer run policy.

### Matched inputs and coverage

Use separate backend-exclusive processes. The GL fixture must instantiate the
unchanged reference notification/template/channel/alert implementation; the native
fixture must use the actual native queue, widget factory, paint and GPU submission.
Never use a GL-owned view or GL-produced pixels as the native implementation.
Pin the historical GL oracle `59108e15a1f8f94d2da7c674d937d19f5cf9450d`; a current
GL build is supplementary evidence unless a reference update is explicitly approved.

For each capture, record source revision plus dirty-patch hash, executable hash,
template/payload hash, skin/theme/font/catalog hashes, locale, UI scale, DPI,
client/render extent, device/driver, render format, color space, capture mechanism,
warning preferences, focus/cursor state and notification-relative time. Use temporary
profiles and synthetic local inputs without secrets. Match backgrounds because
alpha and shadows cannot be compared against different underlying pixels.

Enumerate all 30 admitted reference notification templates, not only
MediaPluginFailed. Group by construction/behavior to organize the work, but retain a
result per template. Include at least media launch failure for login and auxiliary
browsers, AutoReplace invalid entry/import, confirmation and editable forms, and
ignore/default/saved-response variants. Native-only structured error notices and
session agreements have no identical GL template; validate their mapped source
contract separately rather than inventing a pixel oracle for their wording.

Capture each applicable state: initial appearance before default activation;
settled appearance after the 0.5-second guard; hover, press and keyboard focus;
checked/unchecked ignore control; entered/selected text; long-text scroll positions;
stacked notifications over an existing dialog; acknowledgement/focus restoration;
ignored redisplay; persistence failure and retry. Include English and the currently
tested German/French/Japanese locales, other shipped locale fit checks, and normal
and constrained client extents at agreed DPI/UI scales. Transition sequences must
be sampled at matching notification-relative times, not matching frame counts.

### Capture and comparison

Prefer a dedicated controlled fixture using application-owned offscreen targets:
GL readback before swap, and native attachment-to-staging copy before release with
explicit layouts, completion fences and noncoherent invalidation. Alternatively,
use the same external lossless Windows capture path for both visible clients;
exclude occlusion and mismatched compositor/HDR/scaling state. External capture
proves displayed composition, not raw attachment alpha. Either path must capture
the full client plus a notification region including its entire shadow, never just
the message rectangle. OS fallback dialogs are a separate native Win32 capture set.

Repeat GL states to establish reproducibility before comparing native output. Keep
original bytes and metadata. Permit only documented row-origin/channel-order
normalization; do not resize, align away geometry differences, mask discrepant
pixels, blur or alter gamma to improve scores. With the current exact UI acceptance
rule, require zero unexplained pixel differences and matching geometry, clipping,
text/icons, ordering, colors, alpha composition and effects. Reference variability
blocks that state until controlled; it does not authorize widening a tolerance.
Historical opaque/alpha tolerance notes are not new approval under NV-17.

Produce difference images, differing-pixel counts and channel-error maxima alongside
original images. Inspect the originals and differences, and assert callback counts,
selected actions, suppression persistence, stale-response rejection and restored
focus. A matching still image alone does not pass interaction or transition states.
Retain completed state evidence across retries; invalidate only states affected by
the implementation or input changes. Missing captures are UNVERIFIED, mismatches
are FAIL, and only complete applicable evidence is PASS.

### Current preflight result

The approach is feasible, but the checked-in capture path is not currently an
executable matched-notification acceptance harness:

- `gl_capture_frame_once` in `indra/newview/llviewerdisplay.cpp` is one-shot after
   90 frames at/after STATE_LOGIN_SHOW. It does not trigger or sequence notification
   states and does not establish the pinned reference executable's identity.
- `captureFrameOnce` in legacy `indra/llvulkan/llvksession.cpp` is not wired into
   the current native LLVKWindowMgr loop. Its `readbackSwapchain` dependency selects
   the last presented image after device idle, without reacquiring it. NV-15 forbids
   treating idle as renewed ownership of a presented swapchain image. Do not invoke
   this helper as valid native acceptance capture.
- The historical diff script cited by the old UI plan is not present in the
   current workspace search, and `tools/vulkan` is empty. Existing root captures
   gl_login.rgba/vk_login.rgba date from September 3 and vulkan_capture.rgba from
   September 2; they predate the current implementation and lack a matched current
   notification manifest. They are retained, not counted or overwritten.
- Source preflight identifies unresolved alert presentation: GL
   `LLToastAlertPanel::draw` explicitly emits `gl_drop_shadow`; native
   `advanceNotices` builds a plain panel and children. Inspection of the panel branch
   in `indra/llvulkan/llvkwidgetpaint.cpp` confirms only the panel-sized background
   draw, with no external alert drop-shadow primitive. This is a source-level
   presentation mismatch, not a measured pixel result.
- The only registered worktrees are the current native-error-messaging checkout
   and native-session-owner at `485967401a`; neither is the pinned GL oracle.
   Existing Release/RelWithDebInfo viewer binaries are not established as oracle
   executables merely by their presence. No dedicated notification fixture was found.

Status: no matched notification capture pair accepted; capture validation remains
UNVERIFIED, not passed. Previous Widget209/209, Window7/7 and link results establish
only their recorded behavioral/build coverage. No full viewer was launched and no
unsafe capture helper was run. Phase 1 progression is held by the requested gate.
Next prerequisite is a reproducible reference fixture plus safe native capture (or
matched external capture), followed by repair of any observed presentation mismatch.

## Browser launch notification contract (2026-09-13)

NV-00/01/03/15/17, source `9e2f4548b1`, Windows native browser. Reference:
`LLViewerMediaImpl::handleMediaEvent`, MEDIA_EVENT_PLUGIN_FAILED_LAUNCH, marks the
media source failed and queues `MediaPluginFailed`; it does not terminate the viewer.
The `notifications.xml` template is alertmodal with an ignore preference and plugin
substitution. Runtime plugin-crash notification is explicitly disabled in the GL
source to avoid flooding; page-navigation failure is a different error-page contract.

Native design: consume the original localized notification data through the native
parser and modal owner, retaining ignore policy. Browser launch failure must remove
the unavailable browser from paint/input participation while leaving the viewer and
notification responsive. Do not convert page-load failures into plugin-launch alerts
or expose raw browser error strings. Adopted browser owners still retire normally.
Discriminating checks: template substitution/ignore handling and an actual missing-
helper window fixture that presents one acknowledgement and exits normally on close.
These checks are behavioral evidence, not measured alert visual/effects parity.

Implemented follow-up:

- Both login and auxiliary native browser launch failures queue the original
   `MediaPluginFailed` template with `media_plugin_cef`, matching the reference MIME
   implementation label. They no longer terminate the viewer solely because launch
   failed. Unavailable browser widgets are excluded from painting/input, so the
   acknowledgement remains presentable; auxiliary Media Browser retains its original
   `plugin_fail_text` fallback. Failed auxiliary owners are released without consuming
   the live-view limit. Page-navigation errors remain separate, unchanged paths.
- Native modal alerts now construct `alert_check_box.xml` independently, using the
   reference `skipnexttime`, `skipnexttimesessiononly` or `alwayschoose` strings as
   selected by their parsed ignore policy. Reference roots are
   `LLCheckBoxToastPanel::setCheckBoxes/setCheckBox` and
   `LLToastAlertPanel::onButtonPressed`. Acknowledgement records the ignore choice
   and applicable saved response through existing native warning settings. Failed
   persistence keeps the dialog open and does not change suppression; successful
   retry dismisses it. Queued responses retain one-shot delivery.
- AutoReplace already queues `InvalidAutoReplaceEntry` and `InvalidAutoReplaceList`.
   Existing UI fixtures now explicitly exercise invalid entry submission and invalid
   file import, asserting the reference notification names, retained valid entries,
   and absence of a duplicate generic error. Startup AutoReplace logging is unchanged.

Validation: Widget209/209 and Window7/7 passed; the latter uses an actually missing
browser helper, presents and acknowledges both login and auxiliary failure alerts,
keeps login controls enabled, and closes normally. Widget checks cover localized
plugin substitution, saved suppression, the actual ignore checkbox and failed-save
retry. Native Viewer Link Validation and touched-source diagnostics passed.
No full viewer/profile run, GL implementation edit, commit or push was performed.

Remaining qualification: matched reference captures for exact alert geometry,
clipping, shadows, text and interaction effects have not been produced. Behavioral
fixture results are not full visual parity. `NoPlugin` belongs to MIME-plugin
selection, which native CEF-only support does not implement; it is not fabricated
for ordinary page-load errors. Browser error-page and texture-fallback contracts
remain outside this notification-specific change. Earlier scope counts of 29
admitted templates predate the addition of `MediaPluginFailed` (now 30).

## Log-only reporting parity (user clarification, 2026-09-13)

Native error reporting to the log is accepted as reporting parity where the
corresponding OpenGL service path is also log-only. Absence of a native notification
is not a gap for those paths; do not add popups solely to claim reporting coverage.
This accepts the reporting channel, not exact diagnostic text or complete service
behavior/visual parity.

In particular, audio-engine initialization and AutoReplace startup settings loading
are accepted as log-only reporting parity. Their source references are
`idle_startup` in `indra/newview/llstartup.cpp` and
`LLAutoReplace::loadFromSettings` in `indra/newview/llautoreplace.cpp`. Native audio
gain warnings likewise do not require an added notification where the GL path has
none. AutoReplace import/edit validation remains a separate notification workflow.

For browsers and textures, assess each failure path separately: browser error pages,
plugin-launch notifications and texture fallback/loading states remain applicable
consumer contracts. Logging alone does not replace a user-visible response supplied
by the corresponding GL path. An internal unconsumed error is not a logged error.

## Accepted local-service scope (2026-09-13)

The user accepted completing reporting for services already present in the native
viewer, with transport/authenticated reporting delivered alongside those services.
This supersedes the earlier pending-scope paragraph below, not the requirement for
measured visual parity. No new authentication/world service or GL visual code is
introduced by this increment. Affected invariants: NV-00/01/03/15/17.

The local reporting implementation now includes:

- Localized independent OS fallback before visual service initialization and after
   teardown, with immutable catalog snapshots, strict UTF-8 decoding and English
   fallback when no valid catalog is available. Reporting cannot depend on the failed
   renderer. Missing/default-settings discovery before catalog loading remains English.
- Scoped fatal log and `LLUserWarningMsg` handling. Missing files and OOM use stable
   codes 1015 and 1014 rather than arbitrary warning text. Fixed diagnostic literals
   can be written without formatting allocation. The OS presenter has a fixed OOM
   message if formatting allocation fails. Actual exhausted-memory execution is not
   claimed by controlled warning injection.
- An atomic fatal-state signal stops the native window's normal service loop after
   a fatal warning; startup avoids presenting it a second time. LL_ERRS still obeys
   its existing fatal termination contract. Prior warning handlers and preallocated
   OOM strings are restored after native ownership. Registration failure removes any
   installed recorder before unwinding.
- The shared CPU-only `LLUserWarningMsg` API now exposes snapshots of its handler
   and OOM strings and serializes invocation/configuration with a recursive mutex.
   This closes the worker-callback versus handler-retirement race. Existing GL
   callbacks, message selection and rendering are unchanged. No GL warning handler
   is called by native startup. Setup/teardown occurs at the application owner, not
   per dialog. Concurrent backend ownership remains forbidden.
- Runtime audio, voice, translation verification and preview failures report codes
   1016-1019 with cause-specific advice instead of WindowUnavailable. Auxiliary browser
   failures preserve BrowserUnavailable; failed shutdown persistence reports
   SettingsWrite. These retain their existing stop policy and do not invent retries.
   All six added message keys exist in all 13 shipped catalogs (78 XML entries).
- Existing native local notices now share bounded admission (64 queued plus one
   active) and one-shot queued responses. A rejected admission never invokes its
   callback. Ignore/default-response policy remains before queue admission, and saved
   ignore settings retain their existing persistence path. Error copy requeues a new
   notice; session recovery retains exact request-tag checks.

Source check: all 29 notification declarations on the native parser's allowlist
are alert/alertmodal with no persistence, duration, expireOption or unique policy.
They therefore stay transient rather than replaying callbacks against expired
owners after restart. This does not implement the full GL notification channel
graph, persistent offers or timed toasts for future authenticated services.

Verification: standalone real OS-dialog/formatter tests passed; configured native
Window7/7 plus cold-cache regression passed; Widget209/209 passed; shared llerror
regression18/18 passed. Worker warning fixtures exercise both missing files and
OOM, typed fatal state, one-shot reporting and restoration. Catalog tests cover
reload/destruction, Unicode and invalid-byte fallback; all 78 added entries parse,
are unique/nonempty and fit the formatter limit. Source diagnostics and whitespace
checks passed. Native Viewer Link Validation completed successfully after rebuilding
the consumers of the shared warning header.

Not claimed: exhaustive detail classification of every legacy dialog error, a
minidump/crash-upload service, real OOM exhaustion, full-viewer runtime acceptance,
translation review or measured exact visual/effects parity. Future transport/TLS/MFA
and account/world service increments must supply their own producer identity,
recovery policy and native consumer; fixture errors cannot satisfy those gates.

## Fatal and fallback contract follow-up (2026-09-13)

NV-00/01/03/15/17, source `74a22e59bf`, Windows native selection. GL roots remain
`errorCallback/errorHandler` in LLAppViewer: localize before OS presentation, record
fatal context and markers, then preserve the logger's fatal termination. Native
uses independent OS presentation and a scoped CPU-only LLError recorder. Audited
`addRecorder/removeRecorder/log` serialize recorder access under mRecorderMutex;
native callbacks never log or mutate registrations. Raw fatal text is deliberately
not copied to the native report. The existing logger and its other recorders are
unchanged and are not claimed to redact their own outputs.

Native design: preload immutable selected-skin error strings before service startup;
retain snapshots independent of UI, font and renderer owners. Strict UTF-8 conversion
falls back to English on invalid catalog bytes. Fatal logging writes a per-process
native record through Win32 file IO, then invokes the independent presenter once.
The recorder does not swallow the existing fatal function or turn LL_ERRS into a
recoverable condition. Registration lives across native startup and teardown and is
removed before its state is destroyed. No GL crash marker namespace is reused.

Discriminating checks: real OS dialog Unicode/invalid-byte/recursive fallback;
catalog reload/destruction retains old snapshot; controlled LL_ERRS exercises actual
recorder delivery, one-shot presentation, secret-free record and removal while the
test-only fatal function throws. This does not implement OOM/global warning hooks,
minidumps/crash submission or live network protocol reporting.

Implemented and verified in the working tree after `74a22e59bf`:

- Native OS fallback resolves selected-skin strings and converts UTF-8 strictly;
   invalid data or unavailable catalogs use the English fallback. Catalog snapshots
   remain valid after reload or destruction of the loader. Startup loads them before
   browser/cache/service initialization; failures before valid settings/catalog load
   necessarily remain English. Shutdown and window fallback retain the snapshot.
- `LLVKFatalReporting` installs an independent scoped recorder after native backend
   selection. It writes `logs/native-fatal-<pid>.log` with CREATE_NEW, records only
   stable numeric facts and flushes before presentation. Existing records are not
   overwritten. File failure emits a fixed debugger diagnostic and does not suppress
   presentation. An atomic immutable resolver snapshot supports worker reporting;
   an atomic one-shot gate suppresses recursive/repeated fatal presentation.
- Standalone formatter/real OS-dialog tests passed for translated Unicode, invalid
   UTF-8 and recursive fallback. Configured Window7/7, error and cold-cache tests
   passed. Window7 checks catalog lifetime and actual LL_ERRS recorder delivery,
   structured record contents, one-shot behavior and deregistration using a test-only
   throwing fatal function. Native Viewer Link Validation and source diagnostics
   passed. No full viewer or real profile was used.

Full reporting parity is NOT closed. Outstanding local work includes OOM and
LLUserWarningMsg lifecycle hooks, richer producer-specific diagnostics, full native
notification channels/expiry/persistence and visual qualification. Reporting for
actual authentication/TLS/MFA, protocol retry/backoff and authenticated services
depends on the unimplemented Phase 2 transport and the Phase 3 service subset that
the roadmap requires agreeing before implementation. Those services cannot be
represented as complete by fixture messages. Crash submission/minidump parity is
also not established by the native numeric fatal record. Changes remain uncommitted
pending full-task scope resolution; the prior `74a22e59bf` commit is unchanged.

## Production recovery follow-up (2026-09-13)

The later [session integration](native_session_owner.md) report supersedes earlier
cache-only, missing in-app consumer and missing owner-action statements below.
The configured native window now proves that adopted browser/audio/voice producers
retire before a dependency's Pending/failure states, while the native recovery modal
continues presenting. Failed cleanup is not automatically retried; its named
Retry Cleanup button invokes the exact-tag owner command and completes shutdown.
Retired browser widgets are hidden before their frame maps are cleared, avoiding
the reproduced all-frames-Pending stall. Action names survive modal construction.

Long messages use a bounded read-only scrolling surface. All 13 shipped catalogs
contain native error keys and the added agreement Accept/Decline labels. Configured
Widget209/209, Window7/7 with error and cold-cache regressions, owner checks and
viewer link passed. This does not claim localized early OS fallback, real network
retry/backoff, HTML agreement parity, complete producer-specific diagnostics or
measured exact UI/effects parity. Historical test8 evidence below is unrelated to
the resolved window retirement stall and is not overwritten by these passing gates.

## Contract before implementation

Source/configuration: `485967401a08ed289ba5d194f03b9618ae0f5f4e`, Windows
native startup; historical GL oracle remains unchanged. Affected invariants:
NV-00, NV-01, NV-03, NV-12, NV-15 and NV-17.

1. GL contract: `errorCallback` and `errorHandler` in
   [llappviewer.cpp](../../indra/newview/llappviewer.cpp) select fatal/warning
   messages, use `LLTrans` with English fallback for early fatal errors, and
   call `OSMessageBox`. They also write crash/debug markers, manipulate
   `gDisconnected`, and pause/resume the watchdog on the main thread.
   `LLUserWarningMsg` dispatches registered handlers (including fixed missing-file
   and prelocalized allocation warnings). `OSMessageBox` in
   [llwindow.cpp](../../indra/llwindow/llwindow.cpp) hides/restores the GL splash
   and logs the entire message; `OSMessageBoxWin32` uses a global viewer HWND,
   converts UTF-8 and maps OS button results. Debug builds can return early on
   Cancel. These visual wrappers and viewer-global callbacks are not reused.
   Crash-marker/watchdog behavior is outside this non-crash failure slice;
   transitive crash-service migration remains open.
2. Native design: CPU-only structured facts (stable code, operation, unsigned
   generation and attempt) select catalog severity/recovery and message keys.
   No arbitrary text, path, URL, account name, network body or exception payload
   enters the diagnostic contract. English fallback works without a skin,
   widget tree, font, GPU or translation service. The resolver is a trusted
   localization-catalog seam, receives only a key, and must never resolve from
   untrusted request/exception text. Logs do not use resolver output.
3. Smallest ownership model: a standard-library value and fixed-capacity,
   owner-thread gate, plus independently owned Win32 presentation. The caller
   owns failure exit; presentation only acknowledges. No GL dispatch, resources,
   callback reuse, retry, backend switch or session recovery is introduced.
   Per-operation monotonic generation activation rejects cancelled/stale work;
   attempts are diagnostic facts, not permission to retry.

Falsifying check before editing: compile/run a standalone MSVC test without
viewer/GPU dependencies. Check exact deterministic diagnostics, catalog fallback
on missing/throwing/invalid entries, bounded duplicate state, operation isolation,
cancelled/late generation rejection, and absence of retry/cancel actions without
an executable owner. Add independent Win32 acknowledgement tests and verify the
actual startup failure routes after integration. Tests are not visual parity.

## Implemented production routes

- [llvkStartup](../../indra/llvulkan/llvkstartup.cpp): after native backend selection,
  every existing fatal branch now reports a stable code rather than the producer's
  string. This covers settings/defaults/modes/reset/persistence, unsupported
  options, cache planning/start, startup resources, browser DLL setup, window
  execution and shutdown. The catch-all no longer forwards `exception.what()`.
  Nonfatal AutoReplace warnings also log only structured facts. A logging exception
  cannot prevent OS presentation. Acknowledgement always returns `-1` from the
  failure handler; it never continues into GL or retries.
- [LLVKWindowMgr](../../indra/llvulkan/llvkwindowmgr.cpp): an optional `failureCode`
  output classifies missing UI resources, renderer initialization/upload/frame/
  swapchain failures, initial browser/load failures and final shutdown failure.
  Other existing false returns retain `WindowUnavailable` as a conservative
  generic classification. The pointer is borrowed only for synchronous `run()`;
  it must remain valid for that call and is meaningful only when `run()` is false.
  Startup presents after `run()` unwinds its window/visual owners, avoiding a
  dependency on the failed renderer. Existing out-of-date swapchain handling is
  unchanged; no new device-loss recovery is asserted.
- The existing `takeDialogError()` and external-browser launch-failure boundaries
  use a generic, safe, nonfatal OS notice. Each drained notice/user launch failure
  is a new local notice generation. This does not deduplicate different user
  commands or invent identities for legacy producers. Direct window warning logs
  for audio, browser and shutdown no longer include arbitrary producer details;
  file-picker exceptions also become fixed text. URL confirmation remains an
  intentional user-request prompt, not an error diagnostic, and is unchanged.
- [llvkPresentErrorFallback](../../indra/llvulkan/llvkerrorwin32.cpp): independently
  calls Win32 `MessageBoxW`, validates an optional borrowed HWND, uses only OK
  acknowledgement (the semantic action is `Close`), and returns whether the OS
  acknowledged it. The caller owns viewer exit or continued operation. The current
  bootstrap fallback deliberately uses the built-in English catalog, hence its
  narrow-to-wide conversion only encounters ASCII. No widget/GPU/GL owner or
  localization service is invoked. Allocation failure uses fixed literal text;
  OS failure emits a fixed diagnostic to debugger/stderr. A thread-local guard
  rejects recursive presentation. It does not retry a failed OS dialog.

## Consumer contract

The parallel session owner need not include the new header yet. Its reporting seam
can remain `report(uint32_t code, uint64_t generation)`, with operation implicitly
`Session` and an optional numeric attempt supplied later by the adapter. Never send
exception strings, URLs, paths, account identifiers or response bodies through it.

The concrete value is `LLVKError{Code, Operation, uint64_t generation,
uint64_t attempt}` in [llvkerror.h](../../indra/llvulkan/llvkerror.h). All enum
values are explicit and must not be renumbered. Unknown codes normalize to 1000;
an invalid operation formats as `unknown` and is rejected by the gate.

| Code | Meaning | Severity / Recovery |
|---|---|---|
| 1000 | Unexpected | Fatal / Stop |
| 1001 | DefaultSettings | Fatal / Stop |
| 1002 | SettingsMode | Fatal / Stop |
| 1003 | UnsupportedArguments | Fatal / Stop |
| 1004 | SettingsRead | Fatal / Stop |
| 1005 | SettingsWrite | Fatal / Stop |
| 1006 | CacheUnavailable | Fatal / Stop |
| 1007 | StartupResources | Fatal / Stop |
| 1008 | WindowUnavailable | Fatal / Stop |
| 1009 | RendererUnavailable | Fatal / Stop |
| 1010 | BrowserUnavailable | Fatal / Stop |
| 1011 | ShutdownFailed | Fatal / Stop |
| 1012 | OperationFailed | Error / Continue |
| 1013 | OptionalSettings | Warning / Continue |
| 2000 | NetworkUnavailable | Error / OwnerRequired |
| 2001 | TlsRejected | Error / OwnerRequired |

Operations: Bootstrap=0, Settings=1, Cache=2, Window=3, Renderer=4, Browser=5,
Shutdown=6, Session=7. Severity: Warning=0, Error=1, Fatal=2. Recovery: Stop=0,
Continue=1, OwnerRequired=2. Only Action::Close=0 is currently exposed. `Stop`
requires caller teardown; `Continue` only acknowledges an operation failure;
`OwnerRequired` is not permission to retry. Network/TLS codes have no production
network producer or recovery handler in this change.

`policy()` supplies the stable localization key, English message, severity and
recovery. `format(resolver)` returns title/body/diagnostic/action. The trusted
catalog resolver receives only a key; empty, oversized (>2048 bytes), control-
containing, legacy missing-string, or throwing results fall back to English.
It is not a sanitizer for malicious catalog data, nor a UTF-8 validator.
`diagnostic()` is resolver-independent and contains only normalized code,
operation and unsigned numeric generation/attempt/severity/recovery. There are
no free-form detail fields or string substitution parameters.

`LLVKErrorGate` is owner-thread-confined, not internally synchronized:

1. `begin(operation, generation)` activates a monotonically increasing generation.
   Repeating an active generation is idempotent and does not clear duplicates.
   Rewinding, reopening a cancelled generation, and wrapping to zero are rejected.
2. `accept(error)` rejects inactive, future or stale generations and duplicate
   normalized codes within the current operation/generation. Attempt is diagnostic
   context, not part of the deduplication key.
3. `cancel(operation, generation)` invalidates only the matching active generation;
   stale cancellation cannot invalidate newer work. The owner must advance its
   generation before a retry and reject stale actions as well as stale reports.
4. Storage is bounded to eight codes per each of eight operation slots. Full slots
   evict the oldest code, so a sufficiently old duplicate can be admitted again.
   New generations clear only that operation's history. Counters must not wrap;
   allocate a new gate/owner lifetime if the uint64 range is exhausted.

Startup uses generation/attempt 1 for its single synchronous lifetime. Window
notice counters are local to `run()`, not authentication/session generations.
The session adapter, owner-driven Retry/Cancel actions and in-app deduplication
with genuine producer operation identities remain future integration work.

## Verification and limits

2026-09-13, Windows, MSVC standalone `/std:c++20 /EHsc /W4 /WX`: passed
[llvkerror_test.cpp](../../indra/llvulkan/tests/llvkerror_test.cpp), linked only to
the standard library and user32. Tests cover exact diagnostic text, resolver
fallback/exception/control/length behavior, policy, operation isolation, duplicate
eviction, cancellation/late generations, unknown codes and uint64 boundaries.
The actual production OS presenter is exercised without a GPU or viewer; a CBT
hook and timer acknowledge the real dialog and check its close-only controls and
recursive-report rejection. The printed `operating-system error presentation
unavailable` line is expected from the recursive-report negative test. Initial
test acknowledgement timing and a Windows `max` macro collision were corrected;
the final focused runs passed.

Configured main `build-vc170-64`, RelWithDebInfo: the existing Native Vulkan GPU
Validation task compiled `llvkerror` and `llvkwindowmgr`, and ran the new
`INTEGRATION_TEST_llvkerror` successfully. The target follows the existing standalone
integration-test/post-build pattern and is a window-library dependency only when
`LL_TESTS` is enabled. CMake Tools itself could not configure and returned no
diagnostics; the unchanged existing task supplied the required Cygwin convenience
environment. No ignored task file was edited.

The first GPU-context run passed 10/10 on AMD Radeon RX 9070 XT. A later run failed
unchanged context test 8, `new image paired to browser frame`, and stopped before
compiling the window library. A subsequent configured run completed the error
test and window library. This intermittent GPU assertion is unresolved and was
not repaired or hidden by changing reference data or tolerances.

The existing Native Viewer Link Validation task passed, producing
`newview/RelWithDebInfo/vulkanstorm-bin.exe` in the configured build. All started
tests/builds have finished. No full viewer was launched, no real profile
was accessed, and no credential or microphone tests were run. Standalone binaries
and objects were created only in a unique temporary directory. GL implementation,
session sources, the parallel worktree and `mcp-Vulkan` were not edited.

Main-agent follow-up: Native Vulkan Window Validation passed 7/7, including the
standalone error presenter and cold-cache regression. The existing missing-browser-
helper fixture now asserts the production `BrowserUnavailable` failure code before
verifying partial window teardown. The narrowed window gate does not exercise or
resolve the intermittent GPU-context test8 assertion recorded above. No renderer
implementation or test tolerances were changed to mask that failure.

Remaining roadmap requirements: localized catalogs and localized OS presentation;
cause-specific producer migration instead of generic in-app codes; a native
in-app notification/detail/copy surface; executable owner-controlled Retry/Cancel
with backoff; authenticated request identity and stale-action rejection; real
network/TLS failure injection; startup/cache permission and missing-resource
fault-injection through the complete `llvkStartup` entry point; lifecycle tests
before/during/after renderer failure; non-Windows presentation; and measured exact
UI/effects parity. Pre-selection argument/environment/settings discovery in
`llvkStartup` is unchanged and is not covered by its post-selection catch-all.
Internal legacy producer strings and lower-level service logging remain outside
this reporting-boundary audit and must not be forwarded by new consumers.

## Main session/UI integration (2026-09-13)

This dated section supersedes the earlier integration-open items, not the earlier
runtime evidence or the remaining roadmap gates. The user authorized integration
in main using the tested session-owner worktree; no merge or commit was performed.
The [session integration contract](native_session_owner.md) records NV-00 source,
ownership and fatal-retirement behavior.

Production routes now include:

- `LLVKViewerUi::showError/queueError`: the existing native modal queue, focus and
   delayed default response; selected-skin strings through the independently parsed
   catalog; close and safe diagnostic copy. Copy failure retains the original
   notice. New error admission is bounded to 64 queued notices, plus the active
   notice, but this does not globally bound unmigrated legacy producers.
- `LLVKViewerUi::setSessionOwner/refreshSession`: Login invokes the real owner;
   snapshots control input enablement, progress/cancel and failures. Missing transport
   remains PreLogin and reports that no request was sent. An installed transport's
   unavailable response is instead a network failure. Recovery buttons come only
   from the owner's status. Captured pointer/tag checks reject old Retry Login;
   Cancel and Retry Cleanup additionally use the owner's exact-tag overloads.
   Old active/queued session notices are retired on a changed snapshot. Wait is
   polled by the window; RetryCleanup is never an automatic per-frame retry.
- Startup adopts the actual cache application service before acquisition and passes
   the owner to the synchronous window run. The cache owns its shared native status
   presenter through retirement. Existing visual/browser/audio/voice owners remain
   independent; they are not claimed as newly adopted services. Fatal failure after
   an unsuccessful cleanup retains the cache owner until process exit rather than
   silently retrying or destroying live workers.
- Generic drained dialog and external-browser failures now queue native errors
   when the UI exists; unavailable/full presentation falls back to the independent
   OS presenter. Renderer-failure presentation remains outside the failed renderer.

Additional stable codes: 2002 TransportUnavailable (Continue), 2003
SessionCleanupFailed, 2004 AuthenticationFailed, 2005 ConnectionFailed, 2006
SessionTimeout and 2007 SessionFailed (all OwnerRequired except 2002). The base
error formatter still exposes only Close; executable recovery exists solely in
the owner/UI adapter. No TLS bypass, automatic network retry or backoff is invented.

English and German `strings.xml` catalogs have 27 new native-only keys. Existing
keys, GL visual functions, reference output and tolerances are unchanged. Other
locales use the existing English-layer fallback for these entries. The standalone
OS fallback remains English. Localization is not a claim of completed translation
coverage or exact rendered parity.

Final evidence: strict standalone error/Win32 tests pass, including all new code
policies; imported owner passes the source 382-check suite and the new main owner
suite. Final isolated production/widget/window compiles and fixture links pass
using configured headers/definitions and `/O2 /WX`. Widget tests 208/209 and window
test 7 pass with exit zero. Window7 uses temporary directories and real cache
workers, including accepted-write drain, partial acquisition failure and retained
status ownership. No full viewer, credentials, real profile or microphone is used.

A full isolated widget run hit stack overflow `0xC00000FD` at existing test50 after
passing 1-49; an earlier unoptimized fixture failed before reporting a result.
Final exact-test runs pass with all replacement objects optimized. The broad-run
failure remains unexplained; no assertion or stack tolerance was changed. The
configured full widget/window/viewer-link gates must be rerun by main for the
combined change. Previous configured viewer-link evidence above is historical,
not evidence for these new objects. GPU test8 was not run, edited or claimed fixed.

Remaining: complete producer identities, actual transport/TLS fault injection,
retry/backoff protocol policy, remaining application/session/region services,
localized OS fallback, all locales, complete startup and renderer-loss lifecycle
injection, full runtime shutdown and exact visual/effects/keyboard/nesting parity.
The existing native modal differs from the reference's scroll-limited text,
drop-shadow drawing and some keyboard semantics; reuse does not close those gaps.

## Review record

| Field | Evidence |
|---|---|
| Contract | NV-00/01/03/12/15/17; first production error boundary, not full error roadmap closure |
| Reference | Source revision above; no GL oracle or tolerances changed; additive native-only English/German catalog keys |
| Data flow | Stable native codes and numeric identities; optional synchronous window failure output; no visual/material ABI changes |
| GPU safety | Error model/gate/presenter allocate no GPU resources; renderer errors reach startup after window-owner unwind; existing retirement and WSI policy unchanged |
| Validation | Standalone CPU/real OS-dialog tests and configured native compile; GPU test intermittency recorded above; no visual parity claim |
| Limits | Owner recovery and English/German in-app localization are integrated; transport, remaining producers/locales, full configured/runtime gates and exact parity remain open |
| Change class | Native error-boundary implementation and diagnostic privacy correction |