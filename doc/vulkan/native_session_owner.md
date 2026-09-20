# Native session integration

## Enter and default-button routing (2026-09-17)

NV-00/01/12/17: reference FSPanelLogin assigns Connect as the default for
the login and start-location panels and separately commits password_edit to
onClickConnect. LLLineEditor::handleSpecialKey records Return history but leaves
the key unhandled; LLView propagates it to parents, and LLPanel::handleKeyHere
honors an enabled visible default before its text-input commit fallback.
The native window handler previously discarded that unhandled Return and selected
a possibly unrelated active floater as its dispatch root.

Native login now assigns those defaults and the password commit callback. The
window preserves an editor's handled result and routes unhandled Return along
actual keyboard-focus ancestry, selecting the nearest declared panel default
before editor fallback. Modifiers do not activate defaults. Existing modal notice
delay/default handling, browser input ownership and multiline newline handling
remain separate. Disabled or hidden defaults are not activated. Callbacks may
destroy their own dialog without a subsequent node dereference. No GL code or
GPU resource ownership is changed.

Widget210/210 covers password Enter starting the owner, modified Return,
nested defaults, disabled-default fallback and self-closing callbacks. Window7/7
passes with WM_KEYDOWN/WM_KEYUP Return initiating synthetic login after focus
leaves Preferences, followed by connected input, resize and orderly teardown.
Earlier failed fixture runs exposed the stale active-floater dispatch root and
required normal cancellation of a file picker and acknowledgement of an unrelated
alert before cleanup; they are not passing evidence. This verification does not
qualify every dialog's default assignment or connected visual parity.

## Incoming IM receive investigation (2026-09-17)

After checkpoint8e2b68f879, the operator reports that the native connected viewer
does not receive IMs. Region connection is verified separately; incoming live
messaging is a failed acceptance gate. The interim connected UI is not accepted
as OpenGL parity. Friends/account presence and the original Contacts,
Conversations, menu, address and favourites surfaces remain required work.

NV-00/01/15/17: source roots inspected are LLMessageSystem::zeroCodeExpand,
LLTemplateMessageBuilder::buildMessage/compressMessage,
LLEventPollImpl::handleMessage, LLSDMessageReader's field readers and
process_improved_im. The reference supports zero-run continuation bytes and
dispatches named event-queue messages as well as UDP messages. Native code must
decode these representations into its own bounded message records without
calling the GL-owned handlers. This is CPU protocol and publication work;
Vulkan resources, visual assets and GPU synchronization are unchanged.

The native packet decoder now accepts bounded zero-run continuations: a zero
count contributes256 zeros followed by another count byte. The fixture starts
with a reference-compressed IM and replaces two255-zero runs with an equivalent
256+254 continuation, checking the complete600-byte bucket at the queue consumer.
Structured ImprovedInstantMessage event bodies now validate single AgentData and
MessageBlock arrays, UUIDs, text, flags, position, binary bucket and the reference
network-order four-byte timestamp before joining the native incoming queue.
The TLS fixture delivers an unsolicited structured IM and checks exactly-once
transport consumption. Binary-template event framing is not yet implemented;
that route remains retained under the existing event budget, not claimed covered.

Content-free, one-time-per-connection diagnostic stages distinguish UDP IM
arrival/decode/rejection, structured-event arrival/rejection, transport publication,
conversation receipt and transcript display. No message body, name, UUID,
credential or capability URL is written by these markers. Transcript display
means native text-control publication, not measured visual parity or a delivery
receipt sent to another resident.

The native friend-data foundation now reads buddy IDs and granted/held permission
masks from login, matching LLStartUp's buddy-list construction. Initial online=false
is retained, with a separate presenceReceived flag so it is not evidence of a
server-reported offline notification. OnlineNotification322 and
OfflineNotification323 are decoded into bounded ordered circuit queues, applied
only to existing friends, exposed only for the current connected tag and cleared
on shutdown. LLIMModel's reference delivery policy now determines the direct IM
offline flag from that friend data; nonfriends and typing retain their previous
flags. Source roots include LLAvatarTracker::processNotify and deliverMessage.
The integrated TLS/UDP fixture checks permissions, notification order and the
offline message flag. Unknown-buddy reconciliation, friendship/rights updates,
structured presence events, account-status controls and the actual Friends view
remain open. This data foundation is not a completed presence workflow.

Protocol8/8 and widget210/210 pass after these changes. The observed live failure
is not yet attributed to a specific transport representation or UI branch; these
source-backed corrections must not be presented as a proven live fix. Next live
acceptance uses ladyanamarques in the isolated native profile and cooperating
Anne Skydancer, with no automatic sends or credential capture. No previous passed
authentication evidence needs to be repeated for reassurance.

## Fresh-profile login proxy gate (2026-09-17)

Isolated viewer27704 reached native startup and created its own cache while the
installed viewer remained running. The operator subsequently reported code1012
(OperationFailed/window). Its log had no authorization marker; normal exit0 and
Goodbye! were verified. The generic notice alone does not identify the operation.

Source inspection found a deterministic fresh-profile login blocker: shipped
HttpProxyType is Socks while Socks5ProxyEnabled is false. Native prepareLogin
rejected every non-None type. Reference LLStartUp::startLLProxy instead disables
HTTP proxying and normalizes a disabled selection to None without failing login.
NV-00/01/15/17: the native CPU-only login gate now checks effective enablement.
Disabled known selections permit direct login; enabled SOCKS (including UDP) or
selected/enabled HTTP proxies remain explicitly unsupported, and unknown types
are rejected. General LLVKProxy::select behavior and OpenGL are unchanged.

Widget210/210 covers disabled defaults, active HTTP/SOCKS containment and invalid
types. Startup compilation passes. Fixed login-prepare stage labels now identify
grid/proxy/input/transport rejection without writing credentials or user text.
Live login after this correction remains unverified; the prior generic notice
is consistent with this defect but lacks branch-specific evidence.

## Explicit isolated test profile (2026-09-17)

The operator approved a separate native test profile after simultaneous installed
and native viewers encountered cache ownership failure. Reference
LLAppViewer::initCache uses mSecondInstance for read-only access. Native startup
instead retains its exclusive execution-marker and cache locks; this change is
an explicit test-isolation extension, not second-instance parity or a lock bypass.
NV-00/01/03/15/17 apply; OpenGL startup is untouched.

Launch with `--set RenderBackend Vulkan --native-profile ladyanamarques`.
The name is 1-48 lowercase ASCII letters, digits, hyphens or underscores.
Settings, logs, marker, native browser storage and profile assets are rooted at
`%APPDATA%/Vulkanstorm_x64/native_profiles/profile-ladyanamarques`.
The default texture and sound cache root is the corresponding path under
`%LOCALAPPDATA%`. Explicit Vulkan selection is required. Linked profile paths,
duplicate/missing profile arguments and nonlocal settings-file arguments fail.
Without this option, existing profile selection is unchanged.

Native CPU-only path planning uses std::filesystem and Windows reparse-point
inspection before profile IO. No settings or credentials are copied from the
ordinary profile. Cache-location overrides are cleared for this test mode before
cache planning so installed-viewer cache preferences cannot redirect startup
back to shared storage. Profile-local preferences persist normally. The native
cache still rejects two writers selecting the same isolated profile. No GPU
ownership or visual behavior is changed.

Widget 210/210 includes deterministic distinct-root, invalid-name, relative-root
and side-effect-free planning checks; startup library compilation passes. Existing
cache tests cover exclusive writer conflict. Actual simultaneous installed/native
startup is pending the isolated operator run. Full window tests are deliberately
not run while the cooperating live viewer is open. This does not complete live
messaging or connected UI parity acceptance.

## Connected text services in progress (2026-09-17)

The native-sl-login working tree now transitions to a native-owned connected
text workspace. This supersedes the missing-transition observation in the
historical run2496 record below; it does not extend that run's live evidence.
Local chat, direct IM, avatar-name search and group text sessions have synthetic
consumer tests. No live bidirectional messaging or connected visual parity has
been qualified. The current three-column plain-text workspace is an interim
consumer, not the reference conversation/history UI or a completed transposition.

### Source contract and ownership

NV-00/01/03/09/12/15/17 apply. The pinned reference remains
59108e15a1f8f94d2da7c674d937d19f5cf9450d; current source roots inspected are
LLIMModel::sendTypingState/deliverMessage/sendStartSession,
LLIMMgr::computeSessionID, pack_instant_message, process_chat_from_simulator,
LLFloaterAvatarPicker::find/findByNameCoro/processResponse,
LLAgentGroupDataUpdateViewerNode::post, the ChatterBox HTTP handlers,
LLIMSpeakerMgr::setSpeakers/updateSpeakers/allowTextChat/moderationActionCoro,
and FSFloaterIM's typing callbacks/timedUpdate. The message template independently
defines local/IM wire field order and encoding. Full policy/helper closure remains
open; the inspected roots do not establish exhaustive source parity.

The source routes local chat by simulator audibility and source/owner identity,
direct IM by avatar-derived session identity, and group chat by membership and
server-established session state. Group invitations use HTTPS acceptance;
participant updates can precede the initialization reply. Moderator text requests
carry an explicit mute boolean and report HTTP failure without treating it as
session closure. The server's participant updates remain authoritative.

Native CPU-only protocol and session owners now produce bounded, tagged data for
native widget controls. They do not invoke GL viewer UI, message dispatch or
speaker callbacks. Boost endian/URL/Asio, libcurl and existing nonvisual LLSD/UUID
utilities handle encoding and transport. Rendering still uses native widget
preparation, immutable paint packets and the existing Vulkan resource owners;
these service edits add no GPU resource or synchronization mechanism.

- LLVKChatProtocol validates local/IM payloads, membership powers, participant
   updates and invitation data. Local/IM encoding is compared with the independent
   reference template builder. Direct session IDs retain XOR and self semantics.
- LLVKLoginTransport keeps credentials, session secrets and capabilities private.
   Public communication operations require the current owner tag and connection.
   Search replaces its prior HTTP request and publishes the caller's query ID.
   Group membership, join/leave, pending participant updates and invitation
   acceptance have bounded native storage and explicit state.
- Moderation uses one concurrent HTTPS request, requires current server-reported
   moderator authority, and publishes completion only for the matching group
   version. Leave/reconnect invalidate old completion. HTTP 2xx includes empty 204
   success; payload-requiring consumers still validate their decoded bodies.
   A 403 reports permission denial without locally changing mute state or closing
   the group session. No moderation request targets voice.
- Native UI separates local, direct and group transcripts, drafts and send
   controls. Local text waits for simulator echo. IM/group local echoes do not
   claim delivery or read receipts. Search results and moderator actions retain
   UUID identity. Disconnect clears retained text and recipient controls.

### Focus and validation

The real-window connected probe exposed null-focus transfer while an unrelated
modal owned the focus lock. Session progress is dismissed before transition;
unrelated alerts retain their modal ownership and return focus to the connected
composer on acknowledgement. The widget regression checks retained focus,
exactly-once response and return focus using an existing native-catalog alert.
GenericAlert is enqueued internally, not available through queueNotice's catalog.

Focused results: protocol 6/6 includes TLS search, UDP group join/send/leave,
invitation acceptance, pre-reply participant ordering, moderator commands,
204 success, 403 denial and stale completion after leave/rejoin. Widget 210/210
includes transcript/composer routing, query isolation, typing timers, moderator
controls, disconnect clearing and unrelated-modal handoff. GPU 10/10 and startup
library compilation pass. Existing LNK4020 PDB warnings remain a debugger limit.
Window 7/7 now passes with the moderator controls present, covering synthetic
connection, physical local-send input, resize/restore and graceful cleanup.
The browser sequence's existing 90-second budget now starts at its first
presented frame, excluding preceding independent fixtures and service setup;
the connected phase retains its separate 15-second budget. Earlier failures
and intermittent CEF navigation timeouts remain retained evidence, not all
reclassified as the fixed modal-focus defect. This window test does not exercise
live moderation or establish visual parity of the connected controls.

### Remaining gates

Live local/IM/group bidirectional messaging, moderator workflows and reconnect
acceptance remain required. Preserve run2496's network evidence; it is not a
messaging test. Exact original chat/history/editor UI, localized text, complete
name resolution, rich attribution/styles, transcript scroll behavior, account
preferences/history, complete mute/friend/autoresponse policy, and all session
error/permission transitions remain open. Unknown event-queue entries are still
retained under a finite budget rather than dispatched to GL services; a full
retention queue fails explicitly and is not a completed consumer implementation.
Repeated same-ID group join replies need further ordering qualification.
OpenSim, voice and WebGL remain separately deferred; no world rendering is claimed.

## Live network milestone verified (2026-09-17)

On branch native-sl-login, the corrected viewer's operator-driven run2496 logged
authorized, connection-started, seed-ready and region-connected. A subsequent
75-second uninterrupted observation recorded no connection failure. WM_CLOSE
was accepted, the retained process handle reported exit0, and the per-process
native-session-2496.log recorded Goodbye! with status0. Credentials were entered
directly in the viewer; no debugger or forced termination was used.

This verifies the bounded live Second Life authentication, initial region
connection, dwell and graceful shutdown workflow. It does not establish all
login variants, reconnect behavior, authenticated service completeness or world
rendering. The login screen still remains visible after connection: refreshSession
does not yet transition to a connected UI. This missing feedback is a known UI
gap, not evidence that this run failed authentication. Preserve this passed
operator evidence rather than repeating it solely to reconfirm the milestone.

## Live seed-validation correction (2026-09-17)

The operator's diagnostic attempt in viewer26812 recorded authorized,
connection-started, then seed-capability-url-invalid. This establishes live
authorization and receipt of a decoded seed response with an HTTPS EventQueueGet
endpoint; it does not establish a completed region connection. The failing code
required every other capability entry to be an HTTPS string, even though no
native consumer used those entries. The fixed-label log deliberately contains
no capability values, so the specific rejected optional value is unknown.

NV-00/01/15/17: reference LLViewerRegion's seed-response loop calls setCapability
per entry rather than globally requiring HTTPS for every advertised capability.
The native connection now validates its actual EventQueueGet consumer strictly
as an HTTPS string and retains other entries as unused data. Future consumers
must validate their endpoint before use; this change does not enable HTTP login,
HTTP event polling, automatic redirects or unvalidated optional requests.

The existing TLS/UDP fixture now returns an optional HTTP GetTexture endpoint
and empty GetMesh value alongside its HTTPS EventQueueGet. Protocol4/4 passes,
including connection, keepalive and logout; editor diagnostics and diff hygiene
pass. Production relinking subsequently passed, and run2496 completed the live
connection and75-second dwell recorded above.

## Second Life transport implementation in progress (2026-09-17)

The user prioritized native Second Life authentication and region connection
ahead of the remaining prelogin UI parity tasks. Work starts from merged PR44,
master035b1fdb8d. OpenSim and world rendering are not included in this slice.
This is an initial implementation checkpoint with the bounded live network
acceptance above, not complete login/UI parity.

### Source contract and native design

NV-00/01/03/09/12/14/15/17 apply. The reference roots are
LLLoginInstance::constructAuthParams/handleLoginFailure/handleMFAChallenge,
LLCredential::getLoginParams, FSPanelLogin credential transformation,
LLLogin::Impl::loginCoro, LLXMLRPCTransaction, LLStartUp's UseCircuitCode and
AgentMovementComplete stages, LLViewerRegion capability setup, LLEventPoll and
scripts/messages/message_template.msg. Authentication success is not simulator
readiness. Credentials, circuit/session identities and capability URLs must not
enter user diagnostics or synthetic success states.

The native implementation has four independently owned components:

- LLVKLoginProtocol serializes login_to_simulator XML-RPC and validates the
   authorization bootstrap: agent/session/secure-session UUIDs, unsigned circuit
   code, simulator IPv4/port, region handle and HTTPS seed capability. Signed
   XML-RPC circuit integers preserve their32-bit bit pattern. Expat preflight
   rejects DTDs, excessive depth/node count and oversized replies before tree
   decoding. Existing LLSD/XML serialization and parsing are nonvisual data
   utilities; no GL login/UI callback is reused.
- LLVKLoginHttp owns libcurl multi/easy handles, request buffers and response
   bounds. Production requests require HTTPS with peer/hostname verification,
   explicit CA bundle and no automatic HTTP redirects. Cancellation removes the
   easy handle and erases its retained request buffer. Body and endpoint values
   are not logged. Tests alone may explicitly allow literal loopback HTTP.
- LLVKRegionCircuit owns a nonblocking Boost.Asio UDP socket to the authenticated
   simulator endpoint. It implements reliable retries/acknowledgements, bounded
   zero decoding, duplicate suppression, UseCircuitCode, RegionHandshakeReply,
   matching AgentMovementComplete, pings and LogoutRequest/LogoutReply. Native
   code does not invoke the reference message reader, which dispatches through
   global message-system callbacks. Tests independently encode replies with the
   reference template builder and shipped message definitions.
- LLVKLoginTransport composes authorization, seed capability discovery, event
   polling and circuit readiness. The session owner now pumps active transports
   on its owner thread. Connected requires seed discovery plus acknowledged
   circuit, handshake and matching movement completion. Login response data and
   capabilities remain private; UI snapshots carry opaque handles. Event replies
   are retained under count/byte bounds, not dispatched into GL world services.

Startup binds the main-grid transport to native login controls. Request preparation
uses reference-style first/last names and the password digest wire format, without
persisting credentials. Home/Last Location are supported. Unsupported grid/proxy
or custom-location selections fail explicitly. MFA challenge state is generation
tagged; the original PromptMFAToken declaration provides input/Continue/Cancel,
whitespace is stripped before submission, and stale responses are rejected.
Tokens and returned MFA hashes are not persisted or logged. Existing plain-text
critical-notice handling is functional but is not qualified as visual parity with
the reference critical-message floater.

Shutdown cancels pending authentication or sends event-queue done and simulator
logout for an established connection. Pending work is polled under the owner;
failures remain explicit cleanup failures. GPU upload/publication/retirement and
OpenGL implementation are unchanged. NATIVE_LOGIN_AUTHORIZED and
NATIVE_REGION_CONNECTED markers contain only generation/epoch counters. Neither
is STATE_STARTED or a claim of visible simulation.

### Verified evidence

- Session-owner tests pass, including transport pumping/cancellation and tagged
   MFA empty/stale token rejection.
- Native login protocol suite4/4 passes. It covers request escaping, bootstrap
   fields, signed circuit codes, numeric failures, XML bounds, HTTP redirect
   refusal, cancellation, real server rejection and MFA/critical replies.
- The full synthetic flow uses a temporary trusted TLS certificate with the key
   held in memory, actual HTTPS requests for login/seed/event queue, independently
   built UDP replies, a keepalive interval and graceful logout. A separate request
   rejects the untrusted certificate. No real account credentials are used.
- Widget210/210 passes, including the reference MFA form and token normalization.
   Window7/7 and native core build pass. Existing LNK4020 debug-symbol warnings
   remain. RelWithDebInfo production viewer link also passes. The Windows-scoped
   protocol fixture passes again after final build integration.
- The network fixture asserts no desktop OpenGL module is loaded. This is not
   exhaustive API tracing, packet-loss qualification or a live Second Life run.

### Open requirements

Browser-based TOS is deliberately not accepted through the generic text-agreement
route. LLFloaterTOS uses a loading page, site-availability request, real terms page
and checkbox gating; that native consumer and its exact parity remain required.
The transport currently fails TOS requests rather than bypassing consent.

Also open: protected credential/remember-device persistence; full login metadata
(machine identifiers are currently omitted as empty values, not fabricated);
proxy routing; custom start locations and additional Second Life grid selection;
complete redirect/update/error policy; account settings and authenticated Help
integration; MFA refresh after an intervening agreement; world/event consumers,
region crossing and full loss/reconnect qualification. The retained event queue
is bounded and eventually fails explicitly if consumers are not installed; it is
not a complete simulation service. Inbound world packets do not create a scene.

The operator approved NATIVE_REGION_CONNECTED as the network-only readiness gate
instead of STATE_STARTED; the resulting live verification is recorded above.
STATE_STARTED must not be fabricated before world readiness. Credentials must
continue to be entered directly in the viewer, never in chat or command-line
arguments. Broader login and simulation qualification remains open.

## Current production integration (2026-09-13)

This section supersedes the cache-only scope and isolated-build limitations below;
the earlier contract and evidence remain historical records. NV-00/01/03/12/15/17/18
apply. The source roots remain `LLAppViewer::cleanup`, `LLLoginInstance` agreement
responses and the native owners named below; no GL owner or callback is reused.

`LLVKWindowMgr::ApplicationServices` is adopted before its resources are initialized.
It owns login/auxiliary browsers, audio, voice, joystick, translation verification,
file pickers and cache-backed preview producers. It detaches UI callbacks/settings
subscriptions and destroys preview producers before the cache can retire. Browser
close is asynchronous with a 15-second deadline; Pending is polled, failure is
retained for explicit tagged Retry Cleanup. Voice/audio retirement must succeed
before those owners are destroyed. Startup's cache remains the earlier dependency.
The window/UI/GPU presentation host intentionally outlives application retirement;
it waits for device completion and destroys GPU resources only afterwards.

The real window fixture reproduced a presentation stall: detaching browser frames
left visible browser consumers, so `pendingBrowsers` kept every GPU packet Pending,
including the recovery modal. Detach now hides those consumers before clearing
their input/frame maps. The same fixture then exposed missing modal action names;
buttons now retain their declared names while using existing alert-button defaults.
These are native producer/consumer lifetime corrections, not weaker GPU readiness
or reference tolerances. The stalled temporary-profile component test was detached
from LLDB, sent WM_CLOSE, and stopped by exact PID only after it failed to exit.
No full viewer or operator-profile process was stopped.

Agreement replies carry exact ID/revision/text and request tags. Text is bounded to
64 KiB at ingress; empty/NUL-containing content is rejected before publication.
The native UI renders the supplied text in a read-only scrolling editor, offers
localized Accept/Decline, and defaults to Decline. Acceptance is not inferred from
closing a dialog. Both decisions capture the exact agreement and tag; a superseded
dialog cannot act on the next request. Error notices exceeding the available height
also use the bounded scrolling surface. All 13 shipped catalogs have both labels.

Configured RelWithDebInfo verification on 2026-09-13:

- Session integration target passed, including empty/oversized/altered agreement,
   exact acceptance forwarding, stale acceptance and revised-agreement rejection.
- Widget suite 209/209 passed, including complete long text, bounded modal height,
   actual Accept button, keyboard default Decline and stale dialog callbacks.
- Window suite 7/7 passed, plus standalone error and cold-cache regressions. Test7
   retires actual browser/voice/audio producers before a controlled dependency that
   returns Pending then CleanupFailed. Three presented recovery frames prove failed
   cleanup is not retried automatically; the actual Retry Cleanup button completes
   retirement, drains the cache, destroys the window and leaves zero adopted services.
- Native Viewer Link Validation passed. XML parsing confirmed unique, nonempty
   agreement actions in all 13 catalogs. No component process was left running.

This closes the cache-only adoption, absent agreement consumer and unpresentable
cleanup-recovery defects in this slice. It does not establish live authentication,
server agreement retrieval, HTML/URL TOS policy, protocol deadlines/logout, or
measured GL UI/effects parity. Those require their own source contracts and runtime
evidence; a plain-text agreement fixture is not parity for the GL browser-based TOS
floater. Fatal renderer/unrecoverable startup exits still use independent OS
reporting and retain unfinished owners as fatal containment, not successful recovery.

## Contract before integration

2026-09-13, main `native-error-messaging`, source
`485967401a08ed289ba5d194f03b9618ae0f5f4e`. The CPU owner is imported from the
tested `worktrees/native-session-owner` implementation, including exact-tag
cancellation and cleanup. Checkpoint `90af5a7` and the pinned historical GL oracle
are unchanged. Affected rules: NV-00/01/03/12/15/17/18.

1. GL roots: `idle_startup`, `LLLoginInstance` response/agreement callbacks and
   `LLAppViewer` quit/reset/cleanup separate login authorization, region readiness,
   account persistence and application teardown. The source worktree's
   `doc/vulkan/native_session_owner.md` records inspected branches and outstanding
   transport/world dependencies. No GL lifecycle or UI callback is reused.
   `LLToastAlertPanel` constructs form/default buttons, wraps text, restores focus,
   delays the default response, and sends one form response to its notification.
   Its GL draw/shadow, scroll limits, ignore policies, URL actions and channel graph
   are not equivalent merely because the native modal displays successfully.
2. Native design: owner-thread CPU state plus generation/request/region tags.
   Existing native cache `start/update/stop` owns locks, APR/trace runtime, jobs and
   a worker. Its environment captures owned paths and its own implementation;
   responders retain job records, not startup/UI addresses. Adoption precedes
   acquisition. Shutdown drains cache work before releasing its service. Native
   UI submits commands and reads snapshots; no transport means no authentication.
3. Smallest model: `LLVKApplicationCache` owns the actual cache previously local to
   startup. Startup owns the session; the synchronous window run borrows it.
   Preview producers detach before application retirement. Failed retirement
   leaves the owner alive for explicit tagged Retry Cleanup; no frame-loop retry.
   Fatal startup/window exits make one first cleanup attempt when not already
   disconnecting. If cleanup cannot complete, the whole cache owner is deliberately
   retained until process exit, like the existing cache failure containment, rather
   than destructing live workers. This is fatal containment, not recovered shutdown.
   The cache adapter retains a shared native startup-status presenter and hides it
   after retirement attempts; presentation failure must not skip cache draining.

Discriminating checks: compile the imported owner against its existing 382-check
suite; exercise actual cache acquisition/drain through the adapter using temporary
directories; test modal deduplication, safe copy, source catalog lookup, unavailable
transport, exact-tag stale recovery rejection and failed-retirement retry. Compile
the touched UI/startup/window sources without the separately owned GPU gate.

## Scope and open obligations

Agreement consumer follow-up (2026-09-13), NV-00/01/03: the inspected
`LLLoginInstance::handleTOSResponse` contract above requires displaying the supplied
agreement before explicit acceptance/rejection. Native agreement values now carry
the exact text with ID/revision and request tag, bounded to 64 KiB before inbox
copying. Empty or NUL-containing agreements cannot enter AwaitingAgreement. The
native consumer must render that content scrollably and issue `decideAgreement`
with the captured value, never accept through an implicit close/default response.
Owner tests discriminate empty/oversized data, altered content, exact acceptance,
repeated acceptance and revised-agreement rejection. This remains a CPU/UI contract;
the production authentication transport is the separate Phase 2 dependency.

The owner is not an authentication transport. No credentials, wire identities or
account persistence are installed. Connection states in controlled tests are not
`STATE_STARTED`. Browser/audio/voice/renderer lifetime remains in the existing native
window owners; only the audited cache application service is adopted in this slice.
Their full migration is open and must preserve dependency and GPU completion order.

Native errors use the existing selected-skin catalog and modal widgets, bounded
queued error notices, numeric diagnostics and close-only generic legacy errors.
Owner actions are determined by snapshots; copied reports contain no response,
URL, path, account or secret fields. Generic legacy notices still lack producer
request identity. Exact UI/effects parity, complete locale coverage, long-message
scrolling, modal nesting and full lifecycle/runtime acceptance remain unverified.
No viewer launch, real profile, credentials, microphone, commit or GPU test is
authorized for this integration.

## Exact source files

- Owner and standalone regression: `indra/llvulkan/llvksessionowner.h`,
   `llvksessionowner.cpp`, `tests/llvksessionowner_test.cpp`.
- Actual application adoption and window integration: `llvkstartup.h`,
   `llvkstartup.cpp`, `llvkwindowmgr.h`, `llvkwindowmgr.cpp`.
- Error/UI adapter: `llvkerror.h`, `llvkerror.cpp`, `llvkviewerui.h`, `llvkdialogs.cpp`.
- Existing regression fixtures: `tests/llvkerror_test.cpp`,
   `tests/llvkwidgettree_test.cpp` (208/209), `tests/llvkwindowmgr_test.cpp` (7).
- Build: `indra/llvulkan/CMakeLists.txt`, independent owner target/test and public
   widget dependency. The configured dependency graph has not been regenerated here.
- Catalogs: `indra/newview/skins/default/xui/en/strings.xml` and
   `indra/newview/skins/default/xui/de/strings.xml`, additive native-only keys.
- Documentation: this report, `native_error_messaging.md` and the already-dirty
   `native-services-handoff.md`, whose existing changes were read and preserved.

Paths without a prefix in the first four entries are relative to `indra/llvulkan`.
`llvkerrorwin32.cpp`, `llvkviewerui.cpp` and source-worktree files were compiled or
inspected but not edited during integration. GPU/widgetgpu/context-test files were
not edited or executed. No test tolerance was changed.

## Verification

- Main owner against source-worktree suite: 382 deterministic checks passed,
   MSVC 14.44 C++20 `/EHsc /W4 /WX`, standard library only.
- Main standalone owner integration suite: all checks passed, same strict flags.
- Expanded standalone error plus real Win32 fallback suite: passed, including the
   expected fixed stderr diagnostic from recursive-presentation rejection.
- Final isolated production/error/owner/widget/window compiles: passed with
   configured project include directories, external includes and definitions,
   `/MD /O2 /W3 /WX /external:W0 /bigobj`. Widget/window fixture links passed with
   configured library dependencies and library search paths, using existing TUT
   setup and a temporary exact-test runner.
- Final widget tests 208/209: 2/2, exit 0. Covers selected German catalog, safe
   diagnostic copy, failed copy preserving notice, modal delay/focus, duplicates,
   absent versus installed transport failure, snapshots and stale recovery callbacks.
- Final window test7: 1/1, exit 0. Actual application-cache acquisition, queued-write
   drain, partial failure cleanup, shared status lifetime, OS status hiding and no
   remaining adopted application service.

The ignored validation directory is
`worktrees/native-session-owner/build-vc-session-owner`. Its `validate-main.ps1`
reads compiler settings from configured vcxproj XML; `link-main.ps1` reads link
dependencies and substitutes the changed objects; `focused_runner.cpp` uses the
existing test runner setup to execute named test numbers. No configured object or
viewer executable was overwritten. The available session lacked the deferred-tool
loader, so validation used the user's authorized isolated compiler route rather
than changing task files or running a broad build.

A full isolated widget attempt first overflowed the stack before test reporting;
after matching fixture optimization it passed tests 1-49, then returned
`0xC00000FD` at existing test50. Final focused runs used optimized replacement
objects and passed. The broad failure is not explained or dismissed as unrelated;
main still needs the configured Widget/Window and viewer-link gates for the combined
change. Earlier configured passes do not validate the new source integration.
There is no runtime viewer, GPU or measured UI-parity evidence for this change.

## Review record

NV-00/01/03/12/15/17/18; independently owned CPU lifecycle and native UI integration.
Reference roots and revision are above. Data flow is native error values and exact
owner tags; no material/color/depth/shader ABI change. No GPU resources are adopted
by the new owner. Existing window visual retirement remains unchanged; only preview
producers detach before cache retirement. This is implemented production wiring
with scoped fixture verification, not authenticated login, full service adoption,
successful recovery from every failure, or measured visual parity.