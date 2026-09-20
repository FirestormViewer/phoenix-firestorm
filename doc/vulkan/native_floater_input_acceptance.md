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
