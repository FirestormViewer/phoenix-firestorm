# Native error messaging

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