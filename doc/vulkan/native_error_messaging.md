# Native error messaging

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