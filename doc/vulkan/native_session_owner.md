# Native session integration

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