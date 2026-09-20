# Native in-world UI reference survey

Date: 2026-09-18. Scope: reference survey and native gap inventory only.
No implementation, build, viewer launch, credentials, profile access or runtime
comparison was performed. No GL source was changed. This report does not grant
implementation credit, close NV-00 transitive audits or establish exact parity.

## Authority and revision roles

- Native task baseline: `005a4ae869` (user-supplied clean starting point), worktree
  `C:/Dev/vulkanstorm/native-sl-login`. Concurrent login/logout and communications
  edits mean the file observations below are a dated working-tree snapshot, not
  assertions about every file at that commit or the agents' finished work.
- Implementation checkpoint: `90af5a7220f1fec28e3c909c051b6ee7e062f10b`.
- GL oracle: `59108e15a1f8f94d2da7c674d937d19f5cf9450d`. Read-only reference:
  `C:/Dev/vulkanstorm/worktrees/notification-gl-reference`. Its worktree HEAD file
  was read and matches that hash. No Git/index cleanliness or blob comparison was
  run; attribution assumes these designated reference files are unmodified.
- All GL observations below come from that reference worktree, not current-branch
  GL source. There is no silent reference upgrade. Native observations use current
  native files; the older handoff's status is historical, not current proof.
- Authority: [invariants](native_vulkan_invariants.md), read in full, especially
  NV-00/01/02/03/10/11/12/13/14/15/16/17/18; [roadmap](native_viewer_roadmap.md),
  [approved report](reverse-engineering/README.md),
  [service handoff](native-services-handoff.md), and
  [native tree rules](../../indra/llvulkan/AGENTS.md).

GL paths/symbols in this report are relative to `indra/` at the pinned oracle.
Native links resolve within this worktree. Source inspection is distinguished
from unresolved outgoing obligations; neither a file nor a widget test count
establishes a production workflow. Exact UI/effects/behavior parity is required
within each accepted slice, not approximate similarity or marker-only evidence.

## Priority findings

1. The reference in-world shell is a layered, policy-driven root, not a chat
   dashboard. Native connected presentation currently constructs generated
   communications columns and hides login subtrees. This is a useful observable
   integration surface, but not the reference menu/status/navigation/toolbars,
   Conversations/Contacts hierarchy or complete in-world viewer.
2. Session progress needs a screen-level owner coordinated with the root's layer
   policy. The user has selected login/logout screens to supersede existing
   lifecycle modals. That does not prohibit source-required error, MFA, TOS,
   critical-message or dependent confirmation dialogs.
3. CPU preparation is part of visual ownership. Native layout, input, text,
   image resolution, floater construction and notification presentation must not
   call, wrap or extract existing GL-exclusive visual functions. Nonvisual
   services are reuse candidates only after callback/lifetime audits.

## C0: Root composition and shared lifecycle boundary

**Question 1: What does GL do?** Local inspection:
`newview/llviewerwindow.cpp`: `LLViewerWindow::initBase`, `initWorldUI`, `draw`;
`newview/skins/default/xui/en/main_view.xml`.
`initBase` loads common GL fonts and constructs the console before the main view
so it does not float above floaters. It establishes distinct world, login,
toolbar, floater, snapshot, popup, hint, progress, menu and tooltip holders.
Missing main/toolbar declarations are fatal initialization errors. Toolbars
start hidden. The selected top/bottom chiclet setting changes their container.
Toolbar and chatbar reshape callbacks change the floater's usable/snap region.
`initWorldUI` has a noninteractive early return, constructs IM/chiclet/status
owners and makes navigation visible subject to its own controls. The old mini
location/top-info path is commented out here, not an active default requirement.

`draw` applies display scale and screenshot zoom/subregion transforms, draws
tool overlays, conditional mouselook overlays/instructions, then the root and
top control. It reads mutable camera/focus/settings state and calls GL-owned
fonts and child drawing; it is not a reusable CPU service. XML order alone is
insufficient to prove effective painter order after runtime reparenting and
top-control drawing. The console, progress, menu and tooltip ordering must be
tested with actual visibility and focus state.

**Outgoing obligations still open:** `LLView::draw` child order/dirty handling,
`LLFloaterView` reshape/snap callbacks, `LLToolBarView` and `UtilityBar` construction,
menu registration, tool/HUD branches and retained `RenderUIBuffer` behavior.
World overlays and snapshots are dependencies to record, not authorized world
scene implementation. This record is local-inspected/edges-open, not closed.

**Question 2: How in Vulkan?** Independently prepare a native root with named
layers, usable rectangles, focus/capture scopes and immutable paint order.
CPU-only work includes visibility, declaration resolution, layout, hit testing,
focus restoration, timers and callback routing. Native glyph/image versions feed
prepared primitives; UI remains outside world exposure/fog/DoF (NV-10/12).
Session snapshots select the login/progress/connected/logout presentation without
entering `LLViewerWindow`, `LLView` or other GL visual owners.

**Question 3: Cleanest design?** Extend the existing independent widget/tree,
menu and floater owners with a coherent shell/lifecycle presentation owner;
do not transplant `initWorldUI` or instantiate every GL floater at connection.
Prefer explicit layers and service snapshots over a second giant generated panel
or a GL-compatible dispatch wrapper. Keep session transition decisions in the
session owner, presentation/focus in the UI owner, GPU version retirement in the
native resource owners. This is a proposed design, not an implemented class.

**Native evidence:** [session presentation](../../indra/llvulkan/llvkdialogs.cpp),
`LLVKViewerUi::refreshSession`, selects `initializeCommunications`, hides
`login_html`/`ui_stack`, clears the password control, restores saved visibility
on return to prelogin, and uses generation-tagged notice actions.
[Communications](../../indra/llvulkan/llvkcommunications.cpp),
`initializeCommunications`, currently creates a full opaque three-column panel
with plain transcripts, combos and composers, not reference XUI topology.
[Paint preparation](../../indra/llvulkan/llvkviewerui.cpp), `preparePaint`,
updates communications and floater state before preparing the widget paint,
then paints menus, repeats modal commands above them and appends tooltips.
This makes ordering a concrete integration risk, not proof of a runtime defect.

**Discriminating checks, planned and not run:** At a fixed viewport/DPI, compare
root paint/hit-test order for chat console + overlapping floater + submenu +
nested dialog + progress + tooltip. Resize each toolbar/chatbar and assert exact
usable/snap rectangles. Transition prelogin -> connecting -> connected -> logout
with a popup already open: check screen ownership, no click-through, stale-action
rejection, source-defined overlay visibility and focus restoration. Reject a
solution that merely hides the old modal but leaves active invisible controls.

## C1-C3: Operational chrome matrix

Each row answers NV-00 for a bounded behavior family. All are
local-inspected/edges-open, not fully transitive-closed. Paths are pinned GL paths.

| ID / source roots | Q1: observable GL contract and outgoing obligations | Q2: native data / CPU and GPU work | Q3: smallest coherent owner and current gap | Discriminating check, not run |
|---|---|---|---|---|
| C1 Menu/status: `newview/llviewerwindow.cpp`, `initWorldUI`; `newview/llstatusbar.cpp`, `postBuild`, `draw`, `refresh`, `updateClockDisplay`, `setVisibleForMouselook`; menu registration remains an outgoing audit | Status sits behind the menu, adopts its background, and reshapes the menu to its rightmost edge; parcel text moves accordingly. `refresh` changes geometry as well as data. Clock updates after >1 s, net-stat range after >0.5 s, FPS text/color after >1 s. FPS color branches on focus/visibility, cap and VSync; media/stream buttons have separate availability and inverted playing toggles. Mouselook visibility combines individual preferences. Status callbacks reach currency, camera/graphics popups, volume/media, parcel and restriction services; these are not all neutral or authorized actions. | CPU status snapshot: corrected time, actual native frame statistics, focus, policy, parcel/name/location and explicitly available service states. Independently format/localize, measure, lay out and hit-test. Paint native skin images, text and exact colors/alpha. Do not report a fabricated balance/media/health value as live. | Extend native menu/panel preparation with an in-world command registry and status owner, not `LLStatusBar::refresh` or the GL registry. Existing native login menu/Preferences mechanisms are candidates to extend; the observed connected panel is not this shell. A read-only status increment must not automatically enable economy, rebake, land or media actions. | Fake clock immediately below/above each refresh boundary; long localized menu and parcel names; resize; focus/VSync/cap combinations; hidden controls must not receive input. Compare exact strings, enabled/check/visibility states, panel edges and colors. Open a status popup over a floater and test pointer exit/re-entry and keyboard dismissal. |
| C2 Location/navigation/favorites: `newview/llnavigationbar.cpp`, `setupPanel`, `onLocationSelection`, `onTeleportFailed`, `onTeleportFinished`; `newview/llfavoritesbar.cpp`, `changed`, `reshape`, `draw`, `updateButtons`, `createButton`, `showDropDownMenu` | Empty text and unchanged unselected text return without action. Selection distinguishes landmark asset, history/global position, typed SLURL, HTTP URL and region name. Named-region replies are asynchronous; OpenSim/hypergrid has separate conditional routes. History is published on completion, not optimistic submission; failure clears the pending-save flag. Favorites observe inventory, request descendants/SLURLs, rebuild on width and data changes, and overflow into menus. `draw` consumes a one-frame drag marker, services dirty rebuilding and delayed (>1 s) favorites persistence. Full overflow/drag/action and location-control transitive bodies remain open. | Versioned location, region epoch, restrictions, typed-history and read-only landmark/favorites records; native location editor, history popup and width/overflow preparation. Audit SLURL parsing, storage and requests independently; never share `LLAgentUI`, `LLLocationInputCtrl`, `LLFavoritesBarCtrl` or GL inventory UI. GPU work is text/images/clips only. | Independent navigation/favorites presentation with tagged requests and a read-only data adapter first. Do not import the GL singleton or its `getVkDrawState`: it still observes GL-owned images/views. Teleport execution and inventory reorder/create/remove are separate dependencies, not silently included by presenting the bar. Display-only coverage is explicitly partial. | Empty/unchanged entry emits zero requests; failed or stale region reply creates no history; successful same-region/variable-region data uses returned position/origin. Width forces exact overflow membership/order. Delayed data after account change cannot populate old favorites. Inventory mutation callbacks must remain unreachable in the read-only slice. |
| C3 Bottom/side toolbars: `newview/lltoolbarview.cpp`, `postBuild`, `loadToolbars`, `saveToolbars`, command and drag callback roots; `newview/llviewerwindow.cpp`, `initBase`; `panel_toolbar_view.xml` | Three toolbar locations (left/right/bottom) wire start/handle/drop and button add/remove callbacks. Root construction uses center-panel reshapes to update floater exclusion/snap areas, and separately tracks chat/legacy utility bars. Top/bottom chiclet placement follows `InternalShowGroupNoticesTopRight`. Toolbar commands require individual callback/predicate audit; declaration presence is not functional integration. `loadToolbars`/save and the `LLToolBar` implementation are indexed obligations, not claimed closed here. | Native command IDs with explicit availability/check/running/flash state, account-owned arrangement and skin icon IDs; independently prepared orientation, wrapping, labels, badges, animation, tooltips and drag hit regions. Native packets contain no `LLCommand`/GL button/view pointers. | Extend native layout/buttons/menu capabilities behind a toolbar-specific native owner, not a generic row of substitute text buttons. Preserve original declaration/default selection and per-account layout once independently audited. Do not instantiate all command destinations to construct the shell. Current native connected columns do not implement these toolbars. | Exact left/right/bottom positions at several widths; icon-only versus labeled modes; empty toolbar; enabled/running/flash feedback; drag cancellation and layout restoration; floater snap bounds follow chatbar expansion. Commands without approved services cannot execute or appear successfully completed. |

### Chrome assets and service prerequisites

Original XUI, layered skin colors, image-name declarations and font descriptions
are neutral input candidates, not permission to reuse their GL consumers. Match
selected skin/theme/language and user overrides, image dimensions, scale rectangles,
UV orientation, tint/alpha and missing-image policy. A flat colored rectangle is
not an equivalent substitute for a skin image. Source registration/predicates must
be audited separately from declarative labels.

Chrome requires an account/session-ready snapshot, independently owned command
availability and correct region/parcel/restriction data. Clock formatting and
generic parsing are candidate nonvisual services only after their callbacks and
initialization are audited. Media/world/land/economy targets encountered in the
source are scope boundaries, not implementation authorization. Unknown state must
be represented explicitly; permanently disabled controls do not close parity.

## C4-C6: Chat, Conversations and Contacts

### C4: Nearby overlay, composer and history

**Q1 / inspected roots:** `newview/fsfloaternearbychat.cpp`, `postBuild`,
`addMessage`, `loadHistory`; `newview/fschathistory.cpp`, `appendMessage`, `clear`,
`handleUnicodeCharHere`, `draw`; `llui/llconsole.cpp`, `draw` and fade declarations.
Registration in `newview/llviewerfloaterreg.cpp` selects `fs_nearby_chat` /
`floater_fs_nearby_chat.xml`, not the commented-out upstream nearby-chat floater.

- Arrival adds to the muted-inclusive history and, unless muted, the ordinary
  history. Optional archive retention is capped at 200 messages. `do_not_log` and
  muted messages return before tab-flash/logging work. Agent/object arrivals may
  flash an invisible hosted tab; system messages do not follow that branch.
- Timestamp/plain-text choices, source/type, display names, restrictions, IM-in-
  nearby settings and anti-spam affect presentation and logging. `loadHistory`
  reconstructs history/source/type/name information and sets `do_not_log`; replay
  must not be indistinguishable from a newly received live event.
- Rich history derives fonts/colors/alpha via GL visual helpers, embeds headers
  and notification widgets, temporarily changes trusted/plain/nearby content
  policy, blocks undo and follows own messages. Remote named arrivals increment
  unread only when not at the end. After text layout, `draw` performs pending
  bottom-follow, then resets unread if at the end. Ordinary typing transfers to
  the composer; Control combinations preserve history selection/copy behavior.
- Console is a distinct surface with `ChatPersistTime`, a 2-second fade interval,
  line budget, wrapping and session support. Its draw body has an empty early
  return and an `FSShowOnscreenConsole` gate only for the global console. With
  session support, pruning is delegated to `update`, not duplicated in draw.
  Console placement must remain behind floaters as C0 records.

**Open edges:** Incoming dispatch to console versus toast/history, session-aware
console `update`, exact background modes and URL segmentation; editor expansion,
chat-channel parsing, mention/emoji/spelling/autoreplace, restriction/mute/anti-spam
and log/name callback closures. Inspect their actual callees before implementing
those behaviors. Existing GL `LLViewerChat`, `LLTextEditor`, `FSChatHistory` and
console preparation are forbidden reuse even if an individual helper is CPU-only.

**Q2:** Native message records need message/session IDs, origin (live/local replay/
server history), sender/source/type, time, muted/trusted policy, group/moderator
style, notification identity and stable text indices. CPU owners separately
perform arrival policy, transcript preparation, layout/selection, bottom-follow
and read acknowledgment; native text/glyph/image packets carry exact spans/clips.
Console needs its own timestamped layout/fade state, not a permanently visible
transcript panel. Logging must be account-scoped and independently audited.

**Q3:** Extend existing native editor/styled-text/scroll primitives with a native
rich-history consumer and console compositor. Prefer explicit prepare-layout-
acknowledge phases over string replacement every frame or calling GL history.
The current communications panel uses plain `text_editor` transcripts with
`parse_urls="false"`; that is not rich-history, console or original composer
parity. Protocol support and diagnostics are separate evidence categories.

**Discriminating checks (planned):** Feed self/remote/system/object/muted messages
at bottom and while scrolled up; replay the same history without relogging or
live flash side effects. Verify exact unread transitions after layout, selection
preservation, clear/repopulate scrollbar behavior, and typed-character versus
Ctrl+C routing. Test wrap-boundary resize, emoji/fallback, links, moderator styles,
multiline text, timestamp settings and inherited alpha. Fake-clock console checks
at fade start/end, zero persist policy and hidden/reopened state must compare both
text/background alpha and message retention. Source-route audit is required before
claiming console/toast mutual-exclusion behavior.

### C5: Conversations container and IM consumers

**Q1 / inspected roots:** `newview/fsfloaterimcontainer.cpp`, `postBuild`,
`initTabs`, `onOpen`, `onClose`, `onIMTabRearrange`; `newview/fsfloaterim.cpp`,
`updateMessages`, `reloadMessages`, `onInputEditorFocusReceived`.
Registrations select `floater_fs_im_container.xml` and `floater_fs_im_session.xml`.
The container intentionally does not call base `postBuild` that would close all
floaters on close. It subscribes to new messages, permits tab rearrangement and
maps non-null session tabs to chiclet positions minus locked tabs. `initTabs`
configures Contacts before Nearby Chat so Nearby becomes active, honoring
`ContactsTornOff` and `ChatHistoryTornOff`. Hosted visibility is persisted by the
host, not as independent child visibility. App-quit closes/saves IM sessions via
a different branch from ordinary container close.

IM `updateMessages` asks the model for messages after its last index and passes
`hasFocus()` to control unread reset; visibility alone is insufficient. It updates
participant information for nonhistory senders and applies group-moderator style.
Existing notification IDs embed an offer widget and may hide a visible toast;
missing IDs skip that record so the log entry can be used. Embedded offers skip
their following log record. Focus reception enables input only if the session can
accept text and the viewer is not disconnected. Send/typing, container add/remove,
model acknowledgment, speakers, notification and chiclet closures remain open.
Voice-state timer/subscriptions are recorded dependencies, not approved voice work.

**Q2:** Native account/session conversations model publishes membership, stable
message indices, drafts, unread, text-capability and notification references.
Native host/floater/tab owners prepare exact labels/icons, locked order, flash,
tear-off/rejoin, focus and bounds. Rendering consumes the prepared state; it must
not advance protocol history or clear unread merely because it is painted.

**Q3:** Build a native multifloater/host relationship atop the existing native
floater/tree instead of three permanent conversation columns. Keep one transcript
model per session and one explicit presentation/acknowledgment policy, with
generation-scoped subscriptions retired on close/disconnect. Existing
`LLVKViewerUi::CommunicationServices` and protocol/context types are candidate
interfaces to review with the communications owner, not proof of these semantics.

**Checks (planned):** Receive while visible-unfocused, active-focused, minimized,
hidden and torn off; assert exact unread and flash behavior. Switch drafts, reorder
tabs, close host versus session, restore hosted visibility and rejoin a torn tab.
For a notification plus its log message, test present/missing/expired ID and toast
visibility to prevent duplicate offers. Disconnect/reconnect rejects stale session
events and cannot send from an old composer. No live send was performed here.

### C6: Contacts / Friends / Groups / Contact Sets

**Q1 / inspected roots:** `newview/fsfloatercontacts.cpp`, constructor/destructor,
`postBuild`, `draw`, `handleKeyHere`, `updateGroupButtons`, `onOpen`, `openTab`;
`newview/fspanelcontactsets.cpp`, constructor/destructor, `postBuild`,
`generateAvatarList`, `changed`, `resetControls`, `updateSets`.
`floater_fs_contacts.xml` declares the resizable, saved-visibility/rect, tear-off
Contacts floater with Friends, Groups and Contact Sets. Child files are
`panel_fs_contacts_friends.xml`, `panel_fs_contacts_groups.xml` and
`panel_fs_contacts_sets.xml`; the last uses injector `contact_sets_panel`.

- Friends list observes tracker/name/settings/contact-set changes; its columns,
  filter, sort, counts, colors and permission indicators are stateful. `draw`
  can restore a suppressed last-column setting, refresh dirty names and request
  sort. The constructor also observes voice; the destructor removes tracker/voice
  observers and disconnects recorded name/contact/restriction callbacks. Complete
  settings-signal ownership still requires audit.
- Ctrl+W on a hosted Contacts closes the host. Filter shortcuts select the active
  Friends/Groups filter under its policy. `openTab` explicitly accepts `friends`,
  `groups`, `contact_sets` and raises the host or standalone floater appropriately.
- Group buttons use current group, membership count and powers (`GP_SESSION_JOIN`,
  `GP_MEMBER_INVITE`); favorite state changes Pin/Unpin labels. Group list display
  is not permission to create/leave/invite/activate groups or purchase anything.
- Contact Sets distinguishes all sets, no-set buddies, pseudonyms, nonfriends and
  ordinary sets. It applies icon/color/list-style policy, filtering/counts and
  online/name sorting. Internal sets are not mutable; action-selection eligibility
  is bounded to 20. Callbacks include configuration, avatar picker, pseudonym,
  profile, IM and teleport; complete nested-dialog paths remain open.

**Q2:** Account-scoped buddy rights/online status, groups/powers, names and local
contact-set definitions feed native list rows with stable IDs, exact text/style,
selection and filtering. Native visual code owns column constraints, tabs,
tooltips, icons, context menus and nested picker/config dialogs. Nonvisual name,
tracker and contact-set storage code requires independent constructor/callback/
teardown audit; the original UI wrappers are not reusable services.

**Q3:** A native Contacts floater hosted by the C5 container, with three independent
tab controllers, is smaller and more faithful than conflating group-chat controls
with a Groups tab. Begin with read-only lists and explicitly agreed IM entry points.
The observed generated native panel has neither this Contacts hierarchy nor its
read-only friend/contact-set consumers. Existing group protocol operations do not
establish Groups-tab parity. Voice and economy stay deferred/out of scope.

**Checks (planned):** Empty/loading/filter-no-match lists, delayed names, online
changes during filtering, exact columns and contact-set colors; no stale-account
rows. Test internal/ordinary sets and 0/1/20/21 selections, group power transitions,
Pin/Unpin text, Contacts tear-off/rehost, and Ctrl+W/focus behavior. Test nested
picker/config cancellation with parent closure and delayed replies. Use synthetic
data; no contact, group, inventory or economy mutations are authorized by this survey.

## C7-C9: Notifications, nested floaters and inventory boundary

| ID / roots | Q1: source result / branches / outgoing edges | Q2: native production | Q3: chosen ownership / observed gap | Discriminating checks, not run |
|---|---|---|---|---|
| C7 Notification graph: `llui/llnotifications.cpp`, `initSingleton`, `createDefaultChannels`, `LLNotification::respond`; `newview/llnotificationalerthandler.cpp`, `LLAlertHandler::processNotification`, `onChange`, `LLViewerAlertHandler::processNotification`; `newview/llnotificationtiphandler.cpp`, `LLTipHandler::processNotification` | Templates and visibility rules precede channel construction. Enabled feeds Expiration/Unexpired; Unexpired feeds Unique -> Ignore -> VisibilityRules -> Visible, plus persistent channel. Failed filters can respond, expire, combine or suppress; uniqueness callback order matters. `respond` sets response state, invokes responder or named functor, unregisters temporary functors, applies ignore/last-response settings, responds to combined notices and updates. Missing responder can return early. Alerts are centered, nonfading, nonstorable and optionally modal; they use the visible progress rectangle when present. A viewer alert can exit mouselook. Tips log according to policy, suppress duplicate visible-IM presentation, branch on friend/contact-set preferences and use explicit expiry or configured lifetime. | Independent notification records: template/name, substitutions/form, stable ID, type/channel, time/expiry, uniqueness, ignore policy and session-bound response token. Separate policy/channel state from native alert/toast/well/embedded-offer presentation. CPU timers, formatting, wrapping, focus, queue ordering and response admission precede native text/image packets. | Extend native error/notice infrastructure with independent channel and response owners, not `LLNotifications`, `LLToastAlertPanel`, GL handler utilities or UI registry reuse. Existing native queued notices/error gates and template support are foundations, not full channels/persistence/toast/well/console coverage. Owner decides recovery; UI cannot invent a Retry operation. | Duplicate/combined IDs, ignored default/last response, expiry before and during display, response plus concurrent removal, parent close and stale-session callback. Assert exactly-once service effects, exact buttons/default/ignore state and queue order. Place alert over visible progress and nested floater; test IM offer/log de-duplication. Compare timer-driven alpha, hover pause, stacking and click-through against the reference once remaining toast edges are closed. |
| C8 Floaters/nesting: `llui/llfloater.cpp`, `closeFloater`, `draw`; `addDependentFloater`, `bringToFront`, `storeVisibilityControl` are indexed follow-up roots | Close unminimizes then checks `canClose`; hosted floaters detach; focused controls release/commit before teardown; focus returns to a live dependee. Dependents close before `onClose`/close signal and hide/destroy policy. Sounds differ for quit, toast and script floater. Draw chooses opaque/transparent image or flat-color background, multiplies current alpha, applies optional shadow/focus title, updates default button, handles minimized children and disables tear-off if its former host is gone. | Native parent/dependent/host IDs, focus/capture stack, visibility persistence, bounds, minimize/dock state, foreground alpha/shadow, default-button and close-policy records. CPU preparation handles effects now hidden in draw; native rendering uses prepared geometry/text/images. Dialog callbacks carry owner generations and cannot reconstruct dead parents. | Extend existing `LLVKFloater`, tree and dialog owners with explicit host/dependent contracts rather than one global active dialog. `LLVKViewerUi::preparePaint` already prepares foreground/active-control sets and related color pickers, but this does not prove general nesting. Avoid replacing every reference floater with the same modal notice template. | Parent + child picker + confirmation, close denied/allowed, parent close while reply pending, minimized/rehosted state, Esc/Enter/Tab/Ctrl+W, mouse capture and focus restoration. Compare background/image/shadow alpha and title/button states at animation times, not only final placement. Verify release commits exactly once before destruction and quit does not trigger ordinary-close sounds/actions. |
| C9 Inventory, read-only survey: `newview/llinventorypanel.cpp`, `modelChanged`, `getRootFolderID`, `onIdle`, `idle`; `newview/llpanelmaininventory.cpp`, `onFilterEdit`, `onFilterTypeSelected`; `llsidepanelinventory.cpp` callback index | Inventory UI waits for model usability and builds views incrementally. `modelChanged` returns before views initialize, on absent model or empty changed IDs; otherwise it updates per stable ID. Roots distinguish library, explicit ID and preferred folder. Idle tracks clipboard generation/filter dirtiness. Search branches for single-folder gallery/combined view, custom/per-tab settings, saves expanded-folder state before filtering, and propagates search to inbox. Custom filter opens/raises a dependent finder. Full tree building, sorting, finder, gallery, inbox and bridge action bodies remain open. | Account/version-tagged inventory/library snapshots with category/item IDs, parent, name/type, permission metadata, completeness and fetch errors. Independently prepare tree disclosure/selection/filter highlights, labels, icons/link overlays and scroll state. Audited read-only acquisition/cache/name services may feed it; GL `LLInventoryPanel`, folder views, bridges and their CPU layout cannot. | Native folder/tree consumer plus read-only adapter is the first coherent slice; do not use [legacy folder passes](../../indra/llvulkan/llvkuifolder.h), whose API accepts `LLView*` and reads GL-owner state. This existing file is migration debt, not current operational inventory or permitted reuse. Generic widget support/texture preview does not establish inventory acquisition or UI. | Loading versus empty/denied/partial/error are distinct. Expand/filter during delayed page arrival, preserve stable selection and folder-state restoration, handle renamed/moved data and account change. Exact icon/label/highlight/clip/scroll comparison. Assert zero rename/delete/move/copy/give/wear/upload/purchase requests; context menu and double-click routes must not silently invoke those services. |

### Explicit notification and floater follow-up obligations

The notification source index also identifies `LLOfferHandler`, `LLGroupHandler`,
`LLScriptHandler`, `LLHintHandler`, `LLScreenChannel::addToast`,
`redrawToasts`/`showToastsBottom`/`showToastsCentre`/`showToastsTop`,
`onToastFade`, and `LLToast` timer/draw handling. These bodies and the persistent
channel, notification well, chiclets and Notifications Console are not fully
traced by this bounded survey. Do not reduce their contracts to the alert row.
An offer can lead to inventory or transaction work; index it without enabling it.
Script dialogs, group notices, permissions and actionable offers need their own
agreed producer/response classes and nested-dialog tests.

Neither current GL notifications nor `LLNotifications` are approved nonvisual
dependencies as-is: the inspected response path reaches `LLUI` settings, and
construction/registration/template paths carry UI dependencies. Reusing a pure
LLSD/XML parser is a separate, independently audited choice. Persist data, never
live callbacks; restored notifications must revalidate response ownership.

Inventory scope is browsing/inspection planning only. Appearance/outfit/gallery
rendering, texture/mesh/avatar previews, marketplace/inbox actions, sharing,
permissions editing and all mutations are separate unclosed surfaces. Their
presence in source or XUI does not expand this task. Likewise maps/radar,
inspectors/profiles, object/build tools, snapshots and environment/camera controls
are downstream UI families to inventory, not completed or authorized here.

## C10: Shared login, progress and logout presentation

**Q1 / source contract:** `newview/fspanellogin.cpp`, `onClickConnect`,
`show`/`closePanel` (latter two indexed); `newview/llstartup.cpp`, `login_callback`
and startup progress call sites; `newview/llprogressview.cpp`, `postBuild`,
`revealIntroPanel`, `setStartupComplete`, `setVisible`, `fade`, `drawStartTexture`,
`draw`, `setMessage`, `initStartTexture`, `initLogos`, `releaseTextures`,
`onCancelButtonClicked`, `handleUpdate`, `onAlertModal`, `handleMediaEvent`, `onIdle`;
`newview/llviewerwindow.cpp`, `setShowProgress`; `llui/llprogressbar.cpp`,
`draw`/`setValue`; `newview/llappviewer.cpp`, `userQuit`, `requestQuit`,
`finish_quit`, `idleShutdown`, `sendLogoutRequest`; `newview/llviewermessage.cpp`,
`process_logout_reply`.

Login commits fields by releasing focus, gates required-update builds, validates
grid and required fields/credential format, then invokes callback option 0 to
enter `STATE_LOGIN_CLEANUP`. Missing/invalid input has specific notifications.
Credential handling itself is outside this survey; no protected data was accessed.
The callback's quit option is source-commented as apparently unreachable; do not
claim it is the active UI quit path. `userQuit` instead requests quit directly
when disconnected/no progress owner/progress visible, otherwise uses `ConfirmQuit`.

Progress uses `panel_progress.xml` and `panel_progress_mini.xml`. Fullscreen choice
is controlled at startup/shutdown by `FSDisableLoginScreens` /
`FSDisableLogoutScreens`; `setShowProgress` selects fade-in full view or visible
mini view, and on hide fades the full view and hides mini. The full view has
`Rounded_Square`, `LoginProgressBoxCenterColor`, `LoginProgressBoxTextColor`,
title/status/MOTD, progress, logos, cancel and optional intro browser. Message
measurement expands and later restores both content/MOTD panels.

- Fade-in/out uses `FADE_TO_WORLD_TIME = 1.0f`; preparation must preserve the
  >duration completion boundary and inherited alpha on children/logos/background.
  Fade-in idle completion closes the login panel. Fade-out completion releases
  focus, hides progress/media, detaches the observer, unloads media and releases
  textures. These are lifecycle effects, not Vulkan recording work.
- Start image selects last/home account image with a production-grid legacy BMP
  fallback; `UseStartScreen`, missing/decode failure and black fallback are explicit
  branches. Original image aspect uses centered cover-style cropping. Native must
  independently load/decode the source image, never sample GL `gStartTexture` or
  use GL `getVkDrawState` bridges. Native snapshot production remains separate.
- Logos use source PNGs under `skins/default/textures/3p_icons`, label-derived
  placement and compile conditions (`LL_FMODSTUDIO`, `LL_HAVOK`; Vivox logo is
  unconditionally loaded by this source). Branding-image parity is not permission
  to implement voice or load its services. Missing logos log/return; they are not
  interchangeable generic icons. `drawLogos` clip/offset details remain open.
- Progress clamps 0..100 and rounds fill width. Track alpha is replaced by draw
  alpha; fill alpha is multiplied by draw alpha and `0.75 + 0.25*sin(3*time)`.
  Optional track/fill images are independent. Reusing accessor colors alone would
  miss these semantics. Record a controlled timer origin for temporal comparison.
- Intro requires URL + Javascript enabled + not previously viewed; it hides the
  progress stack and focuses media. Close-request behavior depends on startup
  completion. Progress swallows keys except Ctrl+Q; hover outside children selects
  wait cursor. Cancel before `STATE_STARTED` requests quit; after it cancels
  teleport, disables the button and hides the view. Native must not map every
  operation to generic cancel-to-login.
- `onAlertModal` only auto-responds with default to newly added
  `WebLaunchExternalTarget` while progress is visible. It is not permission to
  auto-accept all errors, agreements or confirmations. This specific policy needs
  closure with the external-browser handler before implementation.

Logout first waits for modal resolution, disconnects IM, closes/waits for floaters,
saves teleport/location history and the final snapshot. For the first 5 seconds,
pending asset/metrics work selects SavingSettings/LoggingOut progress. Then it
sends one logout request and waits for reply or the source 6-second default
deadline. The partial-startup/no-region branch of `requestQuit` differs and may
send a best-effort logout before requesting process quit. These source internal
`forceQuit` calls are not authorization to terminate a viewer externally.
World snapshots, uploads, HUD quit effects and voice shutdown are recorded
dependencies, not added implementation scope.

**Reference caveat:** `process_logout_reply` warns on agent/session mismatch but
does not return before inventory bookkeeping and quit. Do not describe native
generation/identity rejection as exact reproduction of that branch. Preserve the
native owner safety contract and record/review the intentional correction under
NV-18, without changing the oracle. Complete transport trust and shutdown closure
belongs to the lifecycle agent.

### Agreements and errors remain distinct

`newview/llfloatertos.cpp`: `postBuild`, `updateAgreeEnabled`, `updateAgree`,
`handleMediaEvent`, `testSiteIsAliveCoro`, `setSiteIsAlive`, `onContinue`, `onCancel`.
The text-only critical path and browser TOS path differ; OpenSim and `EXTERNAL_TOS`
are separate configuration branches. Browser agreement waits for loading-page
completion, a coroutine availability result and real navigation; unavailable-site
policy can enable agreement. A dead floater handle rejects the late probe result.
Continue is gated by the checkbox, posts true to the reply pump and closes;
Cancel emits `MustAgreeToLogIn` with `login_alert_done`, posts false and resets
browser state before closing. Reply-pump consumers, full `login_alert_done`,
critical XML and external-browser policy remain open obligations.

The user-selected login/logout screens supersede the existing generic lifecycle
progress modals. They do not ban TOS, critical, MFA, error, ConfirmQuit, unsaved-work
or cleanup-recovery dialogs. A screen-only implementation that drops required
responses is incomplete. Conversely, repeatedly displaying generic progress
notices does not satisfy the requested original screen presentation.

**Q2:** Publish tagged operation kind/phase, progress, message, allowed action,
agreement/challenge identity, readiness and cleanup state from the session owner.
Native CPU presentation independently resolves original declarations, text metrics,
screen/mini policy, time-based opacity, focus/capture and media events. Native image
versions preserve source dimensions/UVs and remain alive through every submission.
Queue error/agreement presentation in the appropriate explicit layer, not through
GL notification/floater objects. Never fabricate progress percentages or readiness.

**Q3:** One native lifecycle presentation owner shared by login/connected shell/
logout is cleaner than repeated generated modals or separate unrelated screens.
It observes the existing session owner; it does not own network teardown. Keep
the window/error presenter alive while cleanup is pending/failed, cancel stale
callbacks and retire browser/image resources by completed use. Keep screen policy
separate from the service's Cancel/Retry/Quit capabilities. Full and mini reference
settings remain obligations unless explicitly changed, not silently discarded.

**Observed native gap:** The earlier-read `refreshSession` used
`NativeSessionProgress`/`NativeSessionAgreement` notices; another agent is replacing
lifecycle presentation concurrently. This is not a final review of its edits.
Startup does bind `prepareLogin` and communications context/receive/search/group/
local/direct callbacks to `LLVKLoginTransport`, and the window installs the session
owner and refreshes session state. Thus those bindings are not absent as the old
handoff says; full live readiness, screen parity and complete service closure are
not established by those reads.

**Checks, planned:** Fake-time 0/0.5/1.0/>1.0 fade and progress-pulse comparisons;
missing/disabled/portrait/landscape startup image; long localized MOTD expanding
and shrinking; full/mini login/logout settings. Test quit versus teleport cancel,
intro close before/after completion, error/confirmation over progress and TOS
late reply after cancellation. Test unsaved dependent dialog blocking logout,
cleanup failure with usable Retry Cleanup, partial startup, repeated logout and
wrong-session reply. Do not require unrelated world uploads/snapshots to be
implemented simply to test a controlled zero-work shutdown state.

## Native ownership and asset inventory

These are observed foundations or candidate owners, not implementation approvals.

| Concern | Local native anchor | Required gap/closure |
|---|---|---|
| Production session/UI boundary | [startup](../../indra/llvulkan/llvkstartup.cpp), [window](../../indra/llvulkan/llvkwindowmgr.cpp), [UI interface](../../indra/llvulkan/llvkviewerui.h) | Use live tagged snapshots/actions; distinguish authenticated, region-connected, services-ready and world-visible. No inferred `STATE_STARTED` from a panel. |
| Menu/status/shell | [UI preparation](../../indra/llvulkan/llvkviewerui.cpp), [menu](../../indra/llvulkan/llvkmenu.h) | In-world menu declaration and per-action predicates, status/nav/toolbars, shell layers, callback ownership. GL `init_menus` constructs distinct `menu_viewer.xml` and `menu_login.xml`; a login menu is not their union. |
| Chat/Contacts | [communications](../../indra/llvulkan/llvkcommunications.cpp), [chat protocol](../../indra/llvulkan/llvkchatprotocol.h) | Original console/history/Conversations/Contacts topology, rich data/scroll semantics, buddy/contact-set services and session UI contracts; concurrent agent owns implementation. |
| Notifications/nesting | [dialogs](../../indra/llvulkan/llvkdialogs.cpp), [floater](../../indra/llvulkan/llvkfloater.h) | Channel graph, typed producer classes, expiry/persistence/well/toasts, host/dependent lifecycle and exact nested parity. Existing notice queue alone is insufficient. |
| Layout/input/text/assets | [tree](../../indra/llvulkan/llvkwidgettree.h), [factory](../../indra/llvulkan/llvkwidgetfactory.h), [font registry](../../indra/llvulkan/llvkfontregistry.h), [skin images](../../indra/llvulkan/llvkskinimages.h) | Extend independent native behavior for selected XUI classes; factory recognition is not specialized callback/behavior support. Do not restore GL-backed legacy text/folder/view bridges. |
| GPU publication | [packet](../../indra/llvulkan/llvkuipacket.h), [widget GPU](../../indra/llvulkan/llvkwidgetgpu.h), [image publication](../../indra/llvulkan/llvkimagepublication.h) | Packet interface supports clipped images/solids/triangles/text/browser images with retained image pointers; interface inspection does not prove uploads/barriers/all-use retirement. Validate new consumers under NV-13/14/15/16. |

Neutral declaration/asset candidates include `main_view.xml`, `menu_viewer.xml`,
`menu_login.xml`, `panel_toolbar_view.xml`, `panel_navigation_bar.xml`,
`favorites_bar_button.xml`, the C4-C6 floater/panel files and progress declarations.
Skin/widget defaults, localized strings, image catalog scale/clip regions, font
fallback and setting values must travel with the fixture. Names are not pixel
resources: independently decode source assets, preserve color space and alpha,
apply native glyph fallback/metrics, and publish resource-ready versions before use.
Avatar/group icons and inventory thumbnails require permitted asset/name services;
do not replace unavailable content with fake identities and call it parity.

Menu callback inspection also found `LLTogglePanelPeopleTab::handleEvent` in
`newview/llviewermenu.cpp`: `FSUseV2Friends` with non-Vintage skin routes to People;
otherwise Friends/Groups/Contact Sets route to Contacts, with active torn-off tabs
toggling closed. Standalone blocklist policy has its own branch. The requested
Contacts topology must not erase these alternate-state obligations. People/V2,
all skins/locales and unrestricted menu coverage remain unqualified.

## Recommended small vertical slices

| Priority | Bounded delivery | Prerequisites and exit discriminator |
|---|---|---|
| 1 | Lifecycle screens + shell layer/focus contract | Coordinate with login/logout agent; close C0/C10 outgoing screen callbacks. Real tagged phase actions, full/mini settings, exact fades/layout and error/TOS coexistence; no world-renderer expansion. |
| 2 | In-world menu/status shell, then toolbar placement | Close each admitted command predicate and C1/C3 layout callbacks; real session/clock/status data, original assets, exact enabled/checked/hidden state and usable rectangles. Unavailable destinations remain declared gaps, not completed disabled features. |
| 3 | Nearby history and console as separate consumers | Coordinate with communications agent; close arrival/replay/mute/logging routes. Existing transport bindings feed original UI; exact scroll/unread/fade and text interaction plus disconnect cleanup. |
| 4 | Conversations + Contacts read-only tabs | Native host/tear-off capability and audited buddy/group/name/contact-set snapshots; exact topology, filters, selection, permissions, draft/unread state and nested safe cancellation. No inferred voice/economy support. |
| 5 | One agreed notification producer class end to end | Channel policy + real service event + native presentation + tagged response + expiry/ignore/cleanup. Expand from alerts/tips to other classes individually; an embedded offer does not authorize its transaction. |
| 6 | Read-only inventory tree and favorites | Audited inventory/library/landmark acquisition; distinguish incomplete/error/empty, preserve selection/filter/open state and exact visuals. Mutations, purchases, uploads, appearance and world placement stay excluded. |

Each slice follows source contract -> independent native owner -> actual service
binding -> focused deterministic checks -> runtime workflow -> exact visual/effects/
interaction acceptance. A synthetic fixture closes a mechanism, not a production
workflow. No new test files are prescribed: extend the appropriate existing
[widget](../../indra/llvulkan/tests/llvkwidgettree_test.cpp),
[window](../../indra/llvulkan/tests/llvkwindowmgr_test.cpp),
[session-owner](../../indra/llvulkan/tests/llvksessionowner_test.cpp),
[login protocol](../../indra/llvulkan/tests/llvkloginprotocol_test.cpp),
[browser](../../indra/llvulkan/tests/llvkbrowser_test.cpp) or
[GPU context](../../indra/llvulkan/tests/llvkcontext_test.cpp) checks after inspecting
their helpers. This survey did not run or claim those tests.

## Acceptance and unsupported states

Record oracle revision, source cleanliness, build feature macros, skin/theme and
locale, font files/fallback, original image inputs, settings, viewport/DPI, time
origin, interaction script, selected device/driver and renderer formats. Compare
geometry, exact text and IDs/order, colors/alpha/images, clipping, fonts/icons,
focus/capture, nested dialogs and temporal effects. A final screenshot cannot
establish input, lifecycle, animation or resource safety. No tolerance is introduced
or relaxed here; exact UI acceptance remains mandatory. Any separate numeric
renderer tolerance requires prior reference-derived approval under NV-17.

Check packets/layout and input transitions before GPU work; then verify actual
native glyph/image upload, format/encoding, blend and scissor, noncoherent flushes,
descriptor reuse, all-submission retirement, resize/WSI/device failure and shader
ABI. Never sample a GL-produced frame or share its visual owners. Ordinary UI is
outside world postprocessing. Snapshot/retained-buffer and auxiliary-view policies
need separate qualification; they cannot be inferred from the main window.

Unqualified states include People/V2 and legacy-skin variants, OpenSim/hypergrid,
EXTERNAL_TOS and all platform/build alternatives, complete rich-text/context-menu
and editor/IME behavior, all notification classes/wells/console, all toolbar/menu
destinations, live read-only inventory/favorites, full nested/tear-off persistence,
world/HUD/mouselook overlays, snapshots/previews and voice. These are explicit
coverage boundaries, not assertions that every such feature is absent in native
code. Do not hide or disable them and count the underlying milestone closed.

Future full-viewer acceptance is separately authorized work: notify the operator,
allow manual login, wait for genuine `STATE_STARTED`, allow 75 uninterrupted
seconds, close through WM_CLOSE, verify exit zero and `Goodbye!`; never force-stop.
Preserve already passed operator evidence unless a concrete relevant change
invalidates it. None of that workflow was executed for this documentation task.

## Documentation review record

| Field | Record |
|---|---|
| Contract | NV-00/01/02/03/10-18; bounded operational UI survey, not code migration |
| Reference | Pinned GL worktree HEAD verified by file read; user-supplied native baseline and live concurrent native observations distinguished; cleanliness/blob identity not independently verified |
| Data flow | Proposed native snapshot/preparation/packet boundaries only; no ABI or producer/consumer edits |
| GPU safety | Requirements recorded; N/A to executable changes because only this document is edited; GPU proof remains open |
| Validation | Focused editor diagnostics reported no errors. File discovery/read checks confirmed the local link targets. Targeted document searches found no trailing whitespace, conflict markers, TODO/TBD placeholders or machine-local Markdown link targets. Source anchors were checked against the designated reference files; no build, terminal, runtime, capture or parity evidence generated |
| Limits | Per-family outgoing obligations and unsupported configurations above; source-body reads are not exhaustive transitive closure |
| Change class | Documentation/reference investigation; no GL baseline/tolerance amendment and no implementation completion claim |

## Evidence limits

All checks in this report are proposed acceptance tests unless explicitly marked
as documentation checks. No screenshots, captures, timing measurements, Vulkan
validation-layer results or live login/logout/communications results were produced.
The reference revision and source contracts are not evidence of current native
completion. Inventory mutations, economy, purchases/uploads, world scene work and
voice are outside this survey's implementation scope; voice is deferred.

## 2026-09-19 current-source refresh

This additive refresh preserves the September 18 survey above as historical
evidence. Its statements about "current" native code describe that earlier read,
not the September 19 working snapshot. This refresh surveys current GL decisions
and proposes independent native ownership; it does not upgrade the parity oracle,
authorize implementation, or establish runtime/visual parity. Deep top-menu
registration and command auditing belong to another agent and are excluded here.

### Revision and evidence boundaries

- Current-source GL reference: read-only `C:/Dev/vulkanstorm`, `master` at
  `1b7498c2172c500e536d2b9c88e42980c860284d`; tracked-file status is clean.
- Historical parity oracle remains
  `59108e15a1f8f94d2da7c674d937d19f5cf9450d`, with a clean tracked-file status in
  `worktrees/notification-gl-reference`. Current-source findings are discovery
  evidence, not permission to replace pinned expectations (NV-02/18).
- Implementation lineage remains checkpoint
  `90af5a7220f1fec28e3c909c051b6ee7e062f10b`; native HEAD is separately identified
  as `005a4ae8693cf9711093a3c44783bff7af605f99`, not the working files.
  Reverted experiments carry no implementation credit.
- Native working snapshot: the five reported modified files are
  [communications](../../indra/llvulkan/llvkcommunications.cpp),
  [dialogs](../../indra/llvulkan/llvkdialogs.cpp),
  [UI implementation](../../indra/llvulkan/llvkviewerui.cpp),
  [UI interface](../../indra/llvulkan/llvkviewerui.h), and
  [window input](../../indra/llvulkan/llvkwindowmgr.cpp).
  They are concurrent, uncommitted drafts, not reviewed implementation or parity
  evidence. This survey is also untracked at the start of this refresh.
- Required instructions, the full invariants, roadmap, native tree rules and
  approved report README were read. Only this survey is edited. No C++/GL edits,
  other document or memory edits, Git mutations, builds, launches, commits or
  subagents are part of this work.

The local hypothesis guiding this refresh is that declarations and existing
native widgets do not by themselves implement the GL shell's state-dependent
composition. The discriminating static check is to follow the current source's
visibility/layout/input/callback decisions and compare their native owners and
draft diff, retaining every unclosed outgoing edge. Runtime falsification needs
the exact interaction/temporal checks specified below; none ran in this survey.

### Verified current-versus-pinned deltas

`git diff 59108e15a1 1b7498c217 -- <scoped paths>` compared 30 files:
`llviewerwindow`, `llstatusbar`, `llnavigationbar`, `llfavoritesbar`,
`lltoolbarview`, `utilitybar`, `llchiclet`, `llchicletbar`, `fsfloaternearbychat`,
`fschathistory`, `fsfloaterimcontainer`, `fsfloaterim`, `fsfloatercontacts`,
`fspanelcontactsets`, `llscreenchannel`, `lltoast`, `llnotificationalerthandler`,
`llnotificationtiphandler`, `llfloaterpreference`, `llfloatercamera`,
`llfloaterenvironmentadjust`, `llfloatersnapshot`, `llsnapshotlivepreview`,
`llprogressview` (all `indra/newview/*.cpp`); `llnotifications`, `llfloater`,
`llview`, `lltooltip`, `llconsole` (`indra/llui/*.cpp`); and
`indra/newview/skins/default/xui/en/main_view.xml`. Only two differ. Equality here
is file-content equality, not proof that external callees/assets/configuration
or their runtime results are unchanged.

- Current `LLConsole::addConsoleLine`, `Paragraph::makeParagraphColorSegments`,
  `updateLines`, `onUrlLabelCallback`, `update`, `clear`, `removeExtraLines` and
  `draw` add Markdown offsets/emote state, literal URL/label ranges, per-character
  emphasis, styled font measurement/wrapping and segment fonts. Queue trimming
  also removes session and Markdown metadata; empty transformed paragraphs get an
  empty line. Re-resolved URL labels rebuild styles and wrapping. This is a real
  current-source text/geometry delta, not a new requirement silently added to C4.
  `LLMarkdown`, URL registry callbacks and producers of the new arguments remain
  transitive audit obligations; the GL font registry is forbidden native reuse.
- Current `LLFloaterPreference::postBuild` hides `FSMusicSpatialSound` and its
  label on non-Windows builds. `FSPanelPreferenceSounds::postBuild` admits the
  output-device path for `LL_SOLOUD` as well as `LL_FMODSTUDIO`. This does not
  establish native audio or authorize a fake device list. Audio callbacks and
  associated declarations are separate current-source dependencies.

### R0-R3: Shell and operational chrome contracts

The following records supplement C0-C3, using GL `1b7498c217`. Status is
**local bodies inspected, outgoing edges open, design proposed**. Q2 is native
result production; Q3 chooses ownership rather than a GL-shaped wrapper.

**R0 / root composition. Q1:** `LLViewerWindow::initBase`, `initWorldUI`, `draw`,
`updateUI` and `main_view.xml` distinguish console, world/login holders, toolbar,
floater, snapshot, popup, hint, progress, menu and tooltip surfaces. Console is
inserted before the main panel; status is inserted behind menu content. Toolbar
center/chatbar/legacy utility reshape callbacks mutate floater exclusion and snap
rectangles. Initialization has a noninteractive early return and post-login
toolbar reset/load; world-map/build preconstruction and memory-gated CEF preload
are dependencies, not prerequisites to copy into native UI. `draw` applies DPI
and snapshot tiling, calls tool/mouselook overlays, draws the root, then explicitly
draws the visible top control. XML order alone cannot specify this composition.
`updateUI` computes hover membership through popup/captor/top-control/opaque-view
rules, dispatches enter/leave callbacks, then routes hover and tooltip requests.
**Q2:** CPU-owned layer IDs, visibility, usable rectangles, capture/focus and view
purpose produce ordered clipped UI packets outside world postprocessing.
**Q3:** Extend native tree/floater preparation with one shell composition policy;
do not use a full-screen communications panel as the shell or call GL roots.
Open: retained UI buffer invalidation, tool/HUD/mouselook and snapshot transforms,
popup reparenting and all downstream close/reshape callbacks. Cheap discriminator:
assert paint and hit order plus usable rectangles with two overlapping floaters,
top control, tooltip, progress, and an expanding chatbar at two DPI scales.

**R1 / status and location. Q1:** `LLStatusBar::refresh`, `updateClockDisplay`,
`setVisibleForMouselook` read clock/stat timers, focus, cap/VSync, media state,
restrictions and parcel state. Refresh reshapes menu content and parcel placement;
balance refresh can send a request and rebake state flashes on a 0.5-second timer.
These are not pure paint functions. `LLNavigationBar::onLocationSelection`
distinguishes empty/unchanged, landmark, history/global position, location URL,
web URL and named-region inputs. It can invoke `LLWeb`, inventory lookup, agent
teleport or asynchronous region resolution. Failure clears pending history;
completion uses returned global position and region origin, not stale agent-local
position. **Q2:** CPU status/location records carry account/region generation,
corrected time, measured native statistics, availability and localized strings;
native preparation owns metrics, geometry and input. **Q3:** A status/location
controller consumes audited snapshots; start with clock/known region display and
one approved action, not a copied `refresh`. Open: parcel icon/permission producers,
location editor completion, region-reply lifetime, browser dispatch, money/media/
rebake services. Check >0.5/>1-second boundaries, long locale strings, mouselook,
cap/VSync/focus states and stale teleport replies; no optimistic history entry.

**R2 / favorites. Q1:** `LLFavoritesBarCtrl::draw`, `updateButtons` consume dirty
inventory and delayed callback state, preserve unchanged leading buttons, rebuild
the changed suffix, calculate width/overflow and save order after >1 second.
The drag marker is consumed for one frame. Button params come from layered
`favorites_bar_button.xml`; declarations do not supply inventory completeness or
names. **Q2:** CPU versioned favorite IDs/order/names/readiness drive independently
measured native buttons and overflow; published skin images supply marker/chevron.
**Q3:** A native favorites presenter plus audited read-only inventory adapter is
the first boundary; reorder/persistence/teleport are separately admitted actions.
Open: collect/create/overflow callbacks, landmark assets, observer teardown and
drag mutation closure. Check exact overflow membership at width boundaries,
renames during loading and late callbacks after account change, with zero writes
in the initial read-only slice.

**R3 / toolbar, utility and chiclets. Q1:** In addition to C3's toolbar roots,
`UtilityBar::init` stops its 0.5-second timer when the skin lacks the legacy stack;
`tick` refuses media initialization before `STATE_STARTED`. It derives enabled
state and play/pause images from media/stream state and microphone permissions;
press/release callbacks go to `LLAgent`. `LLChicletBar` registers its IM observer
before building children, creates type-specific chiclets, remaps session IDs and
closes an IM floater on removal. `postBuild` selects legacy/new notification well
and subscribes to chiclet visibility. `LLChicletPanel::onMessageCountChanged`
zeros counters only for visible, focused IM; `postBuild` reaches model, script,
voice and transient-floater callbacks; size changes rearrange scrolling children.
**Q2:** CPU command availability, session/well IDs, counters, placement, timers
and explicit subscription lifetimes feed native icon/button packets.
**Q3:** Separate native toolbar layout and chiclet presentation owners sharing
audited session records, not GL toolbar/chiclet objects. Open: toolbar load/save,
drag/flash effects, held-scroll timing, well contents and voice/script callback
teardown. Check top/bottom settings, width/scroll boundaries, selected/unfocused
IM counters, session remap/removal and timer gating. Voice/media controls remain
unsupported dependencies until real service contracts are admitted; disabling
them is containment, not parity. Deep top-menu callbacks remain another audit.

### R4-R7: Communications, notifications and ownership

**R4 / chat, Conversations, Contacts. Q1:** C4's nearby/history contracts remain
historical body evidence, corroborated by unchanged scoped source blobs; the
console delta above is separate. Current `FSFloaterIM::updateMessages` reads from
`mLastMessageIndex + 1` and passes `hasFocus()` to model acknowledgment, applies
history/moderator styles, and hides an existing offer toast when embedding it;
missing notification IDs fall through to the next log record. Its input-focus
handler also checks text capability and disconnect state.
`FSFloaterIMContainer::initTabs` restores Contacts before Nearby Chat, including
last-host and tear-off state; `postBuild` deliberately avoids base close-all
registration. Tab rearrangement updates chiclet index minus locked tabs; normal
host close differs from quit. `FSFloaterContacts::postBuild/draw/handleKeyHere`
own tracker/name/settings subscriptions, context/double-click/default actions,
column recovery, dirty-name sorting, filter focus and hosted Ctrl+W. Group action
availability uses membership/powers, and Pin/Unpin labels are dynamic. Contact-set
filter/picker details retain C6's open edges, not a new transitive-closure claim.
**Q2:** CPU message provenance, stable indices, drafts, read acknowledgment,
buddy/group/name records and subscription generations feed native rich spans,
scroll state, lists and hosted floater packets. Console is a distinct timed
consumer. **Q3:** Extend native communications with a session model separated
from a native host and transcript presenter; reuse native primitives, not the
GL model's visual callbacks or the generated full-root panel. Open: incoming
console/toast routing, logging/mute/restriction chains, names, all Contacts action
targets, model callback teardown, rich links/emoji and current Markdown producers.
Check receive/replay at bottom/scrolled, focused/visible-unfocused/minimized,
tab switch, tear-off/rehost and disconnect, preserving selection and exact unread,
flash, styles and console fades. A tab label does not implement a Contacts list.

**R5 / notification channels and toasts. Q1:** C7's template/filter/respond graph
is unchanged in the compared files. Newly read `LLScreenChannel::addToast` gates
display by startup/show/force policy, otherwise stores eligible notices or takes
the cancel/dispose path (with a distinct DND early return). It registers fade,
destruction and hover timer callbacks; storable displayed notices enter storage
immediately. `redrawToasts` attaches to the snap region for resize and selects
top/centre/bottom layout. `showToastsBottom` iterates a copy in reverse arrival
order, accounts for docked floaters/tongue and `ToastGap`, ensures at least one
toast, hides overflow and sends nonfocused toasts backward unless
`FSShowToastsInFront`. `onToastFade` stores or deletes then rearranges.
`LLToast::expire/setFading/hide/setVisible/draw` implement lifetime then fading
phases, hidden-state nonresurrection, focus/transparency policy and shadow/close
button redraw. Do not assume "fade" means a linear alpha ramp; exact
`updateTransparency`, timer stop/restart and top-stack tails remain open.
**Q2:** CPU notification/channel IDs, admission/storage/expiry/response state and
hover clocks produce native stack geometry and alpha; response tokens are
generation-bound. **Q3:** Extend native notice policy with one independently owned
channel and an agreed producer, rather than converting every toast to a modal.
Open: well persistence, script/group/offer/hint handlers, DND disposal ownership,
channel reentrancy and producer cancellation. Check duplicate/expired/responded
IDs, hover/unhover during expiry, overflow after resize, front/back settings and
embedded-offer deduplication; response executes once and never against a dead owner.

**R6 / floaters and dependent modals. Q1:** `LLFloater::closeFloater` unminimizes,
checks `canClose`, detaches a host, releases/commits focus before dependent close,
restores a live dependee, invokes close callbacks, then hides/destroys according
to reuse policy. `addDependentFloater` sets both relationship and neighboring
placement/snap target. `LLFloaterView::bringToFront` raises sibling/parent/child
groups and handles hosted/minimized/focus-refusal branches;
`highlightFocusedFloater` treats parent and dependents as a foreground group.
`LLView::drawChildren` paints reverse child-list order, gated by visible/valid
rectangles and root/dirty-rectangle overlap: overlap culling is not itself a
nested scissor guarantee. **Q2:** CPU host/dependent graph, close admission,
commit/focus order, saved visibility, usable bounds and foreground alpha produce
native draw order and clips. **Q3:** Extend `LLVKFloater` and tree ownership, using
real relationships instead of more one-off child-close callbacks. Open: complete
multifloater/tab mechanics, modal focus stack, clipping controls and close-veto
callbacks. Check parent + picker + confirmation with edits pending, parent close,
close veto, resize, minimize/rehost and dead delayed reply; commit once, restore
only live eligible focus, and compare shadows/background alpha throughout.

**R7 / hover, tooltips and context layering. Q1:** `LLViewerWindow::updateUI`
routes capture before top control, root and world tool; popup hover membership
and opaque views affect enter/leave. `LLToolTipMgr::show/createToolTip` use idle
delay, defaults, sticky rectangles and optional interactive/custom content;
clickable tooltips become mouse opaque and extend the sticky area. `LLToolTip::draw`
fades using `ToolTipFadeTime` and hides at zero. Context controls share root/floater
ownership but their action registries are not audited here. **Q2:** CPU hover
membership, capture, delay/fade clock, sticky region and native tooltip content
must use the same eligible layer policy as hit testing; glyph/images are prepared
before recording. **Q3:** Extend native `appendTooltip` and tree routing with
explicit interactive-tooltip/popup contracts, not `prepareVkDraw/getVkDrawAlpha`
on GL objects. Open: world picking/tool tips, inspector callbacks, context action
eligibility, custom tooltip content and all nested clip consumers. Check delay
and fade boundaries, capture, hidden cursor, edge placement, popup dismissal,
diagonal versus axis-only movement and parent deletion while tooltip is visible.

### R8-R11: Bounded consumers and lifecycle

**R8 / Preferences. Q1:** `LLFloaterPreference::apply/cancel/onOpen` are not a
generic settings form. Apply dispatches panel overrides, requests UI resolution,
refreshes camera-FOV limits, updates proxy/media and conditionally sends account
information. Cancel invokes panel rollback, hides subsidiary dialogs, cancels
proxy state, may refresh pathfinding, restores preset/ignore state. Default
autoresponse localization is conditional on account readiness and whether the
user customized the value. Current audio deltas are recorded above.
**Q2:** Native CPU transactions need per-setting ownership, preview/commit/rollback
effects, locale inputs and generation-bound subsidiary dialogs; UI scale changes
invalidate native layout/assets coherently. **Q3:** Extend existing native
`showPreferences/applyPreferences` and snapshots per admitted consumer, not GL
panel callbacks. Open: panel-specific side effects, current audio services, FOV,
account writes and all rollback subscribers. Check Apply then edit/Cancel,
parent close with picker open, persistence failure, locale/default/custom strings
and UI-scale resize; saving a setting without its real consumer is not parity.

**R9 / camera controls. Q1:** `LLFloaterCamera::determineMode/switchMode/updateState`,
`postBuild/onOpen/onClose` select pan/preset/free-camera from appearance mode,
active tool and mouselook, preserve previous mode across close, connect joystick/
zoom/preset callbacks and unlock movement on ordinary close. Camera opacity is
the minimum of `CameraOpacity` and active floater transparency. Small/phototools
variants are separate consumers. **Q2:** CPU camera mode/input capture and versioned
view snapshots drive independent controls, selected states and opacity; world
rendering consumes the resulting view, not GL camera/tool globals. **Q3:** Native
camera controller plus native floater, with a single agreed pan/orbit/zoom slice
before presets. Open: joystick/zoom held input, tool switching, restrictions,
appearance, preset storage and scene-view ownership. Check held input/release,
focus loss/close, mouselook return, resize and exact camera/view plus UI feedback.

**R10 / environment controls. Q1:** `LLFloaterEnvironmentAdjust::onOpen` saves
beacon state, calls `captureCurrentEnvironment`, subscribes to environment changes
and resumes the GL reflection manager. Capture freezes/clones a local day cycle
or inherited parcel state, publishes local sky/water, and selects it instantly.
`refresh` disables children without both records; otherwise converts scaled color,
glow and quaternion values into controls. Edits update live settings; Reset opens
`PersonalSettingsConfirmReset` before closing/clearing local state; close disconnects
and resets live references/beacons. **Q2:** CPU native environment owner publishes
versioned local/parcel/region selection and exact unit conversions; native view/
resource consumers receive updates, including explicit reflection dependencies.
**Q3:** A native edit controller atop that owner, not a wrapper around
`LLEnvironment` or `gPipeline`. Open: environment callback/version filtering,
texture picker/assets, trackball math, beacon policy and shader consumers. Check
open-with-day-cycle, edit/reset/cancel, delayed update after close and region
change; compare rendered sky/water as well as controls. A slider-only fixture fails
the end-to-end gate.

**R11 / snapshots, loading and disconnect. Q1:** `LLFloaterSnapshot::onOpen/onClose`
selects the last destination, manages snapshot layer/focus/size and delegates
preview; base close unfreezes, clears avatar pause handles and restores tools.
`LLSnapshotLivePreview::onIdle` gates dimensions/delay/camera capture, detects
camera movement, hides/disables preview before `rawSnapshot`, passes UI/HUD/
balance/no-post/aspect/buffer policy, and updates thumbnail/freeze-frame state.
`prepareFreezeFrame` creates GL textures, so even preview preparation is forbidden
reuse. C10 and current `LLViewerWindow::setShowProgress` / `LLProgressView::fade`,
`handleKeyHere`, `setStartupComplete` preserve full/mini policy, fade in/out,
Ctrl+Q, media focus and cleanup. Disconnected IM input is disabled, not success.
**Q2:** Separate CPU capture request/view/history policy from completion-tagged
native offscreen images/readback/encoding; lifecycle records carry operation kind,
phase, allowed actions and readiness without fabricated percentages.
**Q3:** Native snapshot job owner and floater, coordinated with one lifecycle
layer controller; never a GL readback or screenshot of the ordinary presented
frame. Open: `rawSnapshot` transitive rerender/history closure, thumbnail effects,
destinations, teleport progress versus login/logout, full disconnected UI policy
and shutdown producers. Check delayed capture/cancel/resize, UI/HUD inclusion,
frozen history and output encoding; lifecycle transitions require full/mini,
error-over-progress, input blocking and exact fade sequences. Local PNG is a
proposed first destination, not an approved reduction of the snapshot objective.

Current-source disconnect follow-through: `LLAppViewer::forceDisconnect` suppresses
duplicates and chooses `ErrorMessage` before readiness versus `YouHaveBeenLoggedOut`
and best-effort logout after readiness. `disconnectViewer` restores minimized
floaters, persists account data, tears down services and disables parcel URL
requests. `render_disconnected_background` in `llviewerdisplay.cpp` loads the last
PNG, moves RGB toward its arithmetic mean using `(6*mean + channel)/7`, expands
the image and draws it at window size; load/decode failures return. These two
files were inspected at current GL, but were not in the 30-file equality check.
The render caller chain, decoded-component assumptions and `finish_disconnect` /
`finish_forced_disconnect` callbacks remain open. Native CPU lifecycle policy and
an independently decoded/versioned last-image product must cover the agreed
disconnected mode; never use `gDisconnectedImagep` or `gStartTexture`. Test forced
loss before/after readiness, missing image, resizing, disabled sends and exactly
one notice, separately from intentional logout/relogin. Returning to login is
not automatically equivalent to the reference disconnected presentation.

### Working-snapshot gap matrix and draft corrections

This table supersedes only the earlier survey's present-tense native descriptions.
The five-file `git diff` was read against native HEAD; unchanged native owners
below were inspected as foundations, not qualified services. All gaps are static
observations or expressly unestablished coverage, not runtime failure reports.

| Family | Nearest native owner / September 19 evidence | Remaining gap and proposed bounded edit |
|---|---|---|
| Shell/chrome R0-R3 | [UI](../../indra/llvulkan/llvkviewerui.cpp), `create/preparePaint`; [communications](../../indra/llvulkan/llvkcommunications.cpp), `refreshCommunications`. Creation reads `menu_login.xml`; communications occupies the root minus 19 pixels. | No source-equivalent connected shell is established by these owners. Add explicit root layers and usable rectangles, then status/nav/toolbar consumers. Coordinate only menu geometry/layer interface with the separate menu audit; no deep menu work here. |
| Conversations/Contacts R4 | `initializeCommunications` draft replaces HEAD's three columns with Contacts/Nearby/IM/Group tabs and selects Nearby. Friends/Contact Sets explicitly say their native services are unavailable; Groups uses membership data. | The C0/C5/C6 claim of three current columns is obsolete for this draft. Generated English declarations, combos and plain `parse_urls=false` transcripts still do not implement original rich history, hosted floaters, Friends/Contact Sets or console. Replace presentation incrementally while preserving transport bindings and drafts. |
| Read acknowledgment R4 | `selectConversation` resets unread for a visible IM tab; receive checks selected ID and tab visibility, not keyboard/app focus. Group tab selection resets unread. `preparePaint` calls `refreshCommunications`, which drains messages and can send typing updates. | Separate event/timer advancement from paint preparation and gate acknowledgment by the source-defined focus policy. Visible-but-unfocused receive is the first cheap discriminating fixture; repeated preparation must not consume events twice. App-focus semantics still require model closure. |
| Lifecycle R11 | [UI draft](../../indra/llvulkan/llvkviewerui.cpp), `refreshLifecycleScreen/appendLifecycleScreen/quitLifecycle`; [dialogs](../../indra/llvulkan/llvkdialogs.cpp), `refreshSession`; [window](../../indra/llvulkan/llvkwindowmgr.cpp), lifecycle message gate. Full/mini declarations, tagged Quit, focus/capture clearing and one-second clamped full-screen fade-in now exist. | C10's generic progress-notice observation is historical. Draft explicitly hides progress bars, full-screen logo/intro panels; draws black backing; hides the screen immediately on inactive state rather than source fade-out/cleanup. Preserve this as partial work, then implement admitted progress/image/logo/media and temporal contracts without inventing progress. Error/MFA/agreement notices still exist. |
| Composition R0/R5/R7 | `preparePaint` collects notice commands; removes their earlier pass only when lifecycle screen is active, then appends lifecycle and modal commands. Tooltip append is suppressed during lifecycle. | Non-lifecycle notices are still re-emitted after menus. Audit actual blend/opacity and top-control equivalence before changing it; a screenshot or command count alone cannot prove correctness. Require one explicit ordering policy matching hit testing, with nested dialogs over progress and resize. |
| Notifications/nesting R5/R6 | [Dialogs](../../indra/llvulkan/llvkdialogs.cpp), notice construction/`respondNotice/dismissNotice`, has queue, ignore/default-response persistence, 0.5-second default-button guard, one active notice and saved focus. [Floater](../../indra/llvulkan/llvkfloater.cpp), `close`, has dependent/focus hooks, capture clearing, minimize/resize. | Channel/toast/well coverage and a general host/dependent-modal graph are unestablished. Native close calls dependent-close hooks before hiding/restoring focus; GL explicitly releases focused controls before dependent close. Trace tree commit callbacks before deciding the fix, then test exact callback order, veto, parent destruction and focus restoration. |
| Preferences/assets R8 | `showPreferences/applyPreferences` in dialogs constructs original hierarchy, snapshots settings/warnings/bindings/colors, closes related pickers and invokes persistence callbacks. [Skin lookup](../../indra/llvulkan/llvkskinfiles.cpp), `find/read`, resolves fallback/current locale, skin/theme/user layers and bounded reads. | Reuse these independent foundations, not GL panel implementations. Per-control service effects, rollback and nested-dialog equivalence remain obligations; native asset lookup is not proof of complete localization or decode/GPU parity. |
| Camera/environment/snapshot R9-R11 | Existing floater/tree, [UI interface](../../indra/llvulkan/llvkviewerui.h) and [UI packet](../../indra/llvulkan/llvkuipacket.h) are candidate presentation boundaries. | No end-to-end native camera/environment/snapshot consumer was established by these reads. Do not label generic controls, diagnostics or old GL-backed passes as implementation. Close native view/environment/capture ownership before enabling an action; do not claim a whole-repository absence from this bounded survey. |

Production bindings are present in [startup](../../indra/llvulkan/llvkstartup.cpp):
`communications.context/receive/search/searchResult/groups/joinGroup/leaveGroup/`
`group/moderateGroup/local/direct` call `LLVKLoginTransport` methods; the window
installs the session owner. This is binding evidence, not a fresh transport audit
or live workflow result. Service capability, account isolation and callback
teardown remain gates. No send, moderation, inventory or profile mutation was run.

### Assets, localization and implementation boundary

Consume original declarations and assets through independent native owners:
main/navigation/toolbar/chiclet/progress layouts, original Conversations/Contacts
and consumer floaters, `widgets/tool_tip.xml`, skin image catalog/scale rectangles,
color layers and font/fallback descriptions. Current source decisions above,
not XML presence, choose visibility, enabled state, placement and effects.

Dynamic localization includes clock substitutions, balance labels, permission-
dependent Pin/Unpin, named-region/parcel updates, delayed names and URL labels,
counts, notice substitutions, and default-versus-user-edited autoresponses.
Reformat and remeasure when those inputs change; do not cache one English width
or concatenate English typing/moderator/status phrases as the native draft does.
Locale/skin switching must obey actual restart/live-update policy, still open
where not traced; this survey does not invent live-switch support.

CPU-only does not mean shareable: visual construction, layout, measurement,
wrapping, hit testing, focus, clipping, alpha preparation, animation and callbacks
need independently owned native implementations under NV-00/01. Do not call GL
`LLView/LLFloater/LLUICtrlFactory`, chat/history/console/chiclet owners, `LLFontGL`,
texture controls, `LLViewerWindow::rawSnapshot`, or their `getVk*` bridges.
Generic parsers, IO, clocks and genuinely nonvisual services remain audit
candidates; constructor/global/subscriber/teardown closure is required, including
independently assessed third-party libraries. No common low-level RHI or GL frame
interop is proposed. Native CPU packets must specify exact clips, painter order,
inherited alpha, image orientation/tint and font runs. Native glyph/skin/browser/
capture versions require readiness publication and all-use GPU completion before
descriptor reuse or retirement (NV-03/10-16); these lifetimes were not proven here.

### First end-to-end slices and checks

1. **Shell plus one existing dependent workflow:** extend `preparePaint`,
  `LLVKFloater` and window routing around Preferences + color picker + confirmation,
  with source-derived usable rectangles and lifecycle overlay. First cheap check:
  instrument existing widget/window fixtures for paint/hit order and commit/focus
  transitions; veto/close parent, resize/DPI, Escape/Enter/Tab and capture loss.
  Compare images and alpha at intermediate fade times, not just settled geometry.
2. **Nearby and one direct IM:** preserve startup/transport bindings, separate
  arrival/typing timers from painting, implement rich-history/console preparation
  and focus-correct acknowledgment, then native host/tear-off. First check: same
  message sequence while focused, visible-unfocused, scrolled, hidden and replayed;
  exact unread/draft/selection/fade and no duplicate logging/sends. Contacts lists
  follow audited buddy/name/contact-set data, not placeholder tabs.
3. **Status clock/location plus toolbar placement:** independently prepare actual
  session/region/clock data and original assets, with availability explicit.
  Check refresh boundaries, localized widths, mouselook and exclusion rectangles;
  then admit one audited destination. Favorites follow read-only inventory
  completeness and overflow tests; no teleport/reorder is implicit.
4. **One agreed notification producer:** real service event -> independent
  channel policy -> native toast/alert -> valid response/expiry -> teardown.
  Check hover timer, overflow, notice-over-picker/progress, stale generation and
  exactly-once response. Expand classes only after this production slice passes.

Camera/environment/snapshot are the next dependent slices, not fake controls to
add in parallel: select their real native producer first and use R9-R11 checks.
Reuse the existing widget/window/session/protocol test homes linked above; no
new test files or implementation are prescribed by this documentation task.
All listed executable checks are **planned, not run**. Exact acceptance covers
geometry, text/icons, textures/color/blending, nested clips, order, focus/input,
animation and dependent-dialog behavior. Retain the pinned oracle and record
current-source deltas separately; no baseline/tolerance upgrade is authorized.

### Refresh review record and validation

| Field | September 19 record |
|---|---|
| Contract/change class | Documentation-only source survey/design; NV-00/01/02/03/05/09-18. No feature completion, contract amendment or implementation permission. |
| Reference | GL master `1b7498c217`, oracle `59108e15a1`, native HEAD `005a4ae869` independently resolved; tracked GL/oracle status clean. Native five-file dirty diff distinguished from HEAD. No profiles, credentials or runtime state inspected. |
| Source checks actually performed | Required documents read; scoped body/callback reads and 30-file commit comparison; console/Preferences delta bodies read; native draft diff and nearest preparation/dialog/floater/skin/startup owners read. Unresolved outgoing edges are retained above. |
| Documentation checks actually performed | Local Markdown targets checked with PowerShell `Test-Path`; conflict markers and trailing whitespace checked after each edit. Final link/source-path and editor diagnostics checks are reported in the task close-out. Earlier September 18 validation statements are historical, not results repeated here. |
| Data/GPU changes | None. CPU/GPU design boundaries only; resource uploads, barriers, noncoherent memory, descriptors, retirement, WSI recovery and shader ABI remain unverified for new consumers. |
| Missing evidence | No build/tests, viewer launch, GL API trace, capture, performance/temporal measurement, validation layers or live workflow/parity run. No new runtime claim and no invalidation of prior passed operator evidence. |
| Scope/risk | Concurrent drafts may move after this snapshot. Deep top menus excluded. Callback closure, platform/skin/locale variants, People/V2, world/HUD, voice/media/economy, inventory mutations and snapshot destinations are not silently completed or authorized. |

Source anchors in R0-R11 name functions at the stated GL revision; native links
refer to this worktree and can move with concurrent edits. The highest-priority
risks are shell paint/input disagreement, focus-based unread loss, incomplete
dependent ownership/commit order, lifecycle temporal/asset gaps and unaudited
visual callbacks behind apparently nonvisual services. A successful static survey
or future screenshot cannot close those runtime and exact-parity gates.

## 2026-09-19 connected shell implementation record

This append records the authorized shell implementation, preserving all earlier
surveys and lifecycle WIP. Ownership is limited to viewerui/dialogs/windowmgr,
fixture 209 and the new native connected-shell translation unit. Header and CMake
integration belong to main; menu and communications implementations belong to
their agents. No builds, launches or Git mutations are authorized in this slice.

### NV-00 source contract and design before shell adaptation

Current GL discovery revision is `1b7498c2172c500e536d2b9c88e42980c860284d`;
the separate oracle remains `59108e15a1f8f94d2da7c674d937d19f5cf9450d` and the
implementation checkpoint remains `90af5a7`. Read-only commit comparison found
no differences in the selected viewer-window/menu/navigation/favorites/toolbar
roots and main/navigation/toolbar/menu-viewer declarations. This bounded equality
is not transitive closure or an oracle amendment. R0-R3 and the top-menu survey
provide the source roots, callback obligations and explicit unclosed edges.

1. **GL result:** `LLViewerWindow::initBase/initWorldUI/draw`, `init_menus`,
  `LLNavigationBar::setupPanel`, `LLLocationInputCtrl::refreshLocation`,
  `LLFavoritesBarCtrl::updateButtons` and `LLToolBarView::loadToolbars` construct
  separate login/connected menus and navigation/favorites/three toolbar regions.
  Skin declarations, command metadata and account arrangement select geometry,
  images, labels and state; location/favorites require actual region/parcel,
  teleport-history and inventory producers. Floaters sit over ordinary chrome,
  menus over floaters, blocking progress below permitted modal notices.
  Firestorm explicitly does not activate menus on F10. Modal primitives must
  occur once, not be blended twice. UI preparation is CPU visual work; none of
  these GL constructors, layout methods, font/image owners or callbacks is reused.
2. **Native production:** existing independent skin/XML/font/control/paint owners
  consume the same declarations. A bounded native adapter constructs the
  navigation geometry, read-only region field and favorites label with no fake
  landmarks, parcel permissions, history or teleport callbacks. Toolbar commands
  use original metadata/icons and exact admitted native destinations; absent
  services are recorded as blockers. Connected menu callbacks validate the
  current incarnation/session/layer. Login bindings are retained across logout;
  detached old menu owners retire before replacement. Native 2D rendering clears
  black without entering a GL world path. GPU resource ABI/lifetime is unchanged.
3. **Smallest ownership:** extend `LLVKViewerUi` with one shell subtree, menu
  incarnation and explicit blockers, implemented in `llvkconnectedshell.cpp`.
  Do not add another renderer, GL-compatible registry or fullscreen dashboard.
  Communication floaters remain independently owned by their agent. Native
  navigation specialization, toolbar wrapping/drag/account persistence and
  service-dependent affordances remain open rather than silently accepted as
  generic-control parity. Missing functionality is not closed by disabling it.

Cheap discriminators: fixture 209 counts modal primitives against isolated notice
preparation with lifecycle off/on, tests actual lifecycle Quit rather than removed
Cancel notices, and preserves tagged retry/cleanup/agreement/MFA coverage. Shell
integration must assert menu declaration switching, retained login bindings,
original region data, no generated favorites, stale-menu rejection, source-shaped
chrome below floaters, and disappearance on logout/owner replacement. Editor
diagnostics follow each edit; execution is reserved for main's serial integration.
Exact geometry/color/alpha/images/fonts/clips/effects/input parity remains unverified.

### Resume contract recorded before edits (2026-09-19)

This continuation owns ONLY `indra/llvulkan/llvkconnectedshell.cpp` and this
survey. The earlier, broader ownership paragraph is historical. Parent owns
the shared header, viewer UI, dialogs, window, CMake and tests. Current untracked
shell contents were read in full; they are the implementation delta, not evidence
of a tracked patch. No terminal or Git command is permitted in this continuation,
so earlier revision/cleanliness/diff claims are inherited evidence, not rechecked.
Current GL discovery remains `1b7498c217`; oracle `59108e15a1` is unchanged.

NV-00 refinement for `selectShellMenu`, `prepareConnectedShell`, toolbar binding
and `activateShellCommand`, including their new local admission helpers:

1. **GL:** R0-R3/C1-C3 and the top-menu report trace separate connected/login
  menus, real floater actions, checked visibility, enable predicates and disabled
  ancestors. Hidden Advanced retains shortcuts; disabled Developer/Admin do not.
  `Floater.ToggleOrBringToFront` is not unconditional hide. Toolbars derive their
  actions/running state from command declarations. Location/favorites and
  specialized callbacks are required semantics, not optional XML decoration.
  Registered GL callbacks and visual owners remain forbidden dependencies.
2. **Native:** CPU-only shell admission must use the same live owner/tag,
  connected presentation, modal/lifecycle and actual native service facts for
  dispatch and advertised availability. Menu handlers remain parameter-specific;
  missing predicate implementations must fail closed rather than inherit the
  menu engine's permissive fallback. Visibility checks report real native
  floaters, not dummy successful controls. Existing native menu/tree/floater
  owners retain paint/resources; no GPU ABI or lifecycle changes are proposed.
3. **Smallest design/check:** centralize shell command availability, refresh only
  admitted native bindings, and retain session/incarnation guards on callbacks.
  Preserve original declarations and explicit blockers for every unsupported
  specialized conversion or callback. Static discriminator: trace each bound
  action and each check/enable/visible predicate in the actual menu declaration;
  unbound services, stale owners/tags and blocking layers must never enable or
  execute a command. Parent must extend existing widget fixtures for late service
  installation/removal, hidden Advanced, unavailable predicates, old detached
  menus, owner replacement and reconnect. Editor diagnostics immediately follow
  every edit here; builds/tests/runtime remain prohibited and unverified.

This is a bounded correction and integration handoff under NV-00/01/02/03/12/15/
17/18, not completion of the user's required menu/address/favorites/Conversations/
Contacts tabs/per-group tabs/IM/People/toolbars. Source callbacks, toolbar layout,
specialized controls and backend service obligations remain open below.

### Sequential shell integration: admission and declarations (2026-09-19)

This continuation owns shell/menu integration in viewerui/dialogs/windowmgr,
the existing untracked shell translation unit, minimum shell header declarations
and this append only. Menu/communications, tests, CMake and GL are read-only.
Prior edits are preserved; builds, configure, launches and Git writes are prohibited.

NV-00 pre-edit contract: the preceding R0-R3/source records and menu M3a-M3c
apply to current GL target `1b7498c217`, unchanged oracle `59108e15a1` and
checkpoint `90af5a7`. `LLMenuItemCallGL::updateEnabled/onCommit` and
`LLFloaterReg::toggleInstanceOrBringToFront` distinguish real availability,
focus/frontmost and minimized state. The actual menu and command declarations
select parameter-specific consumers. Unknown predicates are not permission to
execute an action. Native session admission must not mix an old callback or
region record with the current owner/tag.

Native design: retain existing independent native shell/menu/floater owners,
add their missing private header declarations, and use one CPU-only admission
policy for toolbar and menu dispatch. Refresh parameter-specific bindings from
current services and capture owner/tag/incarnation. Consume the communications
context already validated by preparation, without a second producer poll.
Install explicit false defaults for unaudited menu predicates; override only
implemented policy. This changes no visual assets, GPU ABI or resource lifetime.

Hypothesis/check: the prior shell has undeclared members, creation-time-only
bindings, missing-predicate permissive fallback and callbacks without tag checks.
Editor diagnostics immediately follow each substantive edit. Main must test
service changes, missing predicates, stale owner/tag/incarnation, reconnect,
modal/lifecycle admission, login Debug versus hidden Advanced policy, and real
checkmarks using existing widget fixtures. These static repairs are not runtime
or exact visual/effects parity and do not close the disabled service scaffolds.

Adjacent route contract (recorded before implementation): current-target
`LLTogglePanelPeopleTab::handleEvent/togglePeoplePanel` routes Friends/Groups/
Contact Sets into hosted Contacts unless `FSUseV2Friends` outside Vintage selects
People; Nearby uses People. Selected visible People tabs toggle their host.
`LLFloaterReg::toggleInstance` closes a shown host, while
`toggleInstanceOrBringToFront` first restores minimized or unfocused windows.
`LLFloater::closeHostedFloater` closes the host, not its retained child page.
The native shell uses existing show/hide/select routes and tree focus/selected
pages; no borrowed communication floater survives a refreshing call. Torn-off
Contacts cannot be honored and remains unavailable, not mapped to hosted success.
Discriminator for main: actual XUI actions open the right existing page, checked
state follows the selected hosted page, unfocused/minimized Conversations restore
rather than close, and missing producer lists stay explicit with no fake rows.
Opening these existing consumers does not establish original rich-XUI parity or
Friends/Contact Sets/nearby/recent service completion.

The same pre-edit focus/minimize contract applies to the original toolbar
`howto` command (`Floater.ToggleOrBringToFront`, `guidebook`) and the connected
Help toggle. The shell must restore/focus its existing native Guidebook before
closing, using the actual focus ancestry rather than frontmost alone. Existing
Guidebook creation/browser publication stays in its owner. Tear-off requests
use the same refreshed owner/tag/incarnation admission as command dispatch.
Main's discriminator includes an unfocused frontmost and a minimized Guidebook,
plus a retained tear-off callback after a session change. No new browser service
or menu-engine behavior is introduced.

### Sequential shell handoff and evidence

Implemented in this continuation:

- Added only the missing private shell declarations/state to `llvkviewerui.h`.
  No communication API changed. Existing shell initialization, menu selection,
  preparation, floater enumeration, OS activation forwarding and black clear
  integration in viewerui/dialogs/windowmgr were preserved.
- Menu/toolbar availability now uses current owner state/tag, validated native
  communication context and blocking layers. Parameter-specific menu bindings
  refresh during preparation; retained command/toolbar/tear-off callbacks reject
  old owner/tag/incarnation. Address presentation reuses the validated context
  instead of invoking its producer again. No address/favorite action is bound.
- Bound actual XUI Contacts/Nearby/People-tab parameters to existing native
  consumers, honoring hosted Contacts versus V2 People selection and explicit
  unsupported torn-off Contacts. Checkmarks follow the selected hosted page.
  Conversations/Nearby/Guidebook restore minimized or unfocused windows before
  close. No generic Floater action or fabricated data producer was introduced.
- Missing menu predicates explicitly return false. Login Debug visibility and
  enablement update together; hidden Advanced retains accelerator eligibility.
  Develop/Admin remain unavailable without audited policy producers. Missing
  consumers/actions remain disabled, not successful no-ops or completed work.

Exact integration required from main:

1. Add `llvkconnectedshell.cpp` to the existing `llvkwidgets` source list in
   [CMakeLists.txt](../../indra/llvulkan/CMakeLists.txt), for example
   `target_sources(llvkwidgets PRIVATE llvkconnectedshell.cpp)`. It is still an
   untracked source file and was not added to Git here. CMake is unchanged.
   The header now declares `initializeConnectedShell`, `initializeShellToolbar`,
   `prepareConnectedShell`, `selectShellMenu`, `activateShellCommand`,
   `shellSessionCurrent`, `shellCommandAvailable`, `shellFloaterVisible` and
   `refreshShellMenuBindings` plus their private state; no further shell header
   additions or transport signatures are requested by this slice.
2. Extend existing widget fixtures, not a new test framework: actual connected
   XUI routes/checked state and login restoration; hosted versus V2 tab routing;
   focus/minimize behavior; late service availability; stale owner/tag and
   detached callbacks; modal/lifecycle rejection; unavailable predicates and
   unchanged original assets/layout. Cover tagged address clearing on context
   loss and absence of generated favorites. Existing menu fixtures 194/197 and
   communications fixture 219 remain required, execution-pending regressions.
3. Reconcile fixture 209 with the new communication owners. Its lifecycle Quit,
   modal single-emission and shell assertions already exist in the inspected
   WIP; it is not wholly the previously reported progress-notice fixture.
   However, it still expects `conversation_list` and the old shared IM/group
   controls. Use the existing UUID/page APIs and fixture 219's current patterns.
   Do not restore old controls to make the stale assertions pass. The current
   modal pass already returns true from `erase_if` for notice commands before
   appending them once; this continuation did not modify that prior correction.
4. The next narrow compile/test gate is `INTEGRATION_TEST_llvkwidgettree` from a
   main-verified build of this worktree after source registration and fixture
   reconciliation. Then perform the existing window and viewer-link gates
   serially as required. No branch-local `build-vc170-64/CMakeCache.txt` exists;
   other worktrees' binaries cannot qualify these sources.

Explicit remaining gaps: address editing/history/coordinates/parcel/maturity
and teleport operations; session-tagged favorites inventory/landmark readiness,
overflow and actions; full navigation specialization and status producers;
toolbar side regions, account arrangement/persistence, drag/reorder, wrapping,
overflow and nearby chatbar; remaining menu policy/actions/input variants;
Friends/presence/name and Contact Sets producers; nearby/recent People data;
rich chat, original communications XUI/assets, tear-off and dependent actions.
Existing consumers can expose their unavailable states, but those are not the
requested functionality or exact UI parity. Inventory/Outfits and world rendering
remain deferred; the preserved black clear does not qualify world rendering.
Resident search remains a separate existing consumer, not Friends or Nearby.
No native address/favorites producer contract was invented or GL callback reused.

| Review field | This continuation |
|---|---|
| Contract | NV-00/01/02/03/12/15/17/18; bounded shell admission, native consumer routing and declaration integration. |
| Reference | Current GL `1b7498c217`, oracle `59108e15a1`, checkpoint `90af5a7`; actual menu/command declarations and read-only GL routing/floater roots above. No runtime capture/configuration claim. |
| Data flow | Existing session/context tags and native UI owners; no service, image, color, alpha, clip, geometry, shader or GPU ABI change. |
| GPU safety | N/A for these CPU admission/routing edits; existing publication and retirement are unchanged, not requalified. |
| Validation | Immediate editor diagnostics after every substantive patch: no reported errors. Scoped tracked diff hygiene and untracked shell/document hygiene passed; all 13 shell data members have header declarations, nine selected XUI routes exist, and the shell no longer polls the context producer. These are static checks, not C++ compilation or behavioral execution. |
| Limits | No build/configure/install, executable test, viewer launch, GPU test or runtime/visual/effects parity performed. No test/CMake/GL/menu-engine/communications edit, Git write, subagent or parallel execution. |
| Change class | Bounded native integration/correction; no oracle amendment and no completion claim for the engineering minimum. |