# Native top-menu transposition

Date: 2026-09-19. Status: source survey and engineering design, not implementation
or runtime acceptance. Scope: the connected Firestorm top menu and its ownership
boundaries; context menus are identified where they share construction, input or
commands. No claim that every leaf callback has been audited.

## 1. Authority and source identity

Follow [native invariants](native_vulkan_invariants.md),
[native viewer roadmap](native_viewer_roadmap.md),
[native instructions](../../.github/instructions/native-vulkan.instructions.md),
[native-tree instructions](../../indra/llvulkan/AGENTS.md), and the
[approved historical report](reverse-engineering/README.md). All five were read;
the invariants and roadmap were read in full.

| Role | Verified source | Meaning |
|---|---|---|
| CURRENT GL target | `C:/Dev/vulkanstorm`, master `1b7498c2172c500e536d2b9c88e42980c860284d` | Current menu source surveyed below; tracked working files were clean when inspected |
| Native implementation | `C:/Dev/vulkanstorm/native-sl-login`, HEAD `005a4ae8693cf9711093a3c44783bff7af605f99` | Implementation to extend, including the explicitly identified uncommitted observations below |
| Historical parity oracle | `C:/Dev/vulkanstorm/worktrees/notification-gl-reference`, detached `59108e15a1f8f94d2da7c674d937d19f5cf9450d` | Verified revision and clean tracked files; unchanged approved oracle, NOT relabelled as current GL |
| Implementation lineage | `90af5a7220f1fec28e3c909c051b6ee7e062f10b` | `git merge-base --is-ancestor 90af5a7 HEAD` in native returned zero; ancestry is not feature qualification |

GL citations below are immutable links to the CURRENT revision. Native citations
are local worktree anchors, not assertions about pristine HEAD. At inspection,
native modified files were `llvkcommunications.cpp`, `llvkdialogs.cpp`,
`llvkviewerui.cpp`, `llvkviewerui.h`, and `llvkwindowmgr.cpp`. Three pre-existing
untracked UI survey documents were also present. None is changed by this work.
Native menu and widget-test observations below are from unchanged tracked files;
UI integration observations explicitly include the current dirty implementation.

Configuration scope: Windows input route; base default-skin English XUI, with
SL/OpenSim, RLVa, QA/admin and skin/language variations recorded as obligations.
No active user's skin, language, restriction state, device, dimensions or runtime
build configuration was inferred from source. Current-target findings do not
upgrade the oracle. Differences against the oracle require a separate reviewed
NV-02 baseline decision and before/after evidence before current-target parity
can be called historical-baseline parity. No such decision is made here.

## 2. Actual menu hierarchy and counts

[init_menus](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L580)
calls `initialize_menus` before constructing any menus. It constructs a runtime
`Popup`, the hidden edit menu, seven ordinary world context menus, seven Firestorm
pie equivalents, the inspect-self gear menu, the connected bar and a separate
login bar. Thus 18 explicit file-based menu constructions occur in this function,
plus the runtime Popup. This is not the total count of menus across the viewer.

- Main declaration: default-skin English `menu_viewer.xml`, not a generic upstream
  Second Life menu. Advanced, Developer and Admin are nested in this declaration,
  not separate main-menu files.
- Separate login declaration: `menu_login.xml`. It is not the connected menu.
- Edit: `menu_edit.xml`, also used as a hidden accelerator source.
- Ordinary context: `menu_avatar_self.xml`, `menu_avatar_other.xml`,
  `menu_object.xml`, `menu_attachment_self.xml`, `menu_attachment_other.xml`,
  `menu_land.xml`, `menu_mute_particle.xml`.
- Firestorm alternatives: the corresponding seven `menu_pie_*.xml` files.
  This survey identifies both families; it does not close pick/selection or
  pie-versus-context selection policy.
- Inspect self: `menu_inspect_self_gear.xml`. Attach/detach HUD/world submenus are
  looked up inside these trees and populated through additional owners.

The actual parenting at
[construction](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L701)
is main view -> `menu_stack` -> `status_bar_container` -> `menu_bar_holder`.
`gMenuHolder` owns the menu-container context; tooltips are explicitly sent above
menus. Missing required holders/items use missing-files reporting and a fatal
diagnostic. The SL beta grid overrides the otherwise skin-defined bar color.

[Layered XML lookup](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/lluictrlfactory.cpp#L176)
uses `findSkinnedFilenames` followed by `LLXMLNode::getLayeredXMLNode`. The table
below is a reproducible BASE declaration inventory, not the fully merged result
for every installed skin/language. Native must consume the same effective asset
layers through independently owned native consumers.

Counts were obtained with PowerShell's XML DOM, excluding comments: descendants
`menu_item_call | menu_item_check` are leaves; `menu` descendants exclude the
top-level branch itself. Separators are counted separately. Empty branches and
declared-but-hidden rows remain in counts. There are **11 roots, 595 leaves,
61 nested menus, 77 separators and 385 distinct `function` attribute values**
across click/check/enable/visible declarations. These are not 385 audited actions.

| XUI name / displayed label | Leaves / nested / separators | Grouped action inventory and visibility |
|---|---:|---|
| [Me / Avatar](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L9) | 67 / 6 / 11 | Account, marketplace/currency/membership; inventory/protected folders/wearables; picks/experiences/profile/outfits; clothing and attachment removal; movement/sit/fly/run; avatar health/animation/reset/rebake/bridge; move/camera controls; ordinary and 360 snapshot; money tracker/pose tools; Preferences/toolbars/HUD/UI; admin request/release; Exit. Normally declared visible; individual permissions/grid/service gates remain. |
| [Communicate / Comm](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L714) | 26 / 2 / 5 | Friends/contacts/contact sets/groups; nearby chat/people/conversations/Omnifilter; gestures; Flickr/Primfeed/Discord; conversation log/voice/block list/on-screen console. Status includes away, unavailable, autorespond, non-friend response, teleport/group/friend rejection and nested ignore-group policy. |
| [World](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L1028) | 55 / 5 / 11 | Nearby avatars/radar/history/places/destinations/events/maps/tracker; stream title; landmark/location/parcel/region/home/land ownership; graphics speed; teleport home; land display/beacons/coordinates/permissions/Advanced toggle; environment presets/shared/pause/import/editors; photo/camera tools and DoF controls; area/sound/animation search, blacklist and avatar rendering/complexity. |
| [BuildTools / Build](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L1756) | 75 / 9 / 9 | Build/tools, part/face selection, link/unlink/focus/zoom; buy/take/copy/duplicate/return/backup/export; script inspection/recompile/reset/run/remove; pathfinding; permission and selection filters, highlights/probes/grid/LOD/postprocessing options; uploads/image/sound/animation/mesh/material/bulk/linkset; local mesh; undo/redo. Requires actual selection/editing/economy services, outside automatic minimal-world scope. |
| [Content](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L2558) | 12 / 0 / 2 | Search, marketplace/L$ market data, scripting/community links, Firestorm blog/social links and message of the day. URL policy, browser and grid-specific destinations are dependencies, not generic success callbacks. |
| [Help](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L2680) | 16 / 0 / 5 | UI hints/wiki/troubleshooting/support group/classes/events; Guidebook; current-grid Help/About; whitelist adviser/grid status; abuse/problem reports; bumps; sysinfo button; About. Grid rows and substitutions are dynamic. |
| [RLVa Main / RLVa](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L2884) | 20 / 1 / 7 | OOC/filtered/redirected chat, temporary attachments/shared inventory/wear rules, console/restrictions/strings; Debug includes top-level placement, messages/assertions, locked layer/attachment visibility, naming/shared wear and locks. Runtime RLVa policy decides top-level versus embedded placement. |
| [Advanced](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L3105) | 73 / 8 / 11 | Rebake/refresh, UI/window size, selection/camera limits, snapshot options, plugin thread; performance/statistics, highlighting/tooltips, render types/features; embedded RLVa placeholder; stream import/export; shortcuts for search/movement/window close/snapshot/mouselook/flycam/reset/zoom; group cache/mouse smoothing/release keys/fly override/RLVa/debug settings/Developer toggle. Hidden by declaration, controlled by `UseDebugMenus`; hidden does NOT disable accelerators. |
| [Develop / Developer](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L3921) | 218 / 22 / 15 | Consoles/display info/telemetry; force-error diagnostics; glTF declarations; video sizes/render tests/metadata/rendering; network/cache/recorders; world/terrain; UI/XUI inspection; character/bakes/animation/debug; logging levels; HTTP/compression/auth tests/leak/minidump/console/admin. Both visibility and enablement depend on `QAMode`. Source declaration is NOT proof a dormant glTF or destructive diagnostic path is active/required. |
| [Admin](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L5969) | 12 / 3 / 0 | God tools; object ownership/copy/delete/lock/assets; parcel ownership/Linden/public claims; region temp assets/save state. Hidden/disabled unless god level or allowed debug override; server authorization remains independent. |
| [Deprecated / Admin label](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/skins/default/xui/en/menu_viewer.xml#L6095) | 21 / 5 / 1 | Hidden attach/detach placeholders, clothing removal and old help links. Do not expose as another Admin root; resolve dynamic consumers before pruning. |

## 3. Construction, state and interaction contracts

### Registries and preparation

[initialize_menus](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L12783)
uses `LLUICtrl::CommitCallbackRegistry`, `EnableCallbackRegistry`, and
`view_listener_t::addMenu/addEnable`; file commands come from `init_menu_file`.
Generic [Floater.Toggle and Floater.Show](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/llui.cpp#L202)
enter `LLFloaterReg`, not a neutral service registry. `LLMenuBarGL`, menu-item
constructors and their Params also belong to the GL visual implementation.
Reading XUI is allowed; instantiating these owners in native is not.

[Menu item initialization and buildDrawLabel](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/llmenugl.cpp#L861)
bind click/enable/visible callbacks and optional control variables. Preparation
calls enable/visible; check rows evaluate the check callback, potentially overriding
a control variable. These callbacks can mutate labels/state; they are not pure
queries. Accelerator dispatch re-evaluates enablement and applies key-repeat
policy. The selected item can be destroyed by its callback.

Important producers beyond XUI:

- [show_debug_menus](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L7773): Advanced visibility only; Developer visibility AND enabled state; Admin god-level/override; login Debug separately.
- [rlvMenuToggleVisible](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/rlvcommon.cpp#L806): moves actual children between top-level and embedded RLVa menus and updates branch parents. It is not two independently populated menus.
- [LLSLMMenuUpdater](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L496): weak menu handle, grid check, asynchronous merchant initialization, toolbar state and possible marketplace-folder creation. Destructor/default lifetime plus global callback cleanup need further audit.
- [Upload.CalculateCosts and Membership.UpdateLabel](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L12380): enable callbacks also substitute `[COST]` and `[Membership]`; membership result depends on SL grid. Construction sets initial sound/animation costs separately.
- [Grid Help/About updates](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L8825): visibility, separator and `[CURRENT_GRID]` label substitutions. Parcel observers and attachment population are additional producers, not yet transitively closed here.

### Keyboard, focus, pointer and time

[LLMenuGL::handleAcceleratorKey](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/llmenugl.cpp#L3187)
recurses even through invisible menus but stops at disabled menus. Thus visual
visibility and command eligibility MUST be separate. Hidden Advanced shortcuts
are intentional; disabling Developer/Admin remains meaningful. Effective parent
enablement, leaf enable callbacks and lifecycle dispatch eligibility still apply.

[LLMenuBarGL keyboard handling](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/llmenugl.cpp#L3531)
suppresses unmodified accelerators while navigating. F10/Ctrl-F10 menu activation
is commented out for Firestorm gesture compatibility; do not restore it by
following generic desktop conventions. `UseAltKeyForMenus` permits an unrepeated
Alt press/release to activate menus if elapsed time is within `MenuAccessKeyTime`
OR elapsed frames are fewer than two. Another key/Alt accelerator cancels the
pending trigger. Draw polling checks the trigger; native preparation must own
that polling effect explicitly. Jump keys are case-insensitive, declaration or
label-derived; keyboard mode controls underlines and activation. Arrow/Return/
Escape, duplicate mnemonic assignment, disabled rows and torn menus require
separate transition tests, not just shortcut-string equality.

[LLViewerWindow::handleKey](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewerwindow.cpp#L3275)
blocks tooltips, gives key-binding capture priority, protects ordinary text input
and Windows AltGr, then handles menu navigation. Modified accelerators respect
focus lock and focused-floater accelerators before the global bar. Connected-bar
accelerators require initialized agent and teleport NONE/LOCAL. Login and hidden
edit menus have distinct routing. World gestures/camera must not receive a key
already owned by UI. Remaining late unmodified/gesture routing is an open edge.

[Hover](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/llmenugl.cpp#L3218)
uses direction-dependent velocity smoothing and slope threshold `0.9` to preserve
the submenu while crossing diagonally. Stationary pointer does not change
selection; disabled rows can retain hover semantics. This is not a generic
delayed-submenu-open algorithm. Overflow menus have a `.033` second scroll-item
timer. The holder retains a selected-item activation effect for `.3` seconds;
it must be compared as a sequence, including text and alpha. Menu fade-timer
start/stop is observed, but a rendered fade contract is not inferred solely from
that member. Inspect its consumers before adding any new fade.

[Mouselook UI transition](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llagent.cpp#L2876)
branches on `FSShowInterfaceInMouselook`, restores UI-toggle state before selectively
hiding menu/status/toolbars, changes toolset and keyboard focus, and treats chat
and floaters specially. [EnableMouselook](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L10795)
also depends on customize-avatar mode and `FreezeTime`. Native needs camera-mode
and restriction facts, not `LLAgentCamera`/`LLToolMgr` calls.

## 4. Representative end-to-end action traces

These five traces reach concrete downstream owners, including GL-coupled effects.
They establish why the callbacks cannot be reused; they do NOT claim transitive
closure of every destination, constructor, failure path or asynchronous callback.

| Family / declaration | Callback and consumer chain | Native consequence / unresolved edges |
|---|---|---|
| Avatar Preferences, Ctrl-P | `Floater.Toggle(preferences)` -> generic registry -> `LLFloaterReg::toggleInstance` -> `showInstance` restriction signal -> `getInstance` -> registered `LLFloaterPreference`, XUI construction, geometry, `openFloater` and focus. [Registry](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/llui/llfloaterreg.cpp#L182), [registration](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewerfloaterreg.cpp#L543). `onOpen` installs per-account observers; [apply/cancel](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llfloaterpreference.cpp#L992) traverses preference panels, changes viewer resolution/camera bounds/media proxy, sends user/profile updates and closes dependent GL floaters. | Use existing independent native Preferences owner, not the GL registry. Audit each live preference subscriber, profile response, Apply/OK/Cancel and dependent dialog; a working login Preferences toggle does not prove connected account or graphics effects. |
| Avatar Snapshot; Advanced Snapshot to Disk | XUI `Floater.Show(snapshot)` -> registered `LLFloaterSnapshot` -> [onOpen](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llfloatersnapshot.cpp#L1257) -> preview invalidation and selected-tab/layout state -> [LLSnapshotLivePreview::onIdle](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llsnapshotlivepreview.cpp#L707) -> `rawSnapshot`, encode/thumbnail/freeze-frame; `prepareFreezeFrame` creates viewer texture and binds `gGL`. [Direct disk route](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenufile.cpp#L1149) instead samples size/options, doubles high-res dimensions while excluding UI/HUD, calls capture, chooses JPEG/PNG/BMP, then `saveLocal`. | Separate native capture-view request and preview publication. Timed camera invalidation, freeze, resolution/aspect, UI/HUD/balance/no-post, destination, encoding, file picker/cancel and sound/animation need closure. Existing frame readback is not this workflow; local PNG alone is not the whole menu promise. |
| World Environment: sunrise/noon/legacy noon/sunset/midnight/shared/pause/editors | XUI on-check uses misleadingly named `World.EnableEnvSettings`; on-enable separately uses `RLV.EnableIfNot(setenv)`. [LLWorldEnvSettings](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L12094) rechecks `canChangeEnvironment`, branches for non-EEP OpenSim legacy conversion, repeated-toggle-to-shared setting, cloud pause and editor floaters. Shared resets `gPipeline.mReflectionMapManager`, clears local selection; manual settings defocus editors. [setManualEnvironment](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llenvironment.cpp#L1388) reads transition duration -> asset callback -> `onSetEnvAssetLoaded` -> set/update environment or `FailedToFindSettings`. | Native environment request/result with asset and region epochs, explicit transition clock, native probe-history invalidation and independent editors. Preserve check versus enable distinction and repeated-click semantics. Asset loader, environment signal consumers, legacy conversion and complete sky/water/probe execution remain open. |
| World Teleport Home / Set Home | `World.TeleportHome` -> `LLWorldTeleportHome` -> `LLAgent::teleportHome` -> [teleportViaLandmark(null)](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llagent.cpp#L5122): startup/RLVa checks, camera reset, stop typing, queued request -> `startTeleportRequest` -> request execution -> `doTeleportViaLandmark` -> `teleportCore` -> reliable TeleportLandmarkRequest. [Menu enable](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L10826) differs from command RLVa admission: the menu blocks either tplm or tploc, while the home command's restriction expression uses both, with additional unsit protection. [Set Home](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llviewermenu.cpp#L7836) delegates to `setStartPosition`; success is server-reported and triggers a home screenshot through a later alert. | Do not flatten UI enablement and command authorization into one guessed rule. Native session/region transport, camera, typing, progress/notification, cancellation and screenshot owners are required. Request-execution ordering, teleportCore, packet completion, setStartPosition and success-alert/capture closure remain open, so no reuse approval for LLAgent. |
| Avatar Exit / Ctrl-Q | `File.Quit` registered by `init_menu_file` -> `LLFileQuit::handleEvent` -> [LLAppViewer::userQuit](https://github.com/anne-skydancer/vulkanstorm/blob/1b7498c2172c500e536d2b9c88e42980c860284d/indra/newview/llappviewer.cpp#L5285). Disconnected/missing or visible progress bypass confirmation; otherwise `ConfirmQuit` -> `finish_quit` -> `requestQuit`. That path includes metrics, avatar/HUD effect, GL floater close-veto workflow, stats and logout timer. | Native confirmation and session shutdown policy must replace this owner. Reuse neither `userQuit` nor `requestQuit`; their nonvisual-looking names conceal visual dependencies. Native cancel/dirty-editor veto, persistence failure, pending requests, partial startup and graceful final teardown require tests; idleShutdown and all shutdown callbacks remain open. |

Additional high-priority exposure: marketplace combines network, inventory and
toolbar mutations; Build requires selection/permission/edit tools; Comm requires
real conversations, friends/presence/groups/voice and unread state; Help URLs need
the actual browser/confirmation policy. These are inventory entries, not completed
end-to-end audits. Diagnostic crash/hang actions are never exercised by this survey.

## 5. Nearest native owners and concrete gaps

- [LLVKViewerUi creation](../../indra/llvulkan/llvkviewerui.cpp#L445) reads and merges
  `menu_login.xml` and resolves the native small font. The inspected dirty
  [refreshSession](../../indra/llvulkan/llvkdialogs.cpp#L449) switches to connected
  communications and dismisses menus; this is not a `menu_viewer.xml` construction.
- [LLVKMenu::create](../../indra/llvulkan/llvkmenu.cpp#L59) parses branch/call/check/
  separator declarations and callback strings using Expat, with depth/item limits.
  It resolves labels once, special-cases the login `Debug` name, and installs a
  no-op `invoke` for non-branch/non-separator rows lacking a click action. A
  callbackless declaration must not accidentally serve as a completed action.
- [Predicate and shortcut evaluation](../../indra/llvulkan/llvkmenu.cpp#L262) is
  live: missing visibility predicate defaults to visible; missing enable predicate
  does not prohibit an otherwise bound handler; missing check uses stored false.
  `enabled()` also calls `itemVisible()`. Shortcut traversal therefore incorrectly
  suppresses hidden Advanced commands. Missing action handlers are disabled, which
  is useful containment, not service completion.
- [paint](../../indra/llvulkan/llvkmenu.cpp#L492) measures labels, updates hit geometry,
  reads predicates repeatedly, arranges/clamps popups, emits native text/primitives
  and activation effects. Thus it is CPU preparation as well as emission, not an
  immutable prepared-frame consumer yet. Dynamic label arguments and a consistent
  per-event state epoch are not represented in `Item`/`Model`.
- [pointer/key handling](../../indra/llvulkan/llvkmenu.cpp#L370) supports nested menus,
  keyboard mode, tear-offs and stationary hover suppression, but does not implement
  the GL directional slope protection or scrollable overflow contract. `showContext`
  explicitly rejects submenu construction. Do not infer full world context support.
- [initializeDialogs bindings](../../indra/llvulkan/llvkdialogs.cpp#L93) are an
  independently owned foundation: Preferences/About/window size, debugging tools,
  close-window, boolean settings, logging. The generic `ToggleControl` handler is
  not permission to route every current-GL setting into native: audit subscribers
  and implemented effects per setting. Only login `Debug` gets the inspected
  `UseDebugMenus` visibility subscription.
- [showPreferences](../../indra/llvulkan/llvkdialogs.cpp#L3980) already owns native
  XUI construction, snapshots, generation changes and dependent close/rollback.
  Preserve this work; qualify connected consumers individually.
- [menu focus owner](../../indra/llvulkan/llvkviewerui.cpp#L990) selects a focused
  widget or torn menu, but global shortcuts always use `mMenu`. Dirty
  [Win32 dispatch](../../indra/llvulkan/llvkwindowmgr.cpp#L607) processes A-Z/F1-F12
  shortcuts before ordinary widget handling and unconditionally activates on F10.
  Numeric/OEM/navigation-key accelerators, Alt-tap, AltGr, repeat, focused-floater
  precedence and session/teleport gating require explicit parity work.
- Dirty [File.Quit binding](../../indra/llvulkan/llvkwindowmgr.cpp#L1045) sets
  `quitRequested`; that binding alone cannot establish GL ConfirmQuit/close-veto
  equivalence. Dirty [communications construction](../../indra/llvulkan/llvkcommunications.cpp#L4)
  uses a bespoke panel with explicit unavailable friends/contact-set messages.
  It is neither the original Comm menu nor evidence those services are complete.

Existing tests are useful but not parity evidence:
[test 130](../../indra/llvulkan/tests/llvkwidgettree_test.cpp#L5767) covers login
menu geometry/selection; [test 194](../../indra/llvulkan/tests/llvkwidgettree_test.cpp#L1447)
covers nested check/enable/activation effects; [test 197](../../indra/llvulkan/tests/llvkwidgettree_test.cpp#L1425)
explicitly expects a hidden ancestor to block shortcuts. That expectation conflicts
with current GL and needs a source-backed correction, while retaining its hidden
popup dismissal assertions. No tests were run or changed for this document.

## 6. NV-00 design records by behavior family

All records below are **local-source-inspected, transitive-edges-open, proposed**.
None is contract-closed, implemented by this document, runtime-validated or
parity-validated. Named roots and unresolved edges are in sections 3-5.

| Record | Q1: What GL does | Q2: Native result, CPU/GPU split | Q3: Cleanest design and discriminating check |
|---|---|---|---|
| M1 declaration/construction | `init_menus`, `initialize_menus`, factory layering and callback registration create the skinned hierarchy, required holders, colors and initial labels. | Independently load effective XUI into native declaration records; map callback+typed parameter to explicit native actions; CPU validates hierarchy and dependencies. No GL Params/factory/registry reuse. | Extend LLVKMenu and the existing native skin/XML owner rather than a second hardcoded menu. Compare root order and all 595 base declaration identities/counts; reject missing required asset/unknown schema explicitly. Effective-layer counts need their own fixtures. |
| M2 dynamic policy | Menu label-building invokes enable/check/visible callbacks with side effects; settings, grid, parcel, merchant and RLVa mutate content. | CPU service snapshots and a pure native evaluator publish resolved label/flags/parent order; dispatch revalidates authorization. GPU receives only prepared values. | One versioned MenuStateSnapshot, not predicates during every text/rectangle emission. Test a restriction or region change between hover and click, membership/cost relayout, stale merchant completion and RLVa relocation without duplicate rows. |
| M3 accelerator/navigation/focus | GL allows hidden accelerators, preserves disabled ancestors, gives text/AltGr/focus owners priority and uses configurable timed Alt access. | Native CPU input state machine owns navigation mode, key-repeat policy, eligibility and focus return; native window supplies key/character events and timing. | Separate render visibility from command eligibility; keep logical modifier/key codes rather than string concatenation as the long-term interface. First narrow check: hidden Advanced shortcut fires once, disabled Developer does not; test 197's visual dismissal still passes. |
| M4 pointer/submenu/temporal feedback | GL slope-protects submenu selection, uses stationary-pointer and torn-focus rules, scrolls overflow and paints time-dependent activation feedback. | Native CPU event reducer with injected clock and prepared hit regions; emission preserves shadows, glyphs, outlines, alpha, clips and ordering. | Reuse independent native menu, extend its state rather than calling GL input. Feed identical diagonal/stationary pointer sequences and fake-clock boundaries; compare exact selected path and timed output. Close uninspected overflow/tear-off edges before implementation. |
| M5 application/dialog commands | Floater registry, Preferences and Quit construct visual owners, evaluate restrictions, manage dependent dialogs and invoke persistence/shutdown callbacks. | Typed commands to native LLVKViewerUi/floater/session owners; audited nonvisual persistence/transport services may be behind them. CPU owns confirmation, drafts, vetoes and stale-response rejection. | Reuse native dialogs, not GL callback forwarding or an unrestricted string-to-function bridge. Test nested Preferences Cancel and quit Cancel/veto/save-failure through production bindings; no command may outlive its session/owner. |
| M6 connected world/service actions | Environment, home, selection, uploads and communications couple authoritative service facts to camera/rendering/UI. | Native transport/data results plus native environment/camera/selection consumers; GPU work is scheduled by explicit view/resource versions, never menu callbacks. | Deliver service+menu+native consumer vertical slices. Test repeated environment selection, failed asset fetch, teleport restriction changes and stale region responses; missing consumer keeps parity open, regardless of disabled row or successful request. |
| M7 capture and presentation | Snapshot opens/refreshes preview, tracks camera/time, rerenders with options and encodes; menus compose after world postprocessing. | Native capture job plus versioned preview image, explicit inclusion/history policy and CPU encoding after completed readback. Menu prepared primitives/text remain outside tone mapping/fog/DoF. | Native view graph and completion-tagged resources, not GL capture or swapchain screenshot shortcut. Test resize while pending, no extra simulation advance, high-res UI/HUD exclusion and descriptor/image retirement; exact UI and temporal comparison remains required. |

### Sharing decisions

Forbidden as implementations: `LLMenuGL`/`LLMenuBarGL`/menu item classes,
`LLUICtrlFactory`, `LLFloaterReg`, `LLFloaterPreference`, `LLFloaterSnapshot`,
`LLSnapshotLivePreview`, GL font/image lookup, `LLAgentCamera`, `gPipeline`, and
GL-coupled LLAgent/application callbacks above. A wrapper, separate instance or
apparently CPU-only method does not alter this classification. Do not use even
an existing `getVkBgColor` accessor on a GL menu owner.

Candidates for independent audit: immutable XUI assets; Expat and generic parsing;
string/UUID utilities; logging; file IO; protocol serialization, protected settings
storage and network transport without visual callbacks. Existing native font,
skin, widget, dialog and session owners are the preferred integration surfaces,
not blanket proof of all dependencies. In particular, settings signal subscribers,
LLSettingsVOBase asset callbacks, marketplace/global inventory owners, LLWeb and
LLNotificationsUtil are NOT approved nonvisual services merely from their names.
This survey grants no new transitive reuse approval.

## 7. Native data, actions and ownership proposal

Proposed records (design names, not claims of existing symbols):

- `MenuDeclaration`: stable item ID = declaration resource + named ancestor path
  + item name (ordinal only for unnamed separators); retain source label template,
  jump-key metadata, callback names/typed parameters, shortcut/repeat flags, style,
  original order and restriction dependencies. Do not key by translated labels or
  vector indices that change when RLVa relocates or contexts append rows.
- `MenuStateSnapshot`: monotonically increasing menu revision plus session tag,
  region epoch, selection generation, restriction epoch, settings revision,
  service-availability revision, locale/skin/font revision, camera/mouselook and
  teleport state, focus/modal epoch and viewport/DPI generation. Resolve relevant
  service facts on the owner thread, not during GPU recording.
- `ResolvedMenuItem`: label and arguments, visible, presentation-enabled,
  command-eligible, checked, effective parent, capability/readiness reason and
  typed action payload. Source-hidden is not unimplemented; source-disabled is
  not unauthorized-by-default in another entry point. Keep those facts distinct.
- `PreparedMenuFrame`: immutable ordered backgrounds/shadows/text/check/submenu
  glyphs/tear strips with clip, alpha, font and image versions; hit regions and
  open path refer to the same state/layout epoch. CPU preparation consumes the
  clock once. GPU submission never invokes services, predicates or GL `draw()`.
- `MenuCommand`: stable action ID, typed payload, originating item, session/region/
  restriction/selection epochs and dispatch serial. Owner rechecks necessary
  preconditions against current facts, dismisses or retains the menu as required,
  then invokes the native destination once. Workers return tagged results, never
  widget pointers. Late responses cannot recreate a closed menu/floater/account.

Initial explicit IDs and mappings:

| Proposed ID | Current declaration mapping | Native destination / dependency |
|---|---|---|
| `ui.preferences.toggle` | `Floater.Toggle`, `preferences` | LLVKViewerUi native Preferences, with account/service-specific gates |
| `ui.about.show` | `Floater.Show`, `sl_about` | Existing native About owner |
| `ui.window.close` | `File.CloseWindow` | Existing native focused/dependent floater close policy |
| `app.quit.request` | `File.Quit` | Native confirmation, close-veto, persistence and session shutdown |
| `world.snapshot.open` | `Floater.Show`, `snapshot` | Native snapshot floater and capture owner, currently a dependency |
| `world.snapshot.save_disk` | `File.TakeSnapshotToDisk` | Native option snapshot, capture, encode and save workflow |
| `world.environment.select` | `World.EnvSettings`, enum sunrise/noon/legacy-noon/sunset/midnight/region | Native environment owner; original parameter spelling retained in adapter |
| `world.environment.cloud_pause.toggle` | `World.EnvSettings`, `pause_clouds` | Native environment clock |
| `world.environment.editor.show` | `World.EnvSettings`, `adjust_tool` or `my_environs` | Independent native editor and asset service |
| `agent.home.teleport` / `agent.home.set` | `World.TeleportHome` / `World.SetHomeLocation` | Native authoritative session/region request owners plus visual consumers |
| `ui.menu.policy.setting.toggle` | Only individually audited `ToggleControl` parameters | Native settings transaction plus explicit effect subscription; no arbitrary setting escape hatch |

Do not register a generic `Floater.Show` success path for hundreds of nonexistent
native floaters. Compile bindings against an explicit action catalogue and report
unresolved mappings. Such reporting/disabled state is interim containment and
cannot close a requested workflow. Inventory/economy/editing/voice implementations
are separate scoped dependencies, not authorized automatically by this design.

Focus proposal: LLVKViewerUi owns the mutually exclusive menu navigation session
and its return-focus token; native widget tree owns actual widget focus/capture;
native modal/floater owners arbitrate locks and dependent stacks. Open menus
suspend world key dispatch, not unrelated service pumping. Session/region reset,
modal acquisition, app focus loss or destroyed return target invalidate the token.
Torn menus share action/state identity but keep independent geometry/navigation
and explicit focus. RLVa relocation and mouselook changes reconcile open paths
without stale indexed hits. Exact GL focus restoration details remain a required
local audit before implementing that reconciliation.

GPU proposal: consume existing native widget-paint commands through native UI
rendering; keep menu outside world exposure/postprocessing. Declare glyph/image
upload -> sample dependencies and publication, preserve source color/alpha,
painter order, shadow coverage and nested clips. Retain every resource version
through all submissions that use it; noncoherent flush/invalidate and completion-
based descriptor retirement apply. DPI/skin change invalidates prepared geometry
and resource versions coherently. This design does not prove current resource
infrastructure satisfies NV-13/NV-14 or introduce a new shader ABI.

## 8. Small implementation slices and acceptance

These are future implementation targets, not edits performed by this survey.

| Slice | Actual files/symbols to extend | Cheap discriminating test / runtime states |
|---|---|---|
| 1. Hidden accelerator eligibility | LLVKMenu `shortcut`, `enabled` in [llvkmenu.cpp](../../indra/llvulkan/llvkmenu.cpp); revise/extend test 197 alongside test 194 in [llvkwidgettree_test.cpp](../../indra/llvulkan/tests/llvkwidgettree_test.cpp) | Separate visual visibility from accelerator eligibility without enabling missing commands. Hidden Advanced-style parent permits bound shortcut once; disabled Developer-style ancestor and disabled leaf block; invisible popup dismisses; check current enable callback on dispatch; empty/unbound handler never fires. This is a CPU menu contract slice, NOT connected-menu delivery. |
| 2. Explicit action/state evaluation | LLVKMenu Item/Model in [llvkmenu.h](../../indra/llvulkan/llvkmenu.h), LLVKViewerUi `initializeDialogs`/`refreshSession` in [llvkdialogs.cpp](../../indra/llvulkan/llvkdialogs.cpp) | Snapshot evaluator produces exact flags/labels from fake immutable service facts, distinguishes on-check from on-enable, rejects unknown bindings, never publishes a stale account/region result. Retain current native dialogs. |
| 3. Connected declaration and lifecycle binding | LLVKViewerUi creation/menu access/preparation in [llvkviewerui.cpp](../../indra/llvulkan/llvkviewerui.cpp), session switch in llvkdialogs.cpp | Load layered menu_viewer alongside login menu; exact base inventory fixture, session transition and rollback; no duplicate shortcuts or surviving torn menus across logout. Bind each delivered action to an actual consumer. A largely disabled bar remains a partial scaffold. |
| 4. Input/focus parity | Native Win32 message route in [llvkwindowmgr.cpp](../../indra/llvulkan/llvkwindowmgr.cpp), LLVKViewerUi menu focus and LLVKMenu key/pointer reducers | Alt tap/hold/frame boundary, AltGr text, repeats, number/OEM shortcuts, F10 gesture availability, focused floater before global shortcut, modal lock, teleport gate, mouselook UI setting, Escape/Return/arrows and return-focus deletion. Follow up with controlled runtime event replay; no camera motion from consumed keys. |
| 5. Prepared geometry and temporal effects | LLVKMenu paint/menuSize/pointer, LLVKViewerUi updateMenuHover, existing widget-paint/font owners | Directional submenu trajectory, disabled-row crossing, overflow scrolling, left/right viewport edges, long translated labels, fractional DPI, nested clipping, tear-off focus and 0/.15/.3-second activation boundaries. Compare exact visual and interaction sequences, not marker presence. |
| 6. Delivered service vertical slices | Existing native Preferences, session/communications and browser owners; new environment/home/capture owners only after their local contracts close | Preferences with nested dialogs and Cancel/Apply; Quit confirm/cancel/veto/failure; then real environment asset change/repeated toggle/shared/denial/failure; home request/cancel/region change; snapshot preview and agreed destinations. Each service brings notifications/error paths, stale-result rejection and teardown. Missing service means incomplete parity. |

Use the existing widget integration target
[INTEGRATION_TEST_llvkwidgettree](../../indra/llvulkan/CMakeLists.txt#L252), not a
new standalone test framework. For later implementation, select the native-sl-login
CMake project, discover its targets/tests with CMake Tools, build the narrow target
and run the discovered widget test through CMake Tools. Some integration targets
also execute tests after build; record what actually ran. No build or CTest run
is required or performed for this documentation-only task.

Runtime comparison matrix must include:

1. Default visible roots; Advanced hidden/visible with shortcuts in both states;
   Developer and Admin enabled/disabled; RLVa top-level/embedded and runtime
   restriction changes; SL versus supported OpenSim/grid-help/merchant states.
2. Every delivered root open, deepest delivered submenu, edge-clamped/overflow
   geometry, checked/unchecked/disabled/hidden rows, changing cost/membership labels,
   long translations and each supported skin/font/DPI combination.
3. Mouse press/drag/release/outside dismiss, diagonal submenu crossing, stationary
   pointer, Alt-tap/hold, arrows/mnemonics/Escape, keyboard repeat, AltGr, editor/
   browser/floater/modal focus, torn menu, app focus loss and mouselook entry/exit.
4. Async changes while menu is open; disconnect/reconnect/region crossing; denied,
   cancelled and failed action; nested dialog close/return focus; exact activation
   timing and painter order over world, tooltips and notifications.

Record revision, effective assets/layers/settings, input sequence, clock/history,
viewport/scale/formats and device/driver separately for CURRENT-target investigation
and HISTORICAL-oracle evidence. Exact visual/effects/interaction parity is required:
colors, textures, alpha/blends, geometry, text/icons, clipping, order, animation,
feedback and nested dialogs. No redesign, approximate markers, relaxed tolerance
or omitted disabled workflow qualifies. World numerical tolerances, if needed,
require the invariant's prior reference-derived approval, not ad hoc UI tolerance.

No viewer was launched. Future full viewer runs require advance notice and manual
login time, real STATE_STARTED, an uninterrupted 75-second settle, graceful
WM_CLOSE, exit zero and Goodbye!. Never force-stop. Reuse prior valid evidence;
rerun only newly affected or missing states. GPU changes additionally require
actual validation-layer execution and resource lifetime/failure evidence.

## 9. Open obligations and PR review record

Remaining edges: full callback catalogue beyond the five traced families;
effective skin/language/platform layering; actual runtime feature reachability;
context/pie selection and attachment population; parcel/merchant observer teardown;
all RLVa callback subscribers and enforcement; complete keyboard/gesture/IME routing;
overflow/torn-menu focus and mnemonic collision behavior; dependent floater close
vetoes; every setting subscriber; preferences account callbacks; environment asset
and render consumers; teleport completion and Set Home alert capture; snapshot
encoders/destinations; logout persistence and teardown. None is erased by XUI counts.

| Review field | Record for this change |
|---|---|
| Contract | NV-00/01/02/03/12/17/18 govern this survey; proposed implementation also touches NV-04/05/09/10/11/13/14/15/16 where state, capture and GPU resources are involved. |
| Reference | Current GL, native dirty observations, historical oracle and checkpoint are independently identified in section 1. Runtime scene/device/driver/size is N/A: no execution or parity claim. |
| Data flow | Proposal only: explicit declaration/action/state/prepared-frame ownership, CPU evaluation and native visual consumers. No ABI, color, alpha, depth or coordinate changes implemented. |
| GPU safety | N/A for executable change; no executable change. Future upload, publication, dependencies, all-use retirement, noncoherent memory and WSI proof remain required. |
| Validation | Documentation source/count/link/anchor/hygiene checks only; command below. No builds, tests, captures or viewer launch. Source inspection is not runtime or measured parity. |
| Limits | 595 declared leaves and 385 callback names are inventory scope, not complete audits. Five representative action families traced to concrete downstream owners with explicit unresolved edges. All native parity remains unverified by this work. |
| Change class | Documentation-only current-source survey and proposed parity engineering design. Not a baseline amendment, restoration, implementation or improvement to GL. |

## 10. Documentation validation

Read-only count reproduction from the CURRENT GL checkout:

```powershell
[xml]$menu = Get-Content C:/Dev/vulkanstorm/indra/newview/skins/default/xui/en/menu_viewer.xml -Raw
$menu.SelectNodes('/menu_bar/menu').Count
$menu.SelectNodes('//menu_item_call | //menu_item_check').Count
$menu.SelectNodes('/menu_bar/menu//menu').Count
$menu.SelectNodes('//menu_item_separator').Count
@($menu.SelectNodes('//*[@function]') | ForEach-Object { $_.function } | Sort-Object -Unique).Count
```

Expected: `11`, `595`, `61`, `77`, `385`. These commands ignore commented-out XML;
grep counts would not. For link validation, resolve every relative Markdown path
from this document and check line anchors against that file's length. For immutable
GL source URLs, use `git show <revision>:<path>` locally and check the cited line;
do not substitute the dirty native copy. This verifies local source availability
at the exact revision, not remote HTTP reachability. Also reject non-ASCII text,
trailing whitespace, unbalanced code fences and merge-conflict markers in this
new document. Do not modify any referenced source to make the checks pass.

Run this self-contained read-only PowerShell validation command from any directory:

```powershell
& {
  $document = 'C:/Dev/vulkanstorm/native-sl-login/doc/vulkan/native_top_menu_transposition.md'
  $text = Get-Content -LiteralPath $document -Raw
  $sources = @{}
  $links = [regex]::Matches($text, '\]\(([^)\r\n]+)\)')
  $anchors = 0
  foreach ($link in $links) {
    $target = $link.Groups[1].Value
    if ($target -match '^https://github.com/anne-skydancer/vulkanstorm/blob/(?<revision>[a-f0-9]+)/(?<path>[^#]+)(?:#L(?<line>[0-9]+))?$') {
      $identity = $Matches.revision + ':' + $Matches.path
      $line = $Matches.line
      if (!$sources.ContainsKey($identity)) {
        $sources[$identity] = @(git -C C:/Dev/vulkanstorm show $identity)
        if ($LASTEXITCODE -ne 0) { throw "Missing revision source: $identity" }
      }
      $content = $sources[$identity]
    } else {
      $parts = $target -split '#L', 2
      $path = Join-Path (Split-Path $document) ([Uri]::UnescapeDataString($parts[0]))
      if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing link: $target" }
      $content = @(Get-Content -LiteralPath $path)
      $line = if ($parts.Count -eq 2) { $parts[1] } else { $null }
    }
    if ($line) {
      ++$anchors
      if ([int]$line -lt 1 -or [int]$line -gt $content.Count) { throw "Invalid anchor: $target" }
    }
  }
  if ($text -match '[^\x00-\x7F]' -or $text -match '(?m)[ \t]+\r?$' -or
    $text -match '(?m)^(<<<<<<< |=======|>>>>>>> )') { throw 'Text hygiene failure' }
  if ([regex]::Matches($text, '(?m)^```').Count % 2) { throw 'Unbalanced fences' }
  [xml]$menu = Get-Content C:/Dev/vulkanstorm/indra/newview/skins/default/xui/en/menu_viewer.xml -Raw
  $counts = @($menu.SelectNodes('/menu_bar/menu').Count,
    $menu.SelectNodes('//menu_item_call | //menu_item_check').Count,
    $menu.SelectNodes('/menu_bar/menu//menu').Count,
    $menu.SelectNodes('//menu_item_separator').Count,
    @($menu.SelectNodes('//*[@function]') | ForEach-Object { $_.function } | Sort-Object -Unique).Count)
  if (($counts -join ',') -ne '11,595,61,77,385') { throw 'Inventory mismatch' }
  "PASS: $($links.Count) links; $anchors anchors; $($sources.Count) pinned GL sources; counts and hygiene"
}
```

Validation results: 67 links, 56 line anchors, 16 distinct revision-pinned GL
source files and eight distinct native implementation/test/build files cited;
five governing instruction/contract/report documents linked. Paths, anchor bounds,
revision-source availability, XML counts, ASCII, whitespace, fences and conflict
markers passed. Function-entry checks also corrected two nearby native anchors.
The editor reported no diagnostics for this document. These checks do not claim
remote HTTP availability, exhaustive semantic audit, runtime behavior or parity.
Because this document is new/untracked, `git diff --check` alone would not validate
its contents; the direct checks above are the documentation gate.

## 11. Bounded menu implementation record (2026-09-19)

Record M3a, recorded before the implementation edit. This appended record does
not rewrite the survey or upgrade its historical oracle. Source target remains
`1b7498c2172c500e536d2b9c88e42980c860284d`; historical parity oracle remains
`59108e15a1f8f94d2da7c674d937d19f5cf9450d`, with implementation lineage at
`90af5a7220f1fec28e3c909c051b6ee7e062f10b`.

1. **What does GL do?** Current-target `LLMenuGL::handleAcceleratorKey` rejects
  disabled menus but traverses invisible ones in declaration order. Branch and
  branch-down items delegate to their child menus; branch-down additionally owns
  activation feedback. `LLMenuItemCallGL::handleAcceleratorKey` checks key,
  modifiers and repeat before `updateEnabled`, then commits if enabled, without
  consulting visibility. Check items inherit that route. `updateEnabled` invokes
  the enable signal and reconciles any enabled control; `onCommit` records the
  selected item, conditionally hides its visible non-torn menu and invokes the
  registered callback, which may delete the item. `show_debug_menus` changes
  Advanced visibility only, but changes both Developer/Admin visibility and
  enablement. Arbitrary callback bodies, control-variable precedence, repeat,
  branch feedback and torn-menu dismissal parity remain outside this slice.
2. **How does native produce the result?** Independently owned CPU traversal of
  `LLVKMenu::Item` checks ancestor enabled state and native predicates, ignoring
  visibility only for accelerators. A matching leaf requires a real native
  handler and current enablement. Visible pointer/navigation paths retain their
  visibility gate. Native dispatch copies the selected item and handler before
  dismissing and invoking; no GL owner, registry or callback is introduced.
3. **Cleanest implementation and check?** Split `commandEnabled` from visible
  `enabled`, and extract existing leaf dispatch into `invokeItem`; avoid a public
  bypass flag or duplicated dispatch implementation. Evaluate only matching
  leaf predicates once during shortcut dispatch. Add `setEnabled(name, bool)`
  alongside `setVisible` for independently controlled branch policy. Remove the
  parser's callbackless no-op binding: declarations without native handlers are
  unavailable, not successful actions. Extend existing test 197 only: hidden
  Advanced/nested/leaf shortcuts, visible navigation and popup dismissal,
  disabled ancestor/leaf rejection, live predicate evaluation, missing/empty
  handlers and exact dispatch count. Existing test 194 remains a regression
  obligation for checks, activation effects and detached shared state.

| Review field | Bounded implementation |
|---|---|
| Contract | NV-00/01/02/03/12/15/17/18; accelerator eligibility independent of visibility, without fabricating unbound actions. |
| Reference | Revision identities above; source inspection of Windows current-target menu dispatch and debug policy. No runtime settings/device/scene evidence. |
| Data flow | Existing native declaration/model/handler maps; new public enabled-state setter; no shader, color, coordinate or GPU ABI change. |
| GPU safety | N/A: CPU eligibility and native dispatch only; existing paint/resource ownership unchanged and not newly qualified. |
| Validation | Immediately request editor diagnostics after implementation. Builds and test execution deferred to the main serialized integration owner because parallel agents are editing shared sources; no viewer launch. Results appended below. |
| Limits | This is a menu contract correction, not delivery or parity of 595 declared commands. All survey obligations outside this bounded slice remain open. |
| Change class | Independently owned native parity correction; no GL edit or baseline amendment. |

M3b, recorded before the adjacent follow-up edit: actual `menu_viewer.xml`
contains `alt|control|shift|h`, `alt|shift|c` and `Esc`. Q1: current-target
`LLMenuItemGL` construction derives modifier flags independently of order, takes
the final pipe-delimited key, and `LLKeyboard::keyFromString` uppercases ASCII
letters and named keys. Q2: independently normalize the native comparison key
and modifier ordering on the CPU, retaining the raw declaration for display;
do not call GL input owners. Q3: a small private binding-normalization helper
preserves the existing public shortcut API and native ownership. Test 197 adds
exact modifier-mask rejection, reordered/lowercase declarations and named-key
case. Platform-specific `shortcut_linux`/Mac-control mapping, invalid-key-name
validation, aliases and repeat remain open. The same follow-up aligns handler
eligibility with dispatch's existing item-specific-over-generic precedence, so
an empty item-specific binding cannot reach `std::bad_function_call` through an
otherwise available generic handler. Its regression uses both bindings together.

### Implementation outcome and shell handoff

Implemented in `LLVKMenu::shortcut`, `commandEnabled`, `enabled`, `activate`,
`invokeItem`, `setEnabled`, parser finalization and the private `shortcutBinding`
helper. Only existing test 197 was extended; test 194 and other agents' test
cases were not edited. Test additions cover the M3a/M3b assertions above; they
have NOT been executed in this concurrent engineering phase.

- New API: `void setEnabled(std::string_view name, bool enabled)`, independent
  from existing `setVisible(name, visible)`. Both update the shared model used
  by detached views. Set the Advanced visibility without disabling it; update
  both visibility and enablement for `Develop` and `Admin` from audited native
  policy facts. The login `Debug` policy also requires both flags. A false
  declaration/setter enabled flag remains blocking even with a true predicate.
- Existing `bindItem(action, parameter, handler)` takes precedence over generic
  `bind(action, handler)`; an explicitly empty item binding disables that route.
  Prefer parameter-specific real bindings for `Floater.Show`/`Floater.Toggle`.
  An unrestricted generic handler would make unimplemented destinations appear
  eligible. `bindPredicate(action, predicate)` remains the state integration
  surface; missing predicates retain existing fallback behavior, not evidence
  of audited policy. No fake production handler was added.
- Existing `shortcut(std::string key, bool control, bool shift, bool alt)` keeps
  its signature, now matches actual reordered/lowercase declarations, and does
  not query visibility. Hidden roots still cannot open through ordinary menu
  activation. The caller must arbitrate session/teleport eligibility, modal and
  text/AltGr focus, repeat policy, named/OEM/digit key translation and login versus
  connected-menu ownership. No new window-input or lifecycle policy is implied.
- Main shell owner remains responsible for actual layered `menu_viewer.xml`
  creation, policy subscriptions, production command bindings, retained/torn
  menu cleanup and session switching. Address/favorites, Conversations, Contacts,
  Friends, Groups, ContactSets, group tabs, IM, People and toolbars are outside
  these owned files; this slice does not establish their integration.

Static validation: immediate `get_errors` after each substantive patch reported
no diagnostics in the menu implementation/header, widget test file and this
document. Scoped `git diff --check` passed. Read-only XML DOM inspection of the
actual native-worktree declaration found recognized element names, 11 roots and
595 leaves; this is not an execution of `LLVKMenu::create`. Documentation checks
passed ASCII/whitespace/conflict/fence hygiene and 27 local links/anchor bounds.
Immutable current-target GL source was read for the local contract; prior survey
source-link evidence was not relabelled as new runtime evidence.

Deferred gates: the main serialized integration owner must compile and run the
existing widget integration target, including tests 197 and 194 and existing menu
fixtures. No build, test executable, viewer, capture or GPU validation ran here.
Exact visual/effects parity is unverified. Unmodified open menu obligations
include Alt access/F10 policy, unmodified navigation shortcuts, repeat and AltGr,
focus precedence, branch activation feedback, torn-menu behavior, directional
hover, overflow scrolling, dynamic labels/control variables, RLVa relocation,
skin/language/platform variants and the full real action catalogue. None of the
595 declared commands is claimed implemented merely because its XML can be read.

### M3c: root declaration jump-key policy

Pre-edit record, 2026-09-19; same source revisions and checkpoint lineage as M3a.

1. **GL contract:** current-target `menu_viewer.xml` requests
  `create_jump_keys="true"` on `menu_bar`. `LLMenuGL` construction retains that
  flag; inherited `postBuild()` calls `createJumpKeys()` after children exist.
  That CPU operation reserves explicit keys and assigns remaining keys from
  immediate sibling labels, not recursively from descendant labels. Default
  false is declared by `LLMenuGL::Params`. No action callback executes here.
2. **Native design:** retain the root flag in the local Expat parser, then use
  the existing independently owned `assignJumpKeys(mRoots)` after native label
  resolution. Nested menu policy remains independent. No GL visual helper,
  service binding, GPU resource, shared viewer header or input route is added.
3. **Disconfirming check:** extend menu-only test 197 with root true/1,
  false/0/omitted cases, explicit-key preservation and a child whose own menu
  did not request assignment. Previously the true/1 root-key assertion fails.
  Editor diagnostics are the immediate available validation; execution status
  is recorded below, not inferred from inspection.

Review record: NV-00/01/02/03/12/17/18; CPU declaration-policy parity correction.
Reference inputs are the current-target Windows menu declaration and GL
constructor/postBuild/createJumpKeys source. Data flow changes only parsed root
policy and item jump-key fields. GPU safety is N/A (no resource or submission
changes). Alt/F10 access, root character routing, underlines and exact visual
parity are not qualified by this parser fix; all earlier open obligations remain.

M3c outcome: implemented root-flag parsing and root-only native assignment in
`llvkmenu.cpp`; extended existing menu test 197 only. Prior M3a/M3b changes and
other owners' dirty work are preserved; no header/API change was needed. Immediate
editor diagnostics reported no errors in the implementation, fixture or document;
scoped `git diff --check` passed. HEAD remains `005a4ae869`. No CMake cache was found
under this worktree's `build*` directories, so no compile or test was attempted,
no other tree was configured, and no viewer was launched. Main must build/run
`INTEGRATION_TEST_llvkwidgettree` against this worktree's sources (including menu
tests 197 and 194); the newly added assertions and earlier M3a/M3b assertions
remain execution-pending. Static diagnostics are not measured UI/effects parity.

Sequential shell handoff: preserve the M3a/M3b API requirements above. Root keys
are now available through existing `items()[index].jumpKey`; `character()` still
routes within the open submenu, so this patch does not implement top-level
Alt-letter access. Do not route shell text events to that method as a substitute
for auditing root access/focus policy. Use `setVisible` and `setEnabled` separately
for Advanced versus Develop/Admin/Debug policy, and parameter-specific real
`bindItem` handlers for implemented destinations. No generic placeholder handler
or communications/shell integration was added. Conversations remains the next
sequential owner, followed by the connected shell.