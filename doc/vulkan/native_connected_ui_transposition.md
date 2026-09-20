# Connected native UI transposition

## Scope and provenance

Recorded 2026-09-18 before the communications implementation edit. Governing
contracts: [native invariants](native_vulkan_invariants.md), read in full;
[roadmap](native_viewer_roadmap.md); [approved report](reverse-engineering/README.md);
[tree instructions](../../indra/llvulkan/AGENTS.md).

- Implementation lineage: checkpoint `90af5a7220f1fec28e3c909c051b6ee7e062f10b`.
  The supplied native-sl-login branch ref was read directly and contained
  `005a4ae8693cf9711093a3c44783bff7af605f99`. No reset or ancestry claim is made.
- Read-only oracle: `C:/Dev/vulkanstorm/worktrees/notification-gl-reference`;
  its worktree HEAD was read directly and contained
  `59108e15a1f8f94d2da7c674d937d19f5cf9450d`. No terminal command was used.
  This verifies HEAD, not a clean-worktree assertion.
- Configuration studied: Firestorm default English XUI, hosted Contacts/Nearby
  Chat, left Conversations tabs. Other skins, torn-off windows and runtime
  settings are separate obligations. The generic `floater_im_container.xml`
  is NOT the Firestorm container registered by `llviewerfloaterreg.cpp`.
- Only this document and `indra/llvulkan/llvkcommunications.cpp` may change.
  The lifecycle agent owns headers, dialogs, window/status and viewer UI files.
  No GL code, shared widget, build file, setting, credential or memory edits.
- This is a bounded native navigation/service integration, not full in-world
  transposition or an accepted visual substitute. The former three-column
  communications screen is not parity and is not the target architecture.

## NV-00 record: hosted communications navigation

### 1. Source contract

Pinned source roots (all paths below are relative to the oracle's `indra`):

| Root | Inspected behavior and outgoing obligations |
| --- | --- |
| `newview/llviewerfloaterreg.cpp`, registrations `fs_im_container`, `imcontacts`, `fs_nearby_chat` | Selects `floater_fs_im_container.xml`, `floater_fs_contacts.xml`, `floater_fs_nearby_chat.xml` and the Firestorm owners, not the generic LL IM container. |
| `newview/fsfloaterimcontainer.cpp`: constructor/destructor, `postBuild`, `initTabs`, `onOpen`, `onClose`, `addFloater`, `addNewSession`, `removeFloater` | Contacts is first; Nearby Chat follows and is initially active. Host insertion locks these tabs and disables their close buttons. IM/group sessions follow, with configured ordering and tear-off/redock behavior. Non-quitting container close does not close all sessions. Observes IM model, transient floater manager, voice state and chiclets; disconnects its message/session observers on destruction. These GL owners are not reusable. |
| `newview/fsfloatercontacts.cpp`: constructor/destructor, `postBuild`, `draw`, `tick`, `handleKeyHere`, `onOpen`, `openTab` | Nested `friends_and_groups` tabs are Friends, Groups, Contact Sets. Buddy/voice/contact-set/name-cache observers update data; draw processes deferred name sorting and column-setting correction before `LLFloater::draw`. Filter shortcuts and hosted Ctrl-W are explicit behavior, not generic focus traversal. Teardown disconnects subscriptions. |
| Same file: `updateGroupButtons`, `onGroupChatButtonClicked`, `onGroupLeaveButtonClicked`, friend and group action callbacks | Chat requires a selected non-null group and `GP_SESSION_JOIN`; starts a session via `LLGroupActions::startIM`. Contacts Leave invokes membership leave, NOT chat-session leave. Friend double-click/IM uses one-to-one IM or conference by selection count. Picker creation attaches a dependent floater; remove, profile, teleport, pay and group management have distinct callbacks. No native permission to invoke these GL-coupled actions follows from their service-like names. |
| `newview/fsfloaternearbychat.cpp`: `postBuild` | Connects commit, keystroke, focus, text expansion, autoreplace, RLVa, history, search, emoji and unread callbacks. Chooses viewer chat font, single-line setting and opacity override. Sets chat type to say. Rich histories include muted/non-muted views. `LLViewerChat`, `LLFontGL`, emoji and history owners are visual dependencies, including their CPU preparation. |

The source data/layout and callback graph are distinct. XUI specifies left
Conversations tabs of width 115/height 20, nested top Contacts tabs of width
90/height 20, reference tab images and labels. Hosted-window placement,
focus, alpha and session lifetime also depend on `LLMultiFloater`,
`LLTabContainer`, `LLFloater`, `gFloaterView` and settings. Those transitive
implementations, plus name/presence and rich-history callbacks, remain open for
full parity; local root inspection is not transitive closure.

### 2. Native result production

CPU-only native widget nodes hold tab selection, geometry and visibility.
Existing native controls hold editor text; existing per-session maps hold drafts,
transcripts, unread counts and typing deadlines. Existing `CommunicationServices`
carry generation-tagged real transport commands and snapshots. UI construction
must not instantiate `FSFloaterContacts`, `FSFloaterIMContainer`, `LLGroupList`,
`LLAvatarActions`, `LLGroupActions` or the reference's layout/input/font owners.

The narrow implementation replaces simultaneous columns with native tabs:
Contacts first, Nearby Chat second/initially selected, and retained IM/group
service panes. Contacts retains the Friends/Groups/Contact Sets hierarchy;
Friends and Contact Sets show explicit unavailable states, not invented rows.
Groups remains backed by actual `CommunicationServices::groups`. Starting chat
from Contacts selects the existing group service pane, not membership mutation.
The existing bounded IM/group selectors are retained as integration debt; they
are NOT the reference's independent per-session floater tabs.

The native factory's tag dispatch supports `tab_container`, `panel`, native
line/text editors and combos. `LLVKWidgetTree::attachTabPanel` installs owned
selection/focus callbacks. `selectTabPanel` validates ownership and enablement,
toggles page visibility, updates selected-button state, lays out tabs and invokes
the container commit. Hiding a page must not acknowledge a message as displayed
or discard its draft. A tab transition must stop outgoing typing when leaving
the IM editor. No new GPU resource ABI or low-level render calls are needed;
existing native paint/image/font publication remains responsible for rendering.

### 3. Smallest coherent design

Use `mCommunicationPanel`, existing tree tab state and the existing conversation
maps. Bind local callbacks to `this` exactly as existing communications controls
do; tree ownership bounds their lifetime. Do not add static/global state, store
floater owners in unrelated maps, or change header declarations. Preserve control
identities used by existing service tests and lifecycle focus restoration.

Rejected alternatives: wrapping GL floaters; treating recent IM senders as
friends; assuming absent rosters mean empty rosters; representing Contacts Leave
with `leaveGroup` (which leaves a conversation); introducing fake status,
favorites or inventory controls; replacing unsupported rich widgets with a
generic panel and calling it parity; building a speculative multi-floater owner
inside this file without lifecycle integration.

### Discriminating checks (before implementation)

1. Static factory check: every new XUI tag/attribute must be accepted by the
   native parser; Contacts/Nearby ordering, tab metrics and images must cite the
   registered Firestorm assets, not the generic container.
2. Existing `tests/llvkwidgettree_test.cpp` connected communications fixture must
   retain actual local/IM/group sends, drafts, search result generation checks,
   moderation, typing and disconnect/reconnect rejection. Main agent must run it
   serially after compiling; no tests/builds are executed by this agent.
3. Integration fixture must select tabs through native APIs: hidden selected IM
   and group sessions accumulate unread; selecting the visible session clears
   only that session's unread and preserves drafts. Tab changes must not send
   chat or join a group implicitly. Group Chat requires real membership/power;
   unavailable Friends/Contact Sets must never manufacture data or mutations.
4. Editor diagnostics after each substantive edit. They can reject C++ mistakes
   but do not parse embedded XUI or establish compilation/runtime correctness.
5. Exact reference/runtime comparisons remain mandatory: geometry, images,
   fonts, colors, alpha, clips, order, focus, keyboard routing, nested dialogs,
   resize and transitions. No tolerance relaxation, marker-only acceptance or
   screenshot-as-completion. Runtime reference is unavailable here.

## Exact integration blockers

| Boundary | Required integrator work; not supplied by this slice |
| --- | --- |
| Factory / reference XUI | `llvkwidgetfactory.cpp` tag dispatch rejects `multi_floater`, `fs_chat_history`, `chat_editor` and `group_list`. `class="contact_sets_panel"` does not instantiate an independently owned Contacts controller. The reference files cannot simply be loaded as a complete native implementation. Native implementations must close the contracts, not silently accept/ignore unsupported tags. |
| Hosted Conversations | Add lifecycle-owned native multi-floater/session ownership with stable session IDs, separate per-session tabs, ordering/locking, title updates, host close/hide versus session leave, saved rect/visibility, drag/reorder, tear-off/redock, focus restore, dependent-dialog stacking and disposal. Expose named show/select routes to the connected menu. |
| Friends / presence | Extend `CommunicationServices` with generation-tagged buddy snapshot/update, online and rights data, resolved names and failure/loading state, plus separately audited actions where in scope. During this task the concurrently updated transport exposed `LLVKLoginTransport::friends(tag)`, but the inspected UI header still had no Friends binding. Wire that real producer after audit, rather than building a duplicate transport. Current IM `Conversation::name` and resident search are not a friendship or presence service. SIP/voice stays outside this slice. |
| Contact Sets | Supply account-owned set/membership data, change generations, color/name rules, filter/selection and persistence, with audited nonvisual sources and native visual consumers. Do not reuse `LGGContactSets` or its callbacks without tracing their actual closure. |
| Groups | Native roster view needs actual membership selection/filter/sort/count/insignia and action permissions. Distinguish session start/leave from group membership leave. Profile/info, invite, activate, pin, titles and nested dialogs need separate services/owners; membership/economy mutations are not implemented here. |
| In-world chrome | Lifecycle agent owns connected menu/status/location/favorites and nearby-bar placement. It must audit `init_menus`, `LLStatusBar`, `LLNavigationBar`, `LLLocationInputCtrl`, `LLFavoritesBarCtrl`, `FSNearbyChat` and their callbacks before wiring independently owned native equivalents. This file cannot replace those owners or invent region/parcel/balance/favorites facts. |
| Rich chat / errors | Native styled history, source unread/scroll policy, links, muted history, timestamps, chat font/opacity, expandable composer, search, emoji, completion, localization and nested errors remain separate obligations. Existing plain transcript/editor behavior is preserved, not upgraded to rich-history parity by moving it. |

## Review record and evidence

| Field | Record |
| --- | --- |
| Contract | NV-00, NV-01, NV-02, NV-03, NV-12, NV-15, NV-17; bounded native connected navigation and existing real chat services. |
| Reference | Pinned revision/default English XUI above. No device, driver, runtime dimensions, capture history or effective runtime settings observed. |
| Data flow | CPU widget hierarchy/visibility changes only; existing service signatures, generation tags and transcript representation retained. No new color/alpha/depth or coordinate ABI. |
| GPU safety | No upload, descriptor, synchronization or retirement changes. Existing native preparation/paint/resource ownership remains in place; this is not renewed GPU validation. |
| Validation | Source inspection and editor diagnostics only in this task. No terminal, build/configure, test execution, viewer/window fixture, settings or credential access. Main agent serializes compilation/tests. |
| Limits | Factory and service blockers above remain open. Existing plain panes and unavailable states are not exact visual parity or Contacts completion. No live presence, full chrome or whole-viewer claim. |
| Change class | Bounded parity-directed navigation integration; no baseline/tolerance amendment and no GL modification. |

## Additional source survey and local audit

These are source observations, not claims that the complete outgoing graph is
closed or that chrome has been implemented in communications.

| Controlling root in pinned `indra/newview` | Behavior recovered; native owner obligation |
| --- | --- |
| `llviewermenu.cpp`: `init_menus` | Calls `initialize_menus`, constructs GL popup/context menus with the viewer registry and later loads `menu_viewer.xml` into `LLMenuBarGL`. Initialization includes actions, not just XUI parsing. Native menu ownership must resolve every enabled action/checked predicate to an audited service; do not call this initialization or forward its GL callbacks. Complete action registry closure remains open. |
| `llstatusbar.cpp`: `draw`, `postBuild`, `refresh` | Draw calls refresh, parcel text and health preparation before `LLPanel::draw`. Post-build installs clock/preferences, balance refresh/purchase, preset, audio/media and hover callbacks. Refresh applies 0.5-second network-stat and 1-second FPS/clock updates, with color selection depending on focus, cap and vsync facts. These are CPU visual decisions coupled to viewer/UI/window globals, not a shareable status getter. Native status needs real selected-backend measurements and agent/parcel/time snapshots; economy is out of scope. |
| `llnavigationbar.cpp`: constructor/destructor, `setupPanel` | Firestorm uses the existing root `navigation_bar`, not the commented-out `postBuild`/draw path. Registers login, location/search, back/forward/home, held/dragged history-menu, teleport success/failure and RLVa callbacks; teardown disconnects teleport and RLVa subscriptions. Uses `LLUI`, pull buttons and location/search controls. Native lifetime must own equivalent subscriptions and history state, not borrow that root view. |
| `lllocationinputctrl.cpp`: `refresh`, `refreshLocation`, `refreshParcelIcons` | Refresh combines location, parcel icons and landmark affordance. Location refresh early-returns when the control or any editing/popup/landmark child has focus; chooses coordinate format and updates maturity. Icon preparation depends on actual agent and selected parcel state and performs right-to-left layout. Native region-name text alone is not this contract. Parcel, landmark, maturity and focus sources are explicit blockers. |
| `llfavoritesbar.cpp`: `draw` | Draw paints the drag marker and consumes its flag, schedules delayed favorites persistence, and rebuilds dirty buttons after a callback deadline. These draw-time effects must become explicit native preparation/timer work. Inventory-backed favorites, landmark actions and asynchronous callbacks remain unaudited beyond this root; no favorite buttons or persistence were fabricated. |
| `fsfloaterim.cpp`: `setVisible`, `getVisible`, `updateMessages` | Visibility updates toast channels and console sessions, treats inactive/minimized hosted tabs as invisible, and avoids stealing focus for background IMs. `updateMessages` passes `hasFocus()` to `LLIMModel::getMessages` so an unfocused history fetch does not reset unread. Native hidden-pane accounting is only a subset: focused/unfocused visible panes, app focus, minimized hosts, toasts and rich-history scroll acknowledgement still require integration and tests. |

Pinned source links:
[container](https://github.com/anne-skydancer/vulkanstorm/blob/59108e15a1f8f94d2da7c674d937d19f5cf9450d/indra/newview/fsfloaterimcontainer.cpp),
[Contacts](https://github.com/anne-skydancer/vulkanstorm/blob/59108e15a1f8f94d2da7c674d937d19f5cf9450d/indra/newview/fsfloatercontacts.cpp),
[Nearby Chat](https://github.com/anne-skydancer/vulkanstorm/blob/59108e15a1f8f94d2da7c674d937d19f5cf9450d/indra/newview/fsfloaternearbychat.cpp),
[IM](https://github.com/anne-skydancer/vulkanstorm/blob/59108e15a1f8f94d2da7c674d937d19f5cf9450d/indra/newview/fsfloaterim.cpp),
[Contacts XUI](https://github.com/anne-skydancer/vulkanstorm/blob/59108e15a1f8f94d2da7c674d937d19f5cf9450d/indra/newview/skins/default/xui/en/floater_fs_contacts.xml).

Native local audit:

- `llvkstartup.cpp` binds the existing UI services to `LLVKLoginTransport`, not
  GL visual owners. `chatContext` requires Online phase, bootstrap identity and
  matching tag. `takeMessages` merges circuit/event messages and drains its
  event queue; `sendLocal` waits for the simulator echo; `sendDirect` retains
  typing dialog selection, direct-session identity and buddy offline policy.
  Those implementations and presence processing are untouched.
- `joinGroup` requires membership/power and an active tag; it either queues
  invitation acceptance or sends a session-start message, increments group
  version, enters Joining and installs a timeout. `leaveGroup` sends session
  leave and clears versioned participant/pending state, not membership.
  `sendGroup` requires Joined, permission and no server text mute. The UI's
  new Chat route additionally checks the current owner state/tag before calling
  these services. HTTP/circuit/GPU closure is not newly certified by this read.
- `llvkpanel.cpp` owns tab selection/visibility/focus callbacks; tab commits do
  not need GL UI classes. `llvkwidgettree.cpp::setVisible` propagates native
  visibility notification. Native factory/paint resources remain existing debt
  where not inspected; no GL implementation is called or extracted here.

## Implemented changes

- `initializeCommunications` now constructs the source-directed Contacts/Nearby
  hierarchy, retaining the existing IM and group service panes rather than
  inventing friend/group data. The original `native_communications` root,
  status/disconnect controls and service control IDs remain compatible.
- The left host tabs use the reference width 115/height 20 and its three
  unselected images. Contacts uses top tabs of width 90/height 20 and the
  reference flash-image declarations. This does NOT implement unread flashing.
- Friends and Contact Sets explicitly report missing UI service bindings.
  Groups uses actual memberships and reports absent binding/disconnection or
  no received memberships. Empty does not assert that loading is complete;
  loading/failed/ready roster states require a richer service contract.
- Contacts Chat uses real group session start, or opens the already joined
  session. The existing conversation leave action is labelled Leave Chat to
  prevent confusion with the reference's membership Leave action.
- Selecting an IM through existing routes opens its pane. Leaving that pane
  stops outgoing typing. Hidden selected IM/group sessions accumulate unread;
  navigation back acknowledges the selected session under the existing native
  selection policy. Drafts remain in existing editors/maps. No service or map
  ownership was moved into static state.
- Reusing the communications tree at reconnect selects Nearby Chat, retaining
  the lifecycle agent's existing local-composer focus restoration contract.
  No header, lifecycle, widget, test, CMake or GL files were edited by this task.

## Validation results and required gates

Completed here:

1. Read the two worktree metadata pointers and HEAD/ref values without terminal
   commands. Read the required governing documents, including the full native
   invariants and roadmap. No branch/revision mutation or clean-worktree claim.
2. Document editor diagnostics after the pre-implementation contract: no errors.
3. C++ editor diagnostics after the initial hierarchy/state edit: no errors.
4. Parser source inspection found `use_ellipses` is not accepted for native tab
   containers. Removed it from the authored declaration rather than changing
   the factory. Tab metrics, alignment and first/middle/last image declarations
   have native parser branches. Tab label ellipsis parity remains a blocker;
   embedded XUI construction has NOT been executed.
5. Focused read of the existing connected-service fixture prompted IM pane
   activation and reconnect selection fixes. C++ diagnostics after that repair:
   no errors. The fixture itself was not edited or run.

Main-agent/integrator requirements (serialize all builds and tests):

1. Compile the native communications translation unit and its normal viewer
   integration. Run `INTEGRATION_TEST_llvkwidgettree`, especially the fixture
   named `native session UI uses owner snapshots and rejects delayed recovery
   actions`, and the existing native session/protocol tests. No prior passed
   runtime result is invalidated outside the changed navigation surface.
2. Preserve the fixture's existing real-service assertions. Add explicit
   `selectTabPanel` navigation where exercising actual visible interaction:
   `im_box_tab_container` -> `instant_messages` for search/IM;
   `nearby_chat` for local send; `imcontacts` then `friends_and_groups` ->
   `groups_panel` for membership selection/Chat; `group_conversation` for
   session send/moderation. Programmatic commits alone are not input parity.
3. Add cases for hidden selected IM/group unread, draft round trips, returning
   to already joined group chat without another join, no join from selecting
   Contacts/Groups alone, stale-owner Chat rejection, no phantom Friends/Sets,
   idle dropdown preservation and reconnect with an unrelated modal. Keep
   typing delay/expiry, local echo, query generations, mute/moderation and
   disconnect guards as assertions, not relaxed expectations.
4. Lifecycle header integration should supply dedicated native host ownership
   and named show/select commands; generation-tagged Friends and Contact Sets
   snapshots; roster readiness/errors; focused/minimized/app-active state for
   reference unread policy; and explicit dependent-dialog disposal. Do not
   repurpose `mUiTests`, unrelated floater maps or globals for these owners.
5. Exact runtime visual/effects/input comparisons are still required and are
   UNVERIFIED. The retained full-size root/status row, plain histories/editors,
   aggregate IM/group panes, Groups combo and authored unavailable messages
   are known non-parity surfaces, not accepted substitutes for source floaters,
   rosters, rich histories or chrome. Full-window geometry, fonts/colors/alpha,
   nested clips, focus, keyboard, transitions and skin/localization variants
   have not passed acceptance. No claims of working Friends, Contact Sets,
   in-world status/location/favorites, voice, economy, inventory mutations or
   3D world rendering are made.

No terminal command, compilation/configuration, runtime/fixture launch, settings
or credential access, shared-memory write, commit, push, PR, merge or subagent
operation was performed. Diagnostic success is not compilation or parity.

## Architecture and readiness review - 2026-09-19

### Review scope and evidence boundary

Everything above this dated heading is the prior agent's historical record,
including its communications-file edit authority and statements about commands
not run. Those permissions do not carry forward. This review owns only this
document; C++, GL, other documentation, memory, Git state and build outputs are
not edited. No builds, tests, viewer launches or subagents are authorized here.

The inspected working tree has five modified native UI files (viewer UI source
and header, communications, dialogs and window manager) and three untracked
draft documents. Read-only Git reports native-sl-login 3 ahead/18 behind master
`1b7498c2172c500e536d2b9c88e42980c860284d`, with common ancestor
`035b1fdb8d3b3fae7441ba1a0de3855a2c3b65e8`. This is integration context, not a
reason to merge master before native UI work. The implementation checkpoint
`90af5a7` and pinned GL oracle `59108e15a1` retain their separate roles.

### Initial integration finding

**Verified static defect, inherited rather than introduced by this WIP:**
[preparePaint](../../indra/llvulkan/llvkviewerui.cpp#L827) copies notice commands
to `modalPass`, but removes them from the base list only when a lifecycle screen
exists; it then appends `modalPass` unconditionally. With no lifecycle screen,
the notice is emitted twice. This can change translucent composition and text
coverage. The earlier implementation already duplicated this pass. Fix the
owning composition path, not the source assets or alpha values. The smallest
discriminating fixture is a notice with a translucent background: prepare paint
with lifecycle off/on and assert each notice-owned primitive appears exactly
once, after the applicable lower layers. Runtime pixel impact is not measured
here. This is an NV-12/NV-17 blocker, not evidence against native independence.

### Revision reconciliation and current-source contract

Read-only `rev-parse` confirms HEAD and origin/native-sl-login both equal
`005a4ae8693cf9711093a3c44783bff7af605f99`. Its three branch-only commits are
`b011cff0c6` (authentication/region connection), `8e2b68f879` (text services and
isolated profiles), and `005a4ae869` (Enter routing/text receive).
`merge-base 90af5a7 HEAD` returns the full checkpoint hash recorded above,
establishing ancestry, not a claim of checkpoint correctness. No fetch, merge,
rebase, reset, checkout or index operation was used.

Current behavior inspection uses master `1b7498c217`; this is NOT an NV-02
baseline amendment. A read-only oracle-to-master diff found no changes in the
selected roots `fsfloaterim.cpp`, `fsfloaterimcontainer.cpp`,
`fsfloatercontacts.cpp`, `llprogressview.cpp`, `llviewermenu.cpp`,
`llfloater.cpp` or `llfocusmgr.cpp`. In the inspected `llstartup.cpp` diff,
the change selects the SoLoud audio engine under `LL_SOLOUD`; it does not supply
the missing native connected UI. This bounded comparison does not certify all
transitive callbacks, assets, configuration branches or the other 18 commits.

Direct inspection of current-master `FSFloaterIM::setVisible`, `getVisible`,
`toggle` and `updateMessages` confirms: inactive/minimized hosted conversations
are not visible; background opening must not steal focus; ordinary toggling can
hide without destroying draft/scroll state; message fetch passes `hasFocus()`
to `LLIMModel::getMessages` to avoid resetting unread for unfocused histories.
Toast-channel updates, console-session membership and rich-history processing
are outgoing obligations, not approved native service calls.

### WIP findings and unresolved risks

Line anchors refer to the reviewed working files, not immutable commit lines.
No finding below is presented as an executed test result.

| Priority / status | Evidence and consequence | Smallest discriminating check / owning repair |
| --- | --- | --- |
| P1, verified test-contract mismatch introduced by lifecycle WIP | [Session fixture 209](../../indra/llvulkan/tests/llvkwidgettree_test.cpp#L182) still requires a queued Cancel progress notice after login and later a progress modal; [refreshSession](../../indra/llvulkan/llvkdialogs.cpp#L542) now returns for Pending without enqueuing it. The fixture's first progress assertion cannot be satisfied on the changed path. | Adapt this existing fixture to the recovered Quit/lifecycle contract, bind a controlled quit request, and retain all delayed cancellation/retry/agreement/MFA and cleanup assertions. Do not merely delete the assertions or call an unrun fixture passing. |
| P1, verified retained parity defect, partially improved by navigation WIP | [selectConversation](../../indra/llvulkan/llvkcommunications.cpp#L200) clears unread from a local page visibility bit; [refreshCommunications](../../indra/llvulkan/llvkcommunications.cpp#L219) uses the same test for incoming selected IM/group messages. A selected page with keyboard focus in another dialog does not accumulate unread. Current GL explicitly uses focus for acknowledgment. The new hidden-tab accounting fixes only one case. | Existing fixture: show selected IM, focus unrelated dialog, inject a message, prepare twice, assert unread persists. Then focus its actual history/composer and acknowledge according to the source policy. Repeat hidden/minimized host and inactive-window cases after their exact GL focus closure is recovered; app focus and scrolling are not assumed identical to `hasFocus()`. |
| P1, verified inherited composition defect | The duplicate modal pass described above persists whenever no lifecycle screen is present. | Count emitted primitives before measuring translucent pixel output; repair only native composition. |
| P2, verified stale-policy path in WIP | [refreshSession](../../indra/llvulkan/llvkdialogs.cpp#L459) deduplicates equal session snapshots before calling `refreshLifecycleScreen`; [lifecycle construction](../../indra/llvulkan/llvkviewerui.cpp#L511) reads `FSDisableLoginScreens`/`FSDisableLogoutScreens` only there. A setting change during an otherwise unchanged attempt cannot change the selected presentation on ordinary refresh. | Hold the owner snapshot constant, change each setting, refresh, assert the source-defined live policy. If source policy is restart/next-transition-only, record that contract instead; source timing must determine whether this is a user-visible defect. |
| P2, verified architecture boundary issue, not proven visual corruption | [appendLifecycleScreen](../../indra/llvulkan/llvkviewerui.cpp#L609) mutates layout/visibility and filters commands after base paint preparation; `preparePaint` also drains received messages. Calling preparation for a second view/capture can therefore advance service/presentation state. | Separate once-per-owner-tick service drain and CPU preparation from read-only packet consumption in the existing UI owner. Two consumers of the same prepared version must not drain messages, acknowledge unread or advance fade twice. No new renderer dispatch layer is needed. |
| Open control-flow risk, not a demonstrated bypass | [Window input](../../indra/llvulkan/llvkwindowmgr.cpp#L436) adds a lifecycle-only early route, bypassed for modal notices. Normal focus/capture-loss handling remains later in the function; browser hit-testing is explicitly blocked. This does not prove all keyboard, IME, popup, cursor and OS-capture sequences are correct. | Window fixture: enter lifecycle during drag/composition, open/close agreement or unrelated modal, change DPI, lose/regain focus, then cancel/retry. Assert one recipient per event, no hidden editor/menu/browser command, no stuck OS capture or stale surrogate/preedit, and source-correct cursor restoration. |
| Open epoch risk, not a demonstrated cross-account leak | Context loss sets `mCommunicationContext` empty without clearing transcripts; subsequent context comparison only clears when both old and new contexts exist. Ordinary PreLogin teardown clears state, but a transient unavailable context or future region crossing needs an explicit policy. | Inject context A, unavailable, then B without ordinary PreLogin; prove retained data is permitted and correctly retagged or cleared. Reject late A results. Distinguish account/session transcript lifetime from region-only facts. |

No new direct GL visual call appears in the five-file diff. That is a local
inspection result, NOT a GL-free closure proof. Native widget construction,
font measurement, image decoding and input remain native responsibilities even
when CPU-only. Existing link debt (including the window target's viewer texture
cache source) must not be expanded or relabelled independent merely because it
already builds. The unavailable Friends/Contact Sets labels, plain transcripts,
aggregate session panes, suppressed lifecycle media/progress and missing shell
are explicit incomplete behavior, never acceptance evidence.

### NV-00 integration design

Status for these families is **local source inspected / outgoing edges open /
design proposed** unless evidence above says otherwise. No family is promoted
to contract-closed, runtime-validated or parity-validated by this review.

| Family and source contract (question 1) | Native result and CPU work (question 2) | Smallest coherent owner/design and check (question 3) |
| --- | --- | --- |
| Lifecycle: current GL startup/progress/application shutdown and their settings, agreement/challenge, failure, transition and teardown callbacks. Progress presentation must not decide transport ownership. Full versus compact source effects remain shell-agent audit inputs. | `LLVKSessionOwner` owns transitions, tags, admission and reverse-lifetime service drain; `LLVKViewerUi` observes snapshots and independently prepares lifecycle UI; window manager translates OS input and requests shutdown. Authentication, services-ready and world-visible remain distinct. | Complete `refreshSession`/`refreshLifecycleScreen` rather than add a second lifecycle state machine. Keep presentation revision/time distinct from transport tag. Use fixture 209 for stale actions, repeated snapshots, modal survival and cleanup failure; exact lifecycle effects remain a separate gate. |
| Commands: current `init_menus`, registered actions/predicates, `LLFloaterReg` routes and dependent-dialog callbacks select behavior, not merely labels. Menu-agent audit owns the full callback inventory. | `LLVKMenu::bindItem`/`bindPredicate`, native factory callbacks and explicit UI methods resolve native actions; audited transport/application adapters perform nonvisual operations. Enabled/checked state derives from real snapshots, never fabricated facts. | Keep named routes in `LLVKViewerUi` and production bindings in startup/window service owners. Bind a shared native handler to menu, shortcut and toolbar entry points, without calling GL registries or creating a speculative global command bus. Test each route with the same valid and stale command identities. |
| Hosted communication: Firestorm container/Contacts/IM owners determine locked tabs, selected session, close versus hide/leave, focus, unread and dependent floaters. | Existing `LLVKFloater`, widget tabs, conversation maps and `CommunicationServices` produce native windows, session selection and data. Layout, sorting, label/icon state, rich history, timers and input are independent CPU visual work. | Add only the missing host/session records to the existing UI owner. Reuse tab validation and floater close/dependent hooks; use stable service/session IDs, not tab indices as identity. Prove hide/reopen preserves draft/scroll, close-host does not leave all sessions, explicit Leave Chat does not mutate membership, and tear-off/redock preserves ownership. |
| Shell/status/location/favorites: source draw-time refresh, focus-protected location editing, timed status updates and delayed favorites persistence have real data and callback dependencies. Shell-agent audit owns exact geometry/state closure. | Native UI preparation consumes versioned account/agent/region/parcel/status snapshots. Native code owns clipping, layout, text/images, hover and animation; independently audited adapters supply facts and persist nonvisual data. | Attach shell nodes to the existing root with an explicit remaining content rectangle. Replace the communications full-window shape assignment, not a parallel fullscreen UI. Do not add fake balance, parcel permissions, navigation history or favorite buttons. Test resize/DPI and missing/loading/failed/ready data before enabling the corresponding route. |
| Presentation/resources: GL UI traversal combines inherited alpha, nested clips, painter order, fonts/images and preparation effects; GPU lifetime is independent of widget lifetime. | Native `LLVKWidgetPaint` -> `LLVKWidgetGpu` -> `LLVKUiPacket` -> `LLVKContext` consumes prepared commands with independently resolved fonts/images and retained resource versions. UI remains after world postprocessing, outside exposure/fog/DoF. | Finish existing packet/publication ownership; no shared low-level RHI, GL callbacks or GL-produced frame. Freeze one preparation version for all consumers, then validate command order and all-submission image retention across close/reopen/resize. |

### Integration boundaries and lifetimes

**Admission and command epochs.** Existing
[session tags](../../indra/llvulkan/llvksessionowner.h#L27) already carry
generation, request and region epoch. Preserve those rather than introduce a
competing global session counter. An asynchronous action captures owner identity,
the relevant tag, immutable target ID and operation/query revision. At execution
and completion, recheck current state, owner, target existence and permissions.
Do not re-resolve a delayed action through whichever row happens to be selected.
An application-only Preferences/About route need not be invalidated by region
crossing; an account/group/parcel mutation must be gated by its actual lifetime.
Use a small UI/menu incarnation counter only for callbacks whose tree/model can
be replaced without changing the session tag. Detached menu views share
[the menu model](../../indra/llvulkan/llvkmenu.h#L66), so dismiss/retire its
detached views before replacement; an obsolete view must not target a new model
by recycled item index. Menu agent supplies the audited action/predicate matrix.

**Snapshots and adapters.** Keep services UI-free: immutable bounded facts,
explicit unavailable/loading/ready/failed states, sequence/version and tag;
return commands/results, never a `LLView*`, GL font/image, raw widget pointer or
GL-coupled callback. [Startup bindings](../../indra/llvulkan/llvkstartup.cpp#L376)
already connect the narrow communication API to `LLVKLoginTransport` context,
receive, resident search, groups, sends and moderation. Preserve that transport
owner. Extend a concrete missing service only after its producer, cancellation,
callbacks, settings and failure path are audited. No blanket approval follows
for `LLAvatarActions`, `LLGroupActions`, inventory/parcel globals or Contact Sets.
The earlier observation about a transport Friends accessor does not establish a
current UI binding: [CommunicationServices](../../indra/llvulkan/llvkviewerui.h#L28)
still has none. Keep authentication secrets out of UI snapshots and diagnostics.

**Focus, capture and layers.** The native tree remains the single focus/capture
owner; window code synchronizes OS capture and application activation, not a
second widget focus map. Use existing tab selection-generation validation and
floater close/dependent callbacks. On entering a blocking layer, dismiss menus,
release native and OS capture, resolve/cancel IME state according to source,
stop typing, and retain a valid, lifetime-tagged restoration target. Restore only
an existing effectively visible/enabled target; an unrelated modal survives a
session transition without a forced composer focus. Close/minimize/hide and
destroy are distinct operations. Explicit composition must emit each primitive
once in source painter order: ordinary content and floaters, eligible menus and
popups, lifecycle blocking screen when active, then its permitted modal notice.
Tooltip/menu eligibility and any exceptional dependent ordering are source
contracts, not a blanket new z-order rule. Hit testing must use the same active
layer policy as painting. Account-owned dialogs close/invalidate on logout;
application-owned dialogs survive only where the source permits.

**Preparation and resource versions.** Drain service events once on the UI owner
thread, reconcile commands/snapshots, advance source-defined timers once, compute
layout/visibility/focus and then prepare paint. Freeze paint inputs (including
font/image versions, scale, clip, alpha, order and clock) until all consumers
finish; do not let capture or packet recording invoke UI/service callbacks.
[Image publication](../../indra/llvulkan/llvkimagepublication.cpp#L19) retains
current/uploading versions, supports invalidation, polls upload completion and
has a queue-ordered publication path. That path requires the same compatible
queue and upload-before-consumer proof; it is not permission to sample arbitrary
unfinished uploads. Widget GPU preparation has separate image/text/stream caches;
cache eviction time is not GPU completion. Packet draws retain shared immutable
images, and [frame acquisition](../../indra/llvulkan/llvkcontext.cpp#L1202)
clears that frame's retained images only after its fence wait. Every submission
using a version must retain it independently, including previews/capture.
Descriptors and mapped vertex storage need the same all-use rule. Preserve
noncoherent flush/invalidate, layout barriers, format/row orientation, sampler
identity and WSI failure handling; none are newly runtime-qualified here.

**Teardown.** Invalidate command/result admission before destroying dependent
widgets; stop typing where the old transport still permits it; detach observer,
picker, browser, timer and settings callbacks; retire region then session data;
clear account histories/selections/drafts according to the recovered persistence
contract. Preserve actionable Pending/failed-cleanup state and idempotent retry.
For whole-process shutdown, reuse
[ApplicationServices::detach/retire](../../indra/llvulkan/llvkwindowmgr.cpp#L782)
and [VisualServices::prepareShutdown](../../indra/llvulkan/llvkwindowmgr.cpp#L914):
the former detaches callbacks and closes browser/voice/audio services; the latter
disconnects window access and waits for device work before visual destruction.
The bounded shutdown idle wait is not a streaming-upload strategy. Service
failure/partial construction must not permit callbacks to recreate UI or free
resources still used by GPU submissions. Logout-to-login must not destroy
application services needed by the next login.

### Additional local evidence and handoff limits

Current-master host inspection confirms `FSFloaterIMContainer::postBuild`
deliberately avoids the base close-all hookup; `onClose(false)` does not close
every session, whereas `onClose(true)` saves open IMs and closes session floaters.
`initTabs` and `addFloater` preserve Contacts/Nearby order and torn-off state.
The destructor disconnects message and session observation. These are separate
host, session and application lifetimes, not one visibility boolean.
Current `LLProgressView::handleKeyHere` consumes keys except Ctrl-Q;
`onCancelButtonClicked` requests application quit before `STATE_STARTED` but
cancels teleport afterward. Do not reuse login Quit as a future teleport Cancel.
The full/mini progress percentages, startup image, logos, media and fade-to-world
branches remain open; hiding their controls does not satisfy the source contract.

An additional oracle-to-master comparison found no changes in the selected
status/navigation/location/favorites owners, floater registrations or the three
registered default-English Firestorm communications declarations. Historical
root observations above therefore remain applicable to those selected files;
this is not a recursive asset/callback equivalence claim. Coordinate integration
with the other agents' menu and shell audits, preserving their ownership; this
review neither edits their drafts nor declares their open work complete.

The local adapter audit reached
[transport command bodies](../../indra/llvulkan/llvklogintransport.cpp#L511):
`joinGroup` checks context/membership/power, versions pending acceptance and sets
a deadline; `leaveGroup` changes conversation state, not membership;
`sendGroup` checks Joined/power/server text mute. Resident search cancels the
previous request, bounds rows and validates returned identities.
[chatContext and message publication](../../indra/llvulkan/llvklogintransport.cpp#L636)
require Online/bootstrap/matching tag and drain circuit/event messages; local
and direct sends preserve volume, typing and offline-buddy policy. Friends data
is indeed present at `friends(tag)` but still lacks a UI binding. These inspected
bodies are nonvisual; HTTP/circuit construction, workers, cancellation, global
callbacks and complete error closure require the service owner's audit before
extending them. Reuse the existing narrow adapters, not GL action wrappers.

Font/image ownership already exists in
[LLVKFontRegistry](../../indra/llvulkan/llvkfontregistry.h) (named requests,
fallback configuration, DPI/display scale) and
[LLVKSkinImages](../../indra/llvulkan/llvkskinimages.h) (declaration metadata,
immutable pixels and budgets). Resolve the original skin declarations through
these owners; do not substitute a visually similar font/icon or call GL text
measurement. FreeType and decoder libraries require their own audited closure;
their API-independent nature does not approve the viewer's GL wrappers.
[recordUiPacket](../../indra/llvulkan/llvkcontext.cpp#L1340) checks same-device/
queue image compatibility, retains draw images in the current frame, uses
per-frame mapped vertices and flushes the written range. This strengthens the
local retention evidence, not an all-path GPU qualification claim.

### Ordered implementation checkpoints

Dependencies are behavioral gates, not instructions to merge master or to
restore reverted experiments. Each checkpoint requires its source contract,
implementation, production binding and discriminating checks before acceptance.
The first slice is explicitly smaller than connected-shell completion.

1. **C0: Repair lifecycle composition and fixture contract.** Stay within the
  existing UI/notice/session owners. Add single-emission modal assertions for
  lifecycle off/on, adapt fixture 209 from removed Cancel notices to native
  lifecycle Quit, and preserve delayed callback, cleanup-failure, MFA/agreement
  and unrelated-modal assertions. Use controlled clocks and transport; no
  authentication or viewer launch is needed for this first check. Review the
  source Quit behavior before changing fixture expectations. Exit: updated
  fixture and widget gate pass, native notice primitives occur once, and a
  stale quit callback cannot affect a newer attempt. This is testable repair,
  not lifecycle animation or shell parity completion.
2. **C1: Establish connected shell/layer ownership.** Depends on C0 and the shell
  agent's closed construction/layout/input contracts. Replace the full-window
  communications takeover with source-defined chrome plus an explicit client
  area, using native nodes/floaters under the current root. Add state-transition
  preparation before paint and the effective-focus acknowledgment correction.
  Exit: login -> progress -> connected -> disconnect -> login and retry with an
  unrelated modal preserve layers, focus/capture, resize/DPI and account
  isolation. Exact shell geometry, fonts, assets, alpha and transitions must
  pass; absent parcel/status/favorites services leave their workflows open.
3. **C2: Wire connected menu and named native routes.** Depends on C1's lifecycle
  and layer policy plus the menu agent's action/predicate/shortcut inventory.
  Replace the login menu at the defined transition, bind current native
  Preferences/About/communications routes, then each audited service action.
  Retire popups and detached models on replacement. Exit: mouse, keyboard,
  accelerator and toolbar routes share command admission, current checks and
  target identity; old menu callbacks after reconnect do nothing; menu visuals,
  effects and nested-dialog ordering match exactly. Disabled/unbound items are
  containment, not completion of the connected menu inventory.
4. **C3: Complete Conversations host and service dialogs vertically.** Depends
  on C2 for production entry points and the corresponding service contracts.
  First complete Nearby/IM/group host ownership, per-session tabs, draft/scroll,
  focused unread, hide/reopen, lock/order, tear-off/redock and explicit leave.
  Then bind real Friends/presence and Contact Sets independently, with resolved
  names/rights/readiness and cancellation. Existing rosters do not prove
  permission-changing actions. Exit per dialog: actual service data and actions,
  source error/loading behavior, exact rich text/icons/feedback and modal
  descendants, reconnect/late-result tests and teardown. Profile/inspectors,
  inventory and notification consumers follow their audited data dependencies;
  do not fabricate these services to populate a window.
5. **C4: Complete remaining shell services and qualify resources.** Status needs
  real selected-backend timing and agent facts; location needs region/parcel/
  maturity/edit-focus and navigation history; favorites needs inventory/
  landmark/persistence; world-facing controls need native view/world services.
  Add each producer with its native consumer, failures and exact interaction
  contract. Run applicable resource/window gates when image, glyph, browser,
  packet or OS lifecycle behavior changes. Exit: captured temporal sequences,
  input and nested dialogs pass the accepted exact UI contract; all-use resource
  retirement, device/WSI failure and GL-free execution have recorded evidence.

Dependency edges: C0 -> C1 -> C2 -> C3; service contracts -> each C3/C4 consumer;
font/image/widget correctness -> all visual checkpoints; packet/publication
qualification -> any changed GPU consumer. Shell/menu source audits can proceed
independently, but cannot bypass these integration gates. World rendering,
economy, purchases, inventory mutations and voice modes are not implicitly
authorized by a shell button. Missing required behavior stays open; negotiate
scope explicitly rather than silently narrowing correctness.

### Existing gates, not executed

Definitions were read in [native CMake](../../indra/llvulkan/CMakeLists.txt),
[integration-test macro](../../indra/cmake/LLAddBuildTest.cmake#L249),
[test command setup](../../indra/cmake/LLTestCommand.cmake) and its
[environment runner](../../indra/cmake/run_build_test.py). The integration macro
runs tests POST_BUILD with staged library paths; it does not register those
tests with CTest. The session-owner executable separately has `add_test`.
A no-op up-to-date build is not fresh test evidence.

Use the selected native-sl-login source/build configuration, RelWithDebInfo,
the documented convenience environment and `LL_BUILD`, and serialize gates.
No build directory is present at this worktree root in this inspection. Before
implementation validation, identify/configure an isolated build whose
`CMAKE_HOME_DIRECTORY` is this worktree's `indra`; do not repoint a concurrent
agent's build. The workspace tasks named Native Vulkan Widget/Window/GPU/Browser
Validation and Native Viewer Link Validation hard-code the parent worktree's
build directory, so they are not branch evidence unchanged. Their use of
`/p:BuildProjectReferences=false` is suitable only after matching dependencies
are already current. This review neither changes tasks nor runs them.

| Changed slice | Smallest existing target / fixture | Preconditions and evidence needed |
| --- | --- | --- |
| C0 and CPU connected UI/commands | `INTEGRATION_TEST_llvkwidgettree`; [fixture 209](../../indra/llvulkan/tests/llvkwidgettree_test.cpp#L180), group `llvkwidgettree` | `LL_TESTS=ON`, native font/skin fixtures. Harness accepts `--group=llvkwidgettree --test=209` for a focused rerun of the correctly staged executable; full target gate still required after edits. Keep wrapper library paths. This is not a request to execute it in this review. |
| Session transitions/retirement | `INTEGRATION_TEST_llvksessionowner` | Standalone owner tests cover cancellation, stale replies and lifetime cleanup; also an implicit dependency of `llvkwidgets`. Run explicitly if that owner changes. |
| Transport/service commands | `INTEGRATION_TEST_llvkloginprotocol` | Windows + `LL_TESTS`; existing protocol/transport fixtures and message-template definition. Add missing service failure/epoch cases here instead of another harness. |
| Window routing/lifecycle WIP compilation | `llvkwindowmgr`, then `INTEGRATION_TEST_llvkwindowmgr` when runtime execution is authorized | Windows; runtime target requires BOTH `LL_VULKAN_BROWSER_TESTS=ON` and `LL_VULKAN_GPU_TESTS=ON`, packaged CEF helpers, native shaders, and its texture-cache-startup dependency. Fixture already exercises tree/browser/Vulkan in one window; add blocking-layer/IME/capture sequences. |
| Shader/packet/image lifetime changes | `INTEGRATION_TEST_llvkcontext`; add `INTEGRATION_TEST_llvkglyphupload` for glyph/upload changes | GPU coverage requires `LL_VULKAN_GPU_TESTS=ON`; report actual validation-layer loading. Existing packet tests reject out-of-image clips and repeat recording; GPU fixture covers owned images/publication invalidation. Do not infer GPU execution from a CPU-only target success. |
| Font metrics/fallback changes | `INTEGRATION_TEST_llvkfontface` | Required packaged font fixtures, source font/scale settings; widget tests remain the text/layout consumer gate. |
| Browser ownership/publication changes | `INTEGRATION_TEST_llvkbrowser` | Windows + `LL_VULKAN_BROWSER_TESTS=ON`, packaged helpers. Do not run for a documentation-only change. |
| Production linkage after native owner edits | `vulkanstorm-bin` | Rebuild changed `llvkwidgets`/`llvkwindowmgr` dependencies first. `llvulkan` alone is not the smallest relevant compile gate for these files; they belong to other targets. |

Reuse nearby widget fixtures for tab visibility/commit ordering (at
[selection fixture](../../indra/llvulkan/tests/llvkwidgettree_test.cpp#L5029)),
focus restoration, scrolling, image declarations and notice/error behavior.
Resource checks already exist in
[packet/context tests](../../indra/llvulkan/tests/llvkcontext_test.cpp#L203) and
[glyph publication tests](../../indra/llvulkan/tests/llvkglyphupload_test.cpp#L408).
These are test locations, not claims that the WIP passes them. Do not rerun
unrelated passed operator evidence merely because this architecture review exists.

### Acceptance and required PR record

UI must preserve source visuals and effects exactly: color, texture, alpha and
blending, clipping, geometry, text/icons, painter order, animation/transitions,
interaction feedback and nested dialogs. Build success, marker-only evidence,
an approximate screenshot, plain replacement widgets or disabled controls cannot
close a milestone. Any deviation is failure; unsupported/unverified paths remain
open. Retain oracle `59108e15a1`, inputs and tolerances. A real current-master
contract difference requires an explicit NV-02/NV-18 baseline decision with
before/after evidence, not an unannounced substitution.

Later authorized runtime qualification must record deterministic input/time
sequences and reference/native captures, selected device/driver, skin/language,
effective settings/fonts, physical/logical dimensions and scale. Include hover,
pressed/focus/unread states, nested dialogs, resizes, transitions and failure
recovery, not just a settled frame. Notify the operator before a full viewer
launch and allow manual login; require actual `STATE_STARTED`, 75 uninterrupted
seconds settling, graceful `WM_CLOSE`, exit zero and `Goodbye!`. Never force-stop.
No viewer or fixture is launched by this review.

| Required PR field | Record for this review / obligation for implementation |
| --- | --- |
| Contract | NV-00/01/02/03/04/09/10/11/12/13/14/15/16/17/18 as applicable to native UI, session boundaries and resources; phase-3 consumers and phase-4 shell remain distinct. |
| Reference | Current source inspection `1b7498c217`, unchanged oracle `59108e15a1`, implementation checkpoint `90af5a7`, branch tip `005a4ae869` plus enumerated WIP; default English hosted-communications configuration is bounded, not all skins/modes. No device/runtime capture observed. |
| Data flow | Proposed tagged service facts -> owner-thread reconciliation -> native CPU preparation -> immutable paint/resource versions -> packet. No implementation or ABI changed here; implementation PR must specify coordinate/color/alpha/depth and producer/consumer changes. |
| GPU safety | N/A to this documentation edit. Local retention/publication evidence and remaining proof obligations recorded above; implementation must prove barriers, noncoherent handling, descriptors, all-use retirement and failure behavior for changed paths. |
| Validation | Required instructions/invariants/roadmap/approved README read; read-only ancestry/diff/source/CMake/task/script inspection; document links, anchors and hygiene checked. No build, test execution, runtime validation or measured parity in this review. |
| Limits | WIP fixture mismatch and retained unread/composition defects block acceptance. Missing shell/menu/service surfaces and incomplete lifecycle effects remain open. Adapter transitive closure and GPU qualification are not certified. |
| Change class | Documentation-only architecture/readiness review; proposed parity-directed repairs and integration, no GL change or baseline/tolerance amendment. |

**Readiness:** existing native owners are suitable for a bounded C0 repair;
the branch is not ready to claim complete connected UI or exact parity. The
next concrete implementation check is fixture 209 with single-emission notice
and stale lifecycle-Quit cases, followed by the widget target gate on a verified
branch build. Preserve all historical provenance above and record future actual
results separately rather than converting this plan into retrospective success.

## 2026-09-19 engineering: independent communication windows and session tabs

Pre-edit NV-00 record. Scope is communications implementation, communication-only
UI declarations/tests, and this additive record. Preserve prior five-file WIP;
startup, transport, GL, build files and other documents are read-only. Current
source is master `1b7498c217`; oracle `59108e15a1` and checkpoint `90af5a7`
retain their separate roles. The source surveys and invariants above apply.

1. **GL contract:** `FSFloaterIMContainer::initTabs/onClose/addFloater` retains
  Contacts then Nearby, followed by distinct sessions; ordinary host close hides
  without leaving sessions. `FSFloaterIM::updateMessages/getVisible/setVisible`
  distinguishes selected, hosted, minimized and focused states: visibility alone
  does not acknowledge unread. Contacts has Friends/Groups/Contact Sets, and
  group Chat requires actual membership and join power. The registered default
  container is 396x390 with 115x20 left tabs; People is a separate resizable
  400x570 floater. Rich history, tear-off, rosters, dependent actions, persistence
  and full effects retain the surveys' open obligations.
2. **Native design:** existing `LLVKFloater` owns independent chrome/open/hide;
  the native widget tree owns selected pages and input focus. UUID-keyed direct
  and group records retain distinct native pages, transcripts, drafts and typing
  deadlines. Only tagged existing CommunicationServices provide residents,
  memberships, messages and commands. Friends/Contact Sets/nearby People are
  never inferred from chat or resident-search records. All new work is CPU
  visual ownership; no GL constructor/layout/font callback is reused.
3. **Smallest implementation:** extend the current communication owner, not a
  new service or fullscreen shell. Expose named show/hide routes and a floater
  enumeration for main integration into its existing input/foreground/close
  loops. Keep unsupported People lists explicit; resident search can remain a
  separately labelled service consumer, never a fabricated Nearby/Friends list.
  Session pages stay alive on host hide to preserve editor state. On context
  loss, invalidate the old tagged UI data so a later context cannot inherit it.

Cheap checks: editor diagnostics immediately after every edit; existing widget
fixtures extended for two independent IM/group pages, draft isolation, visible
but unfocused receive, focus acknowledgment, hide/reopen without leave, stale
query/context rejection, and missing-service states. No builds or executable
tests run in this agent slice; main must serialize them after source integration.
No new files are planned. Complete visual/effects parity remains unverified.

Affected invariants: NV-00/01/02/03/12/15/17. Data flow retains existing protocol
signatures/tags and native paint ABI. GPU uploads/barriers/descriptors/retirement
are unchanged, not newly qualified. Change class: bounded parity-directed UI
progression, not baseline amendment, full Contacts/People or rich-chat completion.

Pre-edit refinement: the retained resident-search service is not a conversation
and must not become a permanent third host tab. The current source declaration
`floater_avatar_picker.xml` specifies an independent cascading Choose Resident
floater, 500x350, minimum 400x200, legacy header 18. Existing tagged native search
and result-selection callbacks move into that native owner, with query IDs and
context invalidation unchanged. Public `showResidentSearch` opens it without
inventing friendship/presence. Picker Friends/Nearby/key modes, original two-column
result-list behavior and dependent picker semantics remain unsupported. Check:
the host starts with exactly Contacts and Nearby; searching does not insert a
session until an actual UUID/result is selected; stale queries create no rows.

People Q1 refinement: current `LLPanelPeople::onOpen` selects a named tab;
`onChatButtonClicked` uses the selected real group ID to start IM, and
`onImButtonClicked` separates one-to-one from conference selection. Native
`showPeople(error, tab)` selects its independently owned tab; the Groups consumer
uses existing membership/power snapshots and the same explicit group-chat route
as Contacts. No GL action callback is reused. Multi-selection, profile/group-info,
radar, recent-resident and Contact Sets actions remain open and unavailable.

### Bounded resumption: context admission (2026-09-19)

Pre-edit NV-00: preserve native HEAD `005a4ae869` and existing WIP. Current
read-only source `1b7498c217` supplies behavior; oracle `59108e15a1` is unchanged.
The inherited host/focus/source contracts above apply. The GL IM model retrieves
messages by session identity (`FSFloaterIM::updateMessages`) and gates unread by
focus; it does not authorize mixing one session's identity with another's data.
The native `LLVKLoginTransport::chatContext/takeMessages` boundary requires an
Online bootstrap and exact owner tag. Startup binds those existing adapters.
No GL visual function or new network service is reused or introduced.

Native design and smallest repair: `refreshCommunications` must validate the
returned context tag against its owner snapshot before accepting identity,
constructing windows or polling account data. Treat a mismatch as unavailable,
using existing `clearCommunications` to invalidate tree callbacks and account
state. This is CPU-only admission/teardown, not a GPU or paint ABI change.
Hypothesis: an injected mismatched context currently recreates windows and
drains receive despite belonging to a different owner epoch. Discriminating
check: fixture 219 supplies that context, asserts no receive drain, no surviving
host/session page, rejection of a retained callback, and clean recovery when
the matching context returns. Editor diagnostics follow the edit immediately;
fixture execution is prohibited in this slice.

Edit authority is only communications, this document and fixture 210/219.
Fixture 210 currently tests language selection, so remains untouched; the
connected lifecycle fixture is 209 and is outside this agent's ownership.
Header, services, other tests, GL source and pinned oracle remain read-only.

### Sequential Conversations resumption: unread publication (2026-09-19)

Pre-edit NV-00 record: native HEAD `005a4ae869`, existing dirty communications
implementation retained; current read-only GL target `1b7498c217`, pinned oracle
`59108e15a1`, implementation checkpoint `90af5a7`. Affected NV-00/01/02/12/17.

1. Source contract: `FSFloaterIM::updateMessages` calls
  `LLIMModel::getMessages(..., hasFocus())`; `getMessagesSilently` retrieves only
  the addressed session's history. `sendNoUnreadMessages` rejects absent sessions,
  clears both unread counters, then emits `mNoUnreadMsgsSignal`. Thus consumers
  observe the acknowledged state, not the previous count. Signal subscribers,
  rich history and all other focus effects remain under the earlier open audit;
  neither GL visual owners nor the IM model are reused by this repair.
2. Native result: existing CPU-owned `communicationFocused` and per-session
  `Conversation::unread` remain authoritative. Publish the Contacts group list
  after `publish` acknowledges focused pages and updates their tab labels.
  Background, hidden, minimized and inactive-session policies are unchanged.
  Service signatures, GPU resources, assets and paint ABI are unchanged.
3. Smallest repair: relocate the existing group-list publication block within
  `refreshCommunications`; do not add a second unread cache or another refresh.
  Hypothesis: the old order leaves a stale Contacts count for one preparation
  after focus acknowledgment. Fixture 219 focuses an existing group composer
  directly, avoiding the extra refreshes in `showGroupConversation`, then checks
  both labels after one `preparePaint`. A second incoming background group must
  retain its count in both consumers. This is a discrete state-consistency check,
  not measured visual parity or an assertion that the combo matches the GL list.

Only `refreshCommunications`, communications fixture 219 and this record change
in this resumption. No new or changed header declarations or data members.
Required shell bindings remain the pre-existing `showConversations(error)`,
`showContacts(tab,error)`, `showNearbyChat(error)`, `showPeople(error,tab)`,
`showResidentSearch(error)`, `showDirectConversation(recipient,error)` and
`showGroupConversation(group,error)` routes. Enumerate non-null pointers from
`communicationFloaters()` for native floater integration; they are borrowed and
must not survive refresh/context teardown. Forward actual OS activation through
`setCommunicationApplicationFocused(bool)`. Main integration owns those bindings;
this resumption changes no shell/menu/lifecycle members or bindings.

Friends/presence, Contact Sets, nearby/recent People, original rich chat/assets,
dependent actions and live send/receive remain the earlier explicit gaps. No fake
rows or success states were added. Existing transport adapters are retained;
this state-publication repair adds no nonvisual dependency or service API.
Validation results follow separately; no build/configure/install/viewer launch
or Git write is authorized for this resumption.

Service handoff (existing signatures, not new declarations): with
`Tag = LLVKSessionOwner::Tag` and `Chat = LLVKChatProtocol`, main integration
must retain these `CommunicationServices` callable contracts:

| Member | Callable signature |
|---|---|
| `context` | `std::optional<Chat::Context>(Tag)` |
| `receive` | `std::vector<Chat::Message>(Tag)` |
| `search` / `searchResult` | `bool(Tag, std::uint64_t, std::string, std::string&)` / `std::optional<Chat::SearchResult>(Tag)` |
| `groups` | `std::vector<Chat::Group>(Tag)` |
| `joinGroup` / `leaveGroup` | `bool(Tag, const LLUUID&, std::string&)` |
| `group` | `bool(Tag, const LLUUID&, const std::string&, std::string&)` |
| `moderateGroup` | `bool(Tag, const LLUUID&, const LLUUID&, bool, std::string&)` |
| `local` | `bool(Tag, const std::string&, std::uint8_t, std::string&)` |
| `direct` | `bool(Tag, const LLUUID&, const std::string&, bool, bool, std::string&)` |
| `diagnostic` | `void(const char*)` |

The next Friends slice requires an audited UI snapshot binding compatible with
the existing `std::vector<LLVKChatProtocol::Friend>
LLVKLoginTransport::friends(LLVKSessionOwner::Tag) const`; candidate adapter type
is `std::function<std::vector<LLVKChatProtocol::Friend>(LLVKSessionOwner::Tag)>`.
It is **not added or bound here**. Its records contain UUID, rights and
`online`/`presenceReceived`, not resident display names. Unknown presence cannot
be reported as confirmed offline. Name resolution, friendship actions, Contact
Sets persistence and nearby/recent People require separately audited producer
contracts; no supported adapter signature for those services is established by
this change. Resident search results and IM participants are not substitutes.

Actual checks: immediately after implementation, editor diagnostics reported no
errors for the implementation, fixture file and this document. Scoped
`git diff --check` passed for communications, the retained header and widget
tests. The worktree has no `build-vc170-64/CMakeCache.txt`; fixture 219 was not
compiled or executed. No production service, runtime, visual/effects, GPU or
disconnect/reconnect acceptance was performed. Existing fixture coverage is
retained but is not newly passed evidence. No header edits to reconcile for
agent 3; menu fixtures 194/197, lifecycle fixture 209 and language fixture 210
are untouched by this resumption.