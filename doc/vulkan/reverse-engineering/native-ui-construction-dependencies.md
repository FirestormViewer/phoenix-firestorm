# UI construction and registry dependencies

Reference: 3abd661f498329babaf87b49ffab910fcb5f0e6c; implementation checkpoint
90af5a7220f1fec28e3c909c051b6ee7e062f10b. Investigation covers the compiled Windows
RelWithDebInfo source tree, with platform/configuration alternatives retained as
explicit obligations. Status throughout: local-body-inspected, edges-open, not
implemented, not runtime/parity validated. NV-00/NV-03/NV-12/NV-17 govern this work.

The [font/default root](native-ui-dependencies.md) already establishes that parameter
construction is not automatically GPU-neutral. This document records factory
decisions independently of the rendering path. None of the candidate CPU sharing
choices below authorizes reuse until its outgoing constructor/helper targets close.

## UI checkpoint status (2026-09-11)

### End-of-session checkpoint (2026-09-12)

The user requested a commit and pause after successful validation. The live menu
opens the original Preferences hierarchy, not the earlier provisional window.
Latest widget177/177 and native window3/3 pass. The viewer link passed through the
color/tab work; the final permissions/executable-picker changes have widget and
window-target validation, with the executable refreshed separately before commit.
This checkpoint does not declare complete service wiring or measured parity.
The older status table below describes the prior checkpoint, not the live hierarchy.

Default Creation Permissions now opens from the live Viewer tab using original
floater_perms_default.xml. Source LLFloaterPermsDefault postBuild/refresh/cancel/
onCommitCopy/ok and updateCap were inspected: eight categories, one-time upload
migration, no-copy forces transfer, independent acceptance and cancellation, and
optional region capability publication. The XML has 37 editable settings; three
environment permissions are fixed and absent from settings.xml. Native ownership
uses existing settings plus a child-dialog snapshot and existing persistence
callback, with no GL visual calls. Test176 covers actual launch callback, migration,
copy/transfer enablement, Cancel, failed-save retry and painting. Region
AgentPreferences capability submission/retries remain open; no native region is
currently initialized. Accepted defaults are persisted immediately because this
native lifecycle has no general shutdown-save hook.

External Editor Browse now uses the existing native picker worker with an explicit
executable kind, rather than the unbound Viewer action handler. Source
LLFloaterPreference::changeExternalEditorPath on Windows quotes the chosen path
and updates ExternalEditor; it does not execute it. Native selection does the same,
and a Preferences generation rejects callbacks from closed/reopened transactions.
Test177 covers path quoting, Cancel, stale callback rejection, picker cancellation
and accepted settings publication. Windows picker code compiled and window tests
passed; an interactive executable-picker selection was not exercised automatically.

This is a user-requested work-in-progress commit, not the Full UI Parity
checkpoint. The next acceptance gate is full startup UI parity with the original
OpenGL behavior and shared source declarations, without GL visual ownership.

| Area | Actual status |
|---|---|
| Login boot, top menu, font fallback | Implemented; native viewer runtime and focused regressions verified |
| Scroll painting and tabs | Document clipping, top/left/bottom layout, horizontal/vertical overflow controls and horizontal interpolation tested; full hidden-tab, flash, drag and parity qualification remain open |
| About | Original floater_about.xml hierarchy, separate native editors, original credits scroller, generated assets, support Copy and web-link subset; full editor semantics, timestamp, lifecycle and measured parity remain open |
| Preferences | Reduced provisional window still present; original panels, control types, search, callbacks and full apply/cancel behavior are not restored |
| RLVa/audio | Native OpenAL session initialization and driver reporting verified; RLVa service integration and complete audio behavior remain open |
| Settings persistence | Accepted native subset has staged file replacement, subtree snapshots, settings subscribers and color rollback tests; full original settings behavior remains open |
| Validation | Font 18/18 previously passed; widget 146/146, context 9/9 and original picker integrated window/audio 2/2 passed at the recorded increments below. Actual viewer rebuilt through the picker increment; no full-build or visual-parity claim |

The unrelated baseline test repairs and older untracked text-layout experiment
are excluded from this checkpoint. Texture cache, skins and close/logout policy
review remain deferred until the UI parity priority is satisfied. Test counts
and screenshots do not close any outstanding behavior above.

## Native construction implementation (2026-09-10)

Live Preferences integration continuation (2026-09-11, bdb5e59486 source,
Windows RelWithDebInfo, NV-00/01/03/12/15/17): showPreferences now constructs
the original floater_preferences.xml, replacing the provisional General/renderer
window. Pref.OK/Cancel route to the native transaction. Build exclusions match
the source OPENSIM/SINGLEGRID and LL_SEND_CRASH_REPORTS tab policy. All applicable
root panels and their nested tabs construct and paint through the live menu path.
This is live hierarchy integration, not completion of all behavioral services.

Post-crash continuation: widget175/175, native window3/3 and the dedicated
vulkanstorm-bin link target passed after user-color persistence and tab restoration.
LLVKColorTable serializes the source colors/color name/value XML, omitting unchanged
defaults; native startup loads user_settings/colors.xml before widget construction,
and accepted Preferences writes changed overrides with staged replacement.
Source roots are LLUIColorTable::saveUserSettings and LLFloaterPreference::onBtnOK;
CPU-only native XML/file ownership replaces GL UI-table ownership. Runtime voice
meter colors have a separate nonpersistent layer. Test174 covers serialization and
test175 covers live startup/preview/Cancel/failed-write retry/acceptance/recreation
using isolated files. Windows path validation checks actual ancestor reparse points,
not canonical spelling equality (which rejected a valid temporary path). Concurrent
path replacement and cross-file transaction atomicity are not established by this.

LastPrefTab now remembers unfiltered close and restores on unfiltered open; search
does not overwrite that preference. Source LLFloaterPreference::onOpen/onClose
contracts apply, with native persistence at close because this lifecycle has no
general shutdown-save hook. Test171 checks close/reopen/search behavior. Native
tab selection/arrow bounds now use the same filtered list as layout; test142 covers
hidden leading tabs, overflow recomputation, and keyboard skipping/restoration.
Global/account persistence routing and loaded-account gating pass test172; live
warning suppression obeys OK/Cancel in test173. These checks do not close remaining
unbound graphics, asset picker, backup, or account services listed below.

Inspected source LLPanelPreferenceGraphics postBuild/backend selection/restart,
indirect avatar limit conversions and labels, FSPanelPrefs construction/postBuild/
onOpen/apply/cancel/beam lists/permission callbacks, FSPanelPreferenceBackup
postBuild/path selection/confirmation/selection and backup/restore file contracts,
LLFloaterPreference search/copy/onClose/OK/Cancel, and ll::prefs search traversal.
Native Graphics implements backend prompt and indirect-limit calculations; native
Viewer implements prelogin inventory gating and directory-backed beam lists;
Backup implements original row selection and confirmation-gated typed requests.
Hardware recommendations, preset operations, wireframe, beam editors/deletion,
default permissions, anti-spam clearing and backup/restore storage remain explicit
unbound service edges. No GL visual controller or GL feature manager is invoked.

UI gaps handled as components, then integrated: nested checkbox internal-button
commit callback (LLCheckBoxCtrl constructor preserves button commit alongside its
own click callback); widthless checkbox fitting (source reshape/reshapeToFitText);
comma-separated source RGBA literals; inline scroll-list text/checkbox rows with
native checkbox images; text-owned embedded inventory targets using the same
source copy/transfer acceptance rules as the line-editor target. Factory rollback
exceptions now include owning panel/control identity. Owner-scoped find avoids
hidden child-dialog controls with the same name receiving test actions.

Native texture swatch component implements source asset/item/tracking identity,
preview/select/cancel state, immutable caption/border/multiselect controls, native
image/fallback painting and generation-checked preview publication. Inspected
LLTextureCtrl constructor/destructor/showPicker/closeDependentFloater/onFloaterCommit/
setImageAssetID/input/draw; full picker inventory/local/bake branches and asset
acquisition remain open. The original texture_picker declaration constructs, but
its picker callback/asset service are not bound and it is not a completed picker.

Checks: widget171/171 passes. Test171 opens via menu, recursively selects and
paints more than 40 tabs, verifies search/no-results/clear, copied search SLURL,
and Backup confirmation/selection requests. Test169 exercises original Graphics
checkbox callback, test170 checks texture selection/rollback/stale publication and
paint. Window3/3 passes after presenting original Preferences Backup in the actual
GL-free Vulkan lifecycle. A dedicated Native Viewer Link Validation task is running;
no current viewer-executable link success is claimed yet. Search highlighting,
filtered-tab scroll bookkeeping, last-tab persistence, complete account/color/
notification transactions and child-dialog draft inclusion still require work.

The earlier skin shutdown test failure was a fixture violation of the existing
0.5-second click-through guard, resolved by testing before/after that deadline.
Untouched skin catalog fallback no longer counts as an edit. Live transaction tests
now edit an original General control rather than the removed provisional controls.
Passing these checks does not establish measured visual parity or service closure.

Network/Files and Skins continuation (2026-09-11, bdb5e59486 source, Windows,
NV-00/01/03/15/17): inspected LLFloaterPreference onClickSetCache/changeCachePath,
onClickResetCache, sound-cache counterparts/setSoundCacheLocation, directory-open
callbacks, log-path selection/reset, cache confirmation callbacks and
onClickJavascript. Native original panel_preferences_setup.xml now constructs
with staged cache-location settings, sound-path selection/reset, native Shell
directory opening, folder picking and original confirmations. Source prelogin
account gating is retained. The native picker reuses the native worker/pump owner,
with a Shell folder dialog and cancellation timer; no GL picker/window is called.
Browser startup now consumes saved JavaScript/cookie options. Widget167/167 passed
for original-panel construction, selection/cancellation/reset/open routing. Native
window library compiled after Shell integration; context9/9 passed. Cache purge/
relocation consumption, account log migration, live browser options and a real
folder-dialog cancellation runtime check remain open.

Inspected LLPanelPreferenceSkins constructor/postBuild/apply/cancel, skin/theme
callbacks and refreshSkinList/refreshSkinThemeList/refreshPreviewImage, plus
LLFloaterPreference onBtnOK/onBtnCancel/onClose. Native panel_preference_skins uses
the original skins.xml installed-directory catalog and named preview image assets,
with panel-owned skin/theme drafts. Native button image replacement only changes
CPU image references; existing immutable paint/publication ownership applies.
Acceptance merges skin/readable-name/toolbar-reset and StarLight defaults into
the native settings save transaction. Cancel reloads saved drafts. Original skin
notifications are constructed and a native shutdown callback is bound. Catalog,
draft and preview tests passed at widget168/168 before the acceptance test extension.
The latest extended test168 FAILS at 'shutdown handler invoked'; persistence
assertions pass, but notification response/queue routing remains unresolved after
three fixture corrections. Current widget result is 167/168, not a passing gate.
Do not claim skin restart behavior or full Preferences parity from this work.

The user-authorized stalled compiler tree (23468 and eight children) was killed.
The viewer rebuild subsequently lost its MSBuild/CMake owners while compiler25100
and eight children remained; that orphaned tree was also killed. No current viewer
link proof was obtained. Use a dedicated VS Code task for the next full viewer build;
do not reuse the command terminal while a long-running build owns it.

Network/Proxy continuation (2026-09-11, bdb5e59486 source, Windows,
NV-00/01/03/15/17):
LLFloaterPreferenceProxy postBuild/onOpen/onClose/saveSettings/onBtnOk/onBtnCancel/
cancel/onChangeSocksSettings and LLStartUp::startLLProxy were inspected. The dialog
snapshots bound settings, repairs invalid HTTP proxy selection, and saves/deletes
SOCKS5 credentials through the protected-data handler. Runtime setup separately
negotiates SOCKS UDP and enables the selected HTTP proxy. LLProxy::applyProxySettings
uses CURLPROXY_HTTP or CURLPROXY_SOCKS5 (local DNS), and password credentials only
for authenticated SOCKS. LLSecAPIBasicHandler explicit-path construction/init,
credential load/save/delete and protected read/write helpers, LLMachineID::init/
getUniqueID/getLegacyID and Windows WMI initialization/query/cleanup were inspected.
These are CPU-only file/crypto/COM services; no visual owner is installed. Explicit
credential paths skip the handler's default certificate-store initialization.

Q2/Q3: LLVKProxy selects typed endpoints independently of GL network/UI owners.
Native translation curl handles explicitly select direct/HTTP/SOCKS transport and
separate credential options before submission. Direct mode explicitly clears the
proxy, and configured modes clear environment bypass lists. Unlike the source's
implicit curl defaults, environment proxies cannot override the viewer selection.
Native Dullahan startup consumes the browser-specific HTTP proxy endpoint. The
original proxy XML has native callbacks, settings snapshots, Cancel/X rollback,
masked credential inputs, HTTP-choice repair, protected-save failure retention,
and cleared credential editors on close. Credentials use the existing
user_settings/bin_conf.dat credential/SOCKS5 record, not ordinary LLControlGroup
settings or a second persistent credential service. Native startup initializes the
machine-ID service before native workers and injects exact-record callbacks.

The existing handler uses RC4 with an obfuscated stored key, not OS-backed
protection, and swallows some write errors. Native save therefore copies the
encrypted store to an exclusively created staging directory, invokes that handler
there, reloads/verifies the requested record, and only then uses atomic Windows
replacement with write-through. Failed staging cannot delete the live store.
No changes were made to the GL credential implementation or its format. Existing
crypto/read failure handling, concurrent writers and complete ancestor reparse
checks remain follow-up risks; staged verification does not prove those closed.

Checks: widget166/166 passed with explicit policy branches, original dialog
callbacks, save failure/retry, Cancel/X, masking/editability, credential-field
clearing, staged failure preservation and temporary-file retirement. Window3/3
passed after HTTP consumer wiring, before production credential binding. GPU
context9/9 passed and llvkwindowmgr compiled after credential factory binding.
Actual proxy-server handshakes and production credential codec runtime tests remain
open. This curl package lacks CURLOPT_SOCKS5_AUTH; its available username/password
options are used. SOCKS UDP negotiation, live browser reconfiguration and source
post-login restart notifications remain open. The stalled compiler tree was
terminated at the user's explicit request; the real viewer rebuild is in progress.

Notifications continuation (2026-09-11, source bdb5e59486, Windows,
NV-00/01/03/15/17): inspected LLFloaterPreference buildPopupList/onSelectPopup/
onUpdatePopupFilter, LLNotificationForm construction/getIgnored/setIgnored and
handleIgnoredNotification; online-notice dependency and Growl capability callbacks.
Original panel_preferences_alerts.xml now uses native checkbox rows and a filter
editor. Ignore metadata is parsed from the already Expat-validated/merged original
notification XML using Boost.PropertyTree, including form templates/substitutions,
global-control overrides/inversion and default/last-response metadata. Existing
gWarningSettings is passed into native startup and persisted through LLVKSettingsMgr
to user_settings/ignorable_dialogs.xml. Native fallback test owners use the same
LLControlGroup implementation, not a second settings implementation.

Q2/Q3: native metadata and UI callbacks avoid LLNotificationForm's LLUI singleton
and GL notification pipeline. Suppressed queueNotice calls return the original
default button response, or the existing saved response, without creating a modal.
The list retains source true=show/false=suppress semantics. Desktop notifier
availability is explicit; GrowlManager cannot be reused because its constructor
installs viewer notification/chat/script callbacks. A native desktop notifier has
not yet been bound. Session-only delivery, last-response persistence, template
overrides/custom forms, source-exact filtering and save-failure recovery remain
open; original visible notice types outside the existing native form set remain
unsupported rather than replaced with fabricated dialogs.

Tests165/165 pass. Test165 constructs the original Notifications panel, checks
template labels and shared warning mutation/persistence/filter behavior, and proves
OutboxFolderCreated suppression invokes option0 with OK_okignore=true without a
modal. Native window tests3/3 pass after the Warnings-group startup signature change.
Actual viewer relink remains blocked by the existing compiler tree holding its PCH;
no compiler process was terminated or build file deleted. Full Preferences remains
provisional and this is not the requested parity gate.

Privacy and Block List continuation (2026-09-11, Windows RelWithDebInfo,
source bdb5e59486, NV-00/01/03/12/15/17): original Privacy XML now constructs
with native localized pre-login autoresponses, DebugLookAt integer/checkbox
synchronization, read-only inventory target and disabled account-history controls.
LLPanelPreferencePrivacy construction/postBuild/onOpen/saveSettings/apply/cancel
and parent pre-login initialization were inspected. Its pre-login snapshot only
includes AutoDisengageMic; native panel snapshot allowlists preserve this exception.
FSCopyTransInventoryDropTarget construction/postBuild/handleDragAndDrop were
inspected: native data acceptance checks supported item kinds, copy/transfer,
non-link/non-folder and distinguishes hover from drop. Actual native inventory
drag producers, account item lookup and post-login behavior remain open.

Block List source roots: FSPanelBlockList construction/postBuild/refresh/filter/
sort/selection/predicates/remove/toggle/picker callbacks, FSFloaterBlocklist,
LLFloaterGetBlockedObjectName and LLMuteList add/remove/isMuted/isLinden/updateAdd.
The existing mute constructor resolves viewer objects, and mutation calls reach
avatar lights, notifications and server messages; it is not reused by native code.
Native LLVKMuteList implements explicit resolved-ID/name entries, inverted allow
flags, self/staff restrictions, capacity, exact legacy names and queued change
records. These records do not yet have server/visual/notification consumers.
Native Block List controller constructs the original panel, sorts and filters,
evaluates original menu predicates, removes entries, toggles resident flags and
uses unchanged floater_mute_object.xml for by-name additions. Original standalone
floater_fs_blocklist.xml opens when FSUseStandaloneBlocklistFloater is true;
cascading uses the prior native cascading rectangle and UIFloaterOffset, or the
native top-left available area. Saved group rectangles/full registry placement,
People sidebar route, resident picker/profile, mute-list loading/failure/cache/
server synchronization and authoritative self identity remain open. Block List
actions are not yet a functional substitute for the full account mute service.

Supporting native ownership: menu_button uses mouse-down and Return activation,
captured panel callback scopes, anchored popup positions and pressed-state cleanup.
Menu parser retains action/check/enable/visible fields; checked items paint the
source checkmark. Only bounded flat command popups are implemented. fs_scroll_list
supports the miscellaneous-item branch, desired row height and multi-selection
context retention; avatar-drag behavior is rejected rather than silently reused.
Explicit negative-width columns are hidden metadata, omitted widths remain dynamic.
Plain text supports horizontal use_ellipses; multiline last-visible-line behavior
is still open. Unprefixed callback elements and line-editor XML body values now
use the existing native scoped callback/value paths.

Two recursive stack regressions were diagnosed with LLDB without changing test
depth/stack limits. Native nodes now construct directly in their owning map entry
with exception cleanup, avoiding a roughly14KB stack temporary; recursive factory
panel-constructor parameters are heap-owned. Tests164/164 pass: test161 covers
inventory acceptance/deletion, test162 unchanged Privacy and Block List panels,
snapshot policy, original menus/flag toggles/object picker and standalone lifecycle;
test163 mute decisions/change records; test164 menu-button scoping and popup
lifecycle. No full Preferences, RLVa, or runtime account parity is claimed.

Sound and Media continuation (2026-09-11, source bdb5e59486, Windows OpenAL;
NV-00/01/03/12/15/17): original panel_preferences_sound.xml now constructs via
native panel_preference_sounds, fs_panel_preference_ui_sounds and
panel_voice_device_settings controllers. The live full Preferences root remains
provisional; this is not completion of Preferences or audio/voice parity.

Source roots inspected: FSPanelPreferenceSounds postBuild/onMoapInteractionChanged/
updateMoapInteractionSetting; volume_controls_on_click_set_sounds and
volume_controls_set_control_false; LLPanelPreference::updateMediaAutoPlayCheckbox;
FSResetVoiceTimer and LLFloaterPreference::onClickResetVoice; all
FSPanelPreferenceUISounds construction/list/editor/commit/reset/preview/filter/
localization/context callbacks; LLPanelVoiceDeviceSettings construction,
postBuild/visibility/draw/refresh/initialize/cleanup/device commits/apply/cancel.
Q1: the sound controls coordinate mute and autoplay enablement, preserve the media
interaction bitmask, edit source UI-sound UUID/playmode settings, enumerate voice
devices, and tune microphone gain/energy. Snapshot sound uses inverted Boolean
playmode; three IM sounds use four-valued modes. OpenAL hides the FMOD-only output
selector. Voice reset disables voice for five seconds before enabling it again.
UI sound labels resolve through ancestors after hierarchy attachment.

Q2/Q3: native scoped panel controllers use the shared control service and native
list/editor widgets. LLFilterEditor source behavior is independently implemented
as search behavior followed by a commit on every keystroke, including navigation,
with no focus-loss recommit; its original filter_editor.xml defaults are separate.
Native list checkbox cells toggle before selection callbacks, propagate to selected
rows, and paint checked/unchecked/disabled skin images. Original dotted slider
and checkbox label alignment parameters now feed native label layout. Enable and
disable bindings follow source choice-style overrides, not an invented AND rule.
The locate tag constructs a non-drawing native control, not a plain untyped view.

Native VoiceDevices in llvkwindowmgr lazily owns the existing nonvisual llwebrtc
device interface, without LLVoiceClient/LLWebRTCVoiceClient UI or channel owners.
Inspected llwebrtc init/terminate/device observation/enumeration/tuning/gain and
public entry points: native WebRTC threads, platform audio device module and CPU
audio processing, no GL visual dependency. Worker notifications copy device lists
under a native mutex; observers detach before engine shutdown. An unsynchronized
shared observer vector was found and fixed with a recursive mutex and guarded
notification snapshot. Existing viewer OnDevicesChanged only queues copied data;
GL visual functions were unchanged. Native service creates no peer connections.
Tuning starts only for a visible enabled device panel. Five native rectangle meters
use source energy conversion, 0.7 threshold, and speaking/overdriven colors.

Checks: widget159/159 passes. Test141 covers filter callback ordering/navigation/
focus; test158 checkbox toggles, selected-row propagation, images and callback row
replacement; test159 unchanged full sound hierarchy, every tab CPU painting,
localized UI-sound rows, UUID edits, selected preview dispatch, inverted snapshot
playmode, IM mode, reset, filter clear, media mask, injected device selection,
tuning start/stop and meter commands. llwebrtc builds after observer synchronization.
Integrated window3/3 passes with actual WebRTC initialization/device enumeration
and teardown through the original panel; EnableVoiceChat remains false in that
test so no microphone capture is requested. Hardware microphone levels and actual
voice transport are not verified by injected energy or enumeration.

Open: Media Lists auxiliary dialog, UI-sound right-click Copy UUID/double-click
preview/default tooltip, source-exact filtering/selection and per-cell callbacks,
dynamic UI-sound transaction registration and disk acceptance, voice popup device
refresh/rollback/processing settings, immediate hide/delete tuning transitions,
hardware capture/hot-plug/failure paths and source backend alternatives. WebRTC
shutdown still has a pre-timeout BlockingCall and other device-state threading
obligations; observer synchronization alone does not close its lifetime audit.

Media Lists and sound follow-up (same source/configuration, NV-00/01/12/15/17):
FloaterMediaLists postBuild/add/remove/handleAddDomainCallback and
LLViewerParcelMedia extractDomain/saveDomainFilterList/loadDomainFilterList plus
the first-rule media decision loop were inspected. Native LLVKMediaFilter owns
ordered allow/deny LLSD rules and a revision for cached-decision invalidation;
alphabetical dialog display does not reorder policy. The legacy normalization
and suffix matching are explicit, including the lack of a DNS dot-boundary check.
This is not standards-compliant host matching or an approved security improvement.
Native showMediaLists constructs unchanged floater_media_lists.xml, uses original
AddToMediaList confirmation/localized list names, and publishes add/remove changes
through an optional account persistence callback. Source LL_PATH_PER_SL_ACCOUNT
is empty before an account is known; no global media-list filename is invented.
Native account-file lifecycle and actual parcel-media consumption remain open.

Original Media Lists requires resizing. Native floater parameters now retain
can_resize/min_width/min_height; pointer-captured edges/corners update native
rectangles with minimum sizes and child follows. The original Resize_Corner image
is painted. LLFloater::addResizeCtrls and LLResizeHandle pointer/capture/minimum
geometry were inspected. Source snapping/docking, resize cursors, full edge
geometry qualification, overflow and persisted rectangles remain open.

UI-sound default UUID tooltips, double-click preview and right-click Copy UUID
are now wired. Native LLVKMenu supports bounded command context menus anchored
at a screen point with source upward/leftward overflow placement, normal keyboard
activation and dismissal. LLContextMenu::show/hide was inspected; nested context
submenus, spawn-release movement thresholds and measured visual parity remain open.
The source UI-sound controls directly mutate settings and are not part of the
base panel bound-control snapshot. Native direct edits now invoke the configured
settings writer before publication, with failure restoring the editor. This saves
earlier than source shutdown persistence; no claim of cross-file atomicity follows.

Widget160/160 passes: test159 now additionally checks original Media Lists add/
remove/Cancel, callback persistence and rule decisions, corner resize and repaint,
UI-sound save rejection, five-second voice reset, default tooltip, pointer-driven
context Copy UUID with isolated clipboard, and double-click preview. Test160
checks normalization, rule ordering, suffix matching, revisions and invalid-load
nonmutation. Last real-window3/3 predates these Media Lists/context follow-ups.
Full Preferences root and remaining panels are still incomplete.

Shared settings continuation (2026-09-11, source bdb5e59486, Windows
RelWithDebInfo; NV-00/01/03/15/17): the user requested the existing settings
service and the filename llvksettingsmgr. LLVKSettingsMgr replaces the former
LLVKStartupSettings name and no longer contains an Entry table or independent
default/saved/transient stacks. It adapts LLControlGroup and LLControlVariable.

Q1: inspected LLControlGroup construction/cleanup, declaration, getControl,
loadFromFile, saveToFile, applyToAll, and LLControlVariable construction,
setValue/setDefaultValue/resetToDefault/getSaveValue, typed comparison and sanity
signals. Value conversion/comparison uses LLSD, generic string conversion and
CPU math types; file loading uses LLSD XML and generic IO. Signal targets belong
to the caller, not the storage implementation. The optional LL_SETTINGS_PROFILE
cleanup path writes access statistics through generic file/directory utilities.
No existing visual settings subscribers are installed by this integration.

Q2/Q3: extract the unchanged parsed-document loop as LLControlGroup::loadFromLLSD,
with loadFromFile delegating to it. Existing viewer callers retain their original
semantics. After backend selection, llvkStartup receives and loads the existing
gSavedSettings, gSavedPerAccountSettings and gCrashSettings groups passed from
Windows entry. Its preliminary backend probe uses a temporary instance of the
same service, not another implementation. LLVKSettingsMgr manages bounded XML
input and staged file replacement around those groups. Native widget bindings
retain control identities, send edits through service validators as transient
values, and observe service commit signals through scoped connections. The
widget setting table is a UI cache, with local-only controls still supported.
No GPU resources, GL callbacks or renderer-specific storage are introduced.

Native input compatibility explicitly maps the account declarations' Integer
spelling to S32, rejects unknown types/type mismatches before mutation, and
supplies a descriptive comment for previously tolerated commentless definitions.
Boolean XML storage remains the existing service's representation; native UI
views expose Boolean values. Staged saves preserve unrelated file entries and
promote accepted values only after successful replacement. Validation currently
runs before staging and again when publishing saved values; reentrant validators,
concurrent writers and multi-file atomic acceptance remain open obligations.
Account identity/load/save after authentication and complete Preferences behavior
are still incomplete; this integration does not close the full parity gate.

Checks: existing LLControlGroup tests4/4 pass after loader extraction. Widget
tests157/157 pass after the manager rename, including precedence/failed-save
tests128/132 and new test157 proving shared control identity, two-way updates,
validator veto, retained saved layers, structured-value notification deduplication,
nested service writes and scoped disconnection. Integrated window tests3/3 pass
with real global/account service bindings before the final filename rename.

Post-18701018f4 integration work (2026-09-11, NV-00/01/12/15/17): the requested
partial checkpoint was committed as18701018f4. Full Preferences and RLVa
integration remain the active gate, not completed by these changes.

Continuation after18701018f4 (Windows RelWithDebInfo, NV-00/01/03/12/15/17):
the nonverbose build-driven widget runner exhausted its stack in test50 while
the verbose runner passed. LLDB identified recursive native panel construction,
not an assertion failure. Heap-owning the nested Parser in the construction
callback restored the unchanged depth-limit/cleanup regression. Neither the
thread stack size nor the test depth was changed. The canonical build runner
now passes149 tests; verbose-only success was not used as closure evidence.

List source roots are LLScrollListCtrl constructor/updateLayout/updateColumns/
setSelectedByValue/drawItems/handleMouseDown/handleMouseUp/handleKeyHere/
setSort/updateSort/onClickColumn and SortScrollListItem, plus
LLScrollColumnHeader constructor/onClick/draw policy. Q1: lists retain ordered
rows, enabled selection, row-unit scrolling, column widths, stable dictionary
sorting with previous sort keys, and button-owned column headers. Q2: these are
CPU layout/input/data decisions; native font/image/button owners produce the
existing ordered Vulkan UI packet. Q3: the native ScrollList owns rows, columns,
selection anchors, sort keys and child header/scrollbar IDs; immutable parameters
retain resolved original templates. LLStringUtil::compareDict is nonvisual
string ordering, not a GL visual wrapper. Tests70/148 cover original list/header
XML and skin assets, row clipping/scrolling, enabled selection, stable sort and
anchor identity. Alternate/custom sorting, sorting-column redirects, heading
resize, all cell types, type-ahead/filtering and full reentrant rollback remain
open. The current implementation is not complete scroll-list parity.

AutoReplace source roots are LLFloaterAutoReplaceSettings postBuild, selection/
entry/list/priority callbacks, import/export/name-conflict/delete confirmation,
onSaveChanges/onCancel; LLAutoReplace loadFromSettings/saveToUserSettings and
LLAutoReplaceSettings validation/mutation/replaceWord. Q1: ordered named LLSD
replacement maps are copied into a dialog draft, list priority selects the first
matching replacement, Save writes the draft and enable flag, and Cancel discards
the draft. Import/export operate on one list, preserving duplicate-name choices.
Q2: native LLVKAutoReplaceSettings owns those nonvisual values independently;
the unchanged floater_autoreplace.xml creates native controls and callbacks.
Q3: native dialog fields and a generation token protect queued prompt/file-picker
responses across close/reopen. No GL floater, file picker, settings singleton or
notification callback is invoked. The source's punctuation-aware keyword check
uses audited LLWStringUtil character classification. Native startup loads the
user file before app defaults/examples and binds actual accepted-file writes.
LLSD file IO is bounded and replacement is staged; list and enable settings are
two ordered writes, not a cross-file atomic transaction. Tests131/149 exercise
draft edits, New List, imported/exported files, Save/Cancel and exact LLSD round
trips. Text-input AutoReplace consumers and the full Chat Preferences owner are
not yet integrated; model replaceWord alone does not establish that integration.

AutoReplace notice source roots are the five original notifications.xml forms,
LLToastAlertPanel form parsing/layout/default delay/onButtonPressed, and native
modal focus ownership. Native declarations preserve message substitutions,
input names, button labels/defaults/response indices. The native presenter owns
the input and button controls and tears down its modal before responding; Win32
routes ordinary editing and Tab within the modal without invoking menu shortcuts.
Tests131 preserve the RLVa GenericAlert case and exercise editable form response.
This is not the complete notification framework: arbitrary forms, caution/title
policies, exact layout parity and alert sound closure remain open.

File-picker source roots are LLFilePicker getOpenFile/getSaveFile,
check_local_file_access_enabled and LLFilePickerReplyThread startPicker/notify.
Native Windows XML selection uses its own worker, COM apartment, OPENFILENAMEW,
XML filter, overwrite confirmation and optional-path cancellation result. Worker
completion is delivered on the native main loop; the service disconnects before
destruction, requests cancellation through its dialog hook, and services Windows
owner messages while joining. Actual integrated window/audio2/2 passed with
AutoReplace and a real XML picker active through six Vulkan frames and shutdown,
with no OpenGL parent module. Automated accepted native file-dialog selection,
shell-extension failures and all shutdown interleavings remain unqualified.

AutoReplace's original can_close/can_minimize declarations also exposed native
floater policy gaps. Source LLFloater initFloater/setMinimized/closeFloater and
LLFloaterView getMinimizePosition define the behavior. Native chrome now closes,
minimizes/restores the source header height and expanded geometry, and preserves
child visibility/draft contents. Tests131 drive the real minimize pointer route.
Multiple minimized-window slot packing, remembered dragged-minimized positions,
dependent floaters, saved-rectangle registry, full resize policy and window sounds
remain open. Source declarations and GL implementation were left unchanged.

Original Chat FSKeywords requires editable multiline text and focus-loss commit.
Controls/Move & View continuation (NV-00/01/12/15/17, source18701018f4):
Post-bdb5e59486 continuation: the user has explicitly classified Preferences
parity as a chokepoint and a viewer without settable Preferences as a failure.
That gate remains open; neither the Controls increment below nor the existing
provisional Preferences window meets full parity.

Source LLPanelPreferenceControls postBuild/populateControlTable/onModeCommit/
onListCommit/onSetKeyBind/onDefaultKeyBind/onRestoreDefaultsResponse and
LLSetKeyBindDialog postBuild/onOpen/onClose/recordAndHandleKey/handleAnyMouseClick/
onClickTimeout define the current native controller increment. Q1: mode changes
rebuild original section/action rows and suppress empty unassignable commands;
binding-cell clicks open the original modal, keys and mouse combinations replace
the selected slot, Cancel preserves it, Default resets it, and the defaults prompt
chooses all/current/no modes. Q2: native panel callbacks own the native binding
maps; original table files, fonts and images feed native list painting. Q3: one
native dialog owns capture state and a pending single-click deadline, with
keyboard/mouse events routed before menu or ordinary widget handling. The
source's modifier-release capture,0.7second single-click delay, double-click
release, Clear, Cancel and mode restrictions are explicit native state. Original
yesnocancelbuttons template values supply defaults-confirmation responses.
LLVKFloater can_close controls chrome, not programmatic close. Floater-scoped
string declarations use the native panel string owner. No GL input or modal
owner is called.

Native Preferences now snapshots binding maps on open, restores them on Cancel,
and sends changed accepted maps to startup-owned key_bindings.xml persistence.
Tests131 cover original Controls construction and mode rows, captured/localized
binding labels, keyboard/mouse/default operations, Cancel non-write and accepted
binding callback. Integrated window test1 sends an actual WM_KEYDOWN to the
native window and checks the binding table update; widget152/152 and window2/2
pass. A teardown access violation was found with LLDB: the new floater member
had been declared before the widget tree, causing reverse destruction to access
a destroyed tree. Its owner is now declared with the other floaters after the
tree and the unchanged destruction regression passes.

Still open: original live Preferences root, full mode/reset/search refresh
coupling, context-cone drawing, complete menu reservation/name conversion,
keypad-distinction policy, character-message isolation after keyboard capture,
multiple Controls owners, cross-file transaction failure semantics and runtime
in-world input consumers. Current Controls tests do not qualify those gaps or
the remaining original Preferences panels. The keyboard name table is native;
the tested strings retain source MMB/Double LMB labels, not invented labels.
The matching keyboard release is now consumed after closing capture, with a
test proving that a subsequent unrelated release is not swallowed.

Joystick prerequisite (Windows, source bdb5e59486): LLFloaterJoystick device
enumeration/initFromSettings/cancel/updateAxesAndButtons and LLViewerJoystick
di8_devices_callback/EnumObjectsCallback/initDevice/updateStatus were inspected.
The source requests DirectInput game controllers, stores binary GUIDs, negotiates
[-3000,3000] axes and normalizes polling values. Native LLVKJoystick owns a
DirectInput interface/device and an explicit eight-axis/32-button state without
using the viewer's joystick or window owners. The packaged ndofdev_external.h
declares six axes while this source updateStatus loops over eight; native code
does not copy that out-of-bounds access. An independently bounded DIJOYSTATE
buffer supplies the native axis values. Failure to poll clears live readings
and marks the device disconnected; invalid identities do not fabricate a device.
Real-window tests3/3 cover actual interface enumeration, malformed GUID rejection,
no-selected-device polling and teardown. Connected-device readings, driver/range
quirks, hot-plug and thread/fault qualification remain open.

Post-bdb5e59486 continuation (NV-00/01/12/15/17): the window now binds that
DirectInput owner through scoped enumeration, identity selection and polling
callbacks, cleared before owner destruction. Original floater_joystick.xml is
constructed unchanged. Its native controller selects binary device identities,
persists canonical GUID strings, previews eight axis samples/sixteen button lights,
applies the source Windows SpaceNavigator defaults and restores settings/device
selection on Cancel. Native container_view/stat_view owns declaration-order rows,
required heights and collapse state; the scroll-container constructor accepts this
native document owner. Native stat_bar owns bounded samples, range interpolation,
bar/history/label modes and native text/rectangle painting. Test153 checks native
row geometry, collapse ownership and sample painting. Test154 checks original
joystick construction, injected device readings, defaults/Cancel and original
Move & View callbacks. Real-window tests3/3 exercise actual DirectInput services
and original dialog painting, not connected hardware parity.

Move & View now uses the shared native binding store for single/double click
walk/teleport selection and the derived DoubleClickTeleport setting; its original
joystick button invokes the native floater directly. Open obligations include
flycam/world consumers, complete cross-panel refresh, device hot-plug/ranges,
container viewport negotiation/reentrancy, stat_view setting-backed expansion,
and source LLTrace period aggregates, rapid-change mean/median/autoscaling.
Per-frame sample history is not proof of those source statistics contracts.

Crash Reports continuation (NV-00/01/15/17, Windows source bdb5e59486):
LLPanelPreferenceCrashReports postBuild/refresh/apply/cancel reads gCrashSettings,
keeps local checkbox drafts, enables subordinate consent fields only when sending,
substitutes its privacy URL and conditionally shows the BugSplat restart notice.
Apply maps send/ask to 0/1/2 and writes consent fields to the separate crash group.
Native Q2/Q3: independent CPU draft controls plus a dedicated crash-settings map
and save callback avoid reusing GL controls or contaminating global settings.
Startup loads defaults/user settings_crash_behavior.xml and uses the existing
native staged writer for that file. Test155 verifies original controls, privacy
URL, conditional restart notice, Cancel, save rejection and dedicated accepted
writes with no global-writer call. Cross-file atomic acceptance and actual crash
reporter startup/submission remain open; this is not crash reporting parity.

Advanced reset continuation (NV-00/01/15/17, same source/configuration):
LLFloaterPreference onClickClearSettings/callback_clear_settings confirms via
FirestormClearSettingsPrompt, creates logs/CLEAR, then shows SettingsWillClear.
LLAppViewer init/initConfiguration removes enumerated files, matching
feature*.txt/gpu*.txt/settings_*.xml and the selected global settings file, plus
specified immediate per-account files. LLFile::rmdir is nonrecursive; directories
containing custom content survive. LLFile::remove does not glob the literal
screen_last*.* argument, so native reset does not invent snapshot wildcard deletion.
Native Q2/Q3: CPU-only marker scheduling and a bounded filesystem consumer,
independent original notifications/control callbacks, and reload of native settings
before native window creation. GL startup remains untouched. Backend selection
precedes destructive reset and remains process-exclusive after reloading defaults.
Failures are explicit and retain the marker for retry; linked paths and nonlocal
custom settings filenames are rejected. These are deliberate safety differences
from source best-effort deletion, not measured parity. Test156 uses an isolated
UUID-named temporary profile and proves no-marker nonmutation, deferred consent,
invalid-path nonmutation, bounded file removal, custom/nonempty-directory retention,
marker completion and idempotence. Test155 exercises the unchanged Advanced panel
and cancellation/confirmation notification paths. Widget156/156 passes. Native
multi-instance reset coordination, crash interruption, filesystem race resistance,
all Advanced setting consumers and full startup reset parity remain open.

The full live Preferences root is still provisional. These tested panel/controller
increments do not satisfy the user's Preferences parity gate.

LLFloaterPreference updateClickActionControls/updateClickActionViews dispatch
through LLPanelPreferenceControls, not independent boolean settings. Source
LLKeyConflictHandler registerControl/removeConflicts/generatePlaceholders and
loadFromSettings/saveToSettings define mode restrictions, exact-combination
conflicts, reservation veto, the nonconflicting script-left-click mask and
four-mode XML storage. LLKeyData/LLKeyBind were inspected through their LLSD,
matching, duplicate replacement, reset and trim methods: these are independent
nonvisual value algorithms in llcommon and may be reused without LLKeyboard,
LLViewerInput or GL UI owners. Native LLVKKeyBindings owns the mode maps and
stages conflict changes before publication; its bounded Expat parser has an
independent key/mouse/modifier name table. Structured Boost.PropertyTree XML
serialization and staged file replacement retain modes and accepted bindings.
Tests152 verify reservations before mutation, mode restrictions, click conflicts,
the original binding XML, invalid-input nonmutation and serialized round trips.
The file writer also passes an actual disk round trip and a staging-contention
test proving that a failed write preserves the accepted file. No native Controls
controller, runtime dispatch or Move & View callback
is connected yet; the settings-file temporary preview protocol, localized labels,
key-capture dialog, menu reservation wiring and reset workflows remain open.

Controls table source roots are LLPanelPreferenceControls addControlTableColumns/
addControlTableRows/populateControlTable, LLScrollListCtrl selection_type and
LLScrollListItem/LLScrollListIcon/LLScrollListIconText draw/metrics. Native lists
now retain selected binding-cell indices and per-cell highlights; non-sortable
headers do not change sorting. Native text/icon/icon-text styles retain fonts,
images and alignment, and row metrics account for those cells. Factory
loadListContents reads the unchanged columns/rows files into native data through
the existing skin-layer selection and independently owned Expat state. Tests70
load original four binding columns and movement section/action rows and paint
the native section icon with source font metrics; tests148
exercise pointer cell selection and highlight geometry. An initial Expat handle
ownership defect was caught by the executable regression: a copied temporary
freed the parser early. The loader now constructs a noncopyable parser directly
in its final unique owner; the canonical152/152 suite passes afterward. Full
cell-type coverage, exact text/icon raster parity, content-loader attribute
coverage and the live Controls panel remain open.

The actual viewer7.2.5.79283 was relinked after canonical widget152/152 and
integrated window/audio2/2 passed with the latest native libraries. The viewer
link used BuildProjectReferences=false after those focused dependency builds;
this is not a full all-target build or measured parity result. The viewer was
not launched for a new live-user workflow capture at this increment.

The later Chat integration now constructs panel_preferences_chat.xml through the
production panel_preference owner with native AutoReplace/SpellChecker/
TranslationSettings and ResetPerAccountControl callbacks. Native startup loads
the original per-account defaults separately from global defaults; account reset
uses only that default group, with no account persistence before account login.
Conflicting global/account names currently fail explicitly, not a full namespaced
setting resolver. Source LLFloaterPreference postBuild hides the offline-email
checkbox/link before personal info and leaves the login-required label visible;
the native owner applies those states. Keyword master availability follows the
original pre-login XML; post-login enabling remains unimplemented.

Source LLXUIParser::readXUIImpl submits direct body text as value. The CmdLine
panel's literal dot is therefore retained as a nondisplayed native control value,
not removed from the source file. Native inline control commit_callback.function
and parameter/userdata use the same deferred slot as nested callback elements.
Text label is accepted as the unused LLUICtrl base parameter, preserving displayed
body text; skip_link_underline selects hover-only native link underlines.
The keyword swatch's zero border thickness applies to its child bevel border,
not its separate fixed outline. Tests131 exercise these source declarations.

Native spelling source roots are LLSpellChecker dictionary discovery/activation/
checkSpelling/getSuggestions/ignore loading/setSecondaryDictionaries/removal,
LLFloaterSpellCheckerSettings postBuild/refreshDictionaries/commitChanges/move/
remove/onClose, and LLFloaterSpellCheckerImport browse/import/parseXcuFile.
Q1: dictionary metadata overlays user entries by language, installed word files
may come from user or app paths, primary activation requires an affix/word pair,
secondary removal rebuilds Hunspell, and only inactive user dictionaries can be
removed. The settings floater applies dictionary choices immediately and commits
on close, unlike AutoReplace's isolated draft. Q2/Q3: LLVKSpellCheck independently
owns the packaged third-party Hunspell engine, catalog, primary/secondary names,
ignore data and filesystem paths; no LLSpellChecker/LLUI singleton is reused.
Original Spell Checker/import floaters use native controls and callbacks, live
settings subscriptions and the native dictionary-filtered Windows picker.
XCU resolution is bounded Expat parsing of ServiceManager/Dictionaries entries,
DICT_SPELL format, .dic location and %origin% substitution. Tests150 verify real
spelling/suggestions, ignore data, secondary activation/removal, source-file
preservation and XCU resolution. Tests131 use actual packaged dictionaries and
both original floaters. Custom/ignore editing APIs, editor misspelling consumers,
encoding/Unicode-path qualification, complete import error/rollback handling,
signal multiplicity and full import-dialog source parity remain open.

Native translation source roots are LLFloaterTranslationSettings postBuild/
onOpen/updateControlsEnabledState/key getters/verify callbacks/onClose/onBtnOK
and LLTranslationAPIHandler::verifyKeyCoro with Azure/Google/DeepL verification
URL/header/body/response methods. Q1: tentative key editors clear on focus,
service selection controls field availability, enabled translation requires a
verified selected service, saved keys are checked on open, Cancel leaves saved
keys unchanged, and the account TranslatingEnabled flag follows verification.
Azure expects400 plus valid JSON from its intentionally malformed probe; Google
and DeepL expect200. Q2/Q3: independent native dialog draft state and generation
tokens reject stale responses after edits/reopen; LLVKTranslation produces native
request data and checks provider results. Window-owned libcurl multi requests
retain body/headers/callbacks until completion, remove requests before callbacks,
and cancel outstanding requests before UI destruction. Responses are bounded
to1MiB, eight pending requests and30second timeout. Keys/URLs/bodies are not logged.
SECURITY DIFFERENCE, not parity closure: native verification requires HTTPS and
certificate/hostname validation and currently refuses redirects; the inspected
source disables peer verification and follows redirects. Header injection and
empty endpoints are rejected instead of preserving unsafe/undefined source paths.
The packaged curl uses CURLOPT_PROTOCOLS, not newer CURLOPT_PROTOCOLS_STR.
Tests151 verify request/response contracts without external requests or real keys;
tests131 verify original dialog controls, stale responses and Cancel using an
injected verifier. Real provider acceptance, redirect policy, proxy integration,
network fault/completion qualification and actual chat translation remain open.

Latest evidence after this continuation: canonical widget151/151 and integrated
window/audio2/2 pass. The window fixture prepares every original Chat subtab on
CPU, then hides that panel; it does not submit every subtab to Vulkan. Native
Spell Checker/Translation/AutoReplace and existing dialogs are included in the
presentation lifecycle with the real XML picker active. Full Preferences root
remains the reduced provisional implementation, RLVa handler/enforcement remains
absent, and no full startup-UI parity or full viewer rebuild is claimed here.

Source LLTextEditor::handleUnicodeCharHere/addChar/deleteSelection/cut/pasteHelper/
undo/redo/onCommit/focusLostHelper and LLTextBase cursor/line navigation define the
local edit contract. Native plain multiline input now owns draft text, selection,
bounded undo/redo revisions, literal non-URL input, UTF-8 limits, clipboard
normalization, caret geometry, horizontal/vertical navigation and cursor reveal.
Commit publishes the bound setting before the callback; construction applies
enabled_control after composite children exist. Win32 routes native multiline
typing, Return, deletion, clipboard and history commands before parent controls.
Tests70/147 exercise actual FSKeywords attributes, live enable/read-only changes,
draft-before-commit, clipboard, selection, history and caret geometry. Rich editing,
source undo grouping, overwrite/IME, complete page navigation, autoindent,
autoscroll/context menus and allocation/reentrant rollback closure remain open.
History currently has explicit256-revision/16MiB limits; this bounded policy is
not a claim of source unlimited-history parity.

Source LLFloaterPreference::onClickPreviewUISound calls make_ui_sound with forced
play mode; find_ui_sound resolves a UUID and null means silence. Source UI audio
uses nonspatial UI gain; decoded assets are WAV files named UUID.dsf in the sound
cache. Native previewUiSound/PreviewUISound resolves the actual setting and calls
the owned native sound service, bypassing only play-mode suppression. LLVKAudio
uses ALUT file-image decoding and owns each OpenAL source/buffer until completion
or shutdown. UI secondary gain/mute updates active sources; the window pumps
retirement and disconnects the player before audio destruction. Startup selects
FSSoundCacheLocation, CacheLocation or the LocalAppData build-profile directory;
the caller already supplies the_x64 suffix. Cached file reads are UUID-restricted
and bounded. Missing decoded assets explicitly identify the still-unimplemented
asset-fetch path, not playback success. Tests verify forced/null callback behavior
and actual silent-WAV playback/source retention/shutdown with window/audio2/2.
Fetch/decode cache population, preloads, priority/channel stealing, full cache
failure fallback, non-UI channels and streaming remain open.

Integration follow-up (2026-09-11, source d28b718b12, NV-00/01/03/12/15/17):
the user's acceptance gate remains full Preferences and RLVa parity. The live
Preferences window is still the reduced implementation and does not meet that
gate. Production native panel_preference construction now supports the audited
General/Colors application policies via constructPreferencePanel: source
LLPanelPreference::postBuild display-name enablement and map-radius alpha,
LLFloaterPreference::onOpen pre-login maturity choice, and onChangeMaturity rating
icons/legacy search restrictions. Scoped native callbacks replace test-only
maturity callbacks on this production path. Source LLAgentAccess begins with PG
access and no account identity in this pre-login configuration; this is not a
post-login maturity policy. Original General and Colors files are unchanged.
Other panel classes and their application effects are not completed by this
registration. Map-radius slider local state is included in native snapshots and
restored after swatch commits. Tests preserve the captured quantized slider value
separately from the original color alpha; no numeric tolerance was loosened.

Source RlvSettings::onChangedSettingMain formats RLVaToggleMessageLogin while
the handler is inactive and can still be enabled. Native startup subscribes the
RestrainedLove setting and queues the original localized GenericAlert on changed
values, including restoring writes. Its native single-button modal presenter
reads the implicit Close label from notifications.xml, uses the alert button
asset, native message measurement, focus locking/restoration and the0.5-second
Return-default delay recovered from LLToastAlertPanel. Win32 routes modal input
before login/menu input. Widget tests verify notices, duplicate suppression,
focus lock, delay and dismissal; window/audio2/2 passes with an actual queued
RLVa modal and production General/Colors construction. Alert shadow, caution
forms, sound, full translation reload and all notification policies remain open.

This does NOT activate RLVa. Reinspection of RlvHandler::setEnabled, constructor,
onLoginComplete, RlvUIEnabler constructor and RlvExtGetSet constructor/dispatch
confirms reachable agent listeners, inventory fetch, teleport callbacks, retained
commands, environment handlers and visual UI consumers. Existing implementations
cannot be installed in the native path under NV-01. Those missing native service
consumers remain required work, not permission to set an enabled flag or report
RLVa parity from the preference notice.

Source FSResetControl restores the declared default through its settings signal.
Native startup now carries LLVKStartupSettings::defaults separately from active
saved/transient values; ResetControl/resetPreference uses those defaults. Cancel
restores the pre-reset snapshot. Apply compares exact recursive LLSD values using
audited nonvisual llsd_equals rather than asString, preserving structured-setting
changes. Tests131/70 cover production panel policy, default reset/Cancel and array
change detection. Widget146/146 and integrated window/audio2/2 pass. These are
local integration checks, not the unmet full-parity gate.

Preferences Colors and snapshot continuation (post-d28b718b12, NV-00/01/12/15/17):
source LLFloaterPreference::getUIColor/applyUIColor reads the named color and
sets the user color table from a swatch. Native startup factory now binds these
callback names to independently owned native color state. The unchanged
panel_preferences_colors.xml constructs and every tab paints in test70; the test
also verifies a real UserChatColor preview and restoration. Original XUI names
DialogColorFg, which is missing from the source table, then initializes that
swatch from ScriptDialogFg. Source ParamValue<LLUIColor>::updateValueFromBlock
and LLUIColorTable::getColor use a magenta fallback for missing named colors.
Native swatch construction preserves that fallback and the subsequent correct
init value; the source declaration/table was not altered.

LLPanelPreference::saveSettings walks bound controls and saves colors separately;
cancel restores settings, skipping an empty InstantMessageLogPath and explicitly
excluded settings, then restores swatches and commits them. Native tree
snapshotPreferences/restorePreferences implements that CPU transaction with stable
widget IDs and tolerates removed swatches. Test146 verifies setting/color order,
restoration and skips. The live provisional Preferences dialog now uses subtree
snapshots instead of a fixed three-setting snapshot. The native snapshot is not
closure of presets-manager state, account-only settings, advanced-floater sharing,
minimap alpha synchronization or complete Apply/Cancel lifecycle. Colors-panel
tests do not mean all its LLPanelPreference::postBuild hooks are installed in the
live dialog; full Preferences and RLVa integration remain open.

Color swatch/picker continuation (post-d28b718b12, NV-00/01/03/12/13/14/17):
source LLColorSwatchCtrl constructor/set/setValue/onColorChanged/showPicker,
mouse handlers, draw, setEnabled and destructor define caption/border ownership,
RGBA values and RGB-only picker transactions. LLFloaterColorPicker constructor,
createUI/postBuild/initUI/setCurRgb/setCurHsl/updateTextEntry/onTextEntryChanged,
pointer region handlers, palette draw/drop, cancelSelection and select/copy
actions define the independently reconstructed picker state. Native swatches own
native caption/border, stable color and opening/pending RGB; select/cancel override
the default commit while preserving alpha. Bound-setting echoes do not suppress
callbacks. Disable/destruction cancels and closes dependent picker transactions
under the tree's existing erasure guard without focusing an erasing swatch.

Native factory consumes color_swatch.xml and the unchanged floater_color_picker.xml.
LLVKLoginUi retains one native floater per swatch with normal close/cancel ownership.
RGB byte, LSL float, HSL and hex fields are linked to native state. LLColor3
setHSL/calcHSL and their arithmetic-only helpers are audited CPU math; no GL color
picker, swatch, texture or layout owner is shared. Generated256x256 hue pixels
preserve source column*3/767 and row/255 sampling and byte truncation, published
through bounded immutable LLVKWidgetImage::fromRgba. Original Checker pixels are
sampled at a32-UI-pixel period by the native swatch painter. Picker luminance,
current swatch, palette, crosshair and triangle marker use original regions.
Palette drag updates the retained native user color table; Copy LSL uses three
decimal components. Native UI triangle packets validate extent/clip/color/positions
and budgets before mutation and use existing frame-owned vertex submission.

Tests70/131/145 check original XUI, source plane pixels, palette-cell bounds,
numeric synchronization, pointer capture/clamp/release, palette selection/save,
Select/Cancel, bound preview restoration and swatch teardown. Context9/9 passes
with swatch assets and real triangle submission on RX9070XT; original picker
window/audio2/2 passes. That integrated test exposed an incorrect native spinner
height guard: source fixed-size children may extend below a shorter spinner and
setUseBoundingRect(true); the native owner now preserves that behavior and source
bottom-left anchoring. No source XML was resized to conceal the mismatch.

Still open: invalid/fallback swatch painting, active-floater alpha policy,
pipette and system-picker services, palette highlight/cursor/cone details,
notification feedback, user palette disk persistence, complete callback-failure
rollback and actual-viewer interaction/measured parity. These are implementation
increments, not full color-picker or Preferences closure. Current CMake generation
is7.2.5.79282 after adding llvkcolorswatch.cpp; actual viewer needs relinking for
this latest slice. Full Preferences and native RLVa handler remain incomplete.

Live settings/audio continuation (post-d28b718b12, NV-00/01/03/15/17):
LLControlVariable::setValue/firePropertyChanged publishes changed values to
registered callbacks. Native tree subscribeSetting/unsubscribeSetting adds
independently owned subscriptions with stable old/new snapshots and disconnect
checks across nested writes. Test144 verifies change-only Boolean notification,
nested mutation and disconnect. Full validation/saved-layer and mixed widget/
service subscriber ordering remain separate obligations; opaque-value equality
is not qualified by that scalar test.

Source audio_update_volume and LLAudioEngine::setMasterGain/setMuted compute
effective master gain from mute, inactive-window policy and progress visibility;
LLAudioEngine_OpenAL::setInternalGain sends that gain to the listener. Native
LLVKAudio::Volume/setVolume independently implements this policy, validates
finite nonnegative gains, and retains/verifies its actual OpenAL context identity.
listenerGain queries actual OpenAL state for verification. The native window
subscribes AudioLevelMaster/MuteAudio/MuteWhenMinimized and refreshes on activation;
scoped teardown clears the window callback and disconnects subscriptions before
audio-owner destruction. Window/audio test2 verifies actual0.375 listener gain,
mute, inactive/reactivated window, progress mute and invalid-gain nonmutation;
integrated window/audio2/2 passes. The current login has no progress view and
supplies false for that state. Secondary gains, cues, deferred sounds, streaming,
wind/listener spatial policy and voice/media providers remain open. No RLVa
runtime handler is initialized by these setting subscriptions.

Horizontal tab overflow continuation: original measured-width maximum scroll,
partial previous-tab allowance and final-tab pixel clamp are implemented in native
state. Four native arrow buttons use original jump/scroll images; jump actions
only scroll, while next/previous also select. Selection reveals the requested
tab and wheel input is confined to the strip. Painter uses explicit frame delta
for the source0.08-second half-life and integer pixel conversion, and clips tab
children to the source container inset. Test143 verifies targets, actual half-step
movement, arrow selection distinctions, wheel and paint clip. Widget144/144
passes with settings additions. Flash propagation, drag reorder/hover selection,
hidden-tab bookkeeping, extreme-size failures and measured parity remain open.

Vertical tab overflow (post-d28b718b12, NV-00/01/12/17): source
LLTabContainer::updateMaxScrollPos/draw/initButtons/onNextBtn/onPrevBtn,
their held callbacks, setTab and handleScrollWheel define whole-row scroll
positions, source arrow-reserved height, visible-row limits and selected-row
reveal. Native tab state now owns bounded row positions and native Up/Down
buttons using original overlay assets. Arrow actions scroll then select;
held actions use the source 0.4-second step, and wheel input over the strip
scrolls without selecting. Layout preserves content geometry and restores all
tabs/hides arrows after expansion. This is CPU ownership and input/paint
preparation, not reuse of LLTabContainer or GL draw/prepareVkDraw.
Test142 checks eight rows in a120px owner, max position5, hidden rows,
selection reveal, arrow click, wheel region and resize recovery; widget142/142
passes. Horizontal overflow/smoothing, flashing propagation, complete arrow
construction-failure rollback and source resize/callback reentrancy remain open.

Search editor (post-d28b718b12, NV-00/01/12/17): source
LLSearchEditor constructor/draw/setValue/getValue/setFocus/onClearButtonClick/
handleKeystroke in the current Windows tree owns an LLLineEditor plus optional
search and clear buttons. Source GL draw and prepareVkDraw are not reused.
The native SearchEditor owner instead contains its native line editor and button
IDs. CPU preparation controls clear visibility and highlighted background;
input reserves source button padding, forwards focus/value and preserves
text-change-before-commit on clear. Keystrokes notify before text changes;
Left/Right skip text-change notification. Native factory loads search_editor.xml
and the original Preferences nested rect overrides, using native fonts/images.
Tests70/141 verify original padding, clear icon painting, nested pointer capture,
clear ordering, arrow/Unicode transitions and callback deletion. Full search
filter application, teardown/focus-loss reentrancy and binding/dirty propagation
remain open; the live Preferences floater is still provisional.

About/editor continuation: source LLFloaterAbout::onClickCopyToClipboard calls
selectAll/copy/deselect; native Copy now matches that order, with test131 using
an isolated clipboard. Read-only LLTextEditor::handleKeyHere calls its scroller
before selection/control handling. Native textEditorKey preserves that order
and source scrollbar Home/End/line/page behavior, then horizontal Shift selection
and Ctrl-word navigation over display indices. LLWStringUtil::isPartOfWord is
audited CPU character classification; no text visual owner is shared. Win32
routes focused editor navigation before parent tab handling. Tests140 and native
window/audio2/2 pass; vertical/page selection, cursor reveal and all editable
commands/context menus remain open. Current widget suite is141/141.

Original About hierarchy (post-d28b718b12, NV-00/01/03/12/17): source
LLFloaterAbout::postBuild populates support_editor, contrib_names and
licenses_editor, makes each read-only and starts each at document origin.
copy_btn selects and copies the support editor's display text; Firestorm credits
remain separate text controls in their original scroll container. Native
LLVKFloater::createFile and the native XUI factory now construct the unchanged
source declaration with native tab, editor, border and scroll owners. The
obsolete flattened page extraction and hand-built tab/page geometry are removed.
This CPU-only design reuses declarations and audited file/formatting services,
not GL visual constructors or callbacks; rendering continues through native
paint packets and existing completion-owned uploads. Generated contributor
first-line and license assets populate the original fields; a missing license
asset leaves the XUI fallback intact. URL template inheritance distinguishes
support links from literal license text. Tests70/131/140 disprove constructor,
ownership, formatting, independent scroll, paint and template-policy regressions;
window test1 presents the original Firestorm credits hierarchy with no GL parent
module. Full text-editor commands/context menus, floater saved-rectangle lifecycle,
all report data and measured pixel parity remain unresolved, not waived.

Read-only editor composite (post-d28b718b12, NV-00/01/12/17): the audited
LLTextBase/LLTextEditor constructor boundary is implemented as a native public
control owning its scroller, panel document, selectable text and border. Read-only
state does not disable scrolling. Reflow negotiates scrollbar width and retains
scroll position; public value/focus/select/copy operations forward to owned state.
Test140 checks independent ownership, read-only wheel input, start-of-document,
selection, value replacement and resize. Editable commands/undo/context menus and
full wrapping edge cases remain open; this change does not declare editor parity.

Read-only editor constructor boundary (source e9c2af5aa7): LLTextBase::LLTextBase
owns the text scroller and document, then initializes segments/rectangles;
LLTextEditor adds its border and default text. LLTextEditor::initFromParams
forces the view enabled even when text is read-only so scrolling remains usable.
The native composite now owns that internal scroller/document/border and loads
the original About field geometry. Selection/navigation/menu behavior still
requires complete qualification.
Disabling a generic panel or aliasing text_editor to plain text is not closure.

RLVa startup sequence correction (source e9c2af5aa7, NV-00/01/15/17): the
RlvHandler::setEnabled(true) call is inside STATE_LOGIN_CLEANUP in llstartup.cpp,
not the initial login-screen construction. RlvSettings::onChangedSettingMain
posts the pre-login/restart notice but does not call setEnabled. The latter
loads settings/string tables, registers RlvEnvironment/RlvExtGetSet, subscribes
to login completion and constructs RlvUIEnabler. Inspected registered targets
reach GL-owned environment and HUD/UI state; they are not shareable as native
visual callbacks. The native login screen's inactive RLVa report is not proof
of initialized RLVa capability. The complete native handler/service integration
remains an explicit user requirement; no success flag or no-op registration has
been substituted. A previous progress statement suggesting the setting callback
itself enables RLVa was incorrect.

Slider integration continuation: original slider_bar skin parameters now feed
native track/highlight/thumb, pressed ghost, disabled and focus paint states;
test70 checks the actual skin assets. Native SliderControl adds label and value
editor/text, source range validation and rollback, rounded precision display,
and a parent editor-commit callback. Factory loads widgets/slider.xml and its
child parameter blocks; test139 checks parent/editor synchronization and veto.
Full child-parameter override behavior, locale formatting, bound-callback
reentrancy, resize/color propagation and exact visual equivalence remain open.

Current validation after numeric controls: widget139/139 and context9/9 pass,
including actual spinner packet presentation; window/audio2/2 pass. Two generated
MSVC objects were rejected by LNK1163 and recompiled individually, after which
the same tests passed; source was not reverted. General Preferences XUI now
constructs and paints in the source-asset test with explicit test callbacks.
This does not mean the live Preferences floater has been restored: its original
application services and other panels are still incomplete.

Native slider bar owner (NV-00/01/12/17): source LLSlider::setValue quantizes
relative to minimum with increment/2.0001 downward tie bias; setValueAndCommit
commits changes only. updateThumbRect uses integer half-thumb extents and
truncated proportional position. Mouse press preserves grab offset, Ctrl resets
initial value, hover drags, and release drops capture before callback. Native
slider state implements these decisions with checked finite/range geometry and
native focus/capture/settings. Test139 checks ties, unchanged commits, grab
offset, clamping and reset. Composite slider label/editor, skin paint, full XML
construction and visual parity remain follow-up requirements.

Upload completion test correction (NV-13/14/17): context test8's single
queue-idle-then-poll assertion repeatedly failed on browser or glyph readiness.
The test now explicitly waits on each pending owner's upload fence through the
already checked LLVKGlyphUpload::wait, with a five-second bound per upload.
Publication still occurs in the subsequent prepare/advance operation and the
unchanged assertions require Ready and retained image ownership. The runtime
frame loop does not call these diagnostic/bootstrap wait methods and remains
asynchronous. This records the observed failure without attributing it to a
driver or layer; precise cause of queue-idle observation remains unqualified.

General Preferences construction check: unchanged panel_preferences_general.xml
contains a bracketed secondlife:///app/openfloater/preferences target. Source
LLUrlEntrySLLabel uses the same owned label/target split as HTTPLabel. Native
web ranges now parse bracketed secondlife/hop targets, while factory consumers
must explicitly supply their native link handler. Test70 constructs and paints
the original General panel using a test maturity callback and link recorder;
this is construction evidence, not application-service completion. Internal URI
dispatch must select native panels, never invoke an external browser or GL
floater callback. Full target navigation remains an integration obligation.

Native radio owner (post-e9c2af5aa7, NV-00/01/12/17): LLRadioGroup
initFromParams/setSelectedIndex/setValue/onClickButton/getValue and LLRadioCtrl
setValue define mutually exclusive checkbox children, payload-string matching
before integer-index fallback, selected-only tab stops and repeated-click commits.
Native radio state owns child/payload pairs and routes through native checkbox
controls; selection publishes bound values before commit without wrapping arrow
selection. Test138 checks these discrete contracts, optional deselection,
source disabled-index reselection and owner deletion from commit. Factory loads
the packaged radio_group/radio_item templates and applies item XUI layout only
after the owning group exists; test70 checks actual RadioButton skin assets and
top-left coordinates. Arrow keys route through the native group before enclosing
tabs. Mouse pre-focus and remaining registry/style behavior remain open.

Native spinner owner (post-e9c2af5aa7, NV-00/01/12/17): LLSpinCtrl constructor,
onUpBtn/onDownBtn/onEditorCommit/setValue/onCommit and LLF32UICtrl establish child
label/editor/buttons, precision rounding, modifier increment, range clamp,
proposed-value validation/rollback and bound-setting-before-commit ordering.
LLLineEditor::evaluateFloat delegates to LLCalc; audited llmath LLCalc/LLCalcParser
use private constants/variables, Boost.Spirit arithmetic, strings and logging,
without visual callbacks. Native controls own composite state and instantiate a
private calculator. Test137 checks expression evaluation, clamping, modifiers,
veto and binding order. Locale-specific editing, focus-loss reconciliation,
resize and full locale qualification remain required follow-up. Native factory
loads widgets/spinner.xml and original Preferences ranges; test70 checks real
Stepper assets and numeric initial values. Native focus forwards to the editor,
focus loss reconciles updated values, disabled state forwards read-only state,
and live modifier snapshots feed held/released arrow actions. Test137 checks
focus/draft preservation and disabled input. Capture-lost uses native button
commit-on-capture-lost. This is not yet complete spinner parity.

Native startup audio owner (post-e9c2af5aa7, NV-00/01/15/17): source
LLStartUp startup_state_machine at llstartup.cpp1095 initializes the compiled
audio provider before login, honors NoAudio and starts muted; failure leaves
gAudiop absent. OpenAL init calls ALUT and driverName queries its active context;
base listener allocation only initializes CPU vectors. Native LLVKAudio owns
ALUT/OpenAL for the native window lifetime, guards preexisting context ownership,
starts listener gain at zero and shuts down on the creating thread. It uses the
same OpenAL library and active driver string, not an inspection-only temporary
device. Context creation failure is explicit and startup continues without audio,
matching the source's nonfatal policy. Test2 in the window suite checks NoAudio,
real initialization, conflicting-owner refusal and shutdown. This closes only
OpenAL initialization ownership: full cue playback, gain subscriptions, streaming
media, alternate compiled providers and RLVa remain open, not parity-complete.

Read-only text selection (post-checkpoint e9c2af5aa7, NV-00/01/12/17): source
LLTextEditor::handleMouseDown/handleHover/handleMouseUp tracks an anchor and cursor,
extends Shift selections, suppresses link activation during a selection drag and
releases capture; copy transfers the selected display substring. Native selectable
text uses the existing Unicode document/selection state and native clipboard,
with Ctrl+A/Ctrl+C, selection foreground/background and link-drag cancellation.
About's body opts in; its fixed introduction remains nonselectable. Test136 checks
display-label copying, selection painting and drag-vs-link behavior. Primary
selection clipboard, word/double-click selection, keyboard range movement and
drag-autoscroll remain pending parts of full editor parity.

Native web-text ranges (post-checkpoint e9c2af5aa7, NV-00/01/12/17):
LLTextBase::appendTextImpl consumes registry matches as label/query segments;
LLUrlEntryHTTP/HTTPLabel/NoLink define HTTP/FTP targets, bracket labels and
nolink suppression. LLUrlEntryBase escapes targets and percent-decodes labels.
Native LLVKWebText owns Unicode display ranges and escaped targets, preserving
separate suffix coloring and suppressing URL-shaped masking labels. Audited
LLUriParser uses Boost.URL parse/normalize/string operations only; native code
reuses it and LLURI without LLUrlRegistry, LLStyle or GL text controls. Input and
link counts are bounded; parse failures are atomic. Test136 checks Unicode ranges,
bracket labels, nolink, suffixes, punctuation, masking and bounds. Other registry
entry types, trusted-domain masking policy and URL menus are still open. This
parser is not yet a complete rich editor or generic URL-registry replacement.

Native text controls now opt into these web ranges for About. They paint link
and query colors using the original color keys; underline position follows
LLFontGL::render's baseline minus floor(descender) rule. Pointer hit testing uses
the native font and document layout, rejects points outside ancestor scroll clips,
captures a pressed target and dispatches only on release over that same target.
Updates clear pressed-link state. About's introduction/report and clipboard use
display labels rather than raw bracket markup. A native window URL callback
validates HTTP/HTTPS/FTP and uses the existing confirmation/external-browser path;
hand cursor is native Win32. Test136 checks paint, click target, capture release
and release-outside cancellation. Full context menus, keyboard accessibility,
other registry entry types and exact underline raster parity remain open.

Tab orientation extension (post-checkpoint e9c2af5aa7, NV-00/01/12/17):
LLTabContainer::addTabPanel defines left content bounds from tab width/right
padding/vertical spacing, with BTN_HEIGHT=23 (llbutton.cpp); bottom tabs reserve
the strip below content and begin at y=1. Native layoutTabPanels stages these
rectangles, retains top-layout compatibility and selects top/bottom/left skin
images from the same tab template. Test134 checks exact left/bottom geometry.
Source handleKeyHere's vertical Up/Down selection and Right-to-content, and
bottom-tab Up-to-content, are implemented; test134 checks vertical focus and
test70 checks packaged vertical images and Preferences content bounds.
Overflow scrolling remains explicit pending its source-driven implementation.

Border painter (post-checkpoint e9c2af5aa7, NV-00/01/12/17): source
LLViewBorder::draw/drawOnePixelLines/drawTwoPixelLines selects bevel colors,
retains RGBA on one-pixel borders but uses opaque RGB on two-pixel borders,
substitutes focus colors and varies one-pixel focus width. gl_line_2d emits
axis-aligned lines with no extra CPU state beyond color/line width. Native
painting represents covered edges as ordered solid strips, with independently
owned color/focus state; it does not invoke source GL draw/getter callbacks.
Texture-style borders have no source border draw and retain child traversal;
one-pixel bright bevel is source-undefined and remains explicit. Test135 checks
bevel order, alpha and zero thickness. Exact raster endpoints/focus-width pixel
parity still require the controlled GL comparison at the full UI gate.

About service-field contract (2026-09-11, NV-00/01/15/17): at c4d2f9a600
getViewerInfo uses RlvHandler::isEnabled before displaying getVersionAbout,
LLCore::LLHttp::getCURLVersion (a direct curl_version string), the actual J2C
implementation's getEngineInfo, and gAudiop->getDriverName or "Undefined".
Native startup has no initialized RLVa handler or audio engine, so it reports
the same inactive states: localized RLVaStatusDisabled and "Undefined". This is
not implementation or enablement parity for those services. In particular,
RlvHandler::setEnabled also initializes visual UI/environment callbacks; it cannot
be enabled merely to display a version number. OpenAL getDriverName queries an
active audio context and is not an installed-library-version query.

Native curl uses the same API-independent curl_version from ll::libcurl. Native
J2C reports compile/runtime OpenJPEG versions from the translation unit that
actually decodes native images, using the LLImageJ2COJ string format; a GL build
using Kakadu must not cause native OpenJPEG to be mislabeled as KDU. The mode
label now uses an explicitly recorded successfully loaded preset, not a mutable
SessionSettingsFile value; load failure is surfaced before native window startup.
Test131 checks inactive services, codec/runtime labeling and applied-versus-saved
mode distinction. Full RLVa/audio capability restoration remains open.

About scroll correction (2026-09-11, NV-00/01/12/17): original floater_about.xml
fs_credits_scroll_container and LLScrollContainer::draw/updateScroll retain the
whole document and move its viewport, with visible scrollbar controls. The
native flattened page previously sliced off leading lines and had no scrollbar.
Native About now owns a real native scroll_container/panel document and retains
the text while scroll position moves. Test131 requires a visible painted bar,
wheel movement without text mutation, and reset on tab change. This repairs the
missing scrollbar but does not establish original per-tab layout/content parity.

About content/asset correction: c4d2f9a600 LLFloaterAbout::postBuild reads the
first contributor-file line and package-info lines (retaining XUI license text
when the file is missing). The development copy path did not generate either
asset. ViewerManifest now uses its existing extract_names/put_in_file and
BuildPackagesInfo output for development copy too, with Windows dependency
edges. The extraction algorithm, randomized ordering and GL visual consumers
are unchanged. copy_w_viewer_manifest completed and staged contributors.txt
(6816 bytes) and packages-info.txt (5579 bytes). Python syntax check passed.

Native About keys pages by original panel names, merges skin/language XUI layers,
and keeps support/Linden introductions outside scroll documents. Copy is Info-only.
The Info formatter consumes original strings.xml About sections in source order
from getViewerInfoString; native facts use CMake's full version/build/upstream,
CPUID, Win32 memory/OS APIs and the selected Vulkan physical-device properties.
Native Dullahan version getters only format compiled library constants; no GL
viewer/provider singleton is called. Mode/font/quality/voice labels use source
translations. Browser version and resize facts update the displayed Info text.
Tests131 and the Vulkan window test cover formatter/scroll consumers. CPU frequency,
exact original memory accounting, driver-version labeling, uninitialized library
services, SLT timestamp, rich links/editor selection and exact original per-control
layout remain unqualified; unavailable values are explicit, not GL-derived.

Tab selection owner (2026-09-11, NV-00/01/12/17): c4d2f9a600
LLTabContainer::addTabPanel/selectTab/setTab stores panel/button pairs, initially
hides panels, validates the selected panel name, toggles button/visibility and
tab-stop state, then commits the panel name. Native tree-owned stable IDs and
selection generations implement those CPU responsibilities without GL owners.
Snapshot iteration and owner rechecks protect reentrant visibility/commit calls;
a nested selection supersedes the outer operation. Test134 checks validation
veto, callback ordering, button routing, selected tab-stop and callback deletion.
Geometry, overflow scrolling, source XUI constructor integration and application
panel services remain open; this owner alone is not floater parity.

Top-tab layout probe: source LLTabContainer::addTabPanel uses a one-pixel panel
border, configurable tab/panel overlap, optional panel side offsets, and clamped
font-measured tab widths. Native layoutTopTabs stages panel/button shapes before
publication and uses actual native font measurements. Test134 checks exact
content/strip coordinates, resize and atomic rejection of unsupported overflow.
The non-overflow top arrangement is now wired to the native tab_container
factory with the original widgets/tab_container.xml image/font/geometry defaults.
Test70 constructs panels and selects them with the original tab skin images.
Painting reconciles resized tab geometry without publishing unchanged shapes.
Source handleKeyHere/selectNextTab/selectPrevTab/onTabBtn and setValue establish
Alt-arrow and Shift-Alt-parent navigation, strip-only unmodified arrows,
selected-button focus retention, click-to-panel focus and indexed selection.
Native input observes those top-tab rules and WM_SYSKEYDOWN routing; test134
covers wrapping, focus retention and indexed selection. Left/bottom layout,
overflow arrows and flash propagation remain outstanding. Full floater
construction and parity are not established by this template regression.

Scroll painter integration (2026-09-11, NV-00/01/12/13/14/17): source
c4d2f9a600 LLScrollContainer::draw updates autoscroll/layout, focuses active
scrollbars, paints the document inside a local clip, then paints visible chrome.
LLScrollbar::draw paints track/thumb/focus/glow then button children. Native
prepareScrollContainer/prepareScrollbar own the audited CPU decisions; the painter
consumes their screen rectangles, preserves document-only clipping and emits
ordered image/solid commands through the existing immutable Vulkan packet path.
No GL draw/prepareVkDraw/getVkDrawState callback is used. Test133 discriminates
document clipping and chrome ordering before and after a wheel event. Visible
border painting remains a separate open dependency; floater parity is not closed.

Fallback kerning correction (2026-09-11, NV-00/01/17/18): source c4d2f9a600
LLFontFreetype::getXKerning uses the primary face with cached glyph indices,
tabular-digit suppression and a separate auto-hinter side-bearing correction.
Native measurement/layout inherited primary-face selection even for glyphs owned
by fallback faces, causing the checked FreeType owner to reject foreign indices.
The CPU-only pair helper now uses the shared owning face for same-face pairs;
cross-face pairs have no font-table kern, but retain the discrete side-bearing
correction. Measurement, drawing, wrapping and hit-testing use this helper.
This is a native ownership correction, not reproduction of foreign-face glyph
lookup or a change to the GL reference. Font test7 exercises mixed-face layout
and measurement agreement plus same-fallback fitting. The face bounds guard
remains enabled. A one-glyph primary test fixture proves that the previous
cross-face call rejects the fallback index; corrected measurement, drawing and
fitting succeed for both same-fallback and mixed-face pairs. Runtime credits
rendering is the affected consumer check.

Native login dialog content (NV-00/01/12/17): source LLFloaterAbout postBuild/
setSupportText/copy and original floater_about.xml provide read-only runtime Info,
credits, license data and clipboard behavior. Native content reads the packaged
text rather than copying a GL-produced visual; scroll state is CPU-owned.
Preferences currently supports pre-login remember flags and renderer choice only.
Source LLPanelPreference::saveSettings/cancel and onBtnOK establish snapshot,
rollback and explicit persistence; native unported in-world panels remain open.
Renderer choice is pending and never changes the active device; application
binding must confirm shutdown and persist the approved change. Exact full
Preferences layout/panels, rich About links, docking and tab overflow remain
unqualified. These native dialogs do not establish full preferences parity.

Preference file update contract: c4d2f9a600 LLControlGroup::saveToFile writes
Type/Comment/Value/Backup from persistent save values, excluding transient values.
The native CPU-only owner uses the audited LLSD serializer and filesystem IO,
not LLControlGroup callbacks. A bounded parse and same-volume staged replacement
merge only explicitly accepted changes into the existing document; unrelated
entries and metadata survive. Failed parse/write/replacement leaves the in-memory
saved layer unchanged. Test132 checks saved reload, unrelated entries, transient
exclusion and malformed-file refusal. Concurrent writes by the GL viewer and
power-loss durability are not qualified; staging conflicts fail explicitly.

Native floater frame (NV-00/01/12/17): c4d2f9a600 LLFloater close/destruction/
focus lifecycle and widgets/floater.xml establish independent panel chrome, close
button, focus root and drag header. Native LLVKFloater owns an independent panel
and native close/text children; opening centers it and retains prior focus;
closing releases descendant capture and restores the surviving focus owner.
Dragging uses explicit screen coordinates with root bounds, no GL drag callbacks.
This CPU-only frame is shared by native About/Preferences, not a wrapper around
LLFloater. Resize handles, docking, minimization and persisted geometry remain
open; narrow construction/input checks are required before content integration.

Login menu restoration (2026-09-11, NV-00/01/12/17): source c4d2f9a600
llviewermenu.cpp login menu construction and LLMenuItemBranchDownGL nominal width,
draw and mouse-down; LLMenuItemGL nominal height and menu_login.xml/templates.
Q1 the menu is a separate 18px top-edge owner, not a child in the login XML;
bar items use measured label width+25, centered text with bottom pad1, toggle
on mouse-down, and skin highlight colors. Popup rows use font height+4 and
independent foreground/disabled colors. Q2 native Expat menu model and explicit
popup/input state append image-free solid/text paint commands to native packets.
Q3 retain callback names as data and dispatch only explicitly bound native
handlers after dismissing the menu; never invoke GL menu/floater callbacks.
Tests cover actual menu XML, top placement, popup input and action dispatch.
Unbound floater actions are visibly disabled, not implemented. Tear-off, full
accelerator/jump-key/check predicate semantics and full menu parity remain open.

Verification: construction suite 130/130 and integrated native login window 1/1
passed. The integrated log contains no VUID/Validation Error entries. The actual
rebuilt vulkanstorm-bin.exe (PID 9456) displayed Viewer/Help/Debug above the live
browser, including Help and Viewer popups. At resized 1264x861 client extent the
bar stayed anchored at the top, and F10/Down/Enter activated Exit and closed the
viewer. Parent modules included Vulkan, CEF and Khronos validation, not OpenGL/GLU.
Local screen captures: logs/native-login-menu.png, logs/native-login-menu-viewer.png
(Help popup), logs/native-login-menu-resized.png (Viewer popup). This is runtime
render/input evidence, not measured GL parity. Exit and confirmed HTTP(S) help
links have native bindings; Preferences/About/Guidebook and other GL-owned
floater actions are disabled pending native service implementations. The URL
launch confirmation path was inspected/compiled, not exercised to open a browser.
Grid-specific Help entries follow update_grid_help's non-OpenSim hidden policy.

Login password visibility binding (NV-00/01/12/17): source
FSPanelLogin::onShowHidePasswordClick/syncShowHidePasswordButton and
LLLineEditor::setDrawAsterixes switch transient masking, swap eye-button visibility
and update language-input permission without changing text/cursor. Native login
owner installs independent commit callbacks after construction; text hit-testing
and paint masking change together. Test127 checks both toggles and retained value/
cursor. Native Win32 Ctrl+A uses the existing source-backed selectAll operation.

Actual-viewer popup input regression: opening the arrow and then separately
clicking a row failed because root traversal tested ancestor rectangles before
the popup extending outside them. Source top-control dispatch is independent
of normal LLView traversal (which excludes the top control). Native routePointer
now gives uncaptured top-control bounds first dispatch and dismisses an outside
combo press. Test117 exercises separate arrow release and row click, not only
the earlier drag-release selection. Actual viewer resize and masked typing were
observed before this fix; popup selection must be reverified after it.

Native simple combo popup paint (NV-00/01/12/17): source LLComboBox constructor
disables stripes; LLScrollListCtrl::drawItems, LLScrollListItem::draw and
LLScrollListText::draw choose enabled/selected/hover colors, selection supersedes
whole-row hover, bottom-aligned text starts one pixel into the cell with ellipsis.
Native combo params retain live packaged color references; paint list defers visible
popup commands until after the main tree with root/item clips. Test127 opens the
actual location combo and checks all three rows and topmost command ownership.
Long-list scrollbar chrome, type-ahead substring highlight, multi-column content
and popup keyboard interaction remain open; this is the login simple-row path.

Native login resource owner (NV-00/01/12/17): source LLFontGL::initClass selects
an install/user descriptor before skin fallback; existing native font/skin/color/
factory contracts supply independent consumers. LLVKLoginUi takes that selection,
font search paths/DPI and settings explicitly rather than reading GL globals.
It owns a native tree and actual descriptor-resolved fonts, loads required widget
templates, constructs the unchanged default login and validates required controls.
Test127 uses real packaged fonts with no fallback font substitution and checks
different requested sizes and bound Home location. Startup descriptor selection,
user colors/settings persistence, login service callbacks and actual boot remain
separate obligations; this owner does not claim those services are implemented.

Native login paint list (NV-00/01/06/11/12/17): source LLView::drawChildren
reverse traversal/visible/root-overlap tests, LLPanel::draw background/transparency,
and preceding native button/editor/text/icon preparation contracts. Q1 preserve
parent background before reverse child painter order and explicit text/stack clips.
Q2 LLVKWidgetPaint emits retained native image/solid/glyph commands in bottom-up
screen coordinates, keeping browser-not-ready IDs explicit. Q3 CPU preparation
is separate from resource publication/packet recording; callbacks use native IDs
and recheck lifetime. Test70 prepares the actual login and asserts logo/connect/
password-link commands and pending browser. Visible unsupported border/scroll/
badge/popup list consumers fail explicitly and remain open. Dirty-region rendering,
full scale/alpha policy, browser fit policy, popup painting and GPU consumption are
not closed by this paint-list test.

Native editor preparation (NV-00/01/06/12/17): source 1819c5fecf
LLLineEditor::drawBackground/draw/findPixelNearestPos, non-spellchecked login
configuration. Q1 background precedence readonly/focus/normal, alpha replacement
for text versus multiplication for solid background, preedit before letters,
three selection runs, hidden programmatic border, application-focus/read-only/
one-second-delay caret gating, overwrite inversion, watermark and IME position.
Q2 native display text (password already masked) and native font layout produce
ordered image/rectangle/glyph parts. Q3 tree owns preparation; caller supplies
explicit application focus and elapsed keystroke time, and consumes IME output.
Test126 checks masked glyphs, focus image, alpha and caret gating. Spellchecking,
custom highlighted text color, platform IME delivery and actual GPU painter remain
open. No reuse of GL-owned editor's historical getVkTextState/getVkBackground.

Native button preparation (NV-00/01/06/12/17): source 1819c5fecf LLButton::draw,
drawBorder/getOverlayImageSize and setToggleState. Q1 image/disabled overrides
precede toggle callback; labels follow its mutation; focus outline precedes base
image/glow, then overlay and trimmed text. Hover/flash interpolate at 0.05 seconds,
pressed label shifts x only, overlay scales down to fit. Q2 explicit native input
state and retained image/color/text primitives, no GL draw callback. Q3 prepare in
the native tree where focus/capture/settings/callback lifetimes are owned; GPU
consumer handles primitives and native font output. Test125 checks image-before
callback/label-after, focus order, pressed x and disabled selected override.
Search-highlight override, checkbox-control-panel child, solid-mask GPU mode and
full frame/visual parity remain open. No button draw-completion claim yet.

Packaged login construction probe: test70 now consumes panel_fs_nui_login.xml
unchanged, with native widget templates/resources and three explicit bound login
settings. This falsifies missing declaration-path assumptions; success only proves
construction/layout, not FSPanelLogin service callbacks, browser navigation or
viewer startup. The fixture font fallback is not a font-size parity oracle.

Browser declaration construction (2026-09-11, NV-00/01/12/17): source 1819c5fecf
LLMediaCtrl::Params/constructor/postBuild, parent LLPanel contract already recorded.
Q1: typed panel construction retains home URL/MIME/error URL, trust/focus policy,
decoupled texture sizing; empty home URL does not create media. Q2 independently
owned native browser component is installed before native control init, with an
independent panel base, and runtime browser owner consumes policy separately.
Q3 typed construction avoids plain LLPanel's two-phase XML reinitialization.
Test124 checks empty-home/trust state before init and border metadata. Notification
shade, native auth/file dialogs, media-ID routing, caret color and visibility
refresh callbacks remain open; typed component registration is not their closure.

Native skin geometry (2026-09-11, NV-00/01/06/11/12/17): source 1819c5fecf
LLUIImage::draw/drawSolid/drawBorder (lluiimage.inl), both border overloads and
zero-angle gl_draw_scaled_rotated_image in llrender2dutils.cpp. Q1: clipped UVs
define natural size, scale region defines nine patches, inner scaling stretches
the center and uniformly shrinks borders if undersized; outer scaling keeps the
center fixed up to target fit. Inner coordinates round after UI transform while
outer edges remain fractional. Unbordered scaling instead rounds its extent.
Q2: image owner prepares bounded CPU quads with explicit target/UI transform and
separate bottom-up UVs. Q3: at most nine quads, no ambient GL state or API handles;
consumer supplies tint/blending/shader and coordinate-system conversion. Test122
checks borders, undersized target, fractional scaling and outer-center behavior.
Negative targets/transforms and empty outer centers reject explicitly; no parity
claim for those inputs. GPU publication and solid-mask shader consumer remain open.

Login literal text declaration path (2026-09-11, NV-00/01/12/17): source
1819c5fecf LLTextBase constructor/initFromParams, LLTextBox::setText/setTextArg,
LLUrlRegistry::findUrl/stringHasUrl/stringHasJira and packaged widgets/text.xml.
Q1: native document child, resolved text value before init, dirty reset/read-only
override afterward, bounded UTF8 bytes, alignment/padding and live color slots;
URL registry returns immediately when neither URL nor issue candidates exist.
Q2: native factory defaults and Expat body declarations feed the independently
owned text document/font; the candidate-free path is CPU-only. Q3: retain the
candidate policy in the text owner so post-construction substitutions/updates
cannot bypass it. Candidate content currently fails explicitly and atomically,
rather than silently dropping links or embedded content. Test121 checks text
body, alignment, reflow, failed-update retention and explicit parse_urls=false;
test70 consumes the packaged text template. Rich candidate resolution, scrolling,
ellipsis, background/border, shadow modes other than none and full text parity
remain open, not disabled in the reference. This is not full text closure.

Literal login link input (2026-09-11, NV-00/01/12/17): source at construction
checkpoint 1819c5fecf, Windows, LLTextBox::handleMouseDown/handleMouseUp/handleHover
and LLTextBase counterparts. Q1: base segment/child handling precedes sound,
capture is taken only when unowned (except modal override), captured release
clears capture before the callback, and base-handled release suppresses the
label callback. Hover uses a hand only for unhandled clickable text. Q2: these
are CPU input transitions with native tree IDs, independently owned callbacks
and native cursor/sound event outputs; no GL widgets or resources participate.
Q3: extend the existing literal text owner and tree dispatch rather than create
another visual wrapper. Reacquire IDs after outgoing callbacks and copy the
final callback so it may destroy its owner. Test120 checks cursor, sound,
capture ordering and destructive release. URL segments, triple-click timer and
modal override are still open; this is literal-owner input, not full text closure
or native login boot. GPU safety is N/A for this CPU-only change.

Login integration refinements (2026-09-11): LLComboBox::onTextCommit calls
setSimple which uses case-insensitive label lookup, then commits item value and
canonical label. Native commit now shares that byte-folding rule with typing.
LLLayoutStack::Params resolves unspecified border_size from UIResizeBarHeight;
native factory retains an unspecified marker until native construction settings
are available. Test119 separates explicit zero from configured spacing and typed
mixed-case label from canonical committed item value. Runtime world-map behavior
and its independent test expectations remain unchanged.

Login combo XML record (2026-09-11, NV-00/01/12/17): source LLComboBox::Params,
ItemParams/constructor/postBuild and widgets/combo_box.xml. Q1 button/dropdown,
editor/list blocks configure internal typed children; dotted editor attributes
combine with nested blocks; items carry separate label/value/enabled state;
postBuild reapplies bound setting after default first-item selection. Q2 native
owned defaults/parameter declarations and native resources/callback registries.
Q3 heap-owned ComboDefaults avoids recursive frame inflation, resolveCombo loads
real images/fonts, construction binds native text validators then creates owned
combo. Extended test70 consumes packaged combo template, nested editor overrides,
ASCII validation, real arrow image and explicit Home selection. List multi-column
metadata, allow_new_values=true and full inherited template loading remain open;
the login uses simple items with allow_new_values default false.

Login combo typing record (2026-09-11, NV-00/01/12/17): Q1 onTextEntry invokes
text-entry callback before classifying current key; left/right return, deletion
matches exact label without completion, ordinary text calls updateSelection.
Exact label matching lowercases encoded bytes and rejects empty labels. Prefix/
substring matching lowercases wide text, trims item labels, selects enabled rows,
retains typed prefix and selects generated suffix via LLLineEditor::setSelection.
One-character input invokes prearrange before searching; no match removes earlier
completion and marks tentative. Q2 native editor retains originating native key,
combo owns matching/selection and native callbacks, no global GL keyboard access.
Q3 keystroke callback connects actual editor mutation to refreshComboText with
staged selection/text and lifetime rechecks. Test118 types through native input,
replaces generated suffix, clears stale selected values, tests tentative/deletion
and callback counts. Up/down popup navigation and committed case folding still
require follow-up; non-Windows wide classification has not been parity-qualified.

Login combo popup record (2026-09-11, NV-00/01/12/17): Q1 LLComboBox showList/
hideList/onButtonMouseDown/onItemSelected plus LLScrollListCtrl fitContents/
calcMaxContentWidth/hitItem/handleMouseUp. Simple text rows use font height+2,
two-pixel list border and column text width+15. Popup width clamps to control..
max(control,500), height to window-50; preferred above/below flips for available
space. Source focus precedes visibility; button capture transfers to list and
selection returns focus/selected text to editor before close/commit. Q2 native
owned list-ID/item state, measured popup and capture/focus transactions, no GL
list controls. Q3 show/hide and list-pointer methods use existing native tree
dispatch, checking IDs across callbacks. Test117 clicks actual native combo arrow
then Home row and verifies value/focus/capture/visibility. Popup rows' prepared
draw consumer, scrolling long lists, prearrange callbacks, force-pressed visual,
outside-click popup dismissal and autocomplete remain required follow-up work.

Login combo owner record (2026-09-11, NV-00/01/12/17): Q1 LLComboBox constructor/
createLineEditor/initFromParams/getValue/setValue/updateLabel/onCommit/onTextCommit
and LLScrollListCtrl::setSelectedByValue. Combo constructs button, hidden list,
then optional editor before init; editor width excludes max(8,arrow width)+two
button shadows. Selected value is the item's value while editor shows its label;
unmatched value clears selection without replacing text. Commit chooses selected
value or typed text. Q2 native composite IDs and owned single-column item data,
native editor/button children and callbacks, no GL list or combo owner. Q3 typed
createCombo plus checked selection/commit paths; test116 exercises actual child
ownership, labels/values, typed commit, disabled selection and teardown.
This increment does not implement popup list rows/drawing, autocomplete/key
classification, list sorting, dynamic prearrange or full list control behavior.
Those are required next consumers before native login combo closure. Binary
values compare bounded equal-length vectors, not source's unchecked prefix read.

Login layout frame record (2026-09-11, NV-00/01/12/17): Q1 stack reshape marks
dirty; panel visibility marks its owner; animatePanels interpolates visibility
with open/close half-life and snaps above0.99/below0.001. Its shared per-frame
animation flag permits only the first changing panel to interpolate per call.
Native preparation uses explicit delta, retains visible amounts, and marks dirty
through resize publication. Parent-first prepareLayoutStacks updates nested
geometry in one traversal. Test115 covers resizing, dirty state, both half-lives
and settled visibility. updateResizeBarLimits shows no resize bars when every
panel has user_resize=false (login configuration). Collapse/user resizing and
same-frame repeated external preparation remain explicit open coverage, not login
completion. Native frame owner must call preparation once before draw collection.

Login stack declaration record (2026-09-11, NV-00/01/12/17): Q1 registry maps
layout_stack to LLView-derived owner and layout_panel to typed LLPanel construction,
not LLPanel::fromXML; init clears follows then stack attachment sets orientation,
fractions and resize ownership before descendants construct. Q2 native factory
uses typed native panel/init and ordered stack attachment. Q3 independent stack
defaults and explicit min/max aliases, rejecting interactive resize/persistence
not used by login. Test114 checks login proportions, nested rows, exactly-once
typed init before attachment, cleared follows and rejected child cleanup.
Animation settings are retained but frame transitions remain pending; initial
visible panels are already fully visible in source. Resize-bar omission is not
declared closure; login non-user-resizable bar visibility must still be qualified.

Login stack record (2026-09-11, NV-00/01/12/17): source revision
3abd661f498329babaf87b49ffab910fcb5f0e6c, LLLayoutPanel constructor/setOrientation/
getRelevantMinDim/getVisibleDim, LLLayoutStack updateFractionalSizes/
normalizeFractionalSizes/updateLayout. Q1 flexible fractions derive from initial
dimension minus minimum, floor1e-5 and normalization; layout assigns minima then
distributes rounded positive extra space and a single leftover-pixel pass. Fixed
targets retain dimensions. Horizontal starts at left, vertical at top; hidden
panels occupy zero visible extent. Q2 native stack owns ordered panel IDs and
native panel sizing state, with checked geometry plans and native panel children.
Q3 attach initializes fractions; updateLayoutStack stages and publishes panel
shapes through existing native resize ownership. Test113 uses the packaged login
dimensions1024/970/27 and vertical row86/152, wider-window and visibility cases.
This slice implements settled noncollapsed layout. Animation/collapse, interactive
resize bars and persistence remain open; they cannot be claimed implemented by
this static layout check. Login XML integration is the next required consumer.

Panel key record (2026-09-11, NV-00/01/12/17): Q1 LLPanel::handleKeyHere/
setDefaultBtn(pointer), LLUICtrl::findRootMostFocusRoot/getParentUICtrl and audited
native focus/commit contracts. Escape defocuses; unmodified/Shift Tab uses the
root-most focus-root control reached while tab stops permit ascent, skipping
non-control ancestors. Return yields to a focused Return-capturing button, else
commits a locally visible/enabled default, else a text-input control. Q2 native
IDs, focus-root params and independently owned commits; no GL focus/default-button
pointer. Q3 panelKey composes native focus/commit paths, rootMostFocusRoot matches
ascent, and default-button lookup tolerates deletion. Factory parses focus_root/
default_tab_group and preserves them in panel reinit. Test112 covers default
precedence, disabled visibility, Tab wrap/reverse, modifiers and Escape. Named
default-button resolution, concrete subclass overrides and OS event routing remain
open; native key entrypoints alone do not close application keyboard integration.

Panel focus handoff record (2026-09-11, NV-00/01/12/17): Q1 LLPanel::setFocus
first invokes base focus on itself when entering, then focusFirstItem; already
focused subtrees only use the base call. Preemptive self-focus prevents recursion
when the query returns the panel itself. Q2 native requestControlFocus applies
the same handoff using native tree/tab-entry owners, rechecking after focus
callbacks. Q3 no extra panel focus wrapper or GL call; reuse native focusFirst.
Test111 covers self-focus before child, editor select-all, repeat preservation and
empty-panel termination. This closes the currently implemented panel handoff,
not focus-root discovery or missing concrete panel subclass overrides.

Container navigation record (2026-09-11, NV-00/01/12/17): Q1
LLScrollContainer::handleKeyHere checks ignore_arrow_keys before document
delegation, then tries vertical/horizontal bars, calling updateScroll only for
a handled bar. LLPanel and LLView handleKeyHere return false for the eight
navigation keys; native line-editor/scrollbar/container documents have independent
implementations. Q2 typed native navigation input and explicit per-owner dispatch.
Q3 scrollContainerKey preserves document-first and axis ordering, including both
bars moving on an unhandled Page key, with ID rechecks. Factory now accepts
ignore_arrow_keys=true because this gate is implemented. Test110 checks Page
semantics, axis priority, ignore gate and editor delegation. Return/Escape/Tab
and generic OS key routing remain separate open obligations.

Directional focus record (2026-09-11, NV-00/01/12/17): Q1 LLUICtrl::focusNextItem/
focusPrevItem add the text-input prefilter when requested or TabToTextFieldsOnly;
LLView::focusNext scans query in reverse, focusPrev forward, wrapping after current
focused subtree. Forward movement invokes onTabInto even for a sole already
focused candidate; backward skips duplicate entry. LLLineEditor accepts text
input, but read-only setEnabled removes its tab stop. Q2 native candidate query
with integrated text-only predicate and native settings. Q3 shared enterFocus
performs native transfer/selection/notification/flash; moveFocus only chooses
the ordered target. Test109 checks order, wrap, settings and singleton asymmetry.
Additional native text-input widget types and OS Tab delivery remain open.

First-focus operation record (2026-09-11, NV-00/01/12/17): Q1
LLUICtrl::focusFirstItem selects result.back, skips work when already focused,
then setFocus/onTabInto/triggerFocusFlash. Its repeated text-preference queries
cannot change an empty result for the currently audited pure filters.
LLLineEditor::onTabInto performs input-validated selectAll then base forwarding;
LLView::onUpdateScrollToChild forwards to ancestors, whose concrete accordion
overrides remain open. LLFocusMgr flash resets a timer and decays over0.3s.
Q2 native tab query/focus/editor state and native tabInto event, with owned tree
time for flash. Q3 focusFirst rechecks target after callback-capable operations;
prepareScrollContainer executes it for captured bars instead of emitting an
unfulfilled request flag. Test108 checks selection/notification/flash ordering,
repeat-focus no-op, decay and callback deletion; test106 now requires actual
native descendant focus. Accordion scroll forwarding, other widget onTabInto
overrides and platform focus delivery remain open.

Native tab-query record (2026-09-11, NV-00/01/12/17): Q1 LLView::getTabOrderQuery,
CompareByTabOrder/SortByTabOrder, LLViewQuery::run/filterChildren/runFilters,
LLVisibleFilter/LLEnabledFilter/LLTabStopFilter/LLLeavesFilter. Base views allow
descent; LLUICtrl::canFocusChildren returns hasTabStop (the local tab-stop flag).
Hidden/disabled nodes prune branches. Matching controls are returned only if
no matching descendants survive. Stable front-order sorting places groups below
default first, descending within partitions, so focusFirstItem takes result.back.
Q2 native immutable traversal of owned IDs/params; Q3 tabOrder computes candidates
without shared query globals or callbacks and exposes default group explicitly.
Test107 distinguishes default-group partition, same-group stability, leaf versus
parent matches, gated descendants and hidden roots. Concrete subclass overrides,
focus transfer/onTabInto/scroll forwarding and focus-flash timing remain open;
this query alone is not focusFirstItem closure.

Container preparation record (2026-09-11, NV-00/01/06/12/17): Q1
LLScrollContainer::draw paints inner background with control-transparency alpha,
updates scroll, clips only the document to inner edges minus bar strips, prepares
border focus, then traverses visible non-document children back-to-front. Its
focusFirstItem call when a bar captures without descendant focus is still an open
native traversal dependency. Q2 native background/clip/document/chrome packet,
with an explicit requestFocusFirst flag rather than GL focus traversal. Q3
prepareScrollContainer retains source clip origin (different from content-window
origin when border is visible), checks coordinates/colors, and updates border
focus only after successful preparation. Test106 checks exact clip/background,
alpha-only modulation, painter order and focus request state. GPU clip consumers,
native focus traversal fulfillment and once-per-frame acceleration scheduling
remain open; producing a packet does not close those responsibilities.

Scroll-container XML record (2026-09-11, NV-00/01/12/17): source roots are
LLScrollContainer::Params/constructor/addChild, ScrollContainerRegistry and
packaged widgets/scroll_container.xml at investigation revision
3abd661f498329babaf87b49ffab910fcb5f0e6c. Q1 templates define native-representable
size/border/background/rate policy; scrollbar children use separate scrollbar
defaults, and registered panel children attach only after their own XML build.
Q2 independent native defaults/resources produce typed container/chrome owners.
Q3 heap-owned container defaults avoid recursive stack growth; panel attachment
dispatches to native attachScrollContent, preserving delayed attachment and bars
in front. Extended test70 loads packaged templates and checks real document,
rates, child ordering, visible bars and working native wheel movement. Requires
explicit prior loading of scrollbar defaults. container_view/scrolling_panel_list
constructors, automatic recursive template loading, ignore_arrow_keys=true and
background GPU consumption remain open; unsupported children fail explicitly.

Auto-scroll record (2026-09-11, NV-00/01/12/17): Q1 LLScrollContainer::autoScroll/
canAutoScroll and draw frame update. Zones use root-local extents intersected with
inner width/height minus visible strips, limited to one third and max zone.
Horizontal left/right precede vertical bottom/top; range eligibility can activate
scrolling even if rounded rate*frameDelta is zero. Draw accelerates active rate by
120*dt capped at max, otherwise resets to min, then clears active flag. Q2 explicit
native root-local rect/delta, native rate/state and callback-safe range updates.
Q3 separate advanceScrollFrame from query/apply autoScroll so preparation, input
and query mutation are explicit. Test105 checks first-frame zero rate, query
purity, diagonal edges, root exclusion, acceleration and idle reset. Native
root-coordinate producer and drag/drop event routing remain open; ordinary draw
must still consume the native frame update exactly once per UI frame.

Scrollbar XML record (2026-09-11, NV-00/01/12/17): Q1 LLScrollbar::Params/
constructor and packaged widgets/scroll_bar.xml define orientation/range/thickness,
four images/colors and up/down/left/right nested LLButton parameter blocks. Native
parser owns these blocks as parameters, resolves assets/fonts without GL and
selects the appropriate button pair for typed construction. Q2 explicit native
defaults/resources and named native callback registry. Q3 heap-owned defaults and
per-build state avoid enlarging every recursive frame; resource resolution remains
separate from callback binding. Extended test70 loads the packaged template and
constructs both orientations with real arrow/track/thumb assets and a named native
position callback. Full recursive inherited defaults, nested badges and GPU image
consumer parity remain open; XML registration alone does not close construction.

Scrollbar preparation record (2026-09-11, NV-00/01/06/12/17): Q1 LLScrollbar::draw
chooses literal fallback rectangles with asymmetric ends, or image track/focus/
thumb/additive-glow order, after optional background. Source fallback selection
tests both thumb images for vertical, both track images for horizontal, which
does not guarantee selected dereferences are valid. Native reports incomplete
image branch rather than reproducing null dereference. Hover glow targets0.15
using LLSmoothInterpolation calcInterpolant=clamp(1-pow(2,-dt/0.05),0,1); explicit
native frame delta replaces its global timer/cache. LLUIImage::draw/drawSolid
both call gl_draw_scaled_image_with_border with false/true solid flag; this is
not blanket permission to treat drawSolid as an untextured rectangle. Rect color
helpers assign supplied alpha directly. Q2 native prepared screen-space commands
retain colors/images/solid and additive modes, then child IDs, without GL draw.
Q3 prepareScrollbar stages checked primitives and commits glow only after success.
Test104 checks fallback/image geometry difference, focus/glow order, alpha, decay
and retained image lifetime. Bordered-image shader/alpha transitive closure,
LLView child alpha/draw policy and GPU compositor consumer remain open; this CPU
packet is not rendered parity or full scrollbar closure.

Container resize completion record (2026-09-11, NV-00/01/12/17): Q1
LLScrollContainer::reshape applies base child geometry, recomputes inner border
rect, sets vertical document/page then horizontal document/page, then updateScroll.
Q2 native shape planning records container completion in child-first order;
completeShapes publishes geometry before native callback-capable range updates.
Q3 all existing shape-owning callers use completion and recheck owners where
subsequent access is needed. Test103 resizes an ancestor and an explicit container
shape, requiring page/visibility/position/document updates without a later query.
Native batching publishes planned sibling geometry before completion callbacks;
cross-sibling callbacks that observe partially reshaped GL siblings remain an
unverified temporal difference, not a claimed full layout parity result.

Scroll callback publication check (2026-09-11, NV-00/12/17, CPU ownership):
updateScroll's callback-capable axis resets can invalidate an already planned
other-axis descendant. Native publication verifies every planned ID still exists
before applying that plan. Test102 removes only the horizontal arrow from a
vertical reset callback and requires an explicit failure instead of stale map
access or recreating callback-deleted state. Geometry already published before
the callback remains source-ordered; this is not rollback of external mutations.

Reveal record (2026-09-11, NV-00/01/12/17): Q1 LLScrollContainer::scrollToShowRect
first updates content window, clips target dimensions to constraint with top-left
bias, clamps current scroll to allowed interval, updates vertical document/page/
position then horizontal, and applies scroll. It converts adjusted target with
container localRectToScreen and notifyParent; base LLView notifyParent recursively
forwards and concrete accordion consumers remain untraced. Q2 CPU native rects
and native range setters; Q3 scrollToReveal returns the parent-notification rect
as native data instead of calling the GL parent chain. Checked intervals and IDs
guard publication/callback lifetime. Test101 checks minimal and repeated reveal,
oversized top-left bias, exact returned coordinates and inverted constraints.
Concrete parent notification handling remains open; returned data is not closure
of that downstream responsibility or platform scrolling parity.

Wheel routing record (2026-09-11, NV-00/01/12/17): Q1 LLView handleScrollWheel/
handleScrollHWheel/childrenHandleMouseEvent(allow_mouse_block=false) traverses
front-order locally visible/enabled containing children. LLScrollContainer first
offers children; then visible enabled vertical consumes ordinary wheel even at
range boundary, otherwise horizontal returns actual movement. Horizontal wheel
only targets horizontal fallback. Fallback movement calls updateScroll; a handled
child returns directly. Q2 native coordinate-checked traversal and scrollbar
state, independent of mouse capture and GL callbacks. Q3 routeWheel/handleWheel
reuse native contains/range ownership, rechecking after callbacks. Test100 checks
offset coordinates, opaque passive content, axis priority, document translation
and differing boundary propagation. Source event-recorder/logMouseEvent hooks,
non-scrollbar widget-specific wheel consumers and OS delivery remain open.

Scroll update record (2026-09-11, NV-00/01/12/17): Q1 LLScrollContainer::
updateScroll/getContentWindowRect/scrollHorizontal/scrollVertical/getBorderWidth.
Hidden/no-document update returns. Visibility calculation precedes top-left
document translation and bar visibility/shape; hidden axes reset position. Range
setters execute horizontal document/page then vertical document/page. Shape
reserves the optional corner. Content window starts at border X and horizontal
bar top when shown, preserving its source origin rather than assuming symmetry.
Q2 native geometry plans plus checked native scrollbar setter callbacks, no GL
view translation. Q3 preflight all extents then execute source-ordered mutations,
rechecking IDs after callback-capable operations. Negative page dimensions fail
explicitly. Test98 checks cross-axis negotiation, content window, scrolling and
large-to-small reset. Callback reentrancy, detached content, resize lifecycle and
clip/input consumer coverage still require follow-up tests; no full parity claim.
Test99 adds reserved-corner geometry/page distinction, deletion of the entire
container during hidden-axis reset, rejection of oversized document rectangles
at the tree boundary and hidden queries at the maximum valid extent. Update
checks document parent/identity after callbacks; broader
reentrant geometry changes and descendant-only deletion remain open.

Scroll-container ownership record (2026-09-11, NV-00/01/12/17): same source
revision/configuration as the scrollbar pointer record. Q1 LLScrollContainer
constructor/destructor/addChild owns an inward border and hidden vertical then
horizontal scrollbars before init. The first content add becomes scrolled view;
horizontal then vertical are brought forward. Q2 native tree owns independent
border/scrollbar/button nodes and stores document/chrome IDs, resolving explicit
size or native UIScrollbarSize. Q3 constructor configuration is retained on the
heap across child callbacks; partial construction cleans up via native subtree
ownership. Explicit attachScrollContent distinguishes document content from
chrome and rejects cycles via native reparent. Test97 checks before-init owners,
initial ranges, front ordering, first-content policy and teardown. Scroll updating,
Bar controls use explicit native defaults independently of parent control state,
and both steps are16 (source VERTICAL_MULTIPLE); test97 checks both properties.
Clipping, scroll callback configuration and full template inheritance
remain open; this is not a completed scroll container.

Scrollbar pointer record (2026-09-11, NV-00/01/12/17): investigation revision
3abd661f498329babaf87b49ffab910fcb5f0e6c, Windows RelWithDebInfo. Q1 roots:
LLScrollbar::handleMouseDown/handleHover/handleMouseUp/handleDoubleClick plus
pageUp/pageDown/changeLine/setDocPos and native-audited child/capture contracts.
Children get down first; thumb captures with original pixel rect and origin;
track pages by full page, double-click repeats down. Drag permits one-pixel edge
overshoot, maps float ratio to clamped document position and suppresses thumb
refresh for responsive pixel motion. Same delta only recomputes after document
change. Hover clears documentChanged after cursor dispatch; release does not
snap the thumb. Q2 native CPU state/capture/child dispatch, no GL mouse handlers.
Q3 store original thumb and signed64 delta in native scrollbar, retain pixel
publication separately from range updates, check IDs after callbacks. Test96
checks both axes, rounded positions, repeated hover, release, double-click and
callback destruction. Native OS pointer routing, visual glow and full scroller
integration remain open; source overflow is not reproduced.

Scrollbar key/wheel record (NV-00/01/12/17): Q1 LLScrollbar::handleKeyHere,
handleScrollWheel/handleScrollHWheel/pageUp/pageDown/changeLine at the investigation
revision below. Only hidden+zero-range skips all keys; Home/End/Up/Down report
handled even at bounds, Page keys move by page-1 but report unhandled. Ordinary
wheel moves by clicks*step and returns actual change; horizontal wheel excludes
vertical bars. Q2 explicit native key/wheel inputs and the native range owner.
Q3 reuse setScrollPosition callback/refresh semantics with widened arithmetic and
clamping. Test95 checks propagation, overlap, visibility gate and extreme wheel
input. OS routing, drag/track interactions and native scroll-container dispatch
remain open; source arithmetic overflow is not reproduced as defined behavior.

Scrollbar reshape record (NV-00/01/12/17): Q1 LLScrollbar::reshape returns on equal
dimensions, otherwise applies LLView follows, limits arrow length to min(half
container length,thickness), anchors arrows to the two ends and updates thumb.
Q2 native shape plan stages final button rectangles and native thumb alongside
all other dependent widget geometry. Q3 add thumb values to the existing atomic
ShapeChanges publication, keeping callbacks outside geometry computation. Test94
checks short vertical/horizontal bars, explicit shape and failure rollback for
parent/buttons/thumb. Source draw/input and full scroll-container ownership remain
open; this closes only the typed scrollbar's geometry publication path.

Scrollbar constructor/range record (NV-00/01/12/17): Q1 LLScrollbar constructor,
setDocPos/setDocSize/setPageSize/setValue, changeLine/onLineUpBtnPressed/
onLineDownBtnPressed. Constructor initializes thumb then owns two non-tab-stop
buttons with orientation-specific rect/follows, click and held callbacks before
control init. Position clamps to max(0,doc-page); mutation/change callback precedes
optional thumb refresh. Size/page changes reclamp position then refresh even if
position stayed unchanged. Q2 native optional scrollbar state, immutable shared
button configuration and real native child owners; explicit scalar settings
resolve thickness, no GL registry/button/callback. Q3 native range methods retain
callbacks and recheck IDs before post-callback work; wide step arithmetic avoids
source overflow. Test93 checks children-before-init, callback/thumb order, no-op,
clamping, button steps and callback destruction. Native scrollbar reshape, pointer/
wheel/key behavior, images/colors/draw and XML/default templates remain open.

Thumb geometry record (NV-00/01/12/17/18): Q1 LLScrollbar::updateThumbRect uses
track=max(0,length-2*thickness), integer proportional length with minimum16 capped
to track, visible=min(document,page), and integer positional travel. Vertical
origin is top-down with its own minimum-start clamp; horizontal is left-right.
Zero document fills track. Q2 explicit native CPU range/orientation/extent values.
Q3 LLVKScrollLayout::thumb uses signed64 intermediate products, preserving defined
integer rounding and zero-track behavior; it does not emulate source signed
multiplication overflow. Test92 checks both ends/midpoint, empty doc, short track,
large dimensions and negative-size failure. Range state setters, change callback
timing, native button children, drag/wheel/key handling and draw remain open.

Scroll visibility record (NV-00/01/12/17): Q1 LLScrollContainer::calcVisibleSize
at the investigation revision below consumes document/view extents, visible border
width and explicit size or UIScrollbarSize. It removes both border edges, tolerates
one pixel overflow, checks vertical then horizontal, and checks vertical again if
horizontal consumed height. Hidden bars reserve no space. Q2 native CPU dimensions
and flags with explicit resolved size/border inputs, not GL scroller/view getters.
Q3 LLVKScrollLayout::visible implements checked signed arithmetic, preserving
negative inner dimensions of small valid windows rather than silently clamping.
Test91 covers exact overflow allowance, both dependency directions, hidden bars,
border handling and arithmetic failure. Native scrollbar constructors, owner
callbacks, document translation, updateScroll and scrolling input remain open;
this calculation is a prerequisite, not a completed scroll-container widget.

Plain styled append record (NV-00/01/12/17): Q1 LLTextBase::appendText empty-input/
prepend-newline branch, appendAndHighlightText newline splitting and the
non-highlight/non-markdown normal-segment branch of appendAndHighlightTextImpl.
Each LF owns a line-break segment; literal spans use supplied style, while the
terminal normal EOF owner remains independently styled. Q2 native appendPlain
consumes already-resolved literal scalar text/style, not a substitute for URL/
markdown/highlight parsing. Q3 build the complete segment vector in one pass and
publish via the native document transaction, bounding scalar and segment counts.
Test90 covers empty append, consecutive/trailing newlines, explicit prepend,
font ownership, reflow and atomic segment-budget rejection. Parser selection,
cursor/selection restoration and source zero-length style segments remain open.

Styled truncation record (NV-00/01/12/17): Q1 LLTextBase::truncate determines UTF-8
byte length, applies utf8str_truncate, converts the safe prefix to a character
count and removes the tail with removeStringNoUndo to preserve styles. The early
length/4 check is only a fast path. Q2 native validated scalar text determines
UTF-8 length without transient encoding allocations. Q3 truncate calls the same
native atomic range removal, returning whether anything changed. Test89 covers
every byte limit across1/2/3/4-byte scalars, clipped highlight styles, empty EOF
and idempotence. Caller timing (setText deferred truncation versus immediate
append/edit), application callbacks and complete rich-text construction remain
open; this is not a new fixed cap policy for the future native widget.

Styled text edit record (NV-00/01/12/17): Q1 LLTextBase::insertStringNoUndo and
removeStringNoUndo range handling at the investigation revision below. Insertion
snaps increasing through noneditable interiors, extends an editable containing
span (including predecessor at its end), or inserts a default editable span;
later ranges shift. Removal clamps to document length, clips overlapping spans,
removes covered ones and shifts the tail, retaining EOF. Q2 native document edits
stage Unicode and complete range parameters together. Q3 publishEdit rebuilds
validated segments before swapping text/ranges and invalidates layout from edit
position; Edit returns actual position/counts to the future cursor/view-model
owner. Test88 covers noneditable snapping, boundary style preservation, cross-span
removal, empty EOF, leading default insertion and atomic invalid Unicode failure.
This is the range-edit core only: supplied special spans, emoji splitting,
max-byte truncation, before/onValueChange, undo/selection and inline unlink/lifetime
callbacks remain open. No GL view-model/segment callbacks are shared.

Styled document range record (NV-00/01/12/17): Q1 LLTextBase createDefaultSegment/
clearSegments/insertSegment/getSegIterContaining/getEditableSegIterContaining/
getEditableIndex. Default normal segment covers text plus EOF; overlays split
containing segments, remove fully overlapped spans and retain clipped suffixes.
Splitting inside an existing span creates a normal remainder using that style.
Reflow invalidates from the old containing segment start. Interior noneditable
indices snap to begin/end by direction; insertion exactly at noneditable start
prefers an editable predecessor. Q2 native document owns Unicode text, ordered
native segments, layout and invalidation index, without shared index-segment
globals or GL view model. Q3 staged vector replacement provides atomic validation
and stable full coverage, with10000-segment budget and native shared font/image
ownership. Test87 checks overlay coverage, overlapping spans, editable boundaries,
reflow invalidation, default restoration and rejected mutation. Source inline
link/unlink/deferred destruction callbacks are still open; this range owner does
not claim to own referenced inline widgets or replace the full rich widget.

Base segment flag audit: LLTextSegment constructor defaults mPermitsEmoji=true
independently of its canEdit=false. LLInlineViewSegment and highlighted normal
segments explicitly disable splitting; image/newline segments retain the base
flag. Native flags now preserve this distinction, checked in tests84/85 before
document editing/special-segment insertion is added.

Segmented line-loop record (NV-00/01/12/17): Q1 LLTextBase::reflow inner segment
loop at the investigation revision below rounds remaining pixels before fitting,
accumulates float widths/max segment heights, ceils total line width once, and
uses getLeftOffset for alignment. Partial segments wrap without advancing paragraph
number; explicit breaks advance it and update the segment-relative signed line
index (initially -1). Spacing uses rounded maximum line height plus configured
pixel/font adjustment. Q2 native full line rebuild over contiguous immutable
segment snapshots including EOF, no GL document/scroller callbacks. Q3 reflow in
the native segment module reuses established native line/options values with a
bounded-progress guard and checked geometry, preserving signed relative indices.
Test86 compares normal/newline output with existing native plain-layout lines and
tests mixed inline heights, forced split and missing coverage rejection. Outer
reflow responsibilities (partial invalidation, two-pass scrollbar negotiation,
anchor/cursor preservation, updateRects and inline updateLayout) remain open;
this implements the source line loop, not the entire LLTextBase reflow lifecycle.

Image/inline segment metric record (NV-00/01/12/17): Q1 LLImageTextSegment and
LLInlineViewSegment constructors/getDimensionsF32/getNumChars at the investigation
revision below. Image dimensions add3 pixels to width and height versus font
height; absent image retains font height. Midline fit is strictly greater than
image width+3, while first-line-position always takes one placeholder. Inline
dimensions use explicit widget extent plus four pads. Empty offset/count yields
zero dimensions, or default-font height and line break when force_newline. Forced
inline rejects line index0; otherwise a midline exact-width fit is allowed and
the entire replacement-text span is indivisible. Q2 native immutable metric input
includes widget identity/extent and retained native image, no GL LLView/LLUIImage.
Q3 extend the same native segment kind rather than a parallel hierarchy, validating
padding overflow before publication. Test85 discriminates thresholds, first-line
progress, force-newline and indivisibility. Native document owns the future widget
attachment/updateLayout/draw/retirement responsibilities; those are not implemented
by an extent snapshot. Source tooltip/clone/image readiness paths remain open.

Styled-segment metric record (NV-00/01/12/17): Q1 LLNormalTextSegment constructors,
getDimensionsF32/getNumChars/getOffset and LLLineBreakTextSegment constructors/
getDimensionsF32/getNumChars at the investigation revision below. Normal segments
retain style font/height and reserve style-image logical width during fitting;
highlight background disables editing/emoji splitting. Nonempty runs measure font
width and contribute height, including EOF. Fit uses WordsWhenPossible at line
start, WordsOnly otherwise, forces one character at line start and includes EOF
when reaching the document end. Hit testing retains font count-minus-one semantics.
Line-break segments occupy one newline, zero width and font height, forcing a break.
Q2 native retained font/image/range parameters and CPU measurement; Q3 value-owned
LLVKStyledTextSegment with validated ranges and independent native font calls.
Test84 distinguishes line-start/midline progress, EOF fit/height, hit count,
highlight flags, newline and stale-range rejection. Source image-loaded callbacks
to needsReflow, complete style data/draw, rich document editing/reflow, URL parsing,
markdown, emoji and inline widget/image segments remain open. This is a segment
metric building block, not a completed text widget or rich-text parity claim.

Checkbox spacing record (NV-00/01/12/17): Q1 LLCheckBoxCtrl constructor samples
UICheckboxctrlHPad (S32, packaged value2) when deriving wrapped label width from
outer width minus button width minus padding. Cached spacing/vpad are declared
but not used in that body; reshape instead computes width minus label-left.
Q2 read the audited native scalar settings table during native construction,
without sharing LLUICachedControl or installing GL visual subscribers. Q3 copy
the available setting into construction state, retaining explicitly supplied
native padding when absent. Test83 distinguishes new-construction sampling after
a settings update from unrequested reactive reshaping of existing labels.

Checkbox declaration record (NV-00/01/12/17): Q1 LLCheckBoxCtrl::Params/constructor,
WordWrap::declareValues and packaged widgets/check_box.xml at the revision below.
label_text/check_button are parameter blocks, not ordinary child tags; typed
constructor creates label then button, overrides label with outer provided font,
forces button click/return/follows and boolean initial value. Q2 native factory
owns independent parameter declarations and resolves native fonts/colors/images
before calling existing native createCheckBox. Q3 bounded nested parameter nodes
and heap-owned optional checkbox build state avoid inflating every recursive
construction frame; the existing reentrant depth-limit test remains a required
regression check as parameter bundles grow. Parsing continues
reusing native attribute parsing, no LLTextBox/LLButton/registry callbacks. Test70
now also loads the actual packaged checkbox template, resolves real images and
constructs actual native label/toggle children with initial value and label text.
Test82 additionally checks XML embedded value binding, named on_check/commit,
outer provided font override, nested button font retention and atomic rejection
of missing callbacks/invalid booleans against the typed constructor contract.
Rich label semantics, full text-label parameter coverage, recursive base defaults,
checkbox spacing settings and nested button badge construction remain open; this
declaration path does not establish full checkbox or construction parity.

Preedit geometry record (NV-00/01/11/12/17): Q1 LLLineEditor::getPreeditLocation/
getPreeditRange/getPreeditFontSize/findPixelNearestPos, LLView localRectToScreen/
localPointToScreen, LLUI screenPointToGL/screenRectToGL and LLFontGL getLineHeight.
Visible queries are within active composition and at/after scroll; negative offset
uses cursor. Measured text positions include left padding, composition bounds clamp
right to width-border, caret Y uses integer half height. Pixel conversion rounds
logical coordinates times per-axis UI scale. getPreeditLocation passes getRect()
to localRectToScreen for its control output, adding the local rect offset twice;
native preserves this defined returned value distinctly from actual screen bounds.
Font size rounds (ceil(ascender/scaleY)+ceil(descender/scaleY))*scaleY.
Q2 CPU-native font measurement and native ancestry coordinates, explicit scale;
no GL coordinate/helper or font owner. Q3 immutable PreeditLocation return with
checked ranges and scaled signed32 outputs, local pixelPosition helper reuses
native font contract. Test81 covers nested offset/scale, range, bounds, source
control offset and overflow rejection. Candidate-window/IMM consumer mapping and
visual/runtime parity remain open; scale is supplied by the future native window.

Preedit-state record (NV-00/01/12/15/17): Q1 LLLineEditor::hasPreeditString/
resetPreedit/updatePreedit/markAsPreedit/getPreeditRange, plus caller
LLWindowWin32 composition update at the investigation revision below. Window
converts UTF-16 clauses/caret to scalar counts, resets old composition before
results/new composition, and supplies one segment when missing. Editor retains
absolute clause positions/emphasis and overwritten text, inserts without normal
caps/validators, moves caret first to end then requested offset, and notifies once.
Reset restores overwritten text, clears active positions, and does not notify;
selection is deleted via input validation unless preedit already exists, when it
is deselected. Marked reconversion remembers original text only in overwrite mode.
Q2 native CPU composition state and native-tree mode, with no GL preeditor pointer,
font-buffer reset or window ownership. Q3 transactional native Preedit value,
bounded1Mi-scalar/4096-clause inputs, consistent segment/caret validation before
publication, same native keystroke callback. Test80 covers clause boundaries,
ordinary-limit/validator bypass, reset ordering, overwrite restoration, reconversion
and read-only refusal. Invalid/stale spans fail explicitly rather than reproduce
source out-of-range state. IMM message decoding, reconversion OS buffers, candidate
window placement, font-size query, composition commit and native drawing remain
open. No platform IME runtime claim is made from this state-only test.

Pointer record (NV-00/01/12/17), same investigation configuration: Q1 roots
LLLineEditor handleMouseDown/Up/Hover/DoubleClick/MiddleMouseDown/RightMouseDown,
setCursorAtLocalPos/calcCursorPos/findPixelNearestPos, onMouseCaptureLost/
endSelection/startSelection/selectAll. Child dispatch precedes left-down and
uncaptured hover/up. Shift preserves selection anchor; ordinary down hit-tests,
deselects and captures before focus. Select-on-focus first click skips capture.
Hover drag input-validates selection, scrolls at0.05s intervals and requests I-beam.
Capture release ends selection before up checks it, so up must not re-hit-test an
already-ended drag. Double click selects word or whole line; next click within
0.3s selects all. Source word scans bypass input validation except selectAll.
Middle uses primary paste; right focus/base dispatch precedes context-menu work.
Q2 native text/font CPU preparation, explicit pointer/time inputs, capture/focus
owners and textCursor effect. Q3 reuse native hitTest and copied text transactions;
capture teardown updates native selection before external capture callback. Test79
checks screen/local mapping, drag, I-beam, release ordering and word/triple clicks.
The drag check exposed a native caller count error: LLFontGL::charFromPixelOffset
uses max_chars-1 for its scan limit; calcCursorPos leaves the default unlimited
count. Native editor hitTest must therefore include one terminator position in
its bounded count to allow selection through the final character. The font
contract itself is unchanged; the native editor caller now supplies that count.
Native context menu on right-down, caret timing and platform cursor delivery remain
open; right-down currently executes focus/base behavior only. Auto-scroll timing
and source initial timer state need additional parity qualification. Primary copy
is attempted only when the native transport advertises availability; Windows has
no primary selection. No shared GL input/layout callbacks are called.

Clipboard-command record (NV-00/01/12/17), same source below: Q1 canCut/canCopy/
canPaste/cut/copy/copyPrimary/paste/pastePrimary. Password suppresses copy/cut,
not paste; read-only suppresses cut/paste, not copy. Cut input-validates selection,
writes clipboard, then deleteSelection validates removal again; full-text rollback
does not undo the clipboard write. Source ignores clipboard-write failure and may
still delete; native preserves that text behavior while returning the transport
failure explicitly. Q2 native editor commands retain LLVKClipboard owners and
native text/validators, never call GL edit-menu/clipboard singletons. Q3 tree-owned
transport plus checked IDs after callback boundaries; paste feeds the existing
native text transaction. Test78 covers password nondisclosure, ordinary and
read-only commands, cut rollback/clipboard persistence, primary availability and
reentrant widget destruction/transport detachment. No system clipboard contents
are altered by this test. Real window transport integration, native menu routing
and OS event delivery remain open.

Clipboard owner record (NV-00/01/03/12/15/17): Q1 LLClipboard text copy/add/
paste/isTextAvailable dispatch through LLView::getWindow to LLWindowWin32
isClipboardTextAvailable/pasteTextFromClipboard/copyTextToClipboard. UTF-32 text
maps through ll_convert to UTF-16; source addCRLF inserts CR before every LF,
removeWindowsCR removes only CR immediately preceding LF. Base LLWindow primary
methods return false on Windows. Win32 uses CF_UNICODETEXT, Open/CloseClipboard,
GlobalAlloc/Lock/Unlock and SetClipboardData ownership transfer. UTF conversion
helpers in llcommon/llstring.cpp are audited nonvisual scalar/encoding loops,
with no font/window/GL state; invalid surrogate input is checked before calling
the legacy decoder. Q2 native LLVKClipboard owner borrows an explicit HWND and
uses Win32 directly, no LLClipboard/LLView/LLWindow owner or GL callback. Q3 small
native transport interface supports platform implementations and deterministic
consumer tests; RAII closes access/unlocks memory/frees allocation until ownership
transfers. Window/process/thread validity is checked on every operation. Prepare
and validate before opening/emptying clipboard to avoid allocation-failure loss;
unlike the source, failed transfer frees the owned allocation. Primary and
non-Windows transports remain explicit failures. Test77 checks exact newlines,
supplementary Unicode, malformed input, budgets and invalid-window rejection.
No test writes the user's clipboard. Actual Win32 clipboard round trip, native
window lifecycle consumer and clipboard contention/failure injection remain open.

Paste-text record (NV-00/01/12/15/17/18): Q1 LLLineEditor::pasteHelper,
prevalidateInput/deleteSelection, LLWStringUtil::replaceTabsWithSpaces(1)/
replaceChar and UTF-8 length/truncation at the revision/configuration below.
Writable nonempty paste removes disallowed emoji before input validation, then
checks selection removal only for ordinary paste. Tabs become one space, LF
becomes space or U+00B6, and lone CR remains. Capacity counts encoded bytes then
characters; source reports bad-keystroke whenever a character cap is configured,
even without truncation. Primary inserts without replacing selection. Full-text
validation can restore the prior text/cursor/selection and reset baseline.
Q2 CPU-native text transaction with native validator/effect callbacks; system
clipboard access is a separate still-open owner. Q3 pasteLineEditorText stages
cleaning/insertion and byte-safe scalar-prefix truncation in LLVKLineEditor::paste.
Test76 distinguishes pre-clean validation, removal validation, multibyte truncation,
primary semantics, paragraph substitution, configured-cap effect and rollback.
Safety limit: existing text already beyond the selected cap rejects explicitly
instead of adopting source unsigned-capacity wraparound; no parity claim is made
for that exceptional state. Text is bounded to1Mi scalars, invalid scalars/NUL
reject. Clipboard transport and platform round trips remain open.

Delete-command record (NV-00/01/12/17): Q1 LLLineEditor::canDoDelete/doDelete,
removeChar/deleteSelection and rollback, at the investigation revision/config
below. Writable plus passDelete/selection/cursor controls availability. Empty text
does nothing; at end, a nonempty default field still validates/notifies. Forward
deletion calls input prevalidation before cursor advancement and again through
removeChar; first refusal notifies immediately, second refusal retains advanced
cursor. Selection deletion invokes input validation once. Q2 native CPU text and
explicit command availability, no GL edit-menu global. Q3 deleteLineEditor owns
the transaction and uses the same native finishLineEdit validation/rollback/
callback publication as Unicode and key edits; IDs are rechecked after callbacks.
Test75 distinguishes the two prevalidator calls, refusal cursor state, callback,
read-only and pass-through rules. Spellcheck scheduling and native menu/OS command
delivery remain open; the command implementation is not proof of their closure.

History/special-key record (NV-00/01/12/17), same source configuration below:
Q1 LLLineEditor::updateHistory/onCommit and handleSpecialKey Up/Down/Return/
Escape/Insert branches. Nonempty history updates deduplicate the last entry,
append a blank draft and reset the iterator. Up saves the current draft before
decrement; Down recalls without saving. Text recall directly assigns LLUIString
and moves cursor to end without changing mPrevText. Return updates history but
returns unhandled; Escape setText(mPrevText) optionally invokes onKeystroke and
also returns unhandled. LLKeyboard construction initializes INSERT; its
toggleInsertMode flips only on unmodified Insert, though modified Insert is
handled. Q2 native history/text owners and native-tree insert mode replace the
GL keyboard global; native recall owns label resolution and preserves baseline.
Q3 one updateLineHistory helper is used by both native commit and Return, with
history changes retained if later full-text validation rolls back the text, as
in the source. Test74 covers Return/commit dedupe, draft round trip, Escape
baseline/propagation/callback, and native shared insert mode. No GL callbacks are
invoked. OS key routing, error notifications and spelling/caret timing remain
open; history resource exhaustion and application-specific limits are unqualified.

Keyboard edit record (NV-00/01/12/17), investigation revision/configuration as
below. Q1 roots: LLLineEditor::handleKeyHere/handleSelectionKey/handleSpecialKey,
startSelection/extendSelection/deselect, prevWordPos/nextWordPos/removeWord,
removeChar/deleteSelection and LLLineEditorRollback constructor/doRollback.
Shift navigation precedes writable-only special handling. Selection extension
starts an anchor before input validation; left/right move one then skip words
with Control. Word membership is underscore or ambient CRT wide alphanumeric;
only literal spaces are skipped. Nonshift selection collapse includes cursor+/-1.
Backspace removes a selection or character with input validation; Ctrl-word
removal skips input validation. Ctrl-Delete is handled here; plain Delete belongs
to the separate edit-command route. Arrow-ignore/Alt gates and boundaries are
explicit. Handled input deselects unless selection-modifying, validates full text,
rolls back cursor/scroll/selection/text and resets the baseline on failure, then
notifies only accepted keystrokes. The source read-only check also rolls back an
unchanged-text Shift selection. Q2: native CPU text transactions, followed by
native effect/callback dispatch, no GL keyboard globals or font-buffer reset.
Q3: own key/modifier values, staged text edits and ID rechecks across callbacks;
word/selection helpers operate only on native text. Test73 exercises selection,
word deletion, boundary effects, callback counts, validator and read-only rollback.
Timer/spellcheck effects, context menu, Insert/Return/Escape/history keys, generic
Delete routing and OS keyboard delivery remain open. Invalid wide-domain scalars
are not narrowed for CRT word classification. No rendering parity is claimed.

Numeric/alphanumeric validator record (NV-00/01/12/17/18), same investigation
revision and Windows configuration as the ASCII record below. Q1 roots:
ValidatorFloat/Int/PositiveS32/NonNegativeS32/AlphaNum/AlphaNumSpace::validate;
LLLocale construction/destruction in llresmgr.cpp temporarily set LC_ALL to
English_United States.1252 (same fallback on Windows), then restore it.
LLStringUtilBase::trimHead/trimTail use CRT iswspace; LLStringOps digit/alnum
delegate to iswdigit/iswalnum. Float's LLResMgr::getDecimalPoint reads
localeconv()->decimal_point[0], which is '.' in that configured locale. Int/float
allow empty text and lone minus; float allows repeated dots, not exponent/plus.
Positive rejects empty, leading zero/minus, nondigits and strtol results <=0;
nonnegative accepts empty and leading zeros. Windows strtol saturates positive
overflow at 32-bit LONG_MAX, so these are not strict range validators.
Q2: native CPU predicates own a private CRT locale via _create_locale/_free_locale
and use _iswspace_l/_iswdigit_l/_iswalnum_l, without LLResMgr, LLLocale or mutation
of process locale. With digits already checked, positive strtol's boolean result
is determined by the first ASCII digit; nonnegative cannot become negative on
this platform. No GPU resources or publication are involved.
Q3: retain locale ownership in predicate closures so copies outlive widgets safely;
allocation/locale failure cannot publish an unresolved validator. Reject values
outside the Windows wide classification domain instead of narrowing invalid input.
Test72 discriminates intermediate states, whitespace, decimal/exponent syntax,
leading zero, positive overflow, Latin-1 classification and retained closures.
Non-Windows locale/integer conversion behavior and validation error presentation
remain open; those platforms explicitly leave these six names unresolved rather
than silently assume Windows semantics. This is CPU test evidence, not UI parity.

ASCII validator record (NV-00/01/12/17): source investigation revision
3abd661f498329babaf87b49ffab910fcb5f0e6c, Windows RelWithDebInfo. Roots are
LLTextValidate::Validators::declareValues and the validate overloads of
ValidatorASCII, ValidatorASCIIWithNewLine, ValidatorASCIIPrintableNoPipe and
ValidatorASCIIPrintableNoSpace in lltextvalidate.cpp. Q1: predicates scan UTF-32
input, accept empty strings, accept 0x20..0x7f for ascii, additionally LF for
ascii_with_newline, and require alphanumeric/punctuation (or allowed space) for
the printable variants. No-pipe excludes '|'; no-space excludes whitespace.
LLStringOps wide classification delegates directly to CRT iswalnum/iswpunct/
iswspace; within the admitted ASCII range the native predicate uses the same
independently available CRT classification without the viewer wrapper. Source
failure also records a reverse-scan character index/error token; resetError and
showLastErrorUsingTimeout/LLTrans/LLNotificationsUtil presentation remain open.
Q2: accept/reject is native CPU preparation, with no Vulkan commands/resources.
Q3: a private factory resolver supplies stateless native predicates when no native
scoped registration exists, avoiding GL validator globals and notification owners.
Unknown names still fail before widget publication. Test71 discriminates all256
byte values, empty/non-ASCII/invalid-scalar input and both XML validator slots,
plus explicit native registration precedence. This implements boolean ASCII
validation only; error presentation, numeric/alphanumeric locale contracts and
source runtime parity are not closed by the test.

Native line_editor XML now dispatches to typed native editor construction, resolves
packaged background/color/font defaults, input/whole-text validators from native
scopes, keystroke callbacks and documented parameter synonyms. Source MaxLength
Choice uses bytes4096/chars0 defaults; choosing one restores the other's default.
Test70 loads actual packaged line_editor/view_border templates and real images,
constructs a numeric field, types through its native validator/callback and checks
readonly/password and unknown-validator rejection. Spellcheck=true is still
explicitly unsupported pending a native service; clipboard, IME composition,
remaining keyboard/mouse interactions and concrete built-in validators remain open.
No line-editor rendering or startup parity is claimed by this construction test.

Native shape planning now includes LLLineEditor::reshape/updateTextPadding/setCursor
responsibilities, not only node rectangles. Every planned editor stages reclamped
padding and cursor/scroll state before no-fail publication; normal reshape, explicit
shape, panel reinitialization, badge attachment and document resize share that
publication. Test69 checks ancestor follows, explicit shape, border resize and
all-or-nothing failure on invalid editor width. Font measurement remains native
CPU preparation; it is not a GL reshape callback or an implicit GPU mutation.

Native Unicode line input Q1: LLLineEditor::handleUnicodeCharHere/addChar/
deleteSelection/LLLineEditorRollback, setFocus's completed select-all state.
Input requires direct focus/local visibility/writability and rejects control/DEL.
Input validator precedes editing; selection or overwrite deletion precedes byte/
character limits; whole-text validator failure restores snapshot and resets dirty
baseline, omitting the keystroke callback. Successful user edits preserve the old
baseline and become dirty. Q2/Q3: staged native buffer edits and ID-rechecked native
effects/validators/keystroke handlers. Test68 covers ordering, byte cap, rollback,
selection replacement, focus state and self-deletion. Autoreplace, spellcheck timer,
prevalidator error display and native OS cursor hiding remain consumer obligations;
the native hideCursor/badKeystroke effects do not invoke GL UI helpers.

Native line-editor focus/commit Q1: LLLineEditor::onFocusReceived/onFocusLost/
setFocus/updateAllowingLanguageInput/onCommit/updateHistory/destructor. Language
input shutdown precedes focus-loss dirty check/commit, which precedes base lost
callbacks. Commit records distinct nonempty history plus a blank tail, writes its
binding, invokes callbacks, resets dirty and selects all unless disabled/rejected
by input validation. Direct editor destruction disables focus-loss commit first.
Q2/Q3: native ID-aware effects and retained callback snapshots with deletion checks;
Windows IME eligibility is focused/writable/nonpassword/no-prevalidator. Test67
checks exact order, history deduplication, baseline reset, teardown and password
eligibility. Native OS IME consumer, SDL policy, edit-menu ownership, cursor-show
effects and full text-input validation remain separate open integration work.

Native typed line-editor construction installs owned editor text and an inward,
all-follows native border inset one pixel at top/right before control init. Source
LLLineEditor::setEnabled changes read-only/tab-stop, not LLView enabled; init reapplies
explicit enabled after base settings callbacks. Native setValue/dirty/resetDirty/
clear route to the editor's previous-text baseline. Test66 observes border/value
before init, settings versus explicit read-only precedence and teardown. Focus,
language input, validators, history/commit and XML remain required follow-on paths;
this record is typed constructor evidence, not full interactive editor closure.

Native line-editor text state Q1: LLLineEditor constructor/setText/setCursor/
findPixelNearestPos/calcCursorPos/updateTextPadding/selectAll/deselect/clear/
isDirty/resetDirty and native font fitting contracts. Constructor limits default
text but permits descriptive initial text before control init; cursor starts at end.
Equal assignment preserves selection/cursor, other assignment preserves whole-field
selection or deselects and resets previous-text baseline; clear does not reset it.
Cursor scrolling measures real text, password hit-testing substitutes U+2022.
Q2/Q3: native owned text/source/selection/cursor plus native font metrics, staged
state updates on failures. Test65 covers those paths and bounds. Character-count
truncation is scalar-safe rather than retaining the inspected source helper's
mid-UTF8 truncation defect; this difference requires qualification, not a byte-parity
claim. Native tree constructor, prevalidators, focus/IME/editing/history/clipboard,
border/assets and full line_editor XML remain open follow-on work.

Native named image aliases now mirror source separation between LLUIImage identity
and fetched-file texture ownership: one resolved physical path owns immutable padded
pixels, while each UI name owns independent clip/scale/style metadata and logical
dimensions. Metadata views retain the pixel owner, never mutate it, and residency
counts the physical bytes once. Test64 distinguishes names/identities, shared byte
addresses, differing logical regions and retention. Normalized GPU sampler/image
publication must retain both metadata snapshots and pixel ownership when the native
UI draw consumer is connected; this CPU cache does not imply GPU residency.

Native local J2C Q1: LLImageJ2COJ/JPEG2KDecode full-resolution component extraction,
and LLImageGL luminance/luminance-alpha/RGB/RGBA channel meaning. Source component
planes are copied bottom-up; native output expands luminance and opaque alpha while
retaining alpha for two/four components. Q2/Q3: private direct OpenJPEG codec/stream/
image owners with bounded memory callbacks, checked tile/component dimensions and
full unsigned8-bit local pixels. No viewer JPEG2000 wrapper or GL texture code is
called. Strict local codestream completion rejects partial data instead of exposing
the source network progressive-decode policy. Test63 checks a real rounded_square
codestream, skin dispatch and truncation; test62 now covers all packaged declared
formats including J2C. Signed/high-precision/subsampled/network/discard-level decode
remains explicitly outside this local UI path. Kakadu-versus-OpenJPEG lossy byte
parity and runtime visual sampling require separate measured qualification.

Test62 attempts every available packaged PNG/JPEG/TGA texture declaration through
the native catalog and checks pixel byte counts and nonempty logical dimensions.
Missing source files and other formats are counted separately, not treated as
decoded or substituted. This is source-asset decoder qualification, not rendered
GL/native parity, async asset lifecycle closure or completion of widget construction.
Single-channel TGA output uses RGB luminance expansion, matching LLImageGL's
components1 GL_LUMINANCE selection and RGB swizzle; it is not an alpha-only mask.

Native TGA Q1: LLImageTGA::updateData/decode/decodeTruecolor/decodeColorMap and
their pixel/RLE helpers. Truecolor supports 8/15/16/24/32 bits, monochrome8,
palette8 indices and RLE. Right-origin rejects. Truecolor orientation follows top
flag; source palette RLE flips unconditionally. Palette indices subtract start and
clamp; 15/16-bit RGB rounds 5-bit expansion; truecolor all-opaque32 compacts alpha
before skin padding, while palette32 retains it. Q2/Q3: independently compiled
stb_image2.30 TGA-only/no-stdio/static implementation pinned at
2c980bb59875b0d32144a71867fbdebb2f77cd20 with verified archive SHA256. Its TGA path,
memory access, allocation, format conversion and orientation helpers were inspected;
native preflight validates all encoded packets and pixel/byte limits before library
decode, normalizes palette start/indices and corrects source channel rounding.
Test61 covers raw/RLE, orientation, alpha-padding, palette clamp, 16-bit rounding,
truncation and packaged Folder_Arrow. Interleaved input is explicitly unsupported;
source undefined malformed-buffer behavior is not reproduced. Build downloads the
pinned source via CMake FetchContent; offline builds may supply FETCHCONTENT_SOURCE_DIR_LLVK_STB.
The library's dual public-domain/MIT terms remain in its upstream source archive;
no GL viewer decoder or callback is linked into the native adapter.

Native JPEG Q1: LLImageJPEG::decode plus source/error callbacks force JCS_RGB,
write scanlines bottom-up and reject corrupt-data warnings. Q2/Q3: directly audited
libjpeg-turbo memory decoding in private per-operation state, never LLImageJPEG,
LLImageRaw or global setjmp state; errors longjmp only over trivial locals into
the owner's scope. Native opaque RGBA pixels reuse the checked skin extent/metadata
publication step. Header/pixel/encoded byte limits bound allocation and no partial
image publishes on warning/failure. Test60 uses generated red/blue scanlines,
truncated payloads, auto format dispatch and the actual packaged login JPEG.
TGA remains open; source unsupported JPEG color conversions remain explicit library
errors rather than native approximations. Native non-GL target links libjpeg only.

Native font parameter Q1: ParamValue<const LLFontGL*>::updateValueFromBlock/
updateBlockFromValue, LLFontGL::getFontByName/getStyleFromString. Named font aliases
precede descriptor size/style; style uses case-sensitive BOLD/ITALIC/UNDERLINE
substrings; missing descriptors fall back to the configured default font. Q2/Q3:
native control parameters retain a complete LLVKFontRegistry request; resolution
runs after XML attributes, uses explicit native alias/default/fallback inputs and
native font owners. Test59 distinguishes attribute order, native cache identity,
alias precedence, fallback and case matching. Native application setup must supply
the source alias set/default request/fallback, not GL static font getters. Direct
typed fonts without a request retain existing ownership. Current-source aliases
and platform font discovery remain explicit application setup obligations.

Native constructors now resolve image names through an explicitly attached native
skin catalog before publishing declarations/defaults. LLUIImage parameter name
"none" and empty names remain explicit nulls; registered test images take precedence
over skin lookup. Icon value changes stage resource resolution before changing
value/image state. Test58 loads actual texture metadata and button defaults, checks
shared cached named identity across button/icon, live value replacement, failure
retention and no partial widget on missing assets. This establishes synchronous
local PNG constructor readiness, not asynchronous GL texture callback parity or
network image availability; those source paths remain distinct open obligations.

Native skin image catalog Q1: current LLUIImageDecls::load/mergeFile/readRectAttr,
LLUIImageList::getUIImage/loadUIImageByName/preloadUIImage and existing local-file
pixel contract. All-skin texture metadata layers overwrite only provided fields;
nonempty filenames replace, incomplete clip/scale rectangles are ignored, and
scale_outer selects outer mode. Name fallback permits direct filenames. Q2/Q3:
independent bounded Expat catalog and native skin resolver/decode cache; each name
retains a stable immutable owner under an explicit aggregate byte budget. Test57
checks metadata overlays and actual PushButton_Off PNG/scale data, cache identity,
explicit null and failed-publication budget. Current native loading is synchronous
PNG; source async preload/loaded callbacks, TGA/JPEG/network sources and shared
underlying pixel storage across aliases remain open. The existing UI-owned
LLUIImageDecls implementation is inspected only, not reused despite its historical
shared-helper comments. Malformed metadata fails transactionally rather than
silently continuing an invalid layer.

Native skin image layout Q1: local-file branch in LLViewerFetchedTexture,
LLImageRaw::expandToPowerOfTwo/expandDimToPowerOfTwo/scale(false),
LLUIImageList::onUIImageLoaded and LLUIImage::getWidth/getHeight. UI local pixels
expand (or crop at max4096) to powers of two starting at4, anchored bottom-left,
zero-filling ORIGINAL components. Thus RGB padding is opaque black after RGBA
expansion while RGBA padding has alpha zero. Original dimensions define default
clip UV; explicit clips clamp to padded bounds, then logical size rounds the clip
extent. Scale regions clamp against logical dimensions, separately from clip UV.
Q2/Q3: native immutable pixel owner carries padded/logical extents and normalized
clip/scale/style metadata; native decoder performs copies, not GL image operations.
Test56 verifies pixel bytes, alpha distinction, clip/scale units and invalid clips.
Native invalid zero/inverted clips reject rather than permitting divide-by-zero.
GPU image consumers must use pixelWidth/pixelHeight for upload and logical dimensions
for layout; skin publication/sampling and nine-slice consumers remain open.

Native factory file consumers now fall back to LLVKSkinFiles for logical XUI paths,
read source-selected files and merge locale layers before parsing. Explicit fixture
declarations/layer lists take precedence when supplied. Test55 reads packaged
colors and view_border/badge/button/icon/panel templates through native filesystem
selection and constructs native controls with native fonts/colors. Missing images
remain absent; the test explicitly does not substitute pixels or claim rendering.
Native skin image acquisition/metadata and the remaining widget templates still
need their own construction/resource consumers.

Native skin files Q1: LLDir::setSkinFolder/addSearchSkinDir/findSkinnedFilenames/
walkSearchSkinDirs. Directory order is executable fallback when working directory
differs, default/selected/theme installation roots, then default/selected user
roots, deduplicated. Unlocalized root/textures bypass language probing; other
subdirectories discover en then en-us in default skin. Current policy selects each
language independently at the most-specific existing root; All preserves every
match in root/language order. Q2/Q3: native owned configuration/cache with injected
existence probe and standard filesystem reads, no LLDir skin globals or chat-log
mutation. Test54 checks policy/order, caching, invalidation, texture lookup and real
packaged button IO. Input filenames reject traversal and absolute paths. Native
cache invalidation is explicit rather than preserving stale process-global caches;
root-directory case comparison currently folds ASCII, with non-ASCII Windows path
case qualification open. Nonvisual chat-log updates from setSkinFolder are not a
responsibility of this native visual file resolver.

Native logical declaration files now carry explicit ordered physical-key lists.
Source LLXMLNode::getLayeredXMLNode skips empty overlay paths and repeated base
paths, parses every other layer and fails on missing/invalid files. Native
constructFile/loadDefaultsFile and panel filename resolution all use LLVKXmlLayers
with the same order and cumulative budget, retaining normal native construction
rollback. Test53 covers locale label/string/geometry updates, default-template
inheritance, repeated-base skipping, references to layered panels and missing files.
The in-memory physical-key store is an explicit input, not OS path discovery:
skin/theme/language search and file IO still require native resolver integration.

Layered XML Q1: LLXMLNode::getLayeredXMLNode/updateNode: matching root name values
permit updates regardless of root tag. Body text replaces unconditionally; only
existing attributes update. Child matching uses name then value fallback, ignores
tags, never appends unmatched nodes and rotates the search after a match. Q2/Q3:
bounded native Expat element tree with owned attributes/text/children, source update
algorithm and escaped serialization into native declaration consumers. Limits are
4MiB/layer, 64MiB total input/output, 10000 nodes/layer and depth64; no DTD/external
entities. Test52 distinguishes attribute addition from update, duplicate-key
rotation, missing-key continuation, value keys, root mismatch and parse rollback.
Path selection/repeated base filename skipping belongs to native resolver wiring,
not this in-memory merge. This implementation calls no LLXMLNode or GL XUI helper.

Native view_border declaration construction uses LLViewBorder::Params enum values,
thickness/style synonyms and existing native border constructor contract. The
factory parses the packaged border template into non-control defaults; panel border
fields use the same parser and preserve live color identity. Test51 loads actual
colors/view_border declarations and checks standalone type, follows, panel overrides
and rejection cleanup. Border rasterization, dynamic focus highlights and nested
base-view border parameter overrides remain separate obligations, not covered by
this constructor test.

Reentrant native constructor/post-build child creation now receives an explicit
Construction context instead of relying on process-global registry state. Q1:
source panel scopes remain active through initPanelXML and virtual postBuild,
including factory calls made within those hooks. Q2/Q3: synchronous nested native
construction shares the active registry/factory stack and cumulative budget through
a weak-lifetime context; use after its owning build returns fails without touching
the tree. Every build entry has a 64-level depth guard, including constructor
recursion before panels are parented. Test50 creates a scoped button in post-build,
checks retained handler ownership, expired-context rejection and recursive-hook
cleanup. Each concrete native constructor remains responsible for cleanup if it
fails before transferring its newly created panel; this matches explicit ownership
rather than guessing which unrelated callback-created nodes to erase.

Native panel-constructor scopes Q1: LLPanel::fromXML/createFactoryPanel,
LLRegisterPanelClass dispatch and LLRegistry::addScope/getValue. Registered class
construction precedes scoped XML initialization. New callback scopes go to the
front, but panel factory maps are pushed at the back and searched from the front:
innermost callback wins, outermost named child factory wins. Q2/Q3: native
PanelConstructor returns a newly owned detached panel, local callback table,
native child factories and optional native post-build routine. constructPanel
performs constructor-only ownership without running init callbacks; regular plain
panels retain their distinct default typed-init/post-build path. ID boundaries
prevent adoption/deletion of pre-existing widgets; RAII limits scopes to building
the panel, referenced children and post-build. Test49 checks both precedence rules,
single initialization for custom constructors, retained callable snapshots and
failure cleanup. Unknown native class names fail explicitly. Concrete application
panel classes and reentrant factory entry from custom hooks still need their own
source-backed migration; this mechanism alone does not close those dependencies.

Named callback construction Q1: LLUICtrl::initFromParams/initCommitCallback/
initEnableCallback resolve callback names at constructor/init time, not while XUI
default parameters are parsed; direct supplied functions take precedence. Q2/Q3:
native Callback/Validation parameters retain optional names and fixed arguments,
and factory construction resolves owned callable snapshots without a GL registry.
Test48 distinguishes deferred default loading, missing-handler construction failure,
fixed argument binding and direct-function precedence. This is the prerequisite
for native constructor-local registry scopes; scope installation and concrete
custom-panel migration remain open rather than assumed from global resolution.

Native referenced-panel path Q1: LLPanel::initPanelXML filename branch,
setXMLFilename/setShape, LLUICtrlFactory::createChildren and the ordinary panel
records. Constructor-owned filename wins; otherwise the outer declaration supplies
it. Referenced parameters set raw owner dimensions and their children construct
before outer XML initialization. Then explicit outer fields/callbacks/strings
override reference values, final panel init reshapes existing children, outer
children construct, and external parenting/post-build finish. Q2/Q3: explicit native
declaration table, bounded Expat parse, checked native shape assignment and tracked
reference expansion; missing files/cycles/64MiB cumulative expansion fail with
partial-owner cleanup. Test47 distinguishes dimensions, filename visibility,
callback order, override precedence, retained child sets and rollback. Supplied
documents are expected to be selected by a future native skin/locale resolver;
LLXMLNode layered-file merge and custom panel factories remain open. Source's
ignored initPanelXML failure is not reproduced as a successful partial native panel.

Panel string XML Q1: LLPanel::LocalizedString/initFromParams/getString,
LLXUIParser::readXUI/readStringValue, LLXMLNode::getSanitizedValue/getTextContents,
utf8str_removeCRLF. Attribute strings keep XML-normalized whitespace. Nonempty body
text overrides value attributes; unquoted bodies trim space/tab/LF then remove CR.
Quoted bodies remove escape slashes, retain interior whitespace and append line
feeds (single completed line drops its final LF). Last repeated name wins. Q2/Q3:
native Expat data handling plus owned native string values, installed after control
init and before child construction; no GL-owned LLXMLNode/LLUIString calls.
Test46 covers each form, substitution, installation order, duplicates, malformed
nesting and no partial widgets. Missing mandatory string values reject explicitly;
full malformed-parameter source tolerance remains an open qualification item.

Plain panel transitive correction: LLPanel::createFactoryPanel falls back to
LLUICtrlFactory::create<LLPanel>, which performs default typed init AND post-build
before fromXML calls initPanelXML. LLView::initFromParams changes name/layout but
does not install declared rect; LLPanel installs that after control init. The
native factory now creates a default native panel, runs default post-build, then
initializes that SAME ID from declaration before children/attachment/final post-build.
Existing action callbacks remain connected in order; validators all run and AND
their results (llboost.h::boost_boolean_combiner does not short-circuit). Test45
checks both init phases, names/rectangles, retained commit and validation connections.
Source-dependent custom class factories and filename children remain explicitly
open. Earlier single-phase panel-description text is superseded by this transitive
record; test44 still covers delayed external parenting and border timing.

Post-1819c5fecf plain panel XML Q1: LLPanel::fromXML/createFactoryPanel/
initPanelXML/initFromParams (ordinary unclassified panel with no filename), plus
the existing typed panel/control/border records. Source constructor defaults remain
in panel-specific state while the control init callback runs; declared panel state
and optional border apply afterward. Child constructors run while the panel is
detached from its external parent, then attachment inherits the parent's last tab
group unless explicitly provided, followed by post-build. Q2/Q3: a distinct native
panel declaration branch carries constructor and initialization parameter snapshots,
uses native children and delays parent attachment. Test44 observes state/border
timing, external hidden-parent isolation and tab-group inheritance. Nonempty class
and filename remain explicitly rejected pending native class/factory scopes and
referenced-file support; they are not silently treated as generic panels. This is
plain-panel evidence, not closure of every panel subclass or factory callback.

Native visibility propagation Q1: LLView::setVisible/onVisibilityChange,
LLPanel::onVisibilityChange/initFromParams, LLButton::onVisibilityChange. Equal
local values do not notify; changes below an invisible ancestor do not propagate.
Locally visible descendants receive the effective value before a panel emits its
visible callback. Button visibility invalidates text cache generation. Panel
callback installation occurs after control init. Q2/Q3: native tree recursion over
snapshotted IDs with ownership rechecks; no viewer recorder or GL dirty-region
globals. Setting subscribers snapshot IDs so visible callbacks may delete controls
without invalidating iteration. Test43 checks timing, hierarchy, order and deletion.
Native dirty-region publication, text popup hiding and application event-recorder
integration remain separate open side effects, not claimed by these callbacks.

Typed native panel/border construction Q1: LLPanel constructor/destructor,
addBorder/removeBorder/initFromParams/getString; LLViewBorder constructor,
Params and base view ownership. Typed panel construction creates a border before
control init; panel init replaces it and installs strings/label/background/badge
holder state afterward. Borders fill local rectangles, follow all edges and are
not controls. Q2/Q3: native panel/control and border/view components with retained
native color/image owners; nested ownership is retired by tree IDs. Test42 checks
before/after callback border identity, string timing, bounds/follows and teardown.
Border replacement allocates before retiring prior state for failure safety, not a
GL factory call. Widths outside the implemented source range reject. Texture border
style is retained as dormant source state, not invented texture drawing. XML panel
construction is distinct: class/factory resolution, callback scopes, referenced
files and parenting AFTER children remain open and must not be routed through the
ordinary widget-parenting path without those contracts.

Native default loading now parses view/icon/button/badge template declarations
without constructing controls. Explicit ordered overlays preserve prior fields,
provided geometry and pressed-image flags; button default image identities are
captured after resolution and remain distinct from per-instance customization.
Default button badge payload is separate from global badge defaults, preserving
constructor equality/omission behavior. Test41 distinguishes overlay preservation,
no construction side effects, custom disabled-image fallback and failed load
rollback. Q1 roots: LLUICtrlFactory::ParamDefaults constructor/loadWidgetTemplate/
create and existing typed constructor records; Q2/Q3: native parameter owners and
explicit resource/path order. Full LLXMLNode keyed-child overlay semantics, recursive
base-class provided-field filling, locale/skin path discovery and currently
unimplemented widget templates remain open rather than equated to this API.

Packaged color probe exposed `ChicletFlashColor value="SchemeLightest"`, not a
reference attribute. LLXUIParser::readColor4Value accepts >=3 numeric components;
LLInitParam::Multiple::validate counts valid elements without requiring every
stored element valid. The color insertion loop handles unresolved entries without
failing the whole file. Native loading now warns/skips invalid numeric entries,
accepts RGB with default alpha, and does not reinterpret named values as aliases.
Malformed XML still fails atomically. Test39 separates invalid entry content from
structural parser failure; test40 exercises the actual packaged case without edits
to the reference asset. Default-choice initialization quirks remain source-analysis
evidence, not a mandate to publish an invalid entry under a fabricated alias.

Test40 exercises the packaged default colors.xml and widgets/button.xml through
the native parsers with native font and named image fixtures. It checks retained
color/image identities and declared height/flash/glow fields. Image fixtures are
one-pixel test images, not a skin rendering/parity claim; skin image files, clipping
and scale metadata remain separate asset obligations. Constructing a template as
a declaration tests parsing, not layered default-parameter inheritance.

Native color declaration loading Q1: LLUIColorTable::loadFromFilename/
insertFromParams plus the value-taking setColor overload. Literals overwrite in
declaration order in the requested layer; reference map insertion retains the
first alias declaration per name. Alias chains resolve against loaded colors and
copy terminal values into loaded entries, even when processing a user file (the
inspected source's explicit mLoadedColors target). Missing/cyclic chains warn and
are skipped while independent entries survive. Q2: bounded native Expat parse and
native value-map resolution; Q3: stage maps/slots before no-fail publication so
syntax/allocation failure preserves live references, with no GL XUI parser.
Test39 covers forward chains, copied alias identity, user versus loaded source,
cycles/missing references, independent publication and malformed/DTD rejection.
File-search layering and persisted user serialization remain caller integration
work; the loader does not infer path order or mutate user profile files.

Native color loaded/user layers now implement LLUIColorTable::setColor/
isDefault/resetToDefault/clearTable. First user override moves the existing slot
identity into the user layer and retains a copied original in loaded defaults;
previous widget references therefore see the override and later reset. clear does
not remove names: it sets both layers to magenta. Source user-only colors report
default even without a reset target. Test38 checks these distinctions. Declarative
alias resolution is value-copy resolution, not a live alias graph; loader work
must preserve that distinction from widget references to table slots.

Factory resources now resolve named native fonts and named/live or literal RGBA
colors for icon/button/badge declarations. Native widget parameters carry
LLVKColor reference identity; icon draw preparation snapshots its RGBA before alpha
modulation so later theme updates do not mutate prepared work. Button construction
also retains inspected image/flash/overlay colors, hover glow and overlay delta for
its still-open draw consumer. Test37 checks native font ownership, live update
through constructed controls, immutable prepared colors, literal identity and
explicit unknown-name rejection. Source font descriptor composition/default file
layering and color aliases/loading/persistence are still distinct open obligations.

Native color reference Q1: LLUIColor constructors/set/get/isReference and
LLInitParam::ParamCompare<LLUIColor>::equals, LLUIColorTable::getColor/setColor.
Named colors retain references that survive loaded-to-user overrides; literals
compare by value while any reference compares by identity. Q2: native color slots
owned by a native table and retained by native parameter values; no GL color table
or LLUIColor graph. Q3: single UI-thread updates, retained slot lifetime and explicit
RGBA snapshots for later GPU publication; nonfinite changes reject atomically.
Test36 distinguishes literal/reference equality, distinct names, live updates and
table destruction. Layered color XML, alias graphs, user/default persistence and
widget integration remain follow-on obligations, not implemented by slot ownership.

Checkbox local-state follow-up Q1: LLCheckBoxCtrl::reshape/setLabel/setLabelArg/
setEnabled/setTentative/getTentative and LLUICtrl::setTentative. Checkbox reshape
refits the label from width minus label-left; wrap-down retains prior label top.
The click rectangle expands to the new label bounds and never shrinks. Enabled
changes replace label foreground with captured enabled/readonly colors, not the
label enabled flag. Tentative forwards to the button and clears before commit.
Q2/Q3: native checked reshape planning integrates the specialized checkbox branch,
then publishes all rectangles atomically; native label parameters/child state carry
color and tentative effects. Test35 covers these states, predicate refresh,
argument-driven label refit and overflow rejection. Rich-label support and full
factory/template integration still remain; this is bounded typed checkbox evidence.

Checkbox binding correction from transitive source: LLCheckBoxCtrl::setControlName
forwards to mButton. LLUICtrl::setControlName/setControlVariable disconnect prior
subscription, look up the named setting and assign its value; empty names do
nothing, unresolved names disconnect without changing value. Therefore the button
toggle writes its binding BEFORE invoking checkbox onCommit. The outer checkbox's
setControlValue normally has no bound variable. Native construction/rebinding now
reflects this owner rather than writing late through the outer checkbox. Test34
distinguishes initial binding, rebind/disconnect, empty/missing names, and direct
embedded activation when the outer checkbox is disabled. Scope-dependent source
findControl registries remain a separate native application integration obligation.

Checkbox typed construction Q1: LLCheckBoxCtrl constructor/destructor,
setValue/getValue/isDirty/resetDirty/onCommit/draw and LLUICtrlFactory::create.
Constructor makes a label (blank becomes one space), optionally overrides its
font, fits it, applies wrap-down translation, then creates a frontmost toggle
button whose click rectangle covers the label. Embedded Return commit is disabled;
its click callback calls the enabled checkbox's bound-value write then commit.
The button owns value and dirty state; on_check is a draw-time predicate that
updates differing values without committing. Q2: native label/button/document
children and explicit owner IDs; value/dirty access forwards to the actual button.
Q3: native construction snapshots and rechecks IDs after nested init and retires
partial children; no GL factory, label, button or view model. Test33 covers actual
children before init, front click area, keyboard policy, commit, binding, dirty and
teardown. This initial typed path uses the explicit plain-label control; rich label
URLs, inherited color/tentative/focus forwarding, specialized checkbox reshape,
binding-name rebinding and full checkbox XML remain open, not hidden behind the
plain label implementation.

Plain control follow-up: native settings enabled callbacks now use the native
setEnabled path so text read-only state follows both initialization and later
updates. Constructor rejects failed typed initial assignment and removes its
document. Context refresh stages resolved text/LLSD alongside button/badge labels,
then publishes as one tree update; invalid text leaves both context and nodes
unchanged. Test32 exercises these callback/transaction boundaries. Typed settings
value updates still need a whole-subscriber transaction when one value is invalid;
this constructor fix does not claim that broader setting pipeline closed.

Native plain control Q1: LLTextBase constructor creates text_contents (500x500,
non-mouse-opaque), default segment and document rectangles; LLTextBox constructor
disables triple click. LLTextBase::setValue calls the virtual text assignment,
which resolves label arguments, removes CRs, deselects, truncates by UTF-8 byte
limit and moves cursor to start unless track-end. LLTextViewModel setDisplay/value
dirty semantics are native-owned; init resets dirty after user callback and restores
explicit read-only after enabled initialization. Q2: LLVKPlainControl owns source,
display, cursor/selection, native document ID and optional native layout. Q3:
explicitly plain/non-scrolling typed construction avoids falsely registering the
full text widget; no GL text model/segment/cache objects are reused. Existing native
tree/control lifecycle installs state/document before callbacks and owns teardown.
Test31 checks callback-visible document/value/dirty/read-only, post-init override,
UTF-8 byte truncation, CR removal, two-line reflow, fit invalidation and deletion.
Rich URL/style/inline/scroll behavior, key/pointer selection, full text XML and
clipboard/IME remain open; plain control does not close LLTextBox replacement.

Plain non-scrolling document placement implements the inspected
LLTextBase::updateRects / LLTextBox::reshapeToFitText branch: union includes
zero-width line rectangles, adds vertical padding to bounds top, translates lines
by top/center/bottom/baseline policy, sizes the document to max(view,text) height,
then anchors overflowing documents to the view. Fit adds twice the pads and one
extra width pixel, including the source's already padded bounds height. Test30
checks those exact rules for one line, overflow, integer center rounding and
coordinate failure. LLVKPlainTextLayout is separate from pre-existing untracked
llvktextlayout files using an unavailable LLFontVK API; those files remain intact
and are not integrated into this native target. Newline positions are scanned once
per paragraph, avoiding repeated suffix scans at each narrow-width soft wrap.

Plain text line-layout Q1: LLTextBase::reflow/getLeftOffset,
LLNormalTextSegment::getNumChars/getDimensionsF32/updateLayout,
LLLineBreakTextSegment::getNumChars/getDimensionsF32, LLFontGL::getLineHeight.
In the single-font plain-text path, paragraphs split at explicit newline segments;
word wrapping is permissive on an empty line, forces one character if none fit,
and includes the newline or EOF position. Soft wraps retain paragraph number.
Line height sums separately ceiled ascender/descender in logical units. Width is
ceiled once per line after F32 remaining-width subtraction; line spacing rounds
height*multiple then adds explicit pixels and FSFontLineSpacingAdjustment.
Right alignment reserves one extra pixel. Q2: owned native line records from native
font fitting/measurement and explicit layout parameters. Q3: independent CPU layout
avoids GL segment objects/cache invalidation, with checked coordinate arithmetic
and a one-million-codepoint input bound. Test29 checks empty/trailing newline/EOF,
forced progress, paragraph numbering, line spacing, right padding and overflow.
This covers only single-font plain normal/newline segments; rich runs, per-segment
images, document/scroll rectangles, selection anchoring and two-pass reflow remain
explicit separate obligations. No text widget constructor is claimed by this test.

Native button and badge labels now consume LLVKLabel originals/argument maps with
an explicit tree formatting context. Button setLabel/setLabelSelected/
setLabelUnselected/setLabelArg preserve arguments and invalidate text generation,
but do not auto-resize; context refresh resolves both labels and badges before
publishing any node changes. Source assign and LLUIString copy overload distinctions
are retained as separate future API obligations, not aliased silently. Test28
checks constructor substitution, retained local arguments, default precedence,
selected/unselected independence, currency re-resolution from originals and no
geometry mutation. UTF-32/UTF-8 conversion uses the audited nonvisual llstring
conversion path, not font or text-widget wrappers.

Native label-state Q1: LLUIString assign/setArg/setArgList/clear/updateResult and
Tea::wrapCurrency. Original text and arguments survive independent assignments;
clear preserves arguments. Default-map insertion wins over local duplicate keys;
format replacement is nonrecursive, followed by nonrecursive currency replacement.
Q2: LLVKLabel owns text/arguments and takes an explicit defaults/currency snapshot;
no LLUIString, LLTrans globals or Tea globals are called. Q3: no lazy mutable cache
avoids stale context versions; audited nonvisual LLStringUtil::format is reused as
the formatting service. Inspected roots getSubstitution/getTokens/simpleReplacement,
formatNumber/formatDatetime, convertToS32/F32/F64, LLStringOps date-code/time-offset
accessors and LLDate::toHTTPDateString perform parsing, standard stream/locale/time
formatting, string conversion and diagnostics, not visual construction/callbacks.
This preserves that service's process locale/date configuration; native lifecycle
must supply its nonvisual setup and serialize access (strftime/setlocale/gmtime are
not per-widget state). No sharing of the GL UI label owner or its default registry.
Test27 distinguishes precedence, absent/empty tokens, nested brackets, replacement
order and retained arguments. Date/locale platform qualification, formatted-output
budgets and glyph-consumer integration remain open; this is not a text-control
constructor or a claim that LLTextBase's rich segment/reflow paths are replaced.

Native badge XML now supports both concrete badge controls and button.badge
parameter blocks. The latter are not runtime children: resolved native defaults
and provided payload feed the button constructor equality test, and only changed
badge fields instantiate a badge before button init. Test26 checks entity-safe
labels, relative location, provided-zero offsets, default omission, duplicate
rejection and actual badge components. Native defaults match the inspected badge
template's percentages/padding/requests-front/mouse-opaque fields; fonts and themed
colors/images must still be supplied by native resource resolution. Nested widget
content in a badge parameter block is explicitly rejected rather than discarded.

Button/badge constructor integration now uses resolved native BadgeConstruction
defaults and an optional provided badge payload. LLBadgeOwner's equals comparison
decides whether to construct; its badge factory init/post-build precedes attachment
to the button and the button's init callback. Button post-build moves an existing
badge to the nearest accepting ancestor, retaining its owner ID. Lazy
LLBadgeOwner::setBadgeLabel construction uses defaults, seeks a holder and fronts
the actual badge in its parent. Native code rechecks IDs after nested constructor
callbacks and removes partial children on failure; unlike source raw pointers,
stale badge IDs are detected. Test25 covers both construction routes, init ordering,
holder migration, label fronting, visibility and holder-owned destruction. Dotted
badge XML parameters and scroll-aware positioning remain open integration work.

Badge construction Q1: LLBadge::Params/equals/LLBadge/addToView/setLabel/destructor,
LLBadgeOwner::initBadgeParams/createBadge/addBadgeToParentHolder/setBadgeVisibility/
setDrawBadgeAtTop, LLBadgeHolder constructor/setAcceptsBadge/addBadge. Source
constructor applies relative-location percentages (not clamped), separately tracks
provided center offsets, and creates a control before its factory init. Equality
excludes owner and base-control fields and compares offset values, not provided
bits. Attachment front-parents the badge, reshapes to the parent's local rectangle,
and retains the original weak owner. Holder search begins at the owner's parent and
skips nonaccepting ancestors. Q2: native badge component/control with immutable
native images/font, weak owner ID, explicit holder capability on native tree nodes.
Q3: distinct owner and parent IDs allow source ownership behavior without borrowed
GL LLView/LLHandle or dynamic_cast; existing checked reshape/reparent preserve tree
invariants. Test24 covers init timing, provided offsets/equality, native percentages,
front attachment and original-owner versus parent deletion. Scroll-container
ancestor registration, rendered badge geometry, string substitution, full template
resolution and integration in button constructor/post-build remain open here.

Button flash lifecycle Q1 (same current-source revision/configuration as the button
record): LLButton constructor/setFlashing/setToggleState/destructor,
LLFlashTimer constructor/startFlashing/stopFlashing/tick/onUpdateFlashSettings/unset,
LLEventTimer::updateClass and LLTimer::start/reset/stop. Timer count is twice the
positive parameter or configured FlashCount; nonpositive period uses FlashPeriod.
Each eligible update uses F32 elapsed > period, resets its origin and toggles once,
not elapsed-time catchup. Start sets highlighted/running but preserves count; stop
clears both flags and count. Settings changes stop and replace both settings,
ignoring original parameter overrides. Timer completion leaves the button flashing
flag for subsequent draw policy. Toggle cancels and resets force/alternate color.
Q2: CPU-only timer state owned by the native button, explicit monotonic native tree
clock and native settings updates. Q3: value ownership removes the global timer
registry and dangling settings subscribers; node deletion retires the state
immediately without reproducing dormant unset/deferred-deletion defects. Negative
counts normalize to zero; nonfinite periods/times reject instead of retaining
unusable schedules. Test23 distinguishes strict thresholds, restart versus stop,
one tick per update, runtime settings, zero period/count, deletion and toggle reset.
Draw-time flashing/glow composition remains a separate open consumer obligation.

Native button declaration construction now dispatches to LLVKButton with owned
font/image defaults, explicit per-file image identity comparisons, provided pressed
flags and independent native callback lookup. Dotted callback parameter elements
are parsed as parameters rather than child controls. Init and click/commit callback
ordering is tested from actual XML. UTF-8 label conversion uses the audited
nonvisual utf8str_to_wstring body (llstring.cpp:399): only string/byte operations,
no visual services; Expat rejects malformed input before it reaches that routine.
Pointer focus respects a locked external subtree without aborting button capture,
as the source ignores rejected keyboard-focus requests. Unimplemented parameters
and unresolved callback names fail explicitly; themed font/color/image-declaration
resolution and badge/checkbox/flash extensions still need implementation.

Native button pointer path Q1: LLView::childrenHandleMouseEvent (front order,
visible/enabled/default bounds drilldown, mouse-opaque fallback), base LLUICtrl
pointer signals, LLButton left/right/double/hover handlers and onMouseCaptureLost.
Left-down traverses children before capture/focus, then base dispatch, own down,
timer/frame reset and sound. Captured left-up stops the timer before releasing
capture, then base/own up and inside-only sound/toggle/commit. Right mouse has
separate base signals and no left-button timer setup. Hover gates held callbacks
on both elapsed time and frame count and increments count per eligible hover.
Q2: native pointer events carry explicit screen coords/modifiers/time/frame; native
tree routes capture, converts coordinates and dispatches only native control code.
Q3: snapshots of child IDs/handlers, current ownership checks after mutation,
distinct base/button handlers, no OS/GL globals; cursor/sound effects go to audited
native handlers. Tests cover callback order, capture-loss commit, no duplicate
mouse-up commit, outside release, frame/time threshold, opaque children and callback
deletion. Customized source drilldown, event-recorder integration, and platform
input/IME remain separate open obligations, not inferred from these CPU tests.

Button activation Q1 roots: LLButton::onCommit/handleUnicodeCharHere/handleKeyHere,
setToggleState and LLUICtrl::onCommit. Programmatic commit emits down/up(undefined),
requested sounds, toggle, then the commit signal. Unicode space and unmodified
Return (unless disabled) suppress repeats and only toggle/commit. Click callback
registered in the constructor precedes initFromParams' commit callback; both see
the value snapshotted when the signal starts. Q2: native typed activation methods,
explicit repeat/modifier data, native image/font/control owners and native sound
effect handler. Q3: each callback invocation snapshots its callable and argument
and rechecks the ID after invocation; self-deletion stops subsequent effects.
Sound handling is injected by a future audited application audio consumer, not GL
UI sound helpers. Tests verify exact event ordering, bound toggle writes, rejected
keys and self-deleting handlers. Mouse routing/capture and specialized signal
connection mutation still need their own tests and implementation.

Native button constructor slice, 2026-09-10: Q1 roots LLButton constructor,
postBuild/autoResize/resize, setToggleState and setLabel overloads. These construct
labels/images before LLUICtrl init, default selected label only when absent,
measure space for legacy padding fallback, compare image identities to defaults
for disabled fading and pressed substitutions, and measure the current label on
postBuild regardless of autoResize. Resize only grows, accounting for overlay
height scaling/alignment. Toggle writes the bound value before its own value,
stops flashing and invalidates text generation; setting labels does not auto-resize.
Q2: LLVKButton state in the native tree owns native images, UTF-32 labels and
explicit metric scale; createControlImpl installs it before init callbacks.
Q3: explicit provided-image flags and immutable native asset identities, owned
parameter snapshots, checked resize and native font measurement. No GL font/image
owners or GL constructor callbacks. Check constructor-observed state, image
fallback precedence, padding, label fallback, post-build width and grow-only toggle
resize. Badge/checkbox construction, flash timers, input routing, image/style draw
preparation and XML button dispatch remain required follow-on portions of this
same constructor inventory entry; they are not treated as completed by this test.

Prior font/text work was committed as `dfab8c2fd2` at the user's request. The next
commit is gated on closing native widget construction, followed by startup/
presentation routing. This construction gap is still OPEN; no closure commit or
startup-routing change has been made.

Current implementation: native tree ownership/geometry, typed control state,
focus/capture callbacks, native PNG image input, and concrete base-view/icon
construction from declarations. `INTEGRATION_TEST_llvkwidgettree` executes sixteen
passing cases under Windows/MSVC 14.44 RelWithDebInfo. Generated link dependencies
exclude llrender, llui, llwindow, llimage, llvulkan and GL/Vulkan loader libraries;
executable imports contain no OpenGL/Vulkan loader. Editor diagnostics are clear.
Existing GL code remains unchanged. These checks establish only the tested native
construction operations, not full widget inventory or historical-oracle parity.

Still required before closure: all remaining registered/custom constructors and
child registries (including panel, button, text/editing controls, menus and floaters),
layered templates/provided values, native image clip/scale/skin resolution and other
formats, callback XML resolution, focus history/default focus/popups, specialized
layout and visibility effects, native input/editor/IME integration and the declared
construction exit-gate tests. The factory accepts view/icon only and rejects
unsupported inputs; that rejection is not an implementation of the missing control.

Native scalar setting notification policy: LLControlVariable::setValue (llcontrol.cpp:214)
converts string input for declared booleans then compares by declared type before
notifying; direct LLViewModel::setValue always dirties. Native defineSetting now
takes an explicit scalar type (Boolean/Integer/Real/String), or Opaque for already
dispatched values without equality suppression. It does not infer declaration type
from LLSD storage. Boolean spellings follow convertToBOOL's listed tokens; rejected
spellings map to false. Tests distinguish equal setting writes from equal control
assignments. Settings validation, saved/default/unsaved stacks and composite-type
comparison still belong to the required application service integration.

Factory parent-order correction: construction now attaches each node after its own
init and before creating children, matching the source factory's ancestor visibility.
The descendant-init test observes the constructed root already attached externally
while the descendant itself is still unparented. On failure, erase the new subtree;
restore external-parent tab metadata only if its child list is restored, preserving
unrelated callback mutations. Erasing a plain view containing native controls invokes
the control focus-release protocol, not a callback-free destruction of focused
descendants. Tests cover the actual ancestor chain and descendant focus-loss teardown.
Earlier staging descriptions below are superseded by this verified ordering change.

Native icon declaration integration: factory dispatch now includes an actual
LLVKIcon constructor, using the source icon.xml defaults (name icon, no tab stop,
mouse-transparent, follows left/top, white color). Resolved native font defaults
and named images are explicit native inputs; no GL parameter construction runs.
Icon geometry goes through the independent XUI resolver; image_name, literal RGBA,
interaction/alpha/min-size and supported control bindings are parsed before init.
Native control postBuild runs after children, preserving requests_front handling.
Test constructs an icon beneath a top-left view declaration and checks real icon
state, owner identity, template defaults, geometry and color errors. Themed color
names, per-node font descriptors, full callback XML syntax and image skin metadata
remain explicit unsupported inputs, not skipped parameters. Transactional root
staging still differs from source external-parent visibility during descendant
callbacks; this must close before the full factory construction gate can pass.

Native icon constructor, 2026-09-10: Q1 roots LLIconCtrl constructor, draw,
handleHover, setValue/loadImage in lliconctrl.cpp:54-177. Constructor seeds base
value with image name before initFromParams. It does not reshape to image/minimum
size; both nonzero minimum dimensions affect known texture draw size only on load.
Draw scales to local rect and multiplies alpha only, choosing draw-context alpha
or control transparency. Hover requests a hand only if interactable and locally
enabled. Q2: LLVKIcon in native tree owns immutable image/color/alpha policy with
native control state; prepareIcon returns screen rect and retained native image.
Q3: image/font owners are present before init callback, value overrides resolve
only from the native image table, and missing images stay absent. UUID conversion
uses audited nonvisual LLUUID/LLSD, not viewer texture fetch. No platform cursor
calls occur during preparation. Tests verify constructor/init ordering, unchanged
geometry, alpha-only modulation, known-size hint and missing-image behavior.
Image clip/nine-slice metadata, UUID asset loading/priority residency, themed colors
and XML icon construction remain required; this record does not close the icon
inventory entry until those consumers are implemented and qualified.

Native widget PNG input, 2026-09-10: Q1 roots LLPngWrapper::readPng/normalizeImage/
updateMetaData and readDataCallback: libpng expands palette/gray/tRNS, strips16,
applies screen gamma2.2 using gAMA or reciprocal default and writes reversed rows.
LLUIImage owns the texture and its dimensions; its construction cannot supply a
native control image because that owner is GL-dependent. Q2: LLVKWidgetImage owns
immutable named dimensions and bottom-up straight RGBA bytes, independent of GL
LLImageRaw/LLImageFormatted/LLUIImage and the checkpoint Vulkan image wrappers.
Q3: direct API-independent libpng with bounded memory reads; decoder state and row
vectors live outside the setjmp frame's destructible locals. Libpng errors longjmp
only to that frame and release the heap state. Limits: encoded/decoded64MiB,
8192 per axis, ancillary allocation4MiB/chunk and cache128. Palette/gray pixels
become RGBA; missing alpha becomes255 without premultiplication. Native publication
and clip/scale metadata resolution remain separate work. Test checks exact bottom-up
RGBA and truncated input rejection with caller-byte lifetime independent of output.
TGA/JPEG and layered skin metadata are not yet implemented or implied by this API.

Native control callback safety check: createControl snapshots both input blocks
before callbacks, since native callers may release their original storage during
init. Each setting event updates only its bound property, matching the separate
branches in LLUICtrl::controlListener; updating enabled must not recalculate and
overwrite independently changed visibility. Tests destroy parameter storage from
init and independently mutate local flags between setting notifications.

Native control state construction, 2026-09-10: Q1 roots LLUICtrl constructor,
initFromParams, setControlVariable/enabled/visibility helpers, decideVisibility,
initCommitCallback/initEnableCallback, onCommit/setValue, postBuild and setFocus.
LLViewModel's value assignment always dirties. Init installs bindings and commit/
validation before init callback, hover handlers after it; factory parenting follows
initFromParams. A provided control name suppresses initial_value even when unresolved.
Enabled bindings special-case string "0"; visibility combines both controls.
Commit does not auto-validate or auto-write settings. PostBuild brings marked direct
children forward in the source front-list iteration order. Q2: tree-owned native
control values and settings bindings, explicit native font owner, native callbacks
taking checked IDs/LLSD values rather than GL widget pointers. LLSD is audited
nonvisual value storage/conversion from llcommon; no LLViewModel/LLUICtrl is used.
Q3: publish a fully initialized control to its parent only after native init returns;
recheck ID after callbacks, snapshot callable/value before invocation, and release
all value/font/settings targets with the node. Settings mutations are explicit
native-table updates, not subscriptions to GL settings callbacks. Missing binding
names are currently ignored as in source; diagnostic reporting remains open.
Tests cover init/parent order, binding/initial-value precedence, validation versus
commit, hover-handler timing, live setting updates, dirty state, retained font and
self-deleting callbacks. XML construction of derived controls, their concrete
callbacks, focus history/defaults and popup teardown are still required before
claiming full native widget construction. Generic control state is not a button,
panel, editor or text control replacement.

Native focus/capture construction dependency, 2026-09-10: Q1 roots
LLFocusMgr::setKeyboardFocus/setMouseCapture/setTopCtrl/releaseFocusIfNeeded and
LLFocusableElement focus notifications (llfocusmgr.cpp:92-452). New state is
published before callbacks. Focus loss bubbles up, gain descends, common ancestors
are omitted; a nested focus change cancels remaining outer traversal. Locking
restricts focus to the locked subtree. Control destruction releases descendant
capture then keyboard focus; top references are removed without top-lost callbacks.
Q2: native tree owns focus/capture/top IDs and native-only scoped handlers, not
gFocusMgr/LLFocusableElement. Q3: checked IDs, copied invocation targets and focus
epochs survive callbacks changing focus or deleting unrelated subtrees. Destruction
marks its subtree unavailable before callbacks and rejects overlapping destruction
or reparenting that could resurrect it; this defines safe behavior where source
deletion during callbacks is unsafe. Ordinary view erase is callback-free; control
erase invokes focus-release protocol. Registered native callbacks must be audited
at their concrete call sites; no existing GL callback registry is used.
Check branch order, common ancestors, nested focus redirection, locks, capture-before-
focus teardown, invalid IDs and resurrection rejection. Default-focus restoration,
focus history, control-specific onFocusLost/commit and popup bookkeeping remain open.

Construction resource policy: LLVKWidgetTree and the native XML factory use a
10000-node/64-level limit. Source-invalid cyclic ownership and excessive trees fail
before publication, not silent truncation. Reparent validates the entire subtree's
new depth before detaching it. Tests include external-parent attachment failure,
subtree cleanup and preservation of the old parent and metadata. These bounds are
an explicit native resource policy, not source limits or a full-UI coverage claim.

Native view geometry queries, 2026-09-10: Q1 roots LLView::calcBoundingRect,
calcScreenRect, localPointToScreen and pointInView; LLRect::isEmpty uses edge
equality, containment excludes top/right. Bounds union includes locally visible
children except the current top control and translates into parent coordinates.
Q2: native tree computes current bounds/screen rectangles and containment from
native IDs and an explicit top-control ID. Q3: on-demand CPU queries avoid stale
hidden-view caches; coordinate overflow is an explicit failure. Queries do not
call GL focus/UI globals. Tests exercise union/translation, hidden/top children,
half-open hit edges and deletion invalidation. Dirty-region propagation and
control visibility callbacks remain distinct obligations.

Native base-view factory, 2026-09-10: source LLDefaultChildRegistry::Register<LLView>
(llview.cpp:90), factory createWidgetImpl/defaultBuilder and LLView initFromParams,
getRequiredRect (= own rect) and postBuild (=true). Q1: resolve defaults/explicit
params, transform XUI layout, construct/init, parent at front, build children in
declaration order, invoke postBuild. Q2: LLVKWidgetFactory constructs native value
nodes from typed parameters or Expat-parsed base-view declarations. Q3: native-owned
defaults, explicit geometry-provided flags, inherited layout string, decoded text,
bounded XML input with no DTD/entities, callback exception containment, rollback of
partial subtrees. The constructed root is attached to the external parent only on
success; plain-view construction has no postBuild action to observe staging. Future
control callbacks must get their own ordering contract, not assume this suffices.
Checks: actual nested view XML, sibling layout, inherited defaults, tab0, malformed
children, unsupported tag rejection and unchanged external-parent metadata on failure.
Only `view` is currently accepted; other control tags and unimplemented attributes
are explicit errors, not placeholder nodes. This is deliberately not a claim of
complete factory/widget coverage. Focus, dirty/bounds reporting and all specialized
control constructors remain required work before the requested construction commit.

Native declaration geometry, 2026-09-10: Q1 roots are LLView::applyXUILayout,
get_last_child_rect (llview.cpp:2497-2690), and ParamValue<LLRect>::updateValueFromBlock
(llui.cpp:688). Explicit opposing edges win dimensions; otherwise a supplied dimension
and supplied edge determine the other edge. Parent-relative positive/negative edges,
top-left inversion, recent declared sibling, minimum height10, padding4 and explicit
delta override retain the source's value/provided distinction. Q2: native
LLVKWidgetLayout holds independent value/provided fields and resolves a native rect;
parent/sibling information comes only from LLVKWidgetTree. Q3: pure CPU resolution
on copied params with checked output coordinates, no native/GL owner creation until
geometry succeeds. No use of GL LLView, LLUI parameter code or LLRect interpreter.
Checks cover edge/dimension precedence, top-left/negative coordinates, sibling
padding and explicit delta override. Export conversion and specialized per-control
layout remain separate obligations. This replaces the inspected runtime geometry
operation, not all widget construction.

Native construction work, 2026-09-10, after commit `dfab8c2fd2`:
NV-00/01/03/12/17. This section records an implemented base ownership slice, not
closure of the complete widget inventory. Source roots: LLView constructor,
addChild/addChildInBack/removeChild, reshape, visible/enabled chains and
parseFollowsFlags in llview.cpp. Q1: own parent/child relationships, front-insert
children, reparent before insertion, store tab groups, apply follows translation
and resizing recursively, preserve local flags while querying ancestor flags.
Q2: LLVKWidgetTree owns native value nodes with never-reused IDs, explicit geometry,
local visibility/enabled flags and child order, without LLView/LLUICtrl/GL Params.
Q3: lifecycle-owned CPU tree; mutation is single-threaded, callers retain IDs rather
than raw pointers across mutation. Reshape plans checked geometry before publishing,
so overflow does not leave a half-resized tree. Cycle creation is rejected rather
than reproducing an invalid source ownership graph. Subtree erase invalidates IDs.
Absent factory tab groups map to INT32_MAX; provided zero remains distinct. Follows
string parsing preserves exact case/whitespace token behavior. Allocation exceptions
propagate without partial parenting. This is not a callback-dispatch abstraction.
Check: isolated TUT executable excludes llui/llrender, verifies front insertion,
detach/reparent/cycle rejection, subtree lifetime, all 16 follows combinations,
ancestor flag queries and overflow rollback. Bounds propagation, focus/capture
release, virtual reshape hooks, per-control constructors and declaration factory
remain separate required implementations; no native widget closure is claimed here.

## UI-FACTORY-001: getDefaultParams<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L120).
**1.** Return const reference to prototype in factory-owned heterogeneous map,
obtaining ParamDefaults<T::Params,0>; obtain may create an entry. No value copy.
**2.** Native construction consumes stable typed defaults; resolution is CPU work
but constructors can allocate GL fonts in reference. **3.** Prefer audited neutral
semantic defaults cached under CPU-service lifetime over a GL parameter facade;
factory destruction/reconfiguration must define reference invalidation.
Check repeated/specialized first access and reference lifetime. Outgoing: instance,
LLHeteroMap::obtain, ParamDefaults constructor/get and parameter constructors.

## UI-FACTORY-002: ParamDefaults<PARAM_BLOCK,DUMMY>::ParamDefaults

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L290).
**1.** mPrototype is default-constructed before body. Lookup registry tag by parameter
type. If found, default-construct another PARAM_BLOCK, loadWidgetTemplate into it,
fill prototype from it. Then cast prototype to base_block_t and fill from recursively
obtained ParamDefaults<base_block_t,DUMMY>. Most-specific template is therefore
offered before base defaults. Body does not clear provided bits or catch failures.
**2.** CPU default precedence plus backend-neutral resource identities. **3.** Prefer
preserving audited fill semantics in CPU types to separately reimplementing schema
inheritance; do not cache native device objects in schema defaults.
Check explicit versus absent values at each inheritance level, missing template,
base tag collisions and construction-time font lookup. Outgoing: each parameter
constructor, LLWidgetNameRegistry, loadWidgetTemplate, fillFrom, obtain and base type.

## UI-FACTORY-003: ParamDefaults<BaseBlock,DUMMY>::ParamDefaults

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L308).
**1.** Empty body terminates template recursion; member mBaseBlock still constructs.
**2.** CPU recursion terminator. **3.** Reuse only after BaseBlock construction and
destruction audit; an empty body does not establish no side effects.
Check exactly one base termination and member lifetime. Outgoing: BaseBlock ctor/dtor.

## UI-FACTORY-004: create<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L135).
**1.** Mutates caller params by fillFrom cached defaults, calls createWidgetImpl with
params/parent. If returned pointer nonnull invokes postBuild and ignores bool. Returns
pointer. No layout transformation or child XML traversal in this body.
**2.** Native CPU construction/action registration with explicit owner. **3.** Preserve
direct-construction policy separately from XML-construction failure policy; do not
invent universal postBuild rejection. Check false postBuild remains returned, params
provided flags/default merge and parent insertion order. Outgoing: defaults cache,
fillFrom, createWidgetImpl, every T postBuild/constructor and failure path.

## UI-FACTORY-005: createWidgetImpl<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L210).
**1.** validateBlock false warns with filename/type but still constructs new T(params).
Call widget->initFromParams. If parent nonnull, tab group is params value only if
isProvided(), otherwise S32_MAX; setCtrlParent. Return pointer. No local new failure
handling, deletion on init exception, or parenting-result test.
**2.** CPU model/layout/control/action initialization independent of GPU owners.
**3.** Audited native construction transaction with explicit parent ownership and
failure rollback; preserve defined validation policy rather than rejecting formerly
accepted declarations implicitly. Check invalid block, init failure, provided tab0
versus unspecified, and constructor-time callbacks. Outgoing: validateBlock,
T constructor/initFromParams, tab parameter wrappers, setCtrlParent, diagnostics.

## UI-FACTORY-006: defaultBuilder<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L236).
**1.** Copy cached defaults into params; construct LLXUIParser/readXUI with current
filename. If output node: copy params, T::setupParamsForExport(output,parent), copy
name to output, writeXUI against defaults/default_parse_rules. Then set from_xui=true,
T::applyXUILayout(params,parent), createWidgetImpl, createChildren using
T::child_registry_t::instance(). Finally widget nonnull and postBuild false -> delete
widget and return null. Child creation precedes postBuild; no local child success
aggregate. Export occurs before layout transformation. Exceptions have no local
transaction rollback.
**2.** Native CPU declaration parsing, layout and post-construction semantics.
**3.** Prefer a typed neutral declaration/layout pipeline with explicit prepared
results to invoking reference GL-owning controls. Keep per-type builder overrides
and failure rules explicit. Check export versus runtime rects, child postBuild before
parent postBuild, false parent postBuild destruction, parse failures and partial trees.
Outgoing: all T Params methods, parser read/write/lifetime, default_parse_rules,
layout/export static targets, child registry, construction/children/postBuild/dtor.

## UI-FACTORY-007: createFromFile<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L153).
**1.** Set widget=null; push filename. getLayeredXMLNode failure warns and jumps to
pop/return null. Else createFromXML(root,parent,filename,registry,null output). Nonnull
view dynamic_cast<T*>; cast failure warns, deleteView(view), nulls local view. Always
normal-flow pop then return typed pointer. Exception does not run that pop locally.
**2.** Native CPU file/declaration loading and typed root ownership. **3.** Scoped
diagnostic provenance and ownership, with preserved wrong-type destruction. Check
missing file, wrong type, nested loads, thrown callback and file stack recovery.
Outgoing: push/pop, layered XML, createFromXML, deleteView and T type hierarchy.

## UI-FACTORY-008: getDefaultWidget<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L189).
**1.** Value-initialize T::Params, assign name from string_view converted to string,
return create<T>(params) with default parent null. **2.** CPU fallback-widget
construction. **3.** Audit every fallback type and lifetime; a missing declaration
must not conceal a native GL-resource construction. Check name ownership after input
expires and false postBuild route. Outgoing: T::Params, name assignment and create.

## UI-FACTORY-009: LLChildRegistry<DERIVED>::Register<T>::Register

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L313).
**1.** Base StaticRegistrar gets tag and supplied callback, or defaultBuilder<T> if
callback null. In body, if factory does not exist create it. registerWidget with
T type, T::Params type and tag. Schema registry block is commented out. Static
registration can instantiate factory before application entry.
**2.** Native CPU registry of audited constructors, not a shared GL draw callback
registry. **3.** Separate constructor targets by lifecycle while sharing audited
declaration names; global registration must itself remain backend-independent.
Check static order, custom builder selection, duplicate tags and factory recreation.
Outgoing: StaticRegistrar, singleton lifecycle, registerWidget and each T/custom
callback. Concrete registrations/conditional compilation inventory remains OPEN.

## UI-FACTORY-010: LLUICtrlFactory constructor

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L77).
**1.** Initialize dummy panel null; other members default-construct. No dummy panel
allocation in body. **2.** CPU service ownership. **3.** Prefer lifecycle-owned
factory to implicit graphics construction during static registration; determine
whether audited base/member construction permits sharing. Check before settings,
font service or window initialization. Outgoing: LLSimpleton/base and member ctors.

## UI-FACTORY-011: LLUICtrlFactory destructor

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L82).
**1.** Empty active body; dummy panel deliberately not deleted. Members still
destruct, including parameter cache; referenced UI/font values need ownership audit.
**2.** Native lifecycle teardown with callbacks disconnected before CPU/GPU owner
destruction. **3.** No process-lifetime leak as substitute for explicit ownership;
preserve externally visible teardown ordering while correcting only reviewed leaks.
Check partial init, normal close, cache references and dummy children. Outgoing:
member/base destructors, heterogeneous cached parameter destructors and dummy owner.

## UI-FACTORY-012: loadWidgetTemplate

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L89).
**1.** Build widgets/<tag>.xml with directory helper; findSkinnedFilenames(XUI,path).
Empty list returns. Take front as base_filename; empty front skips body. Else push
base filename directly onto file stack; layered XML success -> local parser reads
into supplied BaseBlock; failure warns. Pop afterward, no exception scope. This body
does not itself parse one file per path or define layering merge semantics.
**2.** CPU skin/localization/default resolution. **3.** Share audited path/XML/default
semantics with neutral parameters, not copy a single skin as native hardcoded style.
Check empty paths/front, layer conflict, parser failure and exception file stack.
Outgoing: directory helpers, LLXMLNode::getLayeredXMLNode, parser, BaseBlock targets.

## UI-FACTORY-013: createChildren

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L124).
**1.** Null node returns. Traverse firstChild/nextSibling in XML order. For each,
create empty output child if exporting. Call createFromXML(child,viewp,empty filename,
provided registry,outputChild). Failure -> lookup tag in default registry: known tag
warns invalid child of this parent, unknown warns could not create; both include
name attributes and line number. Continue siblings on failure. If output child has
no children, attributes or value, remove it regardless of creation result. No local
aggregate failure return, rollback or viewp null guard.
**2.** CPU hierarchical construction with parent-specific legal children and order.
**3.** Preserve registry eligibility and partial-tree policy in native preparation;
an XML tag inventory alone cannot choose the correct child constructor.
Check invalid registered child versus unknown, failed postBuild, empty export,
mutation during custom callback and sibling order. Outgoing: XML traversal/mutation,
createFromXML, parent/default registries, output deletion and diagnostics.

## UI-FACTORY-014: getLayeredXMLNode

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L184).
**1.** findSkinnedFilenames(XUI,filename,constraint); if empty use supplied filename
as only path, then return LLXMLNode::getLayeredXMLNode(root,paths) bool. Header
default constraint CURRENT_SKIN. **2.** CPU asset lookup and layered declarations.
**3.** Shared audited resolver with exact precedence and fallback, independent of
GPU image resource lookup. Check absolute input, current/default skin, locale layers,
empty and malformed files. Outgoing: directory resolver, layered XML merge/parser.

## UI-FACTORY-015: createFromXML

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L217).
**1.** Read node tag, lowercase it; registry.getValue. Missing callback returns null
before dummy-parent work. Null parent: lazily construct LLPanel::Params and create
dummy LLPanel, then use it as parent. Invoke chosen callback(node,parent,output).
Return view. filename argument is unused in body; no node null guard, callback
failure cleanup or dummy-allocation success check.
**2.** Native CPU tag dispatch with explicit root ownership. **3.** Native root
construction must preserve required parent-dependent layout without requiring a
GL-owning hidden panel; alternatives are audited neutral root or explicit root
context after panel behavior is known. Check uppercase tags, unknown tag, first
parentless request, nested callback and failed dummy creation.
Outgoing: XML names, lowercase, registry lookup, LLPanel Params/constructor/postBuild,
selected callback and dummy-panel lifetime. Not closed by defaultBuilder inspection.

## UI-FACTORY-016: getCurFileName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L244).
**1.** Empty file stack -> empty string; otherwise copy back. **2.** CPU diagnostic
provenance. **3.** Scoped preparation context avoids cross-thread shared stack.
Check nested/empty state and concurrent caller obligations. Outgoing: string/container
ownership; relevant stack writers remain individually recorded.

## UI-FACTORY-017: pushFileName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L250).
**1.** Resolve base-language skin XUI filename and push result, even if empty.
**2.** CPU diagnostic provenance. **3.** Keep base-language identity separate from
localized merged declaration sources. Check fallback and nested loads. Outgoing:
findSkinnedFilenameBaseLang, global directory service and allocation failure.

## UI-FACTORY-018: popFileName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L257).
**1.** Unchecked vector.pop_back. **2.** CPU context scope exit. **3.** Prefer scoped
restoration that handles exceptions; underflow is not a compatibility requirement.
Check matching pushes on all normal/error returns. Outgoing: each stack caller.

## UI-FACTORY-019: setCtrlParent

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L263).
**1.** S32_MAX tab group replaced with parent->getLastTabGroup(); then
parent->addChild(view,tab_group). Does not inspect addChild result. **2.** CPU tree,
focus/tab and layout relationships. **3.** Explicit parent-owned native control tree
after auditing addChild's virtual dispatch and reparenting side effects.
Check omitted group versus explicit max/zero, rejection and existing parent.
Outgoing: getLastTabGroup, concrete addChild overrides and lifetime behavior.

## UI-FACTORY-020: copyName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L271).
**1.** dest.setName(src.getName()->mString), no null checks. This copies XML element
tag, not its name attribute. **2.** CPU schema export. **3.** Use same audited XML
node operation if export remains supported; never confuse element tag with control
identity. Check element/attribute name difference. Outgoing: XML name storage/setter.

## UI-FACTORY-021: registerWidget

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L285).
**1.** Lookup existing tag by parameter-block type. Existing different name emits
stderr diagnostic then deliberately writes through null pointer; same name returns.
Otherwise defaultRegistrar.add(parameter type,name). widget_type is unused in active
body; schema/type registry calls commented out. **2.** CPU deterministic schema
identity validation. **3.** Typed registry errors instead of undefined null-write;
retain diagnostic meaning but not undefined behavior. Check same Params/same tag,
same Params/different tag and unused widget type. Outgoing: registry lookup/add,
type identity and static registration ordering.

## UI-FACTORY-022: saveToXML

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L207).
**1.** Active body returns0 without using arguments or writing a file. **2.** No GPU
work or implemented export result exists here. **3.** Do not invent native export
requirements from method name; separately trace defaultBuilder output-node export.
Check callers' expectations and no output side effect. Outgoing: callers only;
dormant stub is not proof that all export routes are dormant.

## UI-FACTORY-023: LLUICtrlLocate::Params::Params

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L55).
**1.** Base LLInitParam::Block<Params,LLUICtrl::Params> constructs, then set name
locate and tab_stop=false. **2.** CPU invisible layout marker defaults. **3.** Use
neutral control defaults after base closure; even an empty marker inherits font
construction effects. Check absent/overridden name/tab and base font accesses.
Outgoing: base constructor and parameter assignment/provided semantics.

## UI-FACTORY-024: LLUICtrlLocate constructor

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L62).
**1.** Delegates to LLUICtrl(p), empty body. **2.** CPU marker node/control semantics.
**3.** Share only audited neutral node behavior; absence of draw content does not
eliminate parent/focus/input ownership. Check construction/initialization via factory.
Outgoing: LLUICtrl constructor, implicit destructor and inherited virtual operations.

## UI-FACTORY-025: LLUICtrlLocate::draw

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L63).
**1.** Empty override; does not call LLUICtrl::draw or render children. **2.** Native
preparation emits no visual content for this target. **3.** Preserve absence of
rendered child traversal, not just transparent marker geometry. Check legal children
and direct versus parent-driven traversal. Outgoing: callers, child registry and
inherited lifecycle; no callee in draw body itself.

## UI-DEFAULT-001: LLHeteroMap::obtain<T>

Source: [llheteromap.h](../../../indra/llcommon/llheteromap.h#L42).
**1.** Lookup typeid(T). Missing -> new T(), capture typed deleter, emplace raw
pointer/deleter by type; use returned iterator without checking inserted bool.
Return dereferenced typed stored pointer. Object construction precedes insertion,
so same-type reentrant obtain has no in-progress marker. If construction reentrantly
inserts same type, outer newly allocated pointer is not deleted on emplace failure;
emplace exception also has no local pointer cleanup. No synchronization in body.
**2.** CPU typed-default cache; native GPU resources must not be hidden in default
values. **3.** Alternatives: eager typed defaults, guarded lazy cache, or existing
cache after audit. Guarded CPU-service-owned lazy cache best accommodates recursive
base defaults, with defined same-type cycle error and transactional insertion;
this is a candidate pending actual reentrancy callers, not an approved rewrite.
Check hit avoids construction; base-type recursion; same-type recursive construction;
constructor/emplace failure; factory teardown with references. Outgoing: T ctor/dtor,
type identity, allocation/map operations and deleter. No GPU effect proven absent
until every T is audited.

## UI-DEFAULT-002: LLHeteroMap::deleter<T>

Source: [llheteromap.h](../../../indra/llcommon/llheteromap.h#L70).
**1.** Cast void* to T* and delete it. **2.** CPU typed ownership teardown; T may own
resources/callbacks transitively. **3.** Keep deletion type explicit; native device
retirement cannot be delegated to arbitrary parameter destruction. Check concrete
parameter destructors and reference ownership. Outgoing: each cached T destructor.

## UI-DEFAULT-003: LLHeteroMap destructor

Source: [llheteromap.cpp](../../../indra/llcommon/llheteromap.cpp#L22).
**1.** Iterate unordered entries, call stored deleter(pointer), then null pointer.
Map destroys afterward. No semantic destruction ordering, reentrant-mutation guard
or exception isolation in body. **2.** CPU cache teardown after its users and callback
subscriptions end. **3.** Cache only independent neutral defaults or impose explicit
owner order; unordered destruction is not an adequate native resource dependency
graph. Check destructor reentrancy, cross-entry references and partial construction.
Outgoing: typed deleters, map/member destruction and caller factory lifecycle.

## UI-PARAM-002: BaseBlock constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L837).
**1.** mValidated=false, mParamProvided=false; body empty. **2.** CPU validation and
provenance flags. **3.** Share the audited value-state model if typed member layout
remains valid; no GPU allocation belongs here. Check fresh block flags. Outgoing:
member/default derived construction and virtual BaseBlock destruction. Local body
has no font lookup; derived Params constructors do.

## UI-PARAM-003: BaseBlock::validateBlock

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L204).
**1.** If not cached validated: get mostDerivedBlockDescriptor, iterate validation
list in its order, resolve const parameter from handle and call validation function.
First false optionally warns with getParamName then returns false without caching
failure. All succeed -> set mutable validated=true. Return flag. Already true skips
callbacks. **2.** CPU schema validation with explicit invalidation. **3.** Reuse
audited validators on neutral values rather than assume const validation is pure;
native constructor failure policy remains caller-specific.
Check repeated success skips callbacks, repeated failure retries, emit_errors=false,
mutation invalidation and callback side effects. Outgoing: virtual descriptor target,
handle lookup, each validation function and name/diagnostic helpers.

## UI-PARAM-004: BaseBlock::mergeBlock

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L451).
**1.** Iterate supplied descriptor's mAllParams. Resolve source at each handle.
If merge callback exists, resolve destination, assert enclosing offset equals handle,
OR-assign callback(dst,src,overwrite) into some_param_changed. All callbacks run,
including after true. No type/size compatibility check or rollback here; comment
requires same derived type. Return aggregate changed bool, not validation success.
**2.** CPU parameter merging; callback can copy resource-owning values. **3.** Preserve
registered typed merge semantics on neutral values. Replacing with dictionary overlay
would lose per-field merge behavior and provided flags; callback inventory must close
before reuse. Check callback ordering, null callback, unchanged values, failures and
typed block compatibility. Outgoing: handles, enclosing offsets, each merge callback,
descriptor initialization and copy/assignment owners.

## UI-PARAM-005: BaseBlock::fillFrom

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L901).
**1.** Returns false, no merge; source unused. Method is not virtual. **2.** CPU base
recursion termination. **3.** Preserve type-resolved dispatch in a neutral parameter
system; invoking this through BaseBlock is not a generic merge operation. Check
derived versus explicitly base-qualified calls. Outgoing: call sites/static type.

## UI-PARAM-006: Block<DERIVED,BASE>::fillFrom

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L2012).
**1.** Cast this to DERIVED*, invoke mergeBlock with this Block's static descriptor,
source and overwrite=false; return result. **2.** CPU typed default filling. **3.**
Share only with closed derived merge overrides and descriptors, not bare BaseBlock
interface. Check caller's static type and any overridden mergeBlock target.
Outgoing: derived merge target/getBlockDescriptor and descriptor lifetime.

## UI-PARAM-007: Block<DERIVED,BASE> constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L2023).
**1.** BASE constructs first, then BaseBlock::init(own descriptor, BASE descriptor,
sizeof(DERIVED)). **2.** CPU schema metadata construction and inheritance. **3.**
Use stable layout/descriptor lifetime; backend substitution cannot insert arbitrary
members/base classes without checking handle arithmetic. Check first/subsequent
instances, inherited sizes and constructor recursion. Outgoing: BASE constructor,
descriptor getters and BaseBlock::init state machine.

## UI-PARAM-008: BaseBlock::getHandleFromParam

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L182).
**1.** Reinterpret parameter and block addresses as const U8*, return difference.
**2.** CPU parameter-field identity. **3.** If existing descriptor scheme is retained,
prove same-object layout/offset validity; explicit typed accessors are an alternative
if native parameter representation changes. No GL handle relevance.
Check inherited block offsets and out-of-object input preconditions. Outgoing:
param_handle_t width/signedness, caller layout and C++ object-model constraints.

## UI-PARAM-009: BaseBlock::getParamFromHandle mutable

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L859).
**1.** Handle0 returns null. Other handle -> reinterpret U8*(this)+handle as Param*.
No bounds or dynamic-type validation. **2.** CPU descriptor lookup. **3.** Retain only
under checked descriptor/type contract, otherwise typed accessors; invalid address
behavior is not a compatibility mandate. Check0, valid inherited offset and mismatch.
Outgoing: offset producers/descriptor ownership and concrete Param layout.

## UI-PARAM-010: BaseBlock::getParamFromHandle const

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L867).
**1.** Reinterpret const U8*(this)+handle as const Param*, WITHOUT mutable overload's
handle0 special case. **2.** CPU descriptor access. **3.** Audit caller valid-handle
preconditions; do not infer equivalent null behavior from overload name. Check0 and
normal descriptors, validation callback inputs. Outgoing: same offset/layout contract.

## UI-PARAM-011: BaseBlock::paramChanged

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L876).
**1.** user_provided=true clears validated flag and sets paramProvided=true; false
leaves both unchanged. changed_param unused in this body. Virtual overrides may do
more. **2.** CPU provenance/validation invalidation. **3.** Explicit mutation semantics
must distinguish default changes from provided data, not invalidate or mark provided
indiscriminately without review. Check true/false transitions and override calls.
Outgoing: concrete virtual targets, callers and dependent validation caching.

## UI-PARAM-012: ChoiceBlock::fillFrom

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1853).
**1.** Derived-cast mergeBlock with own descriptor/source/overwrite=false. No active
choice comparison in this wrapper. **2.** CPU mutually exclusive parameter defaults.
**3.** Preserve distinction from mergeBlockParam eligibility logic below; generic
map filling is not equivalent. Check provided destination choice against direct fill.
Outgoing: derived mergeBlock and choice descriptor.

## UI-PARAM-013: ChoiceBlock::mergeBlockParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1858).
**1.** source_override = source_provided && (overwrite || !dest_provided). If override
OR source choice equals current choice, mergeBlock supplied descriptor/source/policy;
else false. **2.** CPU nested-choice merge eligibility. **3.** Retain source/destination
provenance as explicit metadata, not just values. Check all provided/overwrite flags
with equal/different choice. Outgoing: actual mergeBlock target/choice validity.

## UI-PARAM-014: ChoiceBlock::mergeBlock

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1870).
**1.** Set current choice to source choice, then base_block_t::mergeBlock using OWN
getBlockDescriptor(), not supplied block_data; propagate result. Active choice
assignment precedes per-field merges even when overwrite=false. **2.** CPU choice
state plus field data merge. **3.** Audit callback interactions before modeling this
as a simple tagged union overwrite. Check choice changes when no values merge and
supplied descriptor differs. Outgoing: base merge target/own descriptor and callbacks.

## UI-PARAM-015: ChoiceBlock::paramChanged

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1877).
**1.** Compute changed parameter handle. If differs from current: resolve old choice
with mutable lookup; if nonnull call old.setProvided(false), then set current handle
to changed one. Always call base paramChanged(changed,user_provided). Switching is
not guarded by user_provided; clearing old provided state itself notifies enclosing
block. **2.** CPU exclusive-choice/provenance propagation. **3.** Preserve callback
ordering with a coherent CPU mutation transaction; GPU recording never mutates these
states. Check first choice0, same/different choice, false-provided changes and nested
notifications. Outgoing: handle lookup, Param::setProvided, base override chain.

## UI-PARAM-016: BaseBlock::init

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L158).
**1.** Always replace descriptor.currentBlockPtr with this and maxParamOffset with
block_size. UNINITIALIZED: aggregate base metadata then set INITIALIZING.
INITIALIZING: set INITIALIZED. INITIALIZED: no further action. Thus transition to
INITIALIZED occurs on a subsequent init call, not at end of first construction.
No locking, recursion guard or rollback after failed first construction.
**2.** CPU schema initialization with layout constraints. **3.** Retaining this scheme
requires proven construction order and single-thread assumptions; an immutable
explicit schema is an alternative, but changing it requires every consumer's offset
contract. Check first/second construction, base propagation, failure mid-constructor
and simultaneous construction. Outgoing: aggregateBlockData, descriptor owners and
member TypedParam registration conditions.

## UI-PARAM-017: BlockDescriptor constructor

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L151).
**1.** max offset0, state UNINITIALIZED, current block pointer null; containers
default-construct. **2.** CPU descriptor state. **3.** Define lifetime independent of
device; once-built schema preferable to mutable current-instance pointers if
multi-thread native preparation is selected. Check initial flags/empty collections.
Outgoing: container members and lifetime/static initialization sites.

## UI-PARAM-018: BlockDescriptor::aggregateBlockData

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L108).
**1.** Insert base named map entries without overwrite; append base unnamed params,
validation list and all params in their existing order. Uses shared descriptor
pointers, no descriptor deep copy. **2.** CPU schema inheritance. **3.** Preserve
lookup precedence separately from all-parameter/validation traversal; a single map
cannot represent both. Check preexisting name collision and repeated aggregation.
Outgoing: descriptor smart-pointer lifetime, container allocation and init caller.

## UI-PARAM-019: BlockDescriptor::addParam

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L116).
**1.** Append incoming descriptor pointer to allParams before checks; copy back into
local smart pointer. Construct name string from char*. Handle cast to size_t > max
offset logs fatal. Empty name appends unnamed list, otherwise namedParams[name]
overwrites existing name. Nonnull validation callback appends(handle,callback) to
validation list. No rollback or duplicate removal from allParams/validation list.
**2.** CPU schema registration. **3.** Preserve name overriding and positional
validation as distinct semantics; enforce valid typed offsets before publication in
any new representation. Check duplicate name, unnamed, max boundary and null name
precondition. Outgoing: ParamDescriptor ownership, diagnostics and all consumers.

## UI-PARAM-020: Param constructor

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L46).
**1.** provided=false. Difference this minus enclosing block cast to U32 and masked
0x7fffffff; low field stores low16 bits, high field stores bits16..22 (mask0x007f0000).
Reconstructed offset therefore has23 bits, despite nearby '24 bits' comment. No
overflow/range/enclosing-object checks here. **2.** CPU embedded-field identity.
**3.** Keep parameter layout valid if reused; changing inheritance/member layout
requires coordination, not assuming pointers relocate automatically. Check offset
roundtrip for representative inherited blocks and upper bound. Outgoing: offset
type/bitfields, object-model assumptions and enclosingBlock.

## UI-PARAM-021: Param::getEnclosingBlockOffset

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L964).
**1.** Return (U32(high)<<16)|U32(low). **2.** CPU field lookup only. **3.** Retain as
audited arithmetic under offset limits or use typed accessors with coordinated schema
change. Check packed boundary values. Outgoing: constructor/assignment layout.

## UI-PARAM-022: Param::enclosingBlock

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L955).
**1.** Get byte address of this, subtract stored offset cast ptrdiff_t, reinterpret
as const BaseBlock*, const_cast and dereference. A const Param can thus mutate its
enclosing block. **2.** CPU provenance notification routing. **3.** Audit lifetime and
const mutation; never assume const access makes shared parameter caches concurrent.
Check copied block offsets, nested wrappers and invalid owner precondition.
Outgoing: offset decoding and concrete enclosing block identity/lifetime.

## UI-PARAM-023: Param::setProvided

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L936).
**1.** Assign provided bit, then enclosingBlock().paramChanged(*this,is_provided).
Notification occurs even if bit unchanged; false also calls virtual target.
**2.** CPU mutation propagation. **3.** Preserve exact notification order and
choice-block behavior, not just boolean storage. Check repeated true, false, nested
choices and callback invalidation. Outgoing: enclosingBlock and all overrides.

## UI-PARAM-024: Param assignment

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L942).
**1.** Copy provided bit only; preserve destination enclosing offset. Return this
reference; no paramChanged call. **2.** CPU copied value provenance anchored in new
owner. **3.** Preserve destination ownership identity; raw structure replacement
can break copied parameter blocks. Check different source/destination offsets and
validation caching after callers' assignments. Outgoing: wrapper assignments.

## UI-PARAM-025: scalar TypedParam constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1010).
**1.** Param(currentBlockPtr), named_value_t(value), then only descriptor state
INITIALIZING invokes init with validation/count/name. **2.** CPU typed default and
schema registration; named_value_t can transitively own font/image values. **3.**
Use neutral parameter value types before sharing constructor machinery, retaining
default/provided distinction. Check first/subsequent instance and GL font-pointer
default path. Outgoing: Param, named_value_t constructor, init and type specialization.

## UI-PARAM-026: scalar TypedParam::init

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1151).
**1.** Allocate shared ParamDescriptor with handle from currentBlockPtr, mergeWith,
deserializeParam, serializeParam, supplied validation, inspectParam and min/max;
block_descriptor.addParam(pointer,name). **2.** CPU typed schema callbacks.
**3.** Audit each instantiated value type; callback registration is not purity.
Check callback pointers match specialization, offset correctness and allocation
failure. Outgoing: descriptor ctor/refcount, handle computation, addParam, callbacks.

## UI-PARAM-027: scalar TypedParam::mergeWith

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1137).
**1.** Cast src/dst to same self_t. If source provided AND (overwrite OR destination
not provided): dst.set(src.getValue()), true; else false. This can report true even
when values equal; set clears named-value label rather than copying source label.
**2.** CPU default/value provenance merging. **3.** Retain semantic value and explicit
provided state; do not infer changed bool means bitwise inequality. Check full
provided/overwrite truth table, equal values and named alias loss. Outgoing: set,
getValue, isProvided and typed value copy semantics.

## UI-PARAM-028: scalar TypedParam::set

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1115).
**1.** clearValueName, setValue(val), setProvided(flag), default flag=true. No equality
test or rollback if value assignment fails. **2.** CPU semantic value mutation and
notification. **3.** Neutral resource identities separate from native image/font
publication; assign explicit value before notifying consumers. Check alias clearing,
flag=false, assignment failure and notification order. Outgoing: named_value_t
methods, Param::setProvided and concrete value owners.

## UI-PARAM-029: scalar TypedParam::deserializeParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1024).
**1.** Only empty remaining name range attempts value parse. If named values exist,
read string and resolve into typed value; success stores name, sets provided, true.
Otherwise try parser.readValue directly into stored value; success clears name,
sets provided, true. Else false. new_name argument unused. A failed named resolution
can be followed by direct parsing; parser cursor/side effects need separate audit.
**2.** CPU declaration decoding with typed fallback. **3.** Preserve alias/direct
resolution precedence with audited parser transactions, not ad hoc XML conversion.
Check known/unknown alias, direct numeric/text, residual name path and partially
modified value on failure. Outgoing: lookup methods, parser.readValue specialization,
value mutation/setProvided, name-range ownership.

## UI-PARAM-030: scalar TypedParam::serializeParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1052).
**1.** Build predicate: HAS_DEFAULT_VALUE if diff exists and values compare equal;
VALID from isValid; PROVIDED from anyProvided; EMPTY=false. Reject if rule false.
Nonempty name stack marks last.second=true. Nonempty stored key writes key only if
diff absent or diff key unequal. Empty key branch writes value only if diff absent
OR values compare EQUAL (not unequal). Failed direct write computes alias, writes
nonempty computed alias only if diff absent or diff stored alias differs. Return
last serialization result, initialized false. No mutation rollback for name stack.
**2.** CPU export; no Vulkan command required. **3.** Audit actual export callers and
predicate rules before sharing; do not silently 'fix' equal-value branch during
native migration or assume it represents intended diff semantics.
Check equal/different diff values, alias versus unnamed, failed direct writer and
name-stack mutation. Outgoing: ParamCompare<T/string>, predicate methods, value-name
lookup, parser.writeValue and concrete isValid/getValue. Runtime reachability open.

## UI-PARAM-031: scalar TypedParam::inspectParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1104).
**1.** parser.inspectValue<T>(stack,min,max,null), then if getPossibleValues nonnull
inspectValue<string> with that possible-values set. parameter argument unused.
**2.** CPU schema inspection/export. **3.** Preserve both concrete type and aliases,
not expose GPU-owner types as native schema. Check null/nonempty values and order.
Outgoing: parser inspector callbacks and named-value registry.

## UI-PARAM-032: ChoiceBlock constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1898).
**1.** BASE constructs; currentChoice0, then BaseBlock::init own/base descriptors
and sizeof(DERIVED). **2.** CPU exclusive parameter default selection. **3.** Check
actual first-choice initialization before assuming every constructed block always
has a selected alternative. Check first and subsequent instances and copied blocks.
Outgoing: base constructor, init and alternative construction below.

## UI-PARAM-033: ChoiceBlock::Alternative constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1919).
**1.** TypedParam using DERIVED descriptor, name, default value, null validator and
0..1 count; preserve original value separately. Fetch current block pointer. ONLY
if descriptor INITIALIZING and currentChoice0 set choice to this parameter handle.
Subsequent instances with descriptor INITIALIZED do not select first alternative
in this body, despite nearby comment stating one always chosen.
**2.** CPU choice default/original value semantics. **3.** Resolve actual construction
and copy paths before adopting corrected deterministic choice behavior; observed
comment/body mismatch is investigation, not approved parity requirement.
Check first/second fresh block versus copied defaults; original-value ownership.
Outgoing: TypedParam and value copy, descriptor state, handle calculation.

## UI-PARAM-034: ChoiceBlock::Alternative::choose

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1936).
**1.** Cast enclosing block to ChoiceBlock and call paramChanged(*this,true) directly;
does NOT first set this alternative's provided bit. **2.** CPU choice activation.
**3.** Preserve distinction between chosen and provided; set(val) is a different
operation. Check choose with false provided, validation flag and previous choice.
Outgoing: enclosingBlock, ChoiceBlock::paramChanged and base notification.

## UI-PARAM-035: ChoiceBlock::Alternative::operator() const

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1961).
**1.** If enclosing block current-choice pointer equals this return stored value;
otherwise return mOriginalValue, even if stored value differs. Returns const reference.
**2.** CPU active/default choice query. **3.** Expose selected state separately from
original default in neutral schema; generic value reads can otherwise expose stale
inactive values. Check inactive after mutation, default/no choice, reference lifetime.
Outgoing: enclosingBlock/getCurrentChoice and named-value storage.

## UI-PARAM-036: ChoiceBlock::Alternative::isChosen

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1970).
**1.** Compare this pointer to enclosing ChoiceBlock current-choice pointer; not
provided flag. **2.** CPU branch condition for control state resolution. **3.** Preserve
choice versus provenance in native controls. Check none chosen, choose() and direct
set(). Outgoing: enclosingBlock and getCurrentChoice.

## UI-PARAM-037: ChoiceBlock::getCurrentChoice

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1988).
**1.** const method calls base const getParamFromHandle(currentChoice); zero thus
reinterprets block address instead of yielding mutable-overload null. **2.** CPU
selection lookup. **3.** Define valid selected identity without reproducing invalid
pointer assumptions; confirm all caller behavior for initial zero.
Check0 and valid selected offset. Outgoing: const handle lookup and choice lifecycle.

## UI-REGISTRY-001: LLRegistry::Registrar::add

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L51).
**1.** map.insert(key,value); duplicate warns and false, otherwise true. Does not
replace old value. **2.** CPU constructor/callback registry. **3.** Preserve duplicate
policy per caller; this differs from replace and panel-class registration. Check
duplicate identical/different values and null function. Outgoing: value copy/owners,
key ordering, diagnostics and caller return handling.

## UI-REGISTRY-002: LLRegistry::Registrar::getValue mutable

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L83).
**1.** find key; hit returns pointer to stored value, miss null. A stored null pointer
value still yields nonnull pointer-to-value. **2.** CPU scoped target lookup. **3.**
Retain absent versus present-null distinction and stable registration lifetime.
Check null callback entry and mutation after lookup. Outgoing: map/value lifetime.

## UI-REGISTRY-003: LLRegistry::Registrar::getValue const

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L93).
**1.** Same find/miss behavior, const value pointer. **2.** CPU lookup. **3.** Const
reference does not freeze registry against other mutable users; native preparation
needs stable callback ownership. Check const pointer lifetime and absent/null values.
Outgoing: map comparator and registry mutation/lifetime.

## UI-REGISTRY-004: LLRegistry::getValue mutable

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L134).
**1.** Iterate active scopes in list order, return first nonnull value pointer; if
none, default registrar lookup. Does not skip a stored null callback. **2.** CPU
scoped callback binding. **3.** Resolve action targets while preparation scope is
valid and retain subscription lifetime, never late-lookup from GPU recording.
Check newer/older/default precedence and present-null masking. Outgoing: each scope's
lookup, active-list lifetime, scoped push/pop and callback invocation callers.

## UI-REGISTRY-005: LLRegistry::getValue const

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L144).
**1.** Same active-then-default traversal returning const pointer. **2.** CPU target
resolution. **3.** Same lifetime model as mutable lookup; constness does not snapshot
scope state. Check precedence with const caller. Outgoing: scopes/registrar lookup.

## UI-REGISTRY-006: LLRegistry::exists

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L154).
**1.** Return true at first active scope containing key; else default.exists(key).
Stored value content is irrelevant. **2.** CPU registration validation. **3.** Distinguish
entry presence from callable readiness in native error handling. Check present-null
and default shadowed entry. Outgoing: Registrar::exists and active scope ownership.

## UI-REGISTRY-007: LLRegistry::addScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L207).
**1.** Insert scope pointer at list beginning. No null/duplicate check. **2.** CPU
nested callback environment. **3.** Explicit preparation scope with deterministic
push/pop; GPU resources are not members of callback scope. Check double push and
newest-first lookup. Outgoing: list allocation, scope lifetime and callers.

## UI-REGISTRY-008: LLRegistry::removeScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L213).
**1.** Find first equal pointer; erase it if present, otherwise no-op. Duplicate
pushes need repeated pops. **2.** CPU callback-scope exit. **3.** Prefer balanced
scope lifetime with cancellation before referenced actions disappear.
Check absent scope, repeated pushes and destruction mid-lookup. Outgoing: list owners.

## UI-REGISTRY-009: ScopedRegistrar constructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L248).
**1.** Registrar base constructs; default push_scope=true invokes pushScope, false
does not. mListIt has no role in active push/pop implementation. **2.** CPU scope
registration. **3.** Native action scopes need explicit activation boundaries,
including constructors before application/window initialization. Check false/true
and singleton initialization recursion. Outgoing: Registrar ctor, pushScope.

## UI-REGISTRY-010: ScopedRegistrar destructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L256).
**1.** If singleton exists call popScope; otherwise no pop. Base/value map destruction
follows. Does not track whether this object ever pushed or pushed multiple times.
**2.** CPU subscription/callback teardown. **3.** Disconnect users before callable
storage destruction; do not infer scoped destructor removes duplicate registrations.
Check false-constructed scope, multiple pushes and singleton already gone.
Outgoing: instanceExists, popScope, Registrar/map/callable destructors.

## UI-REGISTRY-011: ScopedRegistrar::pushScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L264).
**1.** singleton.instance().addScope(this); may instantiate singleton. **2.** CPU
scope activation. **3.** Restrict activation to service lifetime; no GPU work.
Check first activation and double push. Outgoing: singleton and addScope.

## UI-REGISTRY-012: ScopedRegistrar::popScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L269).
**1.** singleton.instance().removeScope(this), even if caller previously never
pushed. **2.** CPU scope exit. **3.** Avoid creating destroyed services during native
teardown; audit explicit calls separately from destructor guard. Check no instance
and missing scope. Outgoing: singleton construction and removeScope.

## UI-REGISTRY-013: StaticRegistrar constructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L292).
**1.** If singleton.instance().exists(key), log fatal duplicate. Then add key/value
to mStaticScope; add result ignored. Entry is not in registrar object's own map.
**2.** CPU static target registration. **3.** Native startup must audit singleton
initialization and callback constructors before backend selection; merely not calling
draw does not prove safe globals. Check duplicate in dynamic/default/static scope
and stored callable capture lifetime. Outgoing: singleton, exists, static scope add.

## UI-REGISTRY-014: LLRegistrySingleton constructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L322).
**1.** Registry and singleton bases construct, mStaticScope=null. **2.** CPU service
initial state. **3.** Share only after LLSingleton initialization/teardown closure;
static scope is not yet available in this constructor body. Check first recursive
registration. Outgoing: both base constructors and later initSingleton callback.

## UI-REGISTRY-015: LLRegistrySingleton::initSingleton

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L326).
**1.** mStaticScope=new ScopedRegistrar(), whose default constructor pushes scope
via singleton.instance() before assignment completes. **2.** CPU initialization
with reentrant singleton access. **3.** Need proven singleton construction protocol;
replacing with a generic function-local static without analysis may recurse/fail.
Check first static registration and allocation failure. Outgoing: ScopedRegistrar,
LLSingleton initialization state machine and virtual derived overrides.

## UI-REGISTRY-016: LLRegistrySingleton destructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L331).
**1.** delete static scope; base destruction follows, no local pointer reset.
**2.** CPU target teardown. **3.** Native owner graph must resolve active scopes and
callback references before registry destruction. Check late ScopedRegistrar dtor,
partial init and callable destruction. Outgoing: scoped/base/map destructors.

## UI-PANEL-001: LLPanel::fromXML

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L369).
**1.** Start name='panel', read name attribute; read class attribute. Nonempty class:
createPanelClass(class), warn if null. If no panel, createFactoryPanel(name), assert
nonnull, return null if still null. If panel factory map nonempty push its address
onto BACK of sFactoryStack. Push panel commit then enable callback scopes. Call
initPanelXML using default LLPanel Params and IGNORE bool. Pop commit then enable
scope. If map currently nonempty pop factory stack back (condition recomputed).
Return panel even when initPanelXML false. No local exception scope restoration;
factory-map mutation during callbacks can change push/pop condition.
**2.** CPU typed panel construction with scoped actions and declaration loading.
**3.** Prefer explicit construction context/ownership over invoking GL-owning panels;
preserve class-before-factory precedence and defined partial-failure policy. Scoped
rollback is a candidate failure correction requiring reviewed behavior, not assumed
equivalence. Check injected class, missing class fallback, null factory return,
failed initPanelXML, callback exceptions and map empty/nonempty mutation.
Outgoing: XML getters, class registry, createFactoryPanel, getFactoryMap, deque/scopes,
defaults, initPanelXML and concrete injected ctor/init/postBuild/dtor targets.

## UI-PANEL-002: LLPanel::createFactoryPanel

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L852).
**1.** Iterate sFactoryStack BEGIN to END (not newest/back first). First map with
name invokes mCallback(mData), C-casts result to LLPanel*, returns immediately even
if null. No callback found -> construct LLPanel::Params, create<LLPanel>(params).
**2.** CPU context-specific panel factory. **3.** Preserve factory precedence separately
from callback registry's newest-first scopes. Alternatives: explicit ordered factory
context or audited current deque; native preparation benefits from immutable context
because callbacks can construct nested panels. Check overlapping names, null factory
return, reentrant construction and fallback postBuild before later XML initialization.
Outgoing: LLCallbackMap types/target lifetime, each factory function, Params and create.

## UI-PANEL-003: LLPanel::initPanelXML

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L491).
**1.** Copy default params. If mXMLFilename empty, read node filename/setXMLFilename.
Cache factory/node name/child registry; construct parser. Nonempty filename plus
output node: parse current node, export copied params via setupParamsForExport,
set output tag/write against defaults, return true WITHOUT loading reference,
initializing panel, constructing children, parenting or postBuild.
Nonempty filename without output: push filename; failed layered XML warns/returns
false WITHOUT pop. Success: parse reference into params with pushed provenance,
setShape(reference params rect), create referenced children, pop filename. Then
parse current node into params. Optional output writes copied export params.
Set from_xui=true; applyXUILayout(params,parent); initFromParams. Create current-node
children. Parent nonnull: use provided tab_group else parent's last group, addChild
AFTER children because parent may reshape. Call postBuild ignoring bool. Return true.
No local validation-block call, child success aggregate or rollback in this body.
**2.** CPU layered panel preparation, topology and parent-dependent layout/actions.
**3.** Preserve reference-then-inline child order and pre-parent initialization in a
native construction transaction. One generic defaultBuilder is not equivalent;
export-only early return and failure behavior need deliberate tests.
Check referenced/inline value precedence and duplicate children, missing reference,
export shortcut, tab-container reflow, false postBuild and scope/file stack state.
Outgoing: filename accessors, XML parser/merge, shape/layout, default/typed Params,
child registry, createChildren, virtual initFromParams/addChild/postBuild and exports.

## UI-PANEL-004: LLPanel::initFromParams

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L425).
**1.** set visible/enabled/focus-root/sound flags BEFORE LLUICtrl::initFromParams.
Provided visible_callback -> initCommitCallback/setVisibleCallback. Iterate localized
strings assigning UIStrings[name]=value. Set label/help topic/shape, parse follows,
tooltip/fromXUI, hover cursor. has_border -> addBorder. use_bounding_rect only if
provided. Set default tab group, mouse opaque, background visible/opaque/colors.
Assign GL background images from parameters. Assign checkpoint native image-name
strings from vk_image_name only if provided else empty; these existing fields do NOT
prove GL-free image lookup. Assign image overlays, accepts-badge. No else removes
existing border, no strings clear or old callback removal when omitted in this body.
**2.** CPU panel appearance/layout/action state plus versioned image identities.
**3.** Native prepared panel state must separate image resolution from GL owners and
preserve reinitialization semantics. Existing native-name fields are migration debt,
not closure. Check repeated initialization, omitted/provided background/border,
visible callback ordering, reference-child shape changes and fallback images.
Outgoing: every setter/virtual callback, LLUICtrl initialization, image ParamValue,
LLUIColor, addBorder and badge ownership. All remain open.

## UI-PANEL-005: LLRegisterPanelClass::addPanelClass

Source: [llpanel.h](../../../indra/llui/llpanel.h#L288).
**1.** map[tag]=std::function, replacing prior callable without diagnostic. **2.** CPU
panel class registration. **3.** Preserve actual overwrite policy, unlike generic
StaticRegistrar duplicate fatal. Audit capture destruction on replacement and static
order. Check duplicate tag/custom function. Outgoing: map/function ownership/callers.

## UI-PANEL-006: LLRegisterPanelClass::createPanelClass

Source: [llpanel.h](../../../indra/llui/llpanel.h#L293).
**1.** Find string_view tag; absent returns0, present invokes stored function with no
args, no empty-function check. **2.** CPU dynamic class construction. **3.** Selected
native lifecycle needs audited native/neutral constructor targets under these names,
not cast a GL control result. Check absent, empty function, null return and exception.
Outgoing: transparent hash/equality, each registered target/capture/lifetime.

## UI-PANEL-007: defaultPanelClassBuilder<T>

Source: [llpanel.h](../../../indra/llui/llpanel.h#L301).
**1.** new T() and return pointer; no Params argument, initFromParams or postBuild.
T's constructor can perform all of those itself. **2.** CPU panel class construction.
**3.** Audit each actual T default constructor independently from factory<T>(Params).
Check constructor-created children/callbacks before XML initialization. Outgoing:
each T constructor/base/member chain and allocation failure.

## UI-PANEL-008: LLPanelInjector<T> default-builder constructor

Source: [llpanel.h](../../../indra/llui/llpanel.h#L328).
**1.** singleton.addPanelClass(tag,&defaultPanelClassBuilder<T>). **2.** CPU static
class injection. **3.** Share tags, not unexamined constructors, across selected
lifecycle. Check static initialization and duplicate injection ordering.
Outgoing: singleton, addPanelClass and typed builder; injector inventory open.

## UI-PANEL-009: LLPanelInjector<T> custom-builder constructor

Source: [llpanel.h](../../../indra/llui/llpanel.h#L335).
**1.** singleton.addPanelClass(tag,func). T does not select a constructor in this
overload body. **2.** CPU explicit factory registration. **3.** Resolve supplied
callable target/captures; template type alone is not coverage. Check arbitrary
custom builder, null/empty callable and duplicate tags. Outgoing: singleton,
addPanelClass, function copy/destruction and actual callback target.

## UI-PANEL-010: LLPanel::Params constructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L70).
**1.** Base block/LLUICtrl Params constructs first. has_border=false, border unnamed;
background_visible=false, background_opaque=false; named color/image/overlay params;
min_width/min_height100; multiple string, filename, class, help_topic,
visible_callback, accepts_badge parameters. Add bg_visible synonym for background
visible, border_visible for has_border, title for inherited label. Omitted explicit
defaults use their typed default constructors, not inferred zero/null values.
**2.** CPU declaration defaults with neutral color/image/font identities. **3.**
Preserve schema and synonyms while removing GL-owning value resolution; independent
hardcoded native panel style would not preserve per-skin defaults.
Check absent/provided values, synonyms and member-construction effects.
Outgoing: base and every wrapper/value constructor, addSynonym and template loading.

## UI-PANEL-011: LLPanel constructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L96).
**1.** Construct LLUICtrl(p), LLBadgeHolder(accepts_badge). Initialize background
visible/opaque, colors/overlays, GL image pointers and checkpoint native image names
(provided vk_image_name else empty), default button/border null, label/help,
commit/enable registrars with push=false, filename, visible signal null. Body calls
addBorder if has_border. Header default argument is getDefaultParams(), so default
new T() constructors inheriting it can resolve fonts/images before body runs.
**2.** CPU panel state and child/action ownership, native image identities separated.
**3.** Audit default-argument and base construction before reuse; no late draw branch
can remove earlier GL image/font ownership. Check direct/default/explicit Params,
border child creation and registrar activation absence.
Outgoing: base classes, default params, image/color owners, registrars, addBorder.

## UI-PANEL-012: LLPanel destructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L126).
**1.** Delete mVisibleSignal; member and base destructors follow. Body does not
explicitly remove border or delete children; those are outgoing base responsibilities.
**2.** CPU callbacks and tree teardown with native image versions separately retired.
**3.** Disconnect async users before panel destruction; signal deletion alone does
not establish that callbacks cannot recreate UI. Check signal connections, border,
default-button raw pointer, factory scopes and partial construction.
Outgoing: signal, member image/registrar/string owners, LLBadgeHolder/LLUICtrl/LLView.

## UI-PANEL-013: LLPanel::addBorder(Params)

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L137).
**1.** Parameter copied by value; removeBorder, overwrite p.rect with local rect,
create<LLViewBorder>(p), store pointer, addChild(pointer) with default group. No
success check before addChild. **2.** CPU border child creation and paint ordering.
**3.** Retain border as native prepared child or explicit decoration only after child
focus/layout/ordering contract closes; do not replace it with a rectangle solely
because of its name. Check replace, creation failure, parent reshape and z-order.
Outgoing: Params copy, local rect, removeBorder, factory/LLViewBorder/addChild.

## UI-PANEL-014: LLPanel::addBorder()

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L146).
**1.** Construct LLViewBorder::Params, set border_thickness to LLPANEL_BORDER_WIDTH,
call Params overload. **2.** CPU border defaults. **3.** Preserve declared units and
skin override precedence, not hardcode a native pixel thickness prematurely.
Check constant and DPI/layout application. Outgoing: Params/defaults, assignment,
LLPANEL_BORDER_WIDTH definition and addBorder(Params).

## UI-PANEL-015: LLPanel::removeBorder

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L154).
**1.** Nonnull border: removeChild, delete border, set pointer null. Null no-op.
**2.** CPU tree invalidation/deletion; old prepared frames may still reference native
border geometry/images. **3.** Separate CPU child removal from completed-use native
retirement, with deterministic callback cancellation. Check removal side effects,
destructor reentry and prepared-frame lifetime. Outgoing: removeChild, border dtor.

## UI-PANEL-016: LLPanel::draw

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L203).
**1.** Always getDrawContext().alpha initially. If background visible, replace local
alpha with virtual getCurrentTransparency, obtain local rect. Opaque branch: nonnull
opaque image draws rect with opaque overlay % alpha; otherwise gl_rect_2d with opaque
color.get() % alpha. Nonopaque branch uses alpha image/overlay or alpha color rectangle.
Then nonvirtual updateDefaultBtn (empty at checkpoint), then qualified LLView::draw.
Local alpha is not pushed into a new child draw context by this body. Hidden background
does not call getCurrentTransparency; children still draw.
**2.** Native preparation chooses background contribution/resource/color, then child
painter order, retaining exact alpha source and any callback effects.
**3.** Prepared panel primitives plus prepared children, never invoke this GL draw
callback. Native image/solid branches must honor independent opacity/visibility;
an unconditional opacity multiplier on descendants would change the contract.
Check all four image/color branches, hidden background, transparency callback
invocation count, inherited child alpha and clipping. Outgoing: draw context, virtual
transparency, local rect, LLUIImage::draw, color get/operator%, gl_rect_2d, LLView::draw.

## UI-PANEL-017: LLPanel::updateDefaultBtn

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L245),
[nonvirtual declaration](../../../indra/llui/llpanel.h#L162).
**1.** Empty body. Search finds panel draw and qualified floater draw callers, no
alternate implementation. **2.** No native preparation work follows from this body.
**3.** Do not infer default-button mutation from method name; actual button state
mutation is traced at its real setters/input callbacks. Check call sites remain
nonvirtual and source unchanged. Outgoing: none in body; callers separately audited.

## UI-PANEL-018: LLPanel::getCtrlList

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L187).
**1.** Iterate immediate child list order, invoke view->isCtrl; true -> static_cast
LLUICtrl*, append to local vector. Does not recurse or filter visible/enabled.
**2.** CPU control traversal. **3.** Preserve direct-child semantics and lifetime
during subsequent callback loops. Check mixed children, hidden/disabled and virtual
isCtrl correctness. Outgoing: child list, isCtrl overrides, pointer lifetime.

## UI-PANEL-019: LLPanel::clearCtrls

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L166).
**1.** Copy getCtrlList result, then for each pointer setFocus(false), setEnabled(false),
clear(), in that order. List copy does not retain child lifetime through callbacks.
**2.** CPU control/focus/model mutation. **3.** Preserve ordering while defining
callback-safe native ownership; clear is not a GPU buffer clear. Check reentrant child
deletion, focus effects and each concrete clear target. Outgoing: getCtrlList and
virtual focus/enabled/clear targets plus panel overrides.

## UI-PANEL-020: LLPanel::setCtrlsEnabled

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L178).
**1.** Snapshot direct control pointer list, invoke each setEnabled(b), no recursion
or visible filter. **2.** CPU interactive state update. **3.** Same callback/lifetime
requirements as clearCtrls; derive visuals in preparation after state changes.
Check nested panels and callback mutation. Outgoing: getCtrlList and enabled targets.

## UI-PANEL-021: LLPanel::handleKeyHere

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L273).
**1.** Read current keyboard focus, dynamic-cast LLUICtrl. Escape regardless mask:
setFocus(false), return true. Shift-only Tab: if focused control, findRootMostFocusRoot,
if nonnull focusPrevItem(false). No-mask Tab similarly focusNextItem(false).
If not handled, focused control, Return and no mask: focused LLButton with
getCommitOnReturn true -> leave false for button handling; else visible+enabled
default button -> onCommit, true; else acceptsTextInput -> focus onCommit, true.
Otherwise return handled. No default-button action if focus is absent/non-LLUICtrl.
**2.** CPU input/focus/action dispatch, independent of GPU. **3.** Native interactive
controls need the same dispatch and action policy after auditing transitive service
callbacks; GLFW/Win32 key forwarding alone is insufficient.
Check exact modifiers, no focus, default disabled/invisible, Return-capturing button,
text field commit, cancellation and callback deletion. Outgoing: focus manager, RTTI,
focus traversal/setFocus, button flags, visible/enabled, acceptsTextInput/onCommit.

## UI-PANEL-022: LLPanel::setFocus

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L344).
**1.** b=true and !hasFocus -> qualified LLUICtrl::setFocus(true) first, then
focusFirstItem(). Else qualified LLUICtrl::setFocus(b). No refresh() call in this body
despite nearby refresh comment; transitive base behavior still needs inspection.
**2.** CPU focus ownership/traversal. **3.** Preserve preemptive focus ordering to
avoid reentrant loops; native painting consumes resolved focus state.
Check first focus, already-focused descendant, false and no valid first child.
Outgoing: hasFocus, base setFocus, focusFirstItem and callback effects.

## UI-PANEL-023: LLPanel::onVisibilityChange

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L337).
**1.** Qualified LLUICtrl::onVisibilityChange(new), then if mVisibleSignal invoke
with this and LLSD(bool). No equality test or lifetime guard in body after base call.
**2.** CPU visibility notification/action dispatch. **3.** Preserve base-before-panel
signal order with callback-safe native control lifetime. Check deletion/registration
during base callbacks and repeated same values. Outgoing: base callback, LLSD ctor,
signal target list and connection ownership.

## UI-PANEL-024: LLPanel::setDefaultBtn(pointer)

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L255).
**1.** Assign raw pointer, no focus/visual mutation. **2.** CPU Return-key target.
**3.** Native action target needs lifetime validity independent of prepared geometry;
changing visual default style is not mandated by this setter. Check null/deleted child.
Outgoing: pointer owner/deletion and handleKeyHere consumer.

## UI-PANEL-025: LLPanel::setDefaultBtn(name)

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L260).
**1.** getChild<LLButton>(id); if nonnull set pointer else null. getChild may have
fallback construction behavior; name lookup is not proven passive. **2.** CPU named
action resolution. **3.** Audit lookup/fallback lifecycle before sharing; native
default-button identity must not create a hidden GL control. Check missing/wrong
type/recursive child and fallback ownership. Outgoing: getChild<T>, pointer overload.

## UI-PANEL-026: LLPanel::setBorderVisible

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L359).
**1.** Nonnull border -> border.setVisible(b), otherwise no-op. Does not change
background or create border. **2.** CPU decoration visibility. **3.** Preserve
independent border/background state; prepare after callback-safe visibility changes.
Check null border and repeated visibility. Outgoing: LLViewBorder visibility target.

## UI-PANEL-027: LLPanel::refresh

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L249).
**1.** Empty virtual default. Comment says automatically called from setFocus;
LLPanel::setFocus body does not do so locally. **2.** No work in base, concrete
overrides remain CPU model/preparation obligations. **3.** Resolve actual callers
and overrides rather than preserving a comment as control flow.
Check call graph and derived implementations. Outgoing: no callee in base body.

## UI-PANEL-028: LLPanel::LocalizedString constructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L59).
**1.** Base block constructs, mandatory name/value fields register names without
explicit defaults. **2.** CPU panel localization schema. **3.** Preserve mandatory
validation and localized value ownership, not GPU text pre-rasterization.
Check missing name/value and layered duplicate string names. Outgoing: Block,
Mandatory<string> constructors/validators and parser targets.

## UI-PANEL-029: LLPanel::getDefaultParams

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L65).
**1.** Return factory getDefaultParams<LLPanel>() by const reference. **2.** CPU
panel defaults resolution with resource-neutral types required for native use.
**3.** Reference existing factory/default records; default argument invocation timing
is part of lifecycle, not lazy draw setup. Check first direct panel construction.
Outgoing: UI-FACTORY-001 and LLPanel Params chain, all still transitive-open.

## UI-OPACITY-001: LLUICtrl::getCurrentTransparency

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L1063).
**1.** alpha starts0. TT_DEFAULT -> current draw context alpha; TT_ACTIVE -> global
active transparency; TT_INACTIVE -> global inactive; TT_FADING -> inactive/2;
TT_FORCE_OPAQUE ->1. No default switch branch, unknown enum stays0. If override
callback present return callback(type,alpha), otherwise alpha. No clamping or
automatic inherited-alpha multiplication for nondefault cases.
**2.** CPU opacity policy and action-like callback invocation during preparation.
**3.** Resolve opacity before immutable draw generation, preserving when/where
callbacks run; do not multiply every node by parent opacity unconditionally.
Check each enum, globals, out-of-range callback return, callback mutation/reentry and
call count across background/text/image branches. Outgoing: draw context, global
setting writers, override setter/callable targets and concrete virtual overrides.

## UI-OPACITY-002: LLUICtrl::setTransparencyType

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L1103).
**1.** Assign enum without validation, notification or dirty invalidation in body.
**2.** CPU opacity-mode state. **3.** Native prepared-state invalidation must account
for setter usage without relying on nonexistent local notification.
Check repeated/invalid enum and next-frame versus retained replay result.
Outgoing: callers, enum definition, retained invalidation and getCurrentTransparency.

## UI-BUTTON-001: LLButton::draw

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L957).
Revision/configuration and status are the document's pinned Windows baseline;
local body inspected, outgoing edges OPEN, no implementation/runtime/parity closure.
**1.** First invocation constructs function-static LLCachedControl<bool> from UI
config setting EnableButtonFlashing with fallback true. Alpha uses draw context
when mUseDrawContextAlpha, otherwise virtual getCurrentTransparency. Focused button
reads keyboard space and, when mCommitOnReturn, Return state. Mouse capture causes
local mouse position lookup and pointInView. Cache enabled-chain bool, pressed=
keyboard OR captured-and-over OR forced, selected=getToggleState(). Initialize glow
off, highlight/glow colors white, glow blend ADD_WITH_ALPHA, image pointers null.

Selection and mutation order:

1. If flashing timer exists AND ((selected AND not flashing-in-progress AND not
	force-flashing) OR pressed), set mFlashing=false. flash=mFlashing AND cached setting.
2. Pressed AND displayPressed -> selected?pressedSelected:pressed image. Otherwise
	needsHighlight -> selected/unselected hover image if present, else corresponding
	ordinary image AND enable glow. Otherwise ordinary selected/unselected image.
3. Disabled-selected image, when present, overrides if (enabled AND tentative) OR
	(!enabled AND selected). Else disabled image overrides when present, disabled
	and unselected. Set imageGlow=image.
4. If mFlashing: flash AND flash image -> imageGlow=flash image. If timer exists,
	use alternate/normal flash color, enable glow, blend ALPHA; timer highlighted OR
	not flashing-in-progress -> flash color; else needsHighlight -> white highlight
	color; else flash color. Setting EnableButtonFlashing=false gates flash-image
	substitution, NOT all glow behavior in this body. Highlight with null image also
	enables glow.
5. If toggle signal exists invoke it(this,empty LLSD) then setToggleState(result).
	This occurs AFTER image selection and local selected/enabled snapshots, BEFORE
	text color, geometry size queries and overlay color. Reentrant callback may alter
	button/model/layout; no lifetime guard or snapshot restart here.
6. Label color uses cached enabled bool and newly queried toggle: enabled selected/
	unselected or disabled selected/unselected colors. SearchableControl highlighted
	overrides with highlight font color. Focused AND drawFocusBorder -> drawBorder
	using earlier selected image, focus color % alpha and focus flash width.
7. Update mCurGlowStrength with lerp and LLSmoothInterpolation.getInterpolant(.05).
	Glow enabled target: flashing+timer ? (highlighted OR not in-progress OR hover ?
	1:0) : hoverGlowStrength. Glow disabled target0. This mutates animation state on
	each draw invocation, not necessarily once per simulation frame.

Visual emission follows:

- Nonnull image: disabledColor=disabledImageColor with alpha halved if fadeWhenDisabled,
  else unchanged. ScaleImage -> draw local rect with enabled imageColor or disabledColor,
  each %alpha. If glowStrength>.01, set glow blend, drawSolid imageGlow at(0,0,width,
  height) with glowColor %(strength*alpha), then BT_ALPHA. Non-scaled image y=local
  height-image natural height; draw at(0,y) natural size, same colors; optional glow
  uses imageGlow natural size at same y and restores BT_ALPHA. Missing image logs
  debug and draws unfilled pink rectangle; it emits no glow even if strength>0.
- textLeft=left padding, textRight=width-right padding, textWidth=width-both paddings.
  Overlay exists -> getOverlayImageSize, local centerX/Y. Pressed+displayPressed
  shifts centerY--,centerX++; centerY+=bottomPad-topPad. Overlay tint starts ordinary,
  disabled overrides, else current toggle selects selected tint; multiply alpha.
  Positive rightDelta -> draw at(width-overlayWidth-rightDelta,centerY-overlayHeight/2)
  without reducing text width. Otherwise LEFT shifts textLeft and reduces textWidth
  by overlayWidth+spacing, draws at left padding; HCENTER centers without text-width
  change; RIGHT shifts textRight/reduces textWidth, draws at width-rightPad-overlayWidth;
  invalid alignment draws nothing. Half sizes use integer division.
- If getCurrentLabel() is nonempty, copy another getCurrentLabel(), trim wide string.
  x=right text edge for RIGHT, textLeft+textWidth/2 for HCENTER, textLeft otherwise.
  Pressed+displayPressed increments x. Font buffer renders trimmed label offset0,
  x, y=integer(height/2)+bottomVPad, labelColor%alpha, current HAlign/VCENTER,
  NORMAL, SOFT shadow iff dropShadowedText, S32_MAX chars,textWidth pixels,rightX=null,
  useEllipses/useFontColor. Store returned mLastDrawCharsCount. Empty original label
  skips rendering WITHOUT clearing previous count; whitespace-only label enters
  but becomes empty after trim. Cache caller obligations apply to both flags.
- CheckboxControlPanel exists -> setOrigin(0,0), reshape(current button width,height),
  invoke its virtual draw directly. Then qualified LLUICtrl::draw, whose child
  traversal and potential duplication of that panel remain unresolved.

**2.** Native CPU preparation must resolve input/model callbacks, animation state,
layout changes, image/material choice and text shaping into immutable ordered output.
Native GPU records consume resource versions, clip/alpha and geometry only.
**3.** Prefer explicit phased preparation with documented old-state/new-state reads
to either calling this GL draw or moving all callbacks arbitrarily before rendering.
The source's image-before-toggle/label-after-toggle split is a defined ordering to
test; correcting it requires explicit behavior review. Animation time advancement
must preserve per-view/repeated-draw policy, not assume once-per-frame equivalence.
Owned control handles are needed across reentrant action callbacks; retaining a raw
this/image pointer does not establish native callback lifetime safety.
Checks: toggle callback changes false->true and width; image uses old selection,
label/overlay use current selection. Test flashing setting false with active timer,
every image fallback, tentative state, focus/pressed combinations, .01 glow threshold,
both scale modes, every overlay placement, empty/trimmed label, ellipses/color flags,
checkbox side effects, repeated auxiliary draws and callback deletion/cancellation.
Outgoing: cached setting construction/subscription, draw context/transparency,
keyboard/focus/capture and coordinate helpers, toggle/value/LLSD signal, timer and
interpolation, color/image providers, getCurrentLabel/trim, font buffer render/reset,
drawBorder, checkbox panel layout/draw, base draw and all virtual overrides.

## UI-BUTTON-002: LLButton::setToggleState

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1348).
**1.** If new bool differs from getToggleState: setControlValue(bool) first (settings
callbacks), setValue(bool), setFlashing(false), autoResize(), fontBuffer.reset().
Equal state does nothing. No re-check after callbacks, transaction rollback or
lifetime guard. Callback arguments omitted for setFlashing resolve header defaults.
**2.** CPU model/settings/action mutation and layout invalidation; no Vulkan work
belongs in this setter. **3.** Preserve notification/value/resize order with
callback-safe native control ownership and invalidated prepared text. Do not share
reference autoResize without closing its font measurement/resource effects.
Check equal value suppresses all work, nested settings writes, resize text/geometry,
flashing cancellation and deleted owner. Outgoing: getToggleState,setControlValue,
virtual setValue,setFlashing/defaults,autoResize,font cache reset.

## UI-BUTTON-003: LLButton::getToggleState

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1343).
**1.** getValue().asBoolean(); no separate toggle field. **2.** CPU view-model query.
**3.** Preserve LLSD coercion/default semantics and virtual getValue target rather
than introduce divergent cached native toggle state. Check bool and other LLSD types.
Outgoing: getValue override/model and LLSD::asBoolean.

## UI-BUTTON-004: LLButton::drawBorder

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1329).
**1.** Null image returns. ScaleImage -> image.drawBorder(local rect,color,size).
Otherwise y=local height-image height and drawBorder(0,y,color,size), natural size.
**2.** CPU focus-decoration geometry and native image-coverage material.
**3.** Preserve image-shaped enlarged decoration, not generic outline; dimensions
and resources need prepared snapshots. Check scale/natural placement and null image.
Outgoing: rect/image dimensions, both LLUIImage border overloads and native lifetime.

## UI-BUTTON-005: LLButton::getOverlayImageSize

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L944).
**1.** Read image width then height into output references. factor=min(buttonWidth/
overlayWidth,buttonHeight/overlayHeight,1), scale and ll_round each extent. No null,
zero-size, finite or negative-size guard. **2.** CPU aspect-preserving fit, never
upscale valid positive inputs. **3.** Shared audited geometry can provide native
layout, with explicit invalid-asset policy rather than undefined division/casts.
Check non-square, downscale, natural fit, zero and negative extents. Outgoing:
virtual image dimensions, button rect, llmin/round and image availability timing.

## UI-BUTTON-006: LLButton::setHighlight

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L908).
**1.** Changed bool -> assign mNeedsHighlight and fontBuffer.reset; equal no-op.
**2.** CPU hover appearance/cache invalidation. **3.** Native state generation must
invalidate dependent text/material even if geometry parameters otherwise unchanged.
Check equal/changed value and retained text. Outgoing: buffer reset and setter callers.

## UI-BUTTON-007: LLButton::onMouseLeave

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L901).
**1.** Qualified LLUICtrl::onMouseLeave(x,y,mask) then setHighlight(false).
**2.** CPU pointer-event/callback propagation. **3.** Retain order and callback-safe
owner lifetime; GPU submission never invokes event methods. Check base callback
mutates highlight or deletes control. Outgoing: base callback and setHighlight.

## UI-BUTTON-008: LLButton::handleHover

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L917).
**1.** Enabled chain AND (no mouse capture OR captured by this) -> setHighlight(true).
Does not explicitly clear highlight when condition false. childrenHandleHover first;
if unhandled and mouse-down timer started, getHeldDownTime; when elapsed>=heldDelay
AND current frame count minus mouseDownFrame>=frameDelay, construct LLSD count with
postincrement mouseHeldDownCount, invoke held signal if nonnull. Count increments
even without signal. Then set window cursor to HAND if hoverHandCursor else ARROW,
debug log. Child-handled path skips held callback/cursor. Always return true.
**2.** CPU repeated-hover input and timer/action semantics. **3.** Native event loop
must preserve dual time/frame threshold and child precedence; a time-only repeat
timer would differ. Audit callback lifetime before cursor access after signal.
Check threshold equality, frame delta, captured elsewhere, disabled, child handled,
null held signal, repeated hover and deletion/reentry. Outgoing: enabled/capture,
child hover targets, timer/frame count, getHeldDownTime, LLSD/signal and window cursor.

## UI-BUTTON-009: LLButton::setUseEllipses

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L385).
**1.** Assign flag then fontBuffer.reset unconditionally, even if unchanged.
**2.** CPU text-layout policy/invalidation. **3.** Native prepared text needs an
explicit ellipsis input/version; for this setter the reference's omitted retained
cache key is compensated by reset. Check toggling flag with otherwise equal render
inputs. Outgoing: reset, setter callers and direct field writes still open.

## UI-BUTTON-010: LLButton::setUseFontColor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L391).
**1.** Assign use-font-color then reset unconditionally. **2.** CPU color/grayscale
glyph policy and invalidation. **3.** Explicit native glyph-type selection/version;
reference buffer key omission is compensated for this setter only.
Check glyph request type after switch. Outgoing: reset, direct writes/callers.

## UI-BUTTON-011: LLButton::getCurrentLabel

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1487).
**1.** Return selected or unselected LLUIString const reference based on getToggleState.
No trimming here. **2.** CPU label selection and source-index ownership. **3.** Native
preparation must snapshot text after the appropriate model callback, retaining original
versus trimmed text distinction. Check changed toggle, reference lifetime, whitespace.
Outgoing: getToggleState and LLUIString content/formatting lifetime.

## UI-BUTTON-012: LLButton::setFont

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1435).
**1.** Store supplied font if nonnull, else LLFontGL::getFontSansSerif(); reset buffer.
No owned font reference is acquired here. **2.** CPU font identity resolution and
layout invalidation; native glyph GPU ownership separate. **3.** Native font handle
must survive asynchronous preparation and registry reset; fallback getter's GL font
chain remains open. Check null/explicit font and registry destruction.
Outgoing: default font getter, raw owner, reset and metric consumers.

## UI-BUTTON-013: LLButton::setDropShadowedText

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1526).
**1.** Assign flag, reset buffer. **2.** CPU decoration policy. **3.** Native text
preparation includes shadow mode and ordered glyph copies; no generic blur substitute.
Check same/changed flag. Outgoing: reset, render shadow selection and callers.

## UI-BUTTON-014: LLButton::autoResize

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1541).
**1.** resize(getCurrentLabel()), no mAutoResize guard in facade. **2.** CPU layout
measurement/invalidation. **3.** Native preparation needs audited CPU metrics even
for paths whose resize flag is false; do not call this GL-font-owning facade.
Check disabled autoresize still reaches measurement. Outgoing: current label/resize.

## UI-BUTTON-015: LLButton::resize

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1546).
**1.** ALWAYS mGLFont.getWidth(label.getWString().c_str()), then current width.
Only then mAutoResize gate. Enabled: minWidth=labelWidth+left/right padding. Overlay
present -> source width, factor=(buttonHeight-bottomPad-topPad)/sourceHeight WITHOUT
clamp<=1, round scaled overlay width. LEFT/RIGHT add overlayWidth+spacing; HCENTER
max(label minimum,overlayWidth+both paddings); unknown leaves label minimum. If old
width<minimum, reshape(minimum,current height). Never shrink here. RightDelta drawing
override is not considered by this layout method. Font measurement can rasterize/
allocate GL atlas resources even if mAutoResize=false; no font/image null-size guard.
**2.** CPU native measurement, minimum-width computation and tree reflow with text/
image metadata independent of GPU readiness. **3.** Separate metrics/layout from
glyph publication and preserve this fit rule, which differs from draw overlay fit's
no-upscale constraint. Do not equate draw size to resize minimum automatically.
Check flag false still measures, grow-only, overlay upscale/padding/zero height,
all alignments, rightDelta, and callbacks triggered by reshape.
Outgoing: LLUIString conversion, GLFont getWidth chain, image dimensions, rounding,
rect arithmetic, virtual reshape and parent/child invalidation.

## UI-BUTTON-016: LLButton::Params constructor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L71).
**1.** Base UI control Params first. Explicit defaults: label_shadow=true,
auto_resize=false,use_ellipses=false,use_font_color=true,overlay alignment='center',
overlay-label spacing1,rightDelta0,overlay tint white alpha.75,disabled overlay white
alpha.3,selected overlay white; left/right padding global LLBUTTON_H_PAD; is_toggle
false,scale_image true,commit_on_return true,commit_on_capture_lost false,
display_pressed_state true,use_draw_context_alpha true,draw_focus_border true,
hover_hand_cursor false,button_flash_enable false. Named fields without explicit
values: selected label, ordinary/selected/hover/disabled/pressed images, overlay,
top/bottom image padding, all label colors,image/disabled image colors,flash/alternate
flash colors,bottom label pad,click/down/up/held/toggle callbacks,hover glow,badge,
right-mouse policy,held delay,flash count/rate,checkbox control. Wrapper constructors
decide implicit defaults. Add synonym toggle for is_toggle; changeDefault(initial_value,
LLSD(false)). image_flash member initialization is not explicitly named in this list;
its header/default wrapper remains an obligation.
**2.** CPU declaration schema/default policy, neutral font/image/action identities.
**3.** Preserve provided flags, globals and typed defaults; no device allocation in
new native declaration types. Check overridden skin defaults, implicit versus explicit
false, initial value provided state and first-time font construction.
Outgoing: base/wrapper/value constructors, changeDefault/addSynonym, globals and templates.

## UI-BUTTON-017: LLButton constructor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L143).
**1.** LLUICtrl(p), LLBadgeOwner(getHandle()). Initialize frame/held count0,flashing
false,alternate flash false,glow0,highlight false; labels,font pointer,held time/frame
delays and all GL image owners from Params. Copy each checkpoint native alias only
if corresponding vk_image_name provided, else empty. Read hover images,label/image/
flash/overlay colors, overlay alignment via hAlignFromName,padding/spacing/rightDelta,
toggle/scale/shadow/autoresize/ellipsis/font-color flags,alignment,label padding,
hover glow and commit policies. fadeWhenDisabled=false,forcePressed=false; display
pressed from Params,last drawn count0; down/up/held/toggle signals null; alpha/focus/
cursor/right-mouse policy from Params; flash timer and checkbox panel null, checkbox
control string copied. Member initialization order follows header declarations;
timers/cache/base default constructors are separate dependencies.

Body sequence: flash enabled -> new LLFlashTimer(null callback,count if provided
else0,rate if provided else0); else copy flash count/rate into deferred fields.
Construct static cached UIButtonOrigHPad(setting fallback0) and static copy of
factory default Button Params. Absent selected label -> copy ordinary label.
If rect.right>=0 AND width>0 AND available padded width<font.getWidth(' '), replace
both horizontal pads with cached original pad; glyph measurement happens during
construction. Stop mouse-down timer.
Custom ordinary image compared to defaults: if disabled image equals default,
replace with ordinary image and enable fade; default pressed-selected -> ordinary.
Custom selected image: analogous disabled-selected replacement/fade and default
pressed -> selected. Then absent pressed -> selected and absent pressed-selected
-> ordinary regardless preceding comparisons. Warn if ordinary image null.
Provided click callback -> initCommitCallback/setCommitCallback; provided mouse
down/up/held -> initCommitCallback and respective setters; provided toggle ->
initEnableCallback/setIsToggledCallback. Provided badge -> initBadgeParams.
No callback-safe construction transaction or GL-free owner boundary in this body.
**2.** CPU native control construction, defaults, timer/action subscription and layout;
image/font resource versions publish separately. **3.** Replace GL-owning defaults
with audited semantic identities and inject lifecycle-owned CPU services. Late native
draw selection cannot remove these constructor dependencies. Timer/default-static
lifetimes must be reconciled with backend selection and partial failure.
Checks: first/default/custom-image construction, selected-label omission, narrow
button space metric, optional flash fields, callback resolution and badge failure.
Outgoing: all bases/members, Params/value comparisons, font/default registry,
cached settings, LLFlashTimer, handle, callbacks and badge construction/destruction.

## UI-BUTTON-018: LLButton destructor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L329).
**1.** Delete button down/up/held/toggle signals in order; flash timer if nonnull
unset(), NOT direct delete. Members/bases subsequently destroy; commit signal owned
by base. Timer self-removal/deferred deletion cannot be inferred from unset name.
**2.** CPU callback/timer shutdown and control release; native GPU uses outlive CPU
cache owners as needed. **3.** Explicit cancellation/teardown graph, no resource reuse
until all uses complete. Check callback-held references, unset timing and partial init.
Outgoing: signal destruction, LLFlashTimer::unset, members/font cache/base owners.

## UI-BUTTON-019: LLButton::onCommit

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L345).
**1.** Down signal(this,empty LLSD) if present, then up signal. Sound flags separately
gate UISndClick and UISndClickRelease. Toggle if mIsToggle. Qualified LLUICtrl::onCommit
LAST, explicitly allowing destruction there. Earlier signals/toggle callbacks still
have no local lifetime guard. **2.** CPU programmatic click action with sound and
toggle ordering. **3.** Native actions preserve this route separately from keyboard
and physical mouse paths, not dispatch all through onCommit indiscriminately.
Check both sounds, toggle, event ordering and destruction at each callback boundary.
Outgoing: signals/LLSD, sound service, toggleState and base commit targets.

## UI-BUTTON-020: LLButton::handleUnicodeCharHere

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L719).
**1.** Space AND !keyboard.getKeyRepeated(space): optionally toggle, qualified
LLUICtrl::onCommit, handled=true. Other inputs false. No sound/button down/up signals
in this body. No local focus/enabled check; dispatch caller responsible.
**2.** CPU character activation. **3.** Preserve repeat and dispatch eligibility
independently of native key events; avoid duplicate activation from key+text input.
Check space repeat and base commit versus button onCommit path.
Outgoing: keyboard repeat, toggle, base commit, dispatch focus/enabled policy.

## UI-BUTTON-021: LLButton::handleKeyHere

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L737).
**1.** commitOnReturn AND key RETURN AND mask NONE AND !repeat: optional toggle,
handled=true then base commit. Else false. No button down/up or sound locally.
**2.** CPU keyboard action. **3.** Preserve exact modifier/repeat/parent propagation;
panel default-button logic is a different route. Check all gates and destruction.
Outgoing: keyboard repeat, toggle/base commit and parent key dispatch.

## UI-BUTTON-022: LLButton::handleMouseDown

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L755).
**1.** If childrenHandleMouseDown false: set capture=this before focus; tabStop AND
!chrome -> setFocus(true). Nonempty function name -> debug and UIUsage logCommand/
logControl(pathname). Qualified base handleMouseDown (also emits base mouse signal),
eventRecorder.updateMouseEventInfo(x,y,-55,-55,path), then button down signal with
empty LLSD. Start timer, save frame count cast S32, heldCount0. Sound MOUSE_DOWN
-> UISndClick. Child handled skips these. Always true; base return ignored.
**2.** CPU capture/focus/action/event recording and held-repeat start.
**3.** Native event ownership must preserve preemptive capture and separate base/
button signal signatures; callback deletion/reentry requires explicit lifetime model.
Check child handled, chrome/tabStop, both down signals, timer after callbacks and
capture-lost reentry. Outgoing: child dispatch, focus manager, usage/event services,
base/button callbacks, timer/frame and sound.

## UI-BUTTON-023: LLButton::handleMouseUp

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L796).
**1.** Has capture: resetMouseDownTimer BEFORE release capture to avoid extra
commit-on-capture-lost. Base handleMouseUp, recorder with -55 coordinates, button up
signal regardless release location. If pointInView: optional release sound, optional
toggle, base commit LAST. No capture -> childrenHandleMouseUp only. Always true.
No local enabled test or lifetime guard after up callbacks.
**2.** CPU release/action semantics with cancellation outside bounds. **3.** Preserve
timer-before-capture ordering and up-versus-commit distinction in native input.
Check release outside, capture loss, callbacks mutating geometry before hit test,
disabled transitions and deleted owner. Outgoing: timer reset,capture lost callback,
base/child dispatch, recorder, pointInView,toggle/sound/commit.

## UI-BUTTON-024: LLButton::handleRightMouseDown

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L843).
**1.** handleRightMouse AND child dispatch false -> capture this, tabStop&&!chrome
focus true, base handleRightMouseDown. Does not start button down timer/signals or
sound here. Always true even when handling flag false. **2.** CPU right-button
capture routing. **3.** Preserve consumed-result behavior and separate route.
Check flag false, child handled and focus callback. Outgoing: child/base/focus/capture.

## UI-BUTTON-025: LLButton::handleRightMouseUp

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L872).
**1.** handleRightMouse true: if capture release it; else child right-up dispatch.
Then base right-up ALWAYS within flag branch. No direct click/toggle/sound here.
Always true including flag false. **2.** CPU right-button release propagation.
**3.** Keep child and base dual dispatch when uncaptured, not left-up behavior.
Check captured/uncaptured/disabled flag, capture-lost callback and child mutation.
Outgoing: capture manager, child/base right-up and their callbacks.

## UI-BUTTON-026: LLButton::postBuild

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L469).
**1.** autoResize first. Nonempty checkboxControl -> createFromFile panel_button_checkbox.xml
with this parent and default child registry. Success reshape to button size, find
LLCheckBoxCtrl 'check_control'; found -> setControlName(name,null context), missing
warn. Panel creation failure warns. Always addBadgeToParentHolder then return base
postBuild bool. Does not fail solely for missing checkbox or prevent repeated panel
creation on repeated postBuild. **2.** CPU nested declarations/settings/actions/layout.
**3.** Native control construction must cover checkbox and badge child lifetimes,
not render only a button background/label. Check missing file/control, repeated
postBuild, setting lookup context and parent ownership.
Outgoing: autoResize, factory/panel/checkbox/settings, badge owner/base postBuild.

## UI-BUTTON-027: LLButton::dirtyRect

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L713).
**1.** Base dirtyRect then font buffer reset. **2.** CPU damage and retained-text
invalidation. **3.** Native prepared records need equivalent invalidation dependencies,
not just rect inequality. Check same-shape dirtiness and callback ordering.
Outgoing: base invalidation, reset, all callers/overrides.

## UI-BUTTON-028: LLButton::onVisibilityChange

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L501).
**1.** Reset font buffer BEFORE qualified base visibility notification; return void
base expression. **2.** CPU visibility/cache invalidation. **3.** Keep preparation
invalidation before callbacks that may query rendered state. Check hide/show/repeated.
Outgoing: reset/base and callbacks.

## UI-BUTTON-029: LLButton::setFlashing

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1363).
**1.** Assign force flag. Timer exists: assign flashing bool and startFlashing or
stopFlashing even if same bool. No timer: only changed bool assigns and frameTimer.reset.
Always assign alternate-color flag at end. No font-buffer reset locally.
**2.** CPU animation/timer policy. **3.** Native time-state must preserve restart
versus no-op distinctions and selection cancellation, independently of GPU frames.
Check same bool with/without timer, force and alternate transitions.
Outgoing: timer start/stop/reset, draw consumption, constructor and timer ownership.

## UI-BUTTON-030: LLButton::toggleState

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1382).
**1.** flipped=!getToggleState, setToggleState(flipped), return originally computed
flipped even if nested callback changes final model again. **2.** CPU action result.
**3.** Preserve result versus post-callback state distinction; avoid assuming returned
bool describes final model after reentrancy. Check nested toggle callbacks.
Outgoing: toggle getter/setter and model notifications.

## UI-BUTTON-031: LLButton::setLabel(string)

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1390).
**1.** Chained assignment selected=string then unselected=selected result; reset
font buffer, no autoResize locally. **2.** CPU localized label state/cache invalidation.
**3.** Native label model preserves argument formatting and assignment semantics,
not raw bytes only. Check both labels, arguments, UTF8 and later resize timing.
Outgoing: LLUIString assignment and buffer reset.

## UI-BUTTON-032: LLButton::setLabel(LLUIString)

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1396).
**1.** Chained copy selected then unselected; reset. **2.** CPU formatted-label
ownership. **3.** Preserve source text/arguments/cache copy semantics after LLUIString
audit. Check argument maps and independent future edits. Outgoing: assignment/reset.

## UI-BUTTON-033: LLButton::setLabel(LLStringExplicit)

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1402).
**1.** setLabelUnselected then setLabelSelected, hence two resets. **2.** CPU labels.
**3.** Native logical update can coalesce cache work only after proving no consumer
observes intermediate state. Check both label values and reset side effects.
Outgoing: both setters.

## UI-BUTTON-034: LLButton::setLabelArg

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1409).
**1.** unselected.setArg then selected.setArg, reset, return true unconditionally.
**2.** CPU localization/format invalidation. **3.** Native retains source/format
arguments and recomputes layout without GL; return is not proof a token existed.
Check missing token, unicode replacement and both variants. Outgoing: LLUIString/reset.

## UI-BUTTON-035: LLButton::setLabelUnselected

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1417).
**1.** Assign unselected label, reset unconditionally. **2.** CPU source mutation.
**3.** Invalidate prepared text even while selected if cache may later switch.
Check equal value/inactive label. Outgoing: assignment/reset.

## UI-BUTTON-036: LLButton::setLabelSelected

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1423).
**1.** Assign selected label, reset unconditionally. **2.** CPU source mutation.
**3.** Same ownership rule with selected variant. Check inactive mutation and toggle.
Outgoing: assignment/reset.

## UI-BUTTON-037: LLButton::onMouseCaptureLost

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1680).
**1.** commitOnCaptureLost AND mouseDownTimer started -> button up signal if present,
optional toggle, qualified base commit. ALWAYS resetMouseDownTimer afterward. Unlike
physical mouse-up, base commit is not last access to this object. Potential deletion
in callbacks is an open lifetime defect, not native behavior to emulate.
**2.** CPU capture-transfer action/cancellation. **3.** Native capture state updates
before notifications with stable/weak control identities; no dereference after an
action can destroy its owner. Preserve intended single-commit sequence while testing
normal release's pre-reset suppression separately from unexpected capture loss.
Check timer stopped/started, policy false/true, callback deletion/reentrant capture,
one versus duplicate toggle. Outgoing: timer flag, signals, toggle/base commit/reset.

## UI-BUTTON-038: LLButton::resetMouseDownTimer

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1787).
**1.** stop timer then reset timer. Does not reset held count or saved frame field.
**2.** CPU repeat/activation state. **3.** Preserve stopped status when resetting
elapsed time; a generic restart helper is not equivalent. Check started flag after
reset and next capture-loss handling. Outgoing: LLFrameTimer::stop/reset.

## UI-BUTTON-039: LLButton::handleDoubleClick

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1793).
**1.** Return handleMouseDown(x,y,mask); no double-click-specific signal in this body.
**2.** CPU second-press input. **3.** Native input must not invoke both this and
another synthetic down for one double-click event. Check dispatch/capture sequence.
Outgoing: virtual mouse-down target and window event mapping.

## UI-CALLBACK-001: LLUICtrl::initCommitCallback

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L284).
**1.** Provided function: provided parameter -> bind(function,_1,cb.parameter), else
return function. No explicit function: read name, setFunctionName(name), scoped
CommitCallbackRegistry lookup. Found -> parameter provided binds(*function,_1,param),
else slot copy. Not found/nonempty name warns. Fallback default_commit_handler.
Explicit function path does not set function-name metadata. Parameter wrapper
capture/conversion remains a typed binding obligation, not assumed eager LLSD copy.
**2.** CPU native action resolution with optional parameter override and live source
control argument. **3.** Resolve audited action targets under preparation scope,
retain callable/connection lifetime, no GPU callback invocation or raw GL control
capture. Preserve named action metadata and missing-callback diagnostics.
Check explicit/named/missing/empty, provided parameter, scope override and deletion.
Outgoing: parameter wrappers, bind/slot semantics, registry, default handler and
setFunctionName/logging consumers. Every registered action target remains separate.

## UI-CALLBACK-002: LLUICtrl::initEnableCallback

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L313).
**1.** Provided function+parameter -> bind(function,THIS,param), no parameter ->
function. Otherwise EnableCallbackRegistry name lookup; found+parameter binds THIS,
else slot copy. Missing -> default_enable_handler, no missing-name warning here.
Unlike commit, bound parameter causes capture of construction-time control rather
than forwarding invocation's first argument. **2.** CPU eligibility/toggle query.
**3.** Native lifetime-safe control identity in bound predicate; preserve which
control is queried and default true behavior after missing resolution.
Check invoking with different control, provided override, empty function and scope.
Outgoing: registry, bind/slot, raw-this lifetime and default-enable handler.

## UI-CALLBACK-003: LLUICtrl::onCommit

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L434).
**1.** Only nonnull commit signal: nonempty function name logs command/control
pathname through UIUsage, then invoke signal(this,getValue()). No validation-signal
check or settings write in this body. No post-callback access. Empty signal pointer
means no value lookup/usage log. **2.** CPU action dispatch and usage telemetry.
**3.** Preserve separate validation/commit contracts; naming a signal ValidateBeforeCommit
does not establish invocation here. Native actions use stable owner/model snapshots.
Check missing signal, value mutation, usage metadata, callbacks destroying owner.
Outgoing: getValue virtual/model, signal slot list/combiner, UIUsage/pathname.

## UI-CALLBACK-004: LLUICtrl::setValue

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L459).
**1.** mViewModel->setValue(value), no local null guard or control-variable write.
**2.** CPU shared view-model mutation. **3.** Native model must preserve sharing and
concrete model effects, not conflate with settings setter. Check shared controls,
model derived type and lifetime. Outgoing: virtual model setter and model ownership.

## UI-CALLBACK-005: LLUICtrl::getValue

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L465).
**1.** Return mViewModel->getValue() as LLSD value. **2.** CPU model snapshot/query.
**3.** Audit LLSD/model copy semantics before treating as deeply immutable native
action data. Check shared model and callback changes. Outgoing: model getter/LLSD.

## UI-CAPTURE-001: LLFocusMgr::setMouseCapture

Source: [llfocusmgr.cpp](../../../indra/llui/llfocusmgr.cpp#L377).
**1.** Equal pointer no-op. Different: save old, assign new pointer FIRST. Debug
handling flag logs new name/null. Old nonnull -> virtual onMouseCaptureLost; no
reassignment afterward, so reentrant transfer can replace new pointer. No new
captor notification, retain handle, platform capture API or null validation here.
**2.** CPU capture ownership and transfer event. **3.** Native input manager exposes
stable control IDs/weak references, commits state before old-owner callback, preserves
reentrancy without resurrecting destroyed owners. Platform capture remains a separate
window dependency. Check same/null/reentrant transfers and old callback observing new.
Outgoing: raw pointer lifetime, getName, old virtual target, platform callers.

## UI-FLASH-001: LLFlashTimer constructor

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L30).
**1.** LLEventTimer(period), callback copy,tickCount0,flashing/highlighted/unset=false.
Stop embedded timer. Static cached FlashCount; mFlashCount=2*(count>0 ? count :
setting), no negative/overflow clamp here. If inherited period<=0 use cached FlashPeriod.
Connect FlashCount and FlashPeriod setting signals to raw this->onUpdateFlashSettings,
discard returned connections. Does not start timer after registration.
**2.** CPU timed animation plus live settings subscription. **3.** Native timer owner
must own/disconnect subscriptions and cancellation token, with bounded tick count and
explicit teardown even when stopped. Cannot reuse raw-this lifetime unexamined.
Check explicit/default period/count, stopped new timer, setting callbacks during
destruction and integer limits. Outgoing: base timer/tracker, cached controls/settings,
signal connections/bind and callback target.

## UI-FLASH-002: LLFlashTimer::onUpdateFlashSettings

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L67).
**1.** stopFlashing; count=2*max(config FlashCount,0); period=max(config FlashPeriod,0).
Does not restart; overrides explicit constructor values after either setting change.
**2.** CPU animation configuration transition. **3.** Preserve stop-on-update and
coupled reread behavior in native subscription, with lifetime-safe cancellation.
Check either setting change, zero period, large count and previously explicit values.
Outgoing: stop, config reads/global UI lifetime and setting dispatch.

## UI-FLASH-003: LLFlashTimer::unset

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L76).
**1.** unset=true, callback=null; does NOT start/stop timer, delete object or disconnect
setting signals. **2.** CPU cancellation request. **3.** Native destruction cannot
depend on next tick for stopped timers; use owner removal and explicit subscription
teardown. Existing stopped-timer lifetime is not a feature to emulate.
Check stopped versus running at unset, later setting update and next event loop.
Outgoing: event timer updateClass/tick and raw settings callbacks.

## UI-FLASH-004: LLFlashTimer::tick

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L82).
**1.** Toggle highlighted, callback if present, preincrement tick count and if
count>=flashCount stopFlashing; return unset. Callback may mutate timer state before
count increment/check. Does not guard unset before highlight mutation.
**2.** CPU time transition/action. **3.** Native scheduler needs stable timer ownership
during callbacks and explicit canceled state; preserve active animation sequence.
Check callback stop/start/unset, final tick, zero/negative count and owner deletion.
Outgoing: callback/stop and scheduler deletion policy.

## UI-FLASH-005: LLFlashTimer::startFlashing

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L99).
**1.** flashing=true,highlighted=true,eventTimer.start; does not reset tickCount or
unset flag locally. **2.** CPU animation activation. **3.** Preserve restart semantics
only after embedded timer start audit; native action restart must not silently reset
every field. Check repeated start and start-after-unset. Outgoing: timer start.

## UI-FLASH-006: LLFlashTimer::stopFlashing

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L106).
**1.** eventTimer.stop,flashing=false,highlighted=false,tickCount0; unset unchanged.
**2.** CPU animation stop. **3.** Native cancellation and visual reset distinct from
resource retirement. Check final-tick behavior and subsequent start.
Outgoing: embedded timer stop and draw queries.

## UI-FLASH-007: LLFlashTimer::isFlashingInProgress

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L114).
**1.** Return flashing flag. **2.** CPU appearance query. **3.** Snapshot state during
preparation without advancing time. Check stopped/unset/running combinations.
Outgoing: flag writers/lifetime.

## UI-FLASH-008: LLFlashTimer::isCurrentlyHighlighted

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L119).
**1.** Return highlight flag. **2.** CPU appearance query. **3.** Distinguish from
hover highlight and actual flash-image enable setting. Check tick/stop sequence.
Outgoing: flag writers/lifetime.

## UI-FLASH-009: LLFlashTimer destructor

Source: [llflashtimer.h](../../../indra/llui/llflashtimer.h#L47).
**1.** Empty body; callback member/base destruct. No stored settings connections
exist in class for local disconnect. **2.** CPU subscription/timer teardown.
**3.** Explicit native connection ownership required; late raw-this signal invocation
after scheduler deletion is a lifetime risk, not compatibility requirement.
Check settings update after final tick deletion. Outgoing: callback/base/tracker dtor,
signal lifetime and actual deletion paths.

## UI-EVENTTIMER-001: LLEventTimer(period) constructor

Source: [lleventtimer.cpp](../../../indra/llcommon/lleventtimer.cpp#L40).
**1.** Default embedded event timer construction, assign supplied period. Implicit
instance tracker/base initialization remains open. No range clamp in body.
**2.** CPU scheduler registration/time state. **3.** Native lifecycle-owned scheduler
must audit registration before sharing. Check negative/zero period and tracker state.
Outgoing: timer/tracker constructors and updateClass.

## UI-EVENTTIMER-002: LLEventTimer::updateClass

Source: [lleventtimer.cpp](../../../indra/llcommon/lleventtimer.cpp#L59).
**1.** Iterate instance_snapshot; read elapsed even if stopped. Started AND elapsed>
period (strict) -> reset timer BEFORE virtual tick; tick true -> delete &timer.
No catch/retain guard beyond snapshot semantics. Stopped unset LLFlashTimer never
gets tick via this body. **2.** CPU scheduler events and deletion. **3.** Native timer
ownership must separate active tick scheduling from cancellation/deletion; stable
iteration and reentrant removal need audited tracker semantics.
Check equality threshold, stopped cancel, callback deleting other timers, new timers
during iteration, tick true and callback exceptions. Outgoing: instance_snapshot,
elapsed/started/reset, every tick override and virtual destructor.

## UI-FRAMETIMER-001: LLFrameTimer::start

Source: [llframetimer.cpp](../../../indra/llcommon/llframetimer.cpp#L52).
**1.** reset(), then mStarted=true. **2.** CPU input-repeat time state. **3.** Retain
explicit start versus reset semantics in native scheduler; no rendering API work.
Check repeated start and previously stopped timer. Outgoing: reset/frame clock.
Runtime: existing harness test4 verifies start enables, after either prior state.

## UI-FRAMETIMER-002: LLFrameTimer::stop

Source: [llframetimer.cpp](../../../indra/llcommon/llframetimer.cpp#L58).
**1.** mStarted=false only. Does not freeze/replace timestamps in this body.
**2.** CPU active-state update. **3.** Preserve differences from pause semantics;
native input cancellation cannot accidentally restart on elapsed-time reset.
Check stopped flag and retained timestamps. Outgoing: elapsed getters and reset.
Runtime: test4 verifies stopped status; timestamp behavior not tested there.

## UI-FRAMETIMER-003: LLFrameTimer::reset

Source: [llframetimer.cpp](../../../indra/llcommon/llframetimer.cpp#L63).
**1.** mStartTime=sFrameTime,mExpiry=sFrameTime; started flag unchanged. **2.** CPU
reference-time reset. **3.** Explicit state/time separation; frame-clock epoch and
update schedule need closure for repeat parity. Check running/stopped resets.
Runtime: test4 passed both cases. Outgoing: static frame time initialization/update,
expiry/elapsed consumers. This is LLFrameTimer, not LLFlashTimer's embedded LLTimer.

## UI-TRACKER-001: unkeyed LLInstanceTracker constructor

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L486).
**1.** shared_ptr<T>(static_cast<T*>(this),no-op deleter), assign weak mSelf; lock
static set and emplace shared pointer. Shared ownership is of tracking control block,
NOT object storage. **2.** CPU lifetime observation registry. **3.** Suitable as an
audited observer only; native control/submission owners need actual lifetime retention
or explicit callback validity checks, not a misleading shared_ptr type.
Check stack/heap instances and getWeak expiration after destruction. Outgoing:
LockStatic/StaticData,set allocation,no-op deleter and base construction ordering.

## UI-TRACKER-002: unkeyed LLInstanceTracker destructor

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L498).
**1.** Lock static set, erase shared_ptr from mSelf.lock(). Does not invalidate an
already-held external shared_ptr control block by magic or defer C++ object deletion.
**2.** CPU registry removal. **3.** Native callback safety requires strong owning
objects or validated handles; observer snapshots cannot keep deleted instances alive.
Check pending snapshot and held strengthened pointer separately.
Outgoing: lock/set, mSelf weak destruction and explicit object deletion callers.

## UI-TRACKER-003: snapshot_of constructor (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L413).
**1.** LockStatic member acquired before mData. Copy set shared_ptr entries into
vector of WEAK pointers, unlock after population. Windows owns LockStatic through
shared_ptr and reference member; non-Windows direct member. New registrations after
snapshot absent. No rendering/model snapshot copied, only tracking identities.
**2.** CPU stable enumeration membership with liveness observation. **3.** Native
scheduler can use equivalent snapshot semantics for mutation-tolerant traversal,
but GPU versions need real completion-owned resources, not observer pointers.
Check deletion/new creation after snapshot and platform member lifetime.
Outgoing: LockStatic, vector conversions, set ordering and iterator strengthening.

## UI-TRACKER-004: snapshot_of::strengthen (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L403).
**1.** Return dynamic_pointer_cast<SUBCLASS>(weak.lock()). No object storage ownership
is acquired beyond no-op-deleter tracking pointer. **2.** CPU liveness/type filtering.
**3.** Do not confuse nonnull result with protection from arbitrary explicit deletion
inside callback. Check expired pointer/derived mismatch and held-pointer deletion.
Outgoing: std weak/dynamic cast semantics, tracker lifetime and consumers.

## UI-TRACKER-005: snapshot_of::dead_skipper (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L407).
**1.** Return bool(pointer). **2.** CPU iterator filtering. **3.** Native enumeration
must recheck actual owner lifetime at action boundaries where explicit deletion is
allowed. Check empty/nonempty strengthened pointers. Outgoing: iterator composition.

## UI-TRACKER-006: snapshot_of::make_iterator (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L429).
**1.** Construct filter iterator(dead_skipper, transform iterator(iter,strengthen),
transform iterator(end,strengthen)). Returns shared_ptr values for still-live matching
types. No list mutation during construction. **2.** CPU weak snapshot traversal.
**3.** Preserve skipped dead entries, while treating explicit owner destruction within
current callback separately. Check deleted future/current entries and end iterator.
Outgoing: Boost transform/filter iterator public contracts and helper targets.

## UI-MODEL-001: LLViewModel default constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L39).
**1.** mDirty=false, LLSD member default construction/base refcount. **2.** CPU scalar
model. **3.** Reuse audited neutral data/lifetime if includes and concrete model types
are decoupled; class name alone does not establish GL-free transitive construction.
Check undefined initial LLSD and dirty false. Outgoing: LLSD/LLRefCount constructors.

## UI-MODEL-002: LLViewModel(value) constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L45).
**1.** Dirty false initially, then setValue(value). Constructor virtual dispatch
resolves to LLViewModel setter, not later-derived override. Final dirty true.
**2.** CPU model initialization. **3.** Preserve construction-versus-later-set semantics;
native text model initial representation must not be inferred from overridden setter.
Check derived construction and initial dirty flag. Outgoing: base/LLSD/setValue.

## UI-MODEL-003: LLViewModel::setValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L52).
**1.** Assign LLSD then dirty=true even equal value. No signal invocation in body.
**2.** CPU model mutation. **3.** Preserve dirty semantics distinct from settings
notifications; prepared state may use generation after auditing consumers.
Check equal assignment/sharing and assignment failure. Outgoing: LLSD assignment.

## UI-MODEL-004: LLViewModel::getValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L58).
**1.** Return mValue as LLSD by value, no dirty reset. **2.** CPU snapshot query.
**3.** Audit LLSD value/copy-on-write semantics before cross-thread publication.
Check read preserves dirty. Outgoing: LLSD copy/lifetime.

## UI-MODEL-005: LLTextViewModel default constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L66).
**1.** LLViewModel(false),updateFromDisplay=false; string/display default empty,
displayGeneration header default-1. Base constructor sets LLSD bool false and dirty
true; does not invoke derived UTF8/display synchronization. **2.** CPU text model
initial state. **3.** Native initial value versus display representation must be
explicit, not assume default constructor equivalent to setValue('').
Check initial value/display/string/dirty/generation. Outgoing: base/LLSD/string defaults.

## UI-MODEL-006: LLTextViewModel(value) constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L73).
**1.** Base(value),updateFromDisplay=false; no derived setValue body invocation.
Stored LLSD supplied while display/string start empty, generation-1. **2.** CPU
initial representation policy. **3.** Audit actual caller initialization after
construction before claiming a text display bug or normalizing native behavior.
Check supplied nonempty value and subsequent explicit setValue. Outgoing: base/callers.

## UI-MODEL-007: LLTextViewModel::setValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L81).
**1.** Base setValue; string=value.asString; display=utf8str_to_wstring(string);
increment display generation; updateFromDisplay=false. No equality gate, callbacks
or GPU work locally. Failure midway can leave representations partially updated.
**2.** CPU text/model synchronization and layout invalidation. **3.** Native text
source indices and formatted display require versioned consistent representations;
share audited conversion policy rather than a different decoder silently.
Check same value, Unicode/invalid UTF8, non-string LLSD, generation wrap and failures.
Outgoing: LLSD/converters/string assignment and display consumers.

## UI-MODEL-008: LLTextViewModel::getEditableDisplay

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L92).
**1.** Dirty=true,increment generation,updateFromDisplay=true, return mutable wide
string reference. Actual mutation after return is not observed/incremented again.
**2.** CPU editing and deferred serialization. **3.** Native preparation cannot
publish this reference as immutable; snapshot at defined editing boundary.
Check no-op access, retained mutable reference and later model reads.
Outgoing: callers' mutation/lifetime and synchronization getter.

## UI-MODEL-009: LLTextViewModel::setDisplay

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L100).
**1.** Copy wide display,increment generation,dirty=true,updateFromDisplay=true.
Do not immediately update string or LLSD. **2.** CPU edit-buffer publication.
**3.** Explicit conversion boundary preserves editing efficiency and source indices;
GPU records consume copied glyph/layout data, not mutable display reference.
Check equal text still increments, dirty and delayed getValue synchronization.
Outgoing: wide-string assignment and consumers.

## UI-MODEL-010: updateFromDisplayIfNeeded

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L115).
**1.** If flag true, const_cast model, CLEAR flag first, convert wide display to
UTF8, assign string and LLSD via chained assignment. No generation/dirty change.
If conversion/assignment throws after flag clear, no local retry marker restoration.
**2.** CPU lazy serialization, including mutation through const query.
**3.** Native model snapshot creation must run on owner thread and publish coherent
value/display versions; const pointer is not thread-safety proof. Failure policy
must be explicit rather than inheriting a half-synchronized cache.
Check no-op repeated reads, conversion failure and retained display edits.
Outgoing: converter, string/LLSD assignment, actual callers and concurrency model.

## UI-MODEL-011: LLTextViewModel::getValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L130).
**1.** updateFromDisplayIfNeeded(this), return mValue. **2.** CPU serialized model
query with potential mutation. **3.** Owner-thread preparation before immutable
native publication, not concurrent recording-time reads. Check pending/no pending edits.
Outgoing: lazy helper and LLSD copy.

## UI-MODEL-012: LLTextViewModel::getStringValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L136).
**1.** Lazy update, return const string reference. **2.** CPU text access.
**3.** Native consumers retain copied/versioned text when owner can mutate/delete.
Check reference invalidation after edits and destructor. Outgoing: helper/lifetime.

## UI-TEXT-001: LLTextBase::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L1657).
**1.** reflow first; not read-only -> updateScrollFromCursor. With scroller, convert
its content-window rectangle to this control's coordinates. Without scroller, get
visible lines(mClipPartial), union their rects in order, using isEmpty to distinguish
first rectangle; translate by document view's left/bottom.
BGVisible: resolve current transparency, compute bg_rect=visibleTextRect and intersect
with text_rect if scroller; choose readOnly background else focused/writable background.
Actual gl_rect_2d draws TEXT_RECT, not computed bg_rect, with bgColor%alpha.
Searchable highlight: similarly computes optional-intersected bg_rect but draws
TEXT_RECT with highlight background color, without local alpha modulation.
shouldClip=mClip OR scroller!=null. Regardless shouldClip, if text_rect.top>2 subtract2
from top. Construct LLLocalClipRect(text_rect,shouldClip). Inside clip scope: drawChild
scroller if present else document view; drawHighlightedBackground; if highlightsDirty
refreshHighlights; nonempty highlights -> drawHighlightsBackground; drawSelectionBackground;
drawText; drawCursor. After clip scope, set document view visible-direct=false,
qualified LLUICtrl::draw, then visible-direct=true (not restore prior value).
No local visibility restoration guard after exceptions, no native snapshots and no
assumption reflow is purely computational. Child drawing precedes text/selection.
**2.** CPU native layout, scroll/highlight/cursor state preparation and explicit
ordered clipped contributions. GPU receives immutable glyph/image/decoration data.
**3.** Split preparation from execution while preserving the source ordering and
view policy; do not call GL-coupled segment/child draws. A snapshot of current text
without running required CPU mutations is incomplete. Background clip mismatch and
forced visible restoration need reference reachability tests, not silent correction.
Checks: scroller/no scroller, read-only/focus, top<=2/>2, mClip false, empty visible
lines, overlapping highlights/selection/text/cursor, document initial visibility,
reflow altering size and callbacks deleting child/view. Numeric/raster evidence pending.
Outgoing: reflow,scroll update,coordinate/visible-line/rect helpers,transparency/color,
search highlighting, clip stack, drawChild virtual targets, highlight refresh/draw,
selection/text/cursor, visible-direct/base draw and all retained/resource lifetimes.

## UI-TEXT-002: LLTextBase::reshape

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L1628).
**1.** Width/height changed OR global forceReshape: snapshot scroller isAtBottom if
present, base reshape; if scroller AND previously bottom AND trackEnd -> goToBottom.
Then updateRects before needsReflow (unconditional within branch). No change/no force
skips everything. Reflow remains deferred. **2.** CPU layout/scroll invalidation.
**3.** Native layout transaction preserves pre-reshape scroll anchoring and update
order; recording never calls reshape. Check shrink from top, trackEnd false, forced
same dimensions, scroller mutation in base callback and duplicate invalidation.
Outgoing: scroller state/scroll setter, base reshape and callbacks, updateRects,
needsReflow and global force policy.

## UI-TEXT-003: LLTextEditor::onCommit

Source: [lltexteditor.cpp](../../../indra/llui/lltexteditor.cpp#L2446).
**1.** setControlValue(getValue()) BEFORE qualified LLTextBase::onCommit. Text model
getValue can lazily serialize display; settings write can run callbacks before commit.
No local rollback/lifetime guard. **2.** CPU edit commit/settings/action flow.
**3.** Native text editing requires exact serialized value and ordered notifications,
not just native glyph output. Check pending display edit, settings callback mutation,
validation policy and destruction. Outgoing: model,getValue,setControlValue,base commit.

## UI-TEXT-004: LLTextEditor::setEnabled

Source: [lltexteditor.cpp](../../../indra/llui/lltexteditor.cpp#L2452).
**1.** readOnly=!enabled; if differs from mReadOnly, LLTextBase::setReadOnly then
updateSegments then updateAllowingLanguageInput. Does NOT invoke base enabled setter
or assign ordinary enabled flag locally. Equal read-only no-op.
**2.** CPU editability/style/IME policy. **3.** Native controls must preserve disabled
text editor as read-only interpretation, not generic disabled event suppression.
Check selection/copy/focus while read-only, segment rebuild, IME and repeated value.
Outgoing: readOnly setter,segment update/language input and caller enabled-chain logic.

## UI-TEXT-005: LLTextBase::reflow

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L1970).
**1.** Static cached FSFontLineSpacingAdjustment default0; ALWAYS updateSegments
before testing mReflowIndex==S32_MAX and returning. Pending reflow snapshots scroller
bottom status; cursor local rect and overlap with local bounds (follow_selection).
Convert saved cursor top/bottom relative to visibleTextRect.top. First visible line:
if in line list and scrollIndex not inside its [start,end), set scrollIndex to start.
Save scroll-index local rect in same top-relative coordinates.

While reflowIndex<S32_MAX: increment pass count; >2 logs debug/breaks, leaving
pending index for later invocation. Copy start index and reset reflowIndex=S32_MAX.
WordWrap -> reshape document to visible width/current document height, potentially
causing callbacks/invalidation. Initialize segment iterator at begin,segment offset0,
lineStart0,curTop0,lineCount0; availableWidth=visible width-mHPad,remaining=available.
If existing lines: upper_bound by line end against start index; found -> resume from
that line's start/logical line number/top, getSegmentAndOffset and erase suffix.
No found -> initial segment traversal remains while existing list is not erased.
lineHeight0,segmentLineOffset=lineCount+1.

For each segment: current index=start+offset; getNumChars(wordWrap?max(0,round(remaining)):
S32_MAX,segmentOffset,currentIndex-lineStart,S32_MAX,lineCount-segmentLineOffset).
getDimensionsF32(offset,count,width,height) returns forceNewline. lineHeight=max,
remaining-=segmentWidth,offset+=count. lastChar=start+offset. Actual width=ceil(available-
remaining), getLeftOffset(actualWidth), construct line rect(left,curTop,left+width,
curTop-lineHeight). If segment not exhausted: append line, new lineStart=lastChar,
curTop-=round(lineHeight*lineSpacingMult)+lineSpacingPixels+fontSpacingAdjustment,
reset remaining/height, keep segment and offset. If exhausted LAST segment: append,
advance curTop with same spacing, break before forceNewline lineCount increment.
Otherwise exhausted with more segments: forceNewline appends and resets line/spacing;
advance segment/reset offset; segmentLineOffset=forceNewline?lineCount+1:lineCount.
At iteration end increment logical lineCount only when forceNewline, not ordinary
word wrap. No local progress guard if a segment returns zero chars indefinitely.
After segment loop updateRects; invoke every segment's updateLayout(*this). These
can invalidate/reenter layout, producing another pass under the two-pass bound.

After passes: no mouse capture AND scroller -> previously at bottom AND trackEnd:
endOfDoc. Else hasSelection AND follow_selection: get new doc cursor rect, reconstruct
old saved cursor screen relation from new visibleTextRect.top, scrollToShowRect(new,old).
Else same anchoring for scrollIndex first-character rect. Finally updateCursorXPos.
No post-scroll reflow loop, callback lifetime/exception guard or document immutability.
**2.** CPU native line layout, inline controls and scroll/cursor anchoring. GPU
consumes settled layout versions; auxiliary views must not independently advance
editing/model state (NV-05/NV-12).
**3.** Preserve explicit phased bounded reflow with typed segment preparation and
stable control handles. Reuse audited neutral metrics/algorithms; no invocation of
GL-owning segment draws during recording. A generic paragraph engine is not equivalent
without source-index, logical-line and inline-element tests. Invalid no-progress
behavior needs deterministic error policy, not an infinite native loop.
Check suffix reflow, wrap versus forced newline logical numbering, inline resize
oscillation/pass limit, count0, actual-width rounding once, spacing setting, selection/
bottom/first-line anchoring, capture suppressing scroll and cursor-X reset.
Outgoing: cached setting,updateSegments,all cursor/line/segment lookup/rect helpers,
wordWrap/left offset,segment getNumChars/getDimensionsF32/updateLayout concrete targets,
document reshape/updateRects,endOfDoc/scrollToShowRect,clock-independent layout inputs.
Local body complete; all named transitive dependencies remain open.

## UI-TEXT-006: LLTextBase::drawCursor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L801).
**1.** Read draw-context alpha even if cursor hidden. Gate focus AND app focus AND
!readOnly. Get wide text/pointer,cursor local rect shifted x-1,segment containing
cursor; no segment returns. Read blink elapsed; visible when elapsed<CURSOR_FLASH_DELAY
OR (S32(elapsed*2)&1). Visible: overwrite mode AND no selection -> segment dimensions
for one character, width=max(CURSOR_THICKNESS,segmentWidth); else fixed thickness.
Unbind texture,set cursorColor%alpha,draw filled cursor rectangle. If overwrite,
no selection and current character!='\n': fetch segment color/style font and render
one character in rect with RGB complement of segment text color and alpha=draw context,
LEFT/current textVAlign,NORMAL,NO_SHADOW. No local terminator/index bounds checks.
Still in blink-visible branch: calcScreenRect; IME position=(screen.left+cursor.left,
screen.bottom+cursor.top), multiply UI scale each axis then S32 cast. Under LL_SDL2
add cached SDL2IMEDefaultVerticalOffset to Y. window.setLanguageTextInput(position).
No platform IME positioning on blink-hidden branch; no coordinate restore needed.
**2.** CPU cursor blink/overwrite appearance and platform IME positioning; native
GPU paints prepared cursor and optional inverted glyph under correct clip/depth.
**3.** Separate platform input update from GPU recording, preserving tested timing
or explicitly reviewing correction to blink-gated positioning. Snapshot text/segment
font and source indices before rendering; generic caret triangle is insufficient.
Check app/control focus, read-only, exact blink boundaries, overwrite newline/end,
selection, wide glyph, scale truncation, SDL2 offset and IME moves across reflow.
Outgoing: context/focus,model text,cursor/segment lookup,blink timer/constant,keyboard,
dimensions/style/font render,rect/color,screen conversion and platform window method.

## UI-TEXT-007: LLTextBase::drawText

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L884).
**1.** Get text length; <=0 and empty label returns. Else useLabel -> label wide
length. Selection bounds default-1; if selection set min/max regardless keyboard focus.
Get visible line range; empty returns. lineStart=first line start; containing segment
missing returns. Spellcheck enabled AND model wide length>2: compute range start=lineStart,
end=getLineEnd(lastLine) (lastLine is loop-exclusive below; helper interpretation open).
Only changed start/end recomputes ranges; text/version invalidation depends on callers.

Spell recomputation: clear misspell ranges, iterate segments from start. Null or
segment.start>=end breaks. Noneditable skips. Editable segment starts block at its
full start (not clamped to visible start), end=min(segment.end,rangeEnd); combine
successive editable segments without explicit adjacency equality check. Find first
alphabetic character while <full text length, not merely segment end. While wordStart<
segmentEnd, extend from start+1 over isPartOfWord OR apostrophe with alnum neighbors;
condition reads next character around apostrophe under string bounds obligations.
wordEnd>segmentEnd breaks. Valid substring bounds -> UTF8 conversion; UTF8 BYTE
length>=3 AND !checkSpelling -> append [wordStart,wordEnd). Next wordStart=wordEnd+1,
skip non-part-of-word up to segmentEnd. Store checked start/end after all segments.
Spell gate false clears ranges but does not reset checked start/end here.

Set current segment,misspell iterator=lower_bound ranges by pair(lineStart,0).
For each visible line: nextStart=-1,lineEnd=textLength; next line exists -> nextStart=
getLineStart(next),lineEnd=nextStart. Float rect from line,replace right with document
width then translate document left/bottom. SegmentStart=lineStart; while <lineEnd:
advance segments with end<=segmentStart; exhaustion warns/returns. segmentEnd=min(lineEnd,
current.end),clippedEnd=segmentEnd-current.start. If ellipses AND clippedEnd==lineEnd
(relative compared to absolute, as written) AND last visible line AND more lines
exist, subtract2 from rect.right to force ellipsis.

For misspell ranges overlapping current segment portion: if spell timer not expired
AND cursor within inclusive range, advance range iterator and skip. Otherwise clamp
range to portion. getDimensions for prefix and misspelled span; add integer textRect.left
to prefix. periods=(spanWidth+3)/6; start+=spanWidth/2-periods*3,end=start+periods*6.
baseline=int(textRect.bottom)+int(segment style font descender). Set color bytes
(255,0,0,200). While start+1<end: line(start,baseline,start+2,baseline-2); if start+3<end,
line(start+2,baseline-3,start+4,baseline-1); start+=4. No draw-context alpha applied
locally. If range extends past segmentEnd break without advancing it; else advance.
Then rect.left=currentSegment.draw(relative start,clippedEnd,ABSOLUTE selectionLeft/
Right,rect); segmentStart=clippedEnd+segment.start. After line lineStart=nextStart.
No state restoration or callback-safe segment-set mutation guard in body.
**2.** CPU visible text/spellcheck range maintenance and prepared underline/glyph/image
segments with exact source indices/painter order. Native recording does not run
spellchecking or virtual GL draw methods.
**3.** Use typed segment preparation returning advance and ordered contributions,
with model/segment generations and owner-thread spell service. Preserve selection
index domains and byte-length spell eligibility; replacing with uniformly shaped
plain strings loses inline/style/action behavior. Cache-range quirks require actual
mutation-path tests before native invalidation design is finalized.
Checks: label mode,empty lines,selection without focus,editable/noneditable blocks,
Unicode three-byte words/apostrophes,range unchanged after edit,misspelling across
segment/line boundary,timer skip,relative-versus-absolute ellipsis condition,underlines
before content and segment draw return advance. GPU/numeric parity unmeasured.
Outgoing: model/label/selection,visible-line helpers,segment lookup/dimensions/draw,
spell timer/checker/UTF8 classification/conversion,style/font metrics,GL line/color,
all concrete segment callbacks and invalidation writers.

## UI-SEGMENT-001: LLNormalTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4213).
**1.** end-start>0 -> drawClippedSegment(segment.start+start,segment.start+end,
selection bounds,rect). Otherwise reset pre/selection/post font buffers and width
buffer, return rect.left. Selection bounds are not translated here.
**2.** CPU text-span preparation with absolute selection indices. **3.** Native
segment interface must make source domains explicit and invalidate empty spans;
do not keep stale retained glyphs for zero-length segments.
Check positive/zero/negative span and absolute selection. Outgoing: clipped draw/reset.

## UI-SEGMENT-002: LLNormalTextSegment::drawClippedSegment

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4230).
**1.** rightX=rect.left; invisible style returns before generation/cache update.
Read current draw-context alpha,text and editor text generation. Changed generation:
store new generation,reset three font buffers and width buffer. Font from style;
ordinary color=(readOnly?style readOnlyColor:style color) % (contextAlpha*styleAlpha).
useFontBuffers decides cached versus direct rendering for EACH span, with same inputs.
Before-selection when selectionStart>segmentStart: [segmentStart,min(selectionStart,
segmentEnd)). Font render LEFT/editor VAlign,NORMAL,style shadow,span length,rightX,
editor ellipses/color flags. Update rect.left to returned rightX.
Selection overlap when selectionStart<segmentEnd AND selectionEnd>segmentStart:
[max(selectionStart,segmentStart),min(selectionEnd,segmentEnd)), style selectedColor.get
DIRECTLY, NO context/style-alpha multiplication, NO_SHADOW. Otherwise same font/flags.
Update rect.left. After-selection when selectionEnd<segmentEnd:
[max(selectionEnd,segmentStart),segmentEnd), ordinary color/style shadow and same inputs.
Return rightX. With selection bounds -1/-1, only after-selection covers ordinary span.
No local clipping scope, geometry rollback or rightX availability guarantee beyond
font return contract. Cached buffers record rightX on generation path here.
**2.** CPU ordered span/style selection and glyph preparation, with separate selected
color semantics. **3.** Preserve span boundaries/alpha/shadow per contribution in
native prepared runs; one multiplied parent tint for all text is incorrect. Content
generation invalidates this segment's buffers, but style/ellipsis flag changes need
their own caller closure. Direct GL font rendering is not neutral preparation.
Check selection before/inside/after/absent, style invisible then visible, changed
generation, cached/direct branches, selected opacity and rightX across three spans.
Outgoing: style/getWText/editor generation,useFontBuffers,font render/buffers,
color/alpha,ellipsis/color setters and cache reset callers.

## UI-SEGMENT-003: LLNormalTextSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4526).
**1.** width=height=0. numChars>0 AND start+firstChar>=0: height=cached fontHeight,
text=getWText,font=style font,width+=widthBuffer.getWidth(font,text,start+firstChar,
numChars,noPadding=true). Always return false (does not force newline).
**2.** CPU span metrics. **3.** Preserve advance-only width versus render overhang;
native measurement must not publish GPU glyph pages synchronously.
Check zero/negative ranges,noPadding,height cache versus changed font and generation.
Outgoing: text/style,font height initialization,width buffer and caller invalidation.

## UI-SEGMENT-004: LLNormalTextSegment::getOffset

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4542).
**1.** style font.charFromPixelOffset(text.c_str,segmentStart+startOffset,float(localX),
F32_MAX,numChars,round). **2.** CPU caret/picking index. **3.** Native text layout must
preserve source-index and midpoint rounding policy, not add GPU text picking.
Check local x,negative/end bounds,round flag and Unicode. Outgoing: font hit-test helper.

## UI-SEGMENT-005: LLNormalTextSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4552).
**1.** Get text; optional style image subtracts live image width from pixel budget,
clamped>=0. startOffset=segmentStart+segmentOffset; maxChars=min(input,segmentEnd-
startOffset). Wrap mode=lineOffset0?WORD_BOUNDARY_IF_POSSIBLE:ONLY_WORD_BOUNDARIES.
Compute textLength-startOffset; inconsistent bounds log info but do not abort/clamp.
font.maxDrawableChars(text.c_str+startOffset,float(budget),maxChars,wrapMode).
If result0 AND lineOffset0 AND maxChars>0 force1 character. lastInRun=startOffset+count;
if lastInRun<segmentEnd AND lastInRun>=getLength(),increment count for EOF marker.
line_ind unused. No null/style/index/negative pointer guard in body.
**2.** CPU line-fitting/source-progress contract. **3.** Native layout preserves
first-line-character progress and EOF sentinel independently of rendering. Whole-text
replacement with generic wrapping needs exact boundary/source-index checks.
Check too-wide first glyph,remaining line word boundary,style image budget,EOF marker,
negative offset and maxChars; invalid memory access is not native compatibility.
Outgoing: text/style/image/font maxDrawableChars,getLength,wrap enums and diagnostics.

## UI-SEGMENT-006: LLNormalTextSegment::updateLayout

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4626).
**1.** Base updateLayout(editor), reset all three font buffers and width buffer.
No font-height recomputation in this body. **2.** CPU layout/cache invalidation.
**3.** Native prepared generations must include geometry invalidation, not assume
cached metrics always survive reflow. Check repeated reflow and style change.
Outgoing: base helper,buffer resets,fontHeight writers.

## UI-SEGMENT-007: LLNormalTextSegment destructor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4207).
**1.** Disconnect image-loaded connection; then member/base destruction. This is
explicitly stronger connection ownership than the flash timer's raw callbacks.
**2.** CPU subscription/retained text release. **3.** Native image/font callbacks
must use equivalent owned cancellation and completed-use GPU retirement separately.
Check loaded callback during teardown,last style owner and buffers in flight.
Outgoing: connection semantics,callback installers,style/buffer/base destructors.

## UI-SEGMENT-008: LLOnHoverChangeableTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4739).
**1.** Base normal draw first; if end==segmentEnd-segmentStart reset style to normal;
return base advance. No local cache reset with style assignment. **2.** CPU transient
hover-style lifecycle plus prepared glyph output. **3.** Native preparation must
consume current hovered style then perform source-equivalent reset at final portion,
not retain hover style indefinitely or reset before drawing.
Check multiline segment,last versus partial portion,repeated views and font changes.
Outgoing: normal draw,style ownership,hover caller and invalidation.

## UI-SEGMENT-009: LLOnHoverChangeableTextSegment::handleHover

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4751).
**1.** style=editor.skipLinkUnderline?normal:hovered; then return normal-segment
handleHover. Style changes even if base hit/link test ultimately false.
**2.** CPU pointer-driven text style. **3.** Preserve event-to-preparation ordering
and link eligibility separately; no GPU callback. Check skip-underlining and miss.
Outgoing: editor setting,normal hover,style lifetime/cached font metrics.

## UI-SEGMENT-010: LLInlineViewSegment constructor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4762).
**1.** Base(start,end),view pointer,newline flag/four pads from Params; permitsEmoji=false.
**2.** CPU inline control identity/layout. **3.** Native prepared text must carry
embedded control ownership and document child ordering, not encode as glyph.
Check view null precondition,pads/flags and base segment range.
Outgoing: Params,view owner,base ctor and document linkage.

## UI-SEGMENT-011: LLInlineViewSegment destructor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4774).
**1.** view->die(), no null check or explicit delete here. **2.** CPU inline child
deferred destruction. **3.** Audit die queue/handle invalidation before native owner
reuse; not assume immediate deletion. Check linked/unlinked child and callbacks.
Outgoing: concrete view die/destruction queue,base/member dtors.

## UI-SEGMENT-012: LLInlineViewSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4786).
**1.** firstChar0 AND numChars0: width0; forceNewLine -> default style font lineHeight,
return true; otherwise height0. All other inputs -> width=pads+viewWidth,height=pads+
viewHeight. Return false outside forced-empty branch. Default font lookup can create
GL owners while measuring an inline element with no text.
**2.** CPU inline metrics/newline policy and audited default font metrics.
**3.** Neutral metrics service plus explicit child layout; no GL-font measurement
facade. Check empty forced line,partial segment,padding and missing view/font.
Outgoing: LLStyle default font/lineHeight,view rect and Params semantics.

## UI-SEGMENT-013: LLInlineViewSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4816).
**1.** forceNewLine AND line_ind0 ->0. Else lineOffset!=0 AND pixels<padded viewWidth
->0. Else return segmentEnd-segmentStart, ignoring segmentOffset/maxChars. First
item fits regardless width unless forced-line branch. **2.** CPU indivisible inline
element wrapping. **3.** Preserve logical line_ind versus current-line offset meaning.
Check equality width,oversize first item,forced-newline and nonzero segment offset.
Outgoing: view dimensions,range/line counters from reflow.

## UI-SEGMENT-014: LLInlineViewSegment::updateLayout

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4835).
**1.** editor.getDocRectFromDocIndex(start); view.setOrigin(rect.left+leftPad,
rect.bottom+bottomPad). **2.** CPU document child placement. **3.** Native inline
child origin settles before rendering/picking snapshot; preserve document coordinate
domain. Check scrolled document,baseline/padding and invalidation callbacks.
Outgoing: index-to-rect,view origin setter and child ownership.

## UI-SEGMENT-015: LLInlineViewSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4841).
**1.** Return rect.left+viewWidth+leftPad+rightPad. Does NOT draw view; start/end/
selection ignored. Child is drawn through document view earlier in parent text draw.
**2.** CPU text advance only; native child contributes via prepared document order.
**3.** Avoid duplicate embedded child emission or moving it after text merely because
it appears in segment sequence. Check child painter order and empty span.
Outgoing: viewWidth,document child traversal.

## UI-SEGMENT-016: LLInlineViewSegment::linkToDocument

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4853).
**1.** editor.addDocumentChild(view). **2.** CPU tree registration. **3.** Native
ownership must preserve document versus control parent and draw order.
Check already-parented view and failure. Outgoing: addDocumentChild/reparent callbacks.

## UI-SEGMENT-017: LLInlineViewSegment::unlinkFromDocument

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4848).
**1.** editor.removeDocumentChild(view), no delete here. **2.** CPU detachment.
**3.** Separate removal from destructor die and GPU retirement. Check detached view
lifetime and active input capture. Outgoing: removeDocumentChild/callbacks.

## UI-SEGMENT-018: LLLineBreakTextSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4877).
**1.** width0,height=cached fontHeight,return true regardless requested span.
**2.** CPU explicit newline metric. **3.** Native line-break node preserves empty
line height and logical source unit rather than omitting invisible content.
Check zero span and cached font changes. Outgoing: fontHeight constructors/writers.

## UI-SEGMENT-019: LLLineBreakTextSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4884).
**1.** Return1 regardless budget/offset/maxChars/line. **2.** CPU newline consumption.
**3.** Explicit source sentinel handling, not glyph fit. Check reflow progress/bounds.
Outgoing: segment [pos,pos+1) constructor and caller ranges.

## UI-SEGMENT-020: LLLineBreakTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4888).
**1.** Return rect.left, no graphics. **2.** CPU zero visual advance. **3.** Preserve
newline layout/selection without manufacturing a visible glyph.
Check selection crossing newline. Outgoing: none in body,selection background caller.

## UI-SEGMENT-021: LLImageTextSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4914).
**1.** width0,height=style.font.lineHeight; style image pointer. numChars>0 AND
image nonnull -> width=imageWidth+IMAGE_HPAD3,height=max(fontHeight,imageHeight+3).
Return false. **2.** CPU image-as-text-unit metrics. **3.** Native inline image needs
logical extent/readiness independent of font atlas allocation.
Check missing image,zero chars,oversize image and style font. Outgoing: style/font/image.

## UI-SEGMENT-022: LLImageTextSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4928).
**1.** Null image ->1. Else lineOffset0 OR numPixels>imageWidth+3 ->1, else0.
Strict > means exact fit on nonempty line rejects. Other parameters ignored.
**2.** CPU indivisible inline image wrapping. **3.** Preserve exact threshold and
missing-image consumption; zero geometry is not absent source unit.
Check first item,exact boundary and unavailable image. Outgoing: style/image width.

## UI-SEGMENT-023: LLImageTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4962).
**1.** start>=0 AND end<=segmentLength: color=white%editor context alpha,style image.
Nonnull -> get height then width; textCenter=S32(rect.top-rect.height/2.f),imageBottom=
textCenter-imageHeight/2 integer division; image.draw(S32(rect.left),bottom,width,
height,color),return rect.left+imageWidth+3. Otherwise return0, NOT rect.left.
No end>start test; no style color/alpha/visibility or selection behavior here.
**2.** CPU inline image geometry/advance and native sampled image contribution.
**3.** Keep per-segment color/advance contract distinct from normal text; actual
missing-image return can reset next segment position and needs reference testing.
Check empty span,missing image,negative start,odd dimensions,selection and alpha.
Outgoing: style/image/context,rect math and image draw/provider/lifetime.

## UI-TEXT-008: LLTextBase::drawSelectionBackground

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L668).
**1.** Only hasSelection AND nonempty line info: getSelectionRects,unbind texture,
read selectedBGColor RGB; alpha=(focused?FOCUSED_SELECTION_BG_ALPHA:UNFOCUSED_SELECTION_BG_ALPHA)
*drawContextAlpha, ignoring selectedBGColor alpha. Get visibleDocumentRect.
Each selection rect: scroller -> translate(visible.left-content.left,
visible.bottom-content.bottom). No scroller: vDelta0,hDelta0; TOP v=visible.top-
content.top-vPad; VCENTER v=(max(visible.height-content.top,-content.bottom)+
visible.bottom-content.bottom)/2; BOTTOM v=visible.bottom-content.bottom; other0.
LEFT h=visible.left-content.left+hPad; HCENTER h=(max(visible.width-content.left,
-content.right)+visible.right-content.right)/2; RIGHT h=visible.right-content.right;
other0. Integer arithmetic/division. Translate h/v,draw rect with selection color.
No local scissor setup; caller text draw owns clip. **2.** CPU selection geometry
and typed opacity with native solid material. **3.** Preserve focus-opacity replacement,
alignment-specific positioning and draw order; don't reuse ordinary text tint alpha.
Check unfocused search selection,configured alpha ignored,all alignments,scroller,
odd division and empty rects. Outgoing: selection rect generation,focus/constants,
context,color/rect,visibleDocumentRect and GL solid primitive.

## UI-TEXT-009: LLTextBase::drawHighlightedBackground

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L737).
**1.** Empty line info skips. getHighlightedBgRects; empty result returns before
texture unbind. Unbind,getVisibleDocumentRect. For each rect+LLUIColor: scroller
translation=(visible.left-content.left,visible.bottom-content.bottom). Otherwise
vDelta0,hDelta0; TOP v=visible.top-content.top-vPad; VCENTER v=(max(visible.height-
content.top,-content.bottom)+visible.bottom-content.bottom)/2; BOTTOM v=visible.bottom-
content.bottom. LEFT h=visible.left-content.left+hPad; HCENTER h=(max(visible.width-
content.left,-content.right)+visible.right-content.right)/2; RIGHT h=visible.right-
content.right; unknown alignments retain0. Translate,draw with provided color directly,
no context opacity/focus replacement. **2.** CPU style-highlight geometry/color.
**3.** Native contribution keeps distinct opacity policy from selection; shared
geometry calculation is possible only with all arithmetic/alignment inputs preserved.
Check transparent highlight under fading parent,all alignment modes,scroller and
multiple ordered colors. Outgoing: highlighted rectangle/style generation,LLUIColor
live value,visible-document/rect helpers and primitive caller clip.

## Executed CPU probes

Date: 2026-09-10. Source under test remains the unchanged checkpoint-derived
llinitparam/llheteromap implementation. Test harness uses working branch changes to
[llcommon/CMakeLists.txt](../../../indra/llcommon/CMakeLists.txt) only for registration.
NV-00 bounded investigation; NV-17 CPU runtime evidence; NV-18 observed quirks are
NOT native compatibility mandates and no GL oracle/tolerance has been changed.

[llinitparam_test.cpp](../../../indra/llcommon/tests/llinitparam_test.cpp) contains:

- test1: eight source-provided/destination-provided/overwrite combinations, equal-value
  merge result and base-qualified no-op fill. All passed against real parameter APIs.
- test2: first descriptor construction versus subsequent fresh instance; copied
  block choice ownership; choose versus provided; switching exposes original value.
  All passed. This does not establish reachability in every actual UI Params type.
- test3: missing mandatory parameter, valid provided value, false-provided notification
  retaining cached success, explicit invalidation restoring validation failure. Passed.
  This characterizes current caching, not an instruction to reproduce stale validation.

After loading the established LL_BUILD environment, configured the working build
with `cmake -S indra -B build-vc170-64 -DLL_TESTS=ON` and built targets using
`cmake --build build-vc170-64 --config RelWithDebInfo --target INTEGRATION_TEST_llheteromap`
and `INTEGRATION_TEST_llinitparam`. Repository POST_BUILD harness actually executed
tests: heterogeneous cache1/1 passed; parameter probes3/3 passed. These test executables
are test infrastructure, not additional viewer executables or backend launchers.
Scope still includes remaining callbacks, native design/implementation and qualification.

Additional runtime checks in the same working build:
`INTEGRATION_TEST_llframetimer` passed4/4, including the added
[stop/reset state test](../../../indra/llcommon/tests/llframetimer_test.cpp).
`INTEGRATION_TEST_llinstancetracker` passed8/8 using its existing
[snapshot deletion tests](../../../indra/llcommon/tests/llinstancetracker_test.cpp).
These exercise actual repository implementations, not independent algorithm copies.
They do not verify LLFlashTimer settings-signal disconnection or button callback
deletion safety, and do not validate native GPU ownership.

## Existing cache test

[llheteromap_test.cpp](../../../indra/llcommon/tests/llheteromap_test.cpp#L123)
test1 uses three unrelated types, mutates/retrieves names, verifies construction
order and an unordered set of destructor effects. It does not test constructor or
emplace failures, same-type recursion, concurrency, dependent destructors or any
actual UI Params/font owners. Its result cannot close those edges.

## Coverage boundary

Registered locate target is locally identified; neither complete default-child
registry enumeration nor custom panel/floater/factory callback closure is claimed.
Next controlling dependency is parameter fill/validation and type-specific builders,
not a speculative generic native widget class. Documentation checks validate links
and identifiers only; runtime construction/effect tests are still required.