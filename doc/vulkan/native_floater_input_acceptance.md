# Floater input corrections, 2026-09-20

Pre-edit NV-00 record; NV-01, NV-11, NV-12 and NV-17 apply.

Reference: unchanged GL oracle 59108e15a1f8f94d2da7c674d937d19f5cf9450d,
default Windows skin. Roots inspected: LLResizeBar::handleHover,
LLResizeHandle::pointInHandle/handleHover, LLFloater::layoutResizeCtrls,
LLMultiFloater::buildTabContainer, LLCheckBoxCtrl constructor/reshape.

1. Reference result: four three-pixel resize borders, eleven-pixel corner
   controls (full bottom-right rectangle, edge-shaped other corners), directional
   cursors during hover/capture, minimum size and root bounds during drag.
   Hosted tabs sit below the floater header. Checkboxes include their label in
   the hit rectangle; their reshape deliberately retains the old button extent.
2. Native result: CPU-owned floater hit testing supplies both press capture and
   a typed cursor to the Windows owner. Existing tree shapes remain the single
   geometry source for paint and picking. Reserve the Conversations header in
   its owned declaration. Construct alert checkboxes with final dimensions so
   initial wrapping cannot leave a large retained invisible hit region.
3. Design: share one native edge classifier for hover and press, not separate
   Win32 geometry or GL callbacks. Respect modal, popup and capture ownership;
   frontmost floaters occlude lower ones. Keep existing URL cursor callback
   separate and override it only for a claimed resize edge. Correct notification
   construction rather than changing the reference checkbox reshape contract.
   These are CPU input/layout changes; GPU publication and retirement unchanged.

Discriminating checks: pointer down/move/up on all borders and corners; cursor
classification and minimum size; tabs below header and selectable after reshape;
notification checkbox bounds do not cover buttons, and clicks on actual button
centers invoke that button without toggling ignore. Build RelWithDebInfo with
PACKAGE=OFF and copy-stage runtime. Inspect running viewer after rebuild.

Open limits: snapping/docking parity, full Contacts services and full menu/world
coverage remain open. Unit tests/builds alone do not establish visual parity.

## Validation update

RelWithDebInfo widget target completed successfully: 212/212 tests, including
quit confirmation button capture without checkbox mutation, eight edge/corner
drags and cursor classification, and clicks on exposed Conversations/Contacts
tabs after resizing. Native session integration also passed. Evidence:
`build-vc170-64/floater-input-build.log`.

The initial tab test clicked the Contact Sets button center while an overflow
jump arrow covered that location at minimum width. The test now expands the
floater before checking all three exposed tabs. Overflow-arrow interaction and
post-draw tab layout timing remain separate visual-validation items; this test
does not qualify either. Preferences' default declaration is fixed-size.

The operator stopped Computer Use and requested advance notification before
desktop control for GL/native comparison. No subsequent desktop interaction
was performed during this build check. Interactive parity remains unverified.

Viewer build and `copy_w_viewer_manifest` completed with exit 0, PACKAGE=OFF,
RelWithDebInfo. Evidence: `build-vc170-64/floater-input-viewer-build.log`.
The staged executable has not been launched for this verification pass.

## Subsequent interactive validation, 2026-09-20

The operator subsequently authorized desktop control and left input idle.
The native RelWithDebInfo executable (SHA256
`0F579EF5C6204E92467B740AAB7BB51BFDB46D9D95EFB8AF6D88E2830CE595DF`)
was compared with the existing OpenGL baseline at
`1b7498c2172c500e536d2b9c88e42980c860284d`. That runtime baseline is distinct
from the pinned source oracle above. No implementation changed during this pass.

Verified interactively: all four Conversations side borders resized while their
opposite edges stayed fixed; exposed Friends, Contact Sets and Nearby Chat tabs
selected; Preferences Chat and Chat Windows tabs selected. Both quit-dialog
buttons responded at their displayed centers without toggling the ignore box.
Quit displayed logout progress and the native process exited. The native session
log recorded a region connection and successful logout; login progress was not
captured. No chat messages were sent by this test.

The quit dialogs had similar overall placement at matching outer window sizes,
but this was not a pixel-difference qualification with matched settings/devices.
Overall visual parity remains incomplete:

- Reducing the main window's viewport left Preferences partly off-screen,
  including its header.
- Native Chat Windows displayed an unresolved `[SHORT_VIEWER_GENERATION]`
  label; the baseline resolved it to V7.
- Friends and Contact Sets still contain unavailable-service placeholders.
- Actual resize-cursor appearance was obscured by the automation pointer;
  only CPU cursor-classification checks are qualified.
- Runtime corner, minimum-size and overflow-arrow behavior, complete applicable
  menus, and bidirectional chat remain unverified.

Both viewers were subsequently closed. The detailed local evidence is in
`build-vc170-64/floater-input-interactive-verification.md`. This checkpoint is
not approval to merge or a claim of complete UI transposition.

## Visual parity follow-up: pre-edit contract, 2026-09-20

Reference inspection at native branch 61b340fc3d: LLFloaterView::refresh and
adjustToFitScreen, LLFloater::fitWithDependentsOnScreen, LLView's
getNeededTranslation, and LLAppViewer::initStrings. NV-00/01/02/12/17 apply.

1. GL keeps a floater's top below its container top and preserves a minimum
   visible overlap; resizable, non-minimized floaters shrink to the available
   dimensions subject to their minimum sizes. Fixed-size floaters retain size.
   Global generation substitutions come from localized strings and the actual
   major version, before UI labels are consumed.
2. Native code will perform CPU-owned floater constraint preparation before
   paint, using native root bounds and the menu strip. It will publish the
   existing independently formatted generation strings into the native label
   context. No GL visual implementation is reused and no GPU ownership changes.
3. Add a native floater fitting method used by the existing viewer floater list.
   Preserve partial-offscreen dragging and minimized client geometry. Existing
   snapped dependent movement, toolbar exclusions and docking parity remain
   separate obligations; do not claim their closure. Update label context only
   when generation values change, avoiding frame-by-frame re-resolution.

Discriminators: shrink the root under a displaced fixed-size Preferences-sized
floater and require its header to remain reachable without changing size; repeat
preparation without drift; constrain a resizable floater without violating its
minimum size. Verify a generation token in both an existing and newly constructed
native label resolves from version metadata. RelWithDebInfo tests/build and later
interactive comparison are required; these repairs alone do not close UI parity.

First follow-up run: 213/213 widget tests and the RelWithDebInfo viewer build
passed; the localized V7 label was verified on screen. The viewport test exposed
an insufficient contract: minimum overlap alone leaves most of Preferences
off-screen. Further reference inspection of LLFloaterView::reshape,
LLFloater::applyPositioning and LL_COORD_FLOATER::convertFromCommon/convertToCommon
shows viewport changes reapply normalized positioning before the visibility
constraint. Native will retain the last prepared viewport/rectangle and map each
axis into the new available travel range, including the partial-outside ranges.
The discriminator must now require a previously fully visible Preferences
window to remain fully visible after contraction when it fits. This is a
refinement before acceptance, not a claim that the first repair was sufficient.

## Login Mode label correction: pre-edit record, 2026-09-20

The operator reports [VIEWER_GENERATION] still visible in Mode. The shipped
panel_fs_login.xml declares that label with value settings_v3.xml. Reference
LLComboBox construction sends the localized ItemParams label to its list row;
the selected display and underlying value are separate. Native createCombo
currently copies unformatted row labels and setLabelContext updates only
buttons/badges/plain text. The prior short-generation fix did not cover rows.

Native design: retain the source label only for XML-declared combo rows, resolve
it using the independently owned label context at construction and on context
changes, and refresh the selected display without invoking commits or changing
selection/value. Programmatic resident/user labels stay literal. Regression:
inspect the actual Mode row before/after metadata publication, select its
settings_v3.xml value, change the generation, and require updated display with
unchanged value/selection and no callbacks. No GL implementation changes.

Mode-label validation: the RelWithDebInfo widget suite passed 213/213 tests,
including the actual login Mode row, selected label refresh, preserved selection
and settings value, and absence of commit callbacks. Native session integration
passed. Viewer build and copy-only staging completed with exit 0 and PACKAGE=OFF.
Evidence: build-vc170-64/mode-label-tests.log and mode-label-viewer-build.log.
This dropdown correction has not yet been inspected in the running viewer.

## Dropdown follow-up, 2026-09-20

Operator reports popups open but clicking a row does not select it. Read-only
process inspection found the running executable at
C:/Dev/vulkanstorm/build-vc170-64/newview/RelWithDebInfo/vulkanstorm-bin.exe
(12:06:52 build), rather than the native-sl-login executable (18:34:22 build).
This establishes a build mismatch, not the cause of the reported selection
failure. No PC control was taken and no implementation change was inferred.

Extended test 221 to exercise actual login Mode and Start Location through
menu/floater/tree pointer dispatch, separate press/release pairs, and viewer
paint preparation between each event. Both select the second row and close
their popup. The RelWithDebInfo suite passed 213/213, with session integration
also passing; evidence: build-vc170-64/dropdown-regression-tests.log. Runtime
selection and visual parity remain unverified in the corrected branch binary.

Operator clarified that the failure concerns dropdowns inside floaters; login
dropdown selection works. Extended the same frame-separated pointer check to
the actual Preferences language_combobox. The suite passed 213/213 (evidence:
build-vc170-64/floater-dropdown-tests.log). The parent worktree's floaterPointer
lacks the active-popup guard present in native-sl-login commit 61b340fc3d; it can
run floater activation/drag handling before popup dispatch. This source
difference is relevant but is not proof of the reported runtime cause. No claim
of floater dropdown runtime acceptance follows from the passing test.

## Renderer confirmation timing: pre-edit record, 2026-09-20

NV-00/NV-01/NV-03, native-sl-login 61b340fc3d plus current UI edits.
Reference roots: LLPanelPreferenceGraphics::onRenderBackendCommit and
callbackRenderBackendRestart use ChangeRenderBackend, persist on Shutdown now,
and restore the active selection on Later. After checking baseline, the operator
confirmed that the immediate in-viewer notification must remain unchanged;
the proposed Preferences OK timing was withdrawn before implementation. Existing native
graphicsPreferenceAction("Backend") prompts immediately; LLVKWindowMgr::run
also wraps persistence with MessageBoxW, causing duplicate confirmation.

Native design: remove only the extra MessageBoxW confirmation from the window
manager's save wrapper. Retain immediate ChangeRenderBackend notification,
validation, persistence, failure propagation and shutdown ordering. Later still
restores the active selection without saving or quitting. No GPU ownership or GL
implementation changes. Focused verification: inspect the unchanged notification
callback and save-before-quit ordering, require no Change Renderer MessageBoxW
call in the window manager, and compile the affected native window code.
Runtime appearance remains separately open.

Checkpoint validation: removed the three lines constructing/showing the Windows
renderer confirmation. The immediate native notification callback is unchanged.
The operator reports floater dropdowns operational in the branch build. Prior
widget validation passed 213/213 with the actual Preferences language dropdown.
After worktree relocation, CMake regeneration completed successfully with
RelWithDebInfo and PACKAGE=OFF; the viewer rebuild is in progress at checkpoint.
The duplicate-confirmation removal has not yet been exercised interactively.
