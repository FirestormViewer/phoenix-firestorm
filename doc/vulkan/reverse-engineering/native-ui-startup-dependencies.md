# Native UI startup dependency investigation

Source: 3abd661f498329babaf87b49ffab910fcb5f0e6c, implementation checkpoint
90af5a7220f1fec28e3c909c051b6ee7e062f10b. Windows/MSVC RelWithDebInfo investigation;
preprocessor alternatives noted individually, not claimed executed. Historical GL
oracle remains 59108e15a1f8f94d2da7c674d937d19f5cf9450d. NV-00/NV-03/NV-12 and roadmap
R0/R1 govern this report. Status: local-inspected, transitive-OPEN, no native lifecycle
implementation or runtime qualification. See [coverage ledger](native-ui-coverage.md).

## UI-START-001: Windows WINMAIN viewer entry

### Native boot integration update (2026-09-11)

#### Verified viewer boot, 2026-09-11

The actual build-vc170-64/newview/RelWithDebInfo/vulkanstorm-bin.exe was built
and launched with --set RenderBackend Vulkan. It reached the independently owned
native login window before LLAppViewerWin32 construction. The live default CEF
splash page, packaged panel_fs_nui_login.xml widgets, actual font descriptors,
skin images and native image/text packets were visibly present. No second viewer
or renderer executable was used. Historical binaries with native-login names in
the build directory were not used as evidence.

Runtime observations on AMD Radeon RX 9070 XT / Windows / RelWithDebInfo:

- Parent PID 5684 loaded vulkan-1.dll, libcef.dll and
   VkLayer_khronos_validation.dll, but neither opengl32.dll nor glu32.dll.
   Enumerated child processes were dullahan_host.exe browser helpers, as permitted.
- Synthetic username input and password masking worked. Separate arrow-click /
   Home-row-click selected Home and closed the popup after the top-control fix.
   Password reveal displayed only the synthetic value mask-check and changed the
   eye icon; masking was restored before closing.
- Earlier PID 26452 resized from a 1024x768 client to 1264x861 with live browser
   reflow and retained native field values. Both viewer instances responded to
   normal WM_CLOSE and exited; process inspection did not provide an exit code,
   so no specific process exit code is asserted.
- Local client captures are logs/native-login-current.png,
   logs/native-login-input.png, logs/native-login-resized.png,
   logs/native-login-verified.png and logs/native-login-reveal.png. These are
   screen captures, not Vulkan readback or measured GL/native pixel parity.
- Runtime shader SHA256 matched generated build outputs: fragment
   225C1C935F8B3A06950987E42FA2B8ECBEDD3DEE49ADF9FA2CC5F88547D5D919;
   vertex 47118C2EA27875BAE63961DDDF65B054EB23B32D0414A37472A837A0F37332FF.
- Native construction/settings tests: 129/129; context GPU tests: 9/9;
   browser lifecycle: 1/1; integrated native login window: 1/1;
   native glyph pixel tests: 6/6 at their latest run. Integrated Vulkan test logs
   confirm Khronos layer insertion and contain no VUID/Validation Error entries.
   Actual viewer module loading alone is not a complete validation-error audit.

The viewer was compiled with BuildProjectReferences=false after focused native
dependency builds. This proves executable compilation/linking and the observed
boot, not a clean all-target build/test pass. The separate world-map test
assertion remains unchanged, and no tests were disabled to disguise it.

This is a native login **boot/render/input checkpoint**, not production login
service closure: the Log In authentication action, grid/account population,
credential persistence, account/help link dispatch and viewer-mode application
are not wired into the early native session. Startup supports a bounded option
subset and does not yet reproduce complete settings/locale/DPI/multi-instance
policy. IME, complete keyboard/popup/capture semantics, generic widgets, GPU
pressure policy and measured visual parity remain open. Neither these omissions
nor the successful capture may be treated as full NV-00/12/17 closure.

Native window partial-exit cleanup: a scoped guard detaches browser/UI pointers,
browser event callbacks and clipboard ownership before browser destruction on
every exit after browser construction, including initialization/load/paint errors.
This prevents window messages during CEF teardown from accessing destroyed owners.
The integrated window lifecycle test remains the focused executable check.

Native UI shader build/package correction (NV-00/16/17): llvk_ui_shaders
generates both ui2d stages from reviewed source in build/llvulkan/compiled_ui.
The Windows viewer depends on that target and stages the pair after linking;
viewer_manifest packages the identical generated pair over historical source-tree
SPIR-V. Other legacy shader assets remain unchanged. Focused validation builds
the shader target and checks manifest syntax; executable staging is verified at
the viewer build boundary. No hand-edited binary or stale shader fallback.

Animated browser publication correction: paint commands mark streaming browser
images explicitly. WidgetGpu uses one LLVKImagePublication per browser owner,
sampling the latest completed same-size version while a newer frame uploads.
This prevents perpetual Pending when CEF repaints faster than GPU publication.
Resize still waits for a matching extent. Unused streams drain pending uploads
before retirement; packet/frame references retain sampled images. Context test8
provides a new immutable browser frame at the completion observation and requires
Ready, directly discriminating the starvation defect.

Native login page query (NV-00/01/12/17): source LLGridManager built-in Agni/Aditi
grids use https://phoenixviewer.com/app/loginV3/; FSPanelLogin::loadLoginPage
preserves existing query then adds language, firstlogin string TRUE, version,
channel/grid/OS/source/content/skin and splash preferences. Native Page builder
uses audited nonvisual LLURI structured encoding; test129 checks query retention
and metadata encoding. Early startup currently supplies default Agni/channel/OS
metadata; exact build/channel/selected-grid integration and ForceLoginURL warning
policy remain open rather than reusing GL-owned grid/settings callbacks.

Early native entry integration (NV-00/01/03/17): Windows entry after Velopack and
before LLAppViewerWin32 now selects a native-owned lifecycle from RenderBackend.
Nonvisual OS argument/profile/executable paths and LLVKStartupSettings read
defaults/install/fsdata/user/session/user layers, then explicit --set values.
Non-Vulkan selection returns to existing GL/Zink startup without mutating its
settings or DLL search state. Native CEF DLL lookup uses the staged llplugin
directory and must be delay-loaded by the viewer. No renderer subprocess is used.
Supported initial command-line subset is --set/--settings/--sessionsettings;
other native options fail explicitly. Startup error UI is native Win32. Full
configuration parity, locale/theme/DPI policy, multi-instance/SLURL handling,
crash reporting, grid page parameters and authentication callbacks remain open.
This is early lifecycle integration, not a claim that those services are closed.

Native login window runtime test: when both browser and GPU tests are enabled,
INTEGRATION_TEST_llvkloginwindow loads real default settings, packaged login XML,
native font descriptors and skin assets, a local CEF page, and presents six frames
through LLVKLoginWindow. A separate test process is necessary because CEF cannot
be reinitialized after shutdown; it is not a production renderer subprocess.
The actual viewer entry remains unmodified until startup selection is integrated.

Native login window integration (NV-00/01/03/11/12/14/15/17): independently owned
Win32 window procedure consumes native pointer/key/clipboard operations, native
browser event/frame owner, actual login resources and paint/GPU packets. Source
contracts are the preceding input/paint/WSI records; no LLWindow/LLView/GL owner is
invoked. Close stops the loop while the window remains alive; browser and packet/
cache owners release before the Vulkan context and HWND. Resize uses the actual
swapchain extent and native layout before browser resize. Configuration supplies
the page and application service binding explicitly. This is not yet hooked into
viewer entry. IME, complete shortcut/focus/cursor routing, popup browser policy,
login authentication services and DPI scaling remain open. stopAfterFrames is
an integration-test bound, not a second renderer executable or normal exit policy.

Native settings value layers (NV-00/01/03/17): source 1819c5fecf
LLAppViewer::initConfiguration and LLControlGroup::loadFromFile,
LLControlVariable::setValue/setDefaultValue/resetToDefault. Q1 default replacement
clears saved/transient layers; Firestorm reapplies user settings after mode defaults;
command-line transient values must not change saved values; nonpersistent settings
ignore file overrides. Q2 independently owned LLVKStartupSettings consumes LLSD
configuration through the nonvisual llcommon serializer, without control-group
registry callbacks or visual defaults. Q3 bounded documents/entry counts, atomic
load, explicit type/default/saved/transient layers. Test128 checks precedence and
parses the real packaged defaults. Legacy XML, complete comparable-type semantics,
validation/sanity callbacks, metadata persistence, startup path/CLI selection and
file writes remain open; the model is not yet an early backend selector.

Native widget GPU consumer (NV-00/01/06/11/13/14/17): LLVKWidgetGpu consumes
the native paint-list contract, caches immutable images by source identity and
glyph atlases by widget/part plus exact glyph identity/geometry. Uploads poll
without normal-path waits; pending browser or glyph resources leave the output
packet unchanged. Completed unused entries retire while frame slots independently
retain submitted image references. Text changes coalesce while an upload is in
flight. Extent/clip conversion is explicit; current scope is 1:1 device pixels.
Test8 checks pending-to-ready, unchanged resource reuse and packet presentation.
Global cache budget is checked each prepare; reservation-before-allocation and
pressure eviction, full font-shadow policy and full login runtime remain open.

Native focus/glow mask pipeline (NV-00/06/12/16/17): source solidcolorF.glsl
outputs uniform RGB and sampled-alpha times uniform alpha; ordinary ui2d instead
multiplies texture RGB. Native ui2d specialization constant selects alpha-mask
output, with a separate triangle pipeline per existing blend mode. Packets carry
explicit mask/blend flags; ordinary/legacy draws retain the default specialization.
All variants rebuild from the same shader source. Context test8 records a tinted
additive mask with the real logo; this is runtime validation, not pixel parity.

Asynchronous native image publication (NV-00/06/13/14/17): GL browser image
updates previously hid synchronization behind mutable texture upload. Native
LLVKImagePublication instead owns one pending immutable LLVKGlyphUpload, polls
completion without waiting, and publishes a matching CPU-source/GPU-image pair.
On completion the latest frame is selected for the next upload; intermediates
coalesce without unbounded uploads. Same-size completed frames may publish while
a newer version uploads to avoid starvation; mismatched resize completions do not
publish. Null source invalidates current output without releasing pending work
early. In-flight packet references retain old GPU images independently. Test8
checks first publication, replacement and invalidation; queue waits occur only
in the test, never in advance. Owner destruction inherits upload completion wait.

Browser lifetime defect qualification (2026-09-11): real packaged-browser test
verified initial pixels, clicked pixels and resized pixels, then crashed in
dullahan::~dullahan after shutdown returned. LLDB/PDB identified the destructor,
not startup. Pinned source 49a551c0216ac7db03e36c9cc7ec44650c0be1c4 owns
CEF-refcounted dullahan_impl with unique_ptr but supplies no owning CEF reference.
The native-only vkdullahan target retains AddRef at construction and pairs it
with Release after relinquishing unique_ptr ownership at destruction. This keeps
the object valid through CefShutdown without a leak/double delete. The generated
facade preserves upstream license; existing GL plugin/prebuilt Dullahan remains
untouched. Matching CEF 139.0.40 headers are checksum-pinned; runtime still uses
the viewer's packaged CEF/helper. The same browser test is the discriminating
check; failure/partial-init and other platforms remain separately unqualified.

Browser runtime check: opt-in LL_VULKAN_BROWSER_TESTS stages the installed CEF
runtime/helper/resources beside an isolated integration executable. Test1 uses
a disposable profile and local HTML, asserting exact opaque primary-color pixels
at interior sample points before/after a mouse click and resize, old-frame
immutability, absence of opengl32 in the parent, and asynchronous close completion.
This tests the real packaged browser; it does not exercise remote login policy,
credentials, native widget embedding or complete viewer startup.

Direct native browser owner (NV-00/01/03/12/14/15/17): source roots are the
packaged Dullahan API 1.26.0, viewer MediaPluginCEF init/update/requestExit and
upstream initCEF, OnPaint, OnBeforeClose and callback handlers. Q1 CEF is global,
must pump on its initialization thread, publishes borrowed complete pixels, and
closes asynchronously before shutdown. Q2 LLVKBrowser owns Dullahan directly,
copies pixels into native frames and queues events instead of executing viewer
callbacks in CEF. CPU rendering disables GPU/WebGL; approved browser helpers are
permitted, no GL viewer media class is used. Q3 one initialization per process,
thread checks, bounded events/frames, explicit Closing/Closed and exit-callback
gated shutdown. Destructor pumps completion up to 15 seconds then terminates
rather than free a live CEF owner. Windows only; runtime test pending. HTTP auth,
file dialogs and JS dialogs are currently denied/suppressed, explicitly incomplete
application services. Popup/custom scheme events await native application policy.
Exact packaged-source audit, failed-init partial cleanup and reentrant CEF internals
remain open; this owner is not a completed browser or login milestone.

Browser CPU publication (NV-00/01/06/11/14/17): packaged Dullahan 1.26.0,
build 202510161628, CEF 139.0.40/Chromium 139.0.7258.139; viewer source
1819c5fecf MediaPluginCEF::onPageChangedCallback and media init texture_params.
Q1: callback borrows a complete BGRA page; source copies only matching dimensions,
and GL_RGB internal storage discards incoming alpha. Dullahan's upstream
dullahan_render_handler::OnPaint composites popups before callback; with
flip_pixels_y=false the buffer is top-down. Upstream master inspection is not
proof of exact binary implementation; real browser qualification remains required.
Q2: independently owned LLVKBrowserSurface publishes copied immutable bottom-up
opaque RGBA frames with full UV range, no power-of-two padding, and bounded extent.
Q3: resize invalidates current publication, late mismatched callbacks are ignored,
and externally retained frames stay valid until their consumers release them.
Test123 checks exact channels/orientation/alpha, borrowed-buffer isolation,
replacement retention, stale resize callback and invalid byte counts. Main-thread
owner integration, browser lifecycle, network and Vulkan publication remain open.

Packet CPU validation: context test9 checks exact top-left conversion, preserved
tint/alpha and painter ranges, invalid clip/inverted geometry rejection without
mutating preceding draws, empty clipping and frame reset. Test8 rejects an
unpublished image before retaining a valid packaged image. These are discrete
contract assertions, not image-parity tolerance changes.

Native packet text consumer (NV-00/01/06/11/12/14/17): consumes the established
native LLVKTextDraw::prepare result, byte tint normalized to float, already
scaled glyph geometry plus explicit device-space origin. Position Y conversion
is shared with image packets; texture UVs remain unchanged. UI depth must be
disabled, page count/extents/publication must match, and a failed append rolls
back the whole text operation. Context test8 now shapes packaged-font "Log In",
publishes its real atlas and presents glyphs/shadow in order after the login logo.
Synchronous waits are bounded test/bootstrap work, not per-frame streaming policy.
Full tree painting, input-to-frame updates and actual viewer boot remain open.

Native text preparation reuse (NV-00/01/06/12/17): the independently implemented
LLVKTextDraw glyph ordering/color/shadow/bold contract from dfab8c2fd2 now exposes
CPU quads for the native UI packet consumer. No GL function is shared or changed.
The existing offscreen renderer consumes those same quads; adjacent same-page
runs coalesce without reordering. The six established GPU pixel tests are the
discriminating check for this native-only preparation refactor. Depth remains
the offscreen renderer's explicit pipeline/style responsibility.

Native image packet producer (2026-09-11, NV-00/01/06/11/12/14/17): source image
geometry contract is recorded in native-ui-construction-dependencies.md under
Native skin geometry. Q1 painter order, per-draw clip/tint and bottom-up UI/UVs
must survive presentation. Q2 LLVKUiPacket consumes native prepared quads and
published image resources, converting positions to the context's top-left ABI
without flipping UVs. Q3 append-only bounded frame data retains image resources,
checks clip/extent/color and rejects unpublished/mismatched padded dimensions;
context takes over image retention when the packet is recorded. Test8 now loads
and decodes packaged login_fs_logo, uploads its pixels and presents it over the
native panel color in both frame slots, then checks retirement. This verifies a
native asset-to-presentation path, not a complete login tree or measured parity.

Skin image sampler follow-up (NV-00/06/15/17): source LLUIImageList::loadUIImage
sets TAM_CLAMP, fetched-file construction uses MIPMAP_NO, and
LLTexUnit::setTextureFilteringOptionFast selects linear min/mag for the default
non-point, nonmipped image. Native published images now take an explicit
SkinLinearClamp option, verify selected-device linear format support, and retain
the default GlyphNearestRepeat behavior. Context test8 uses the skin variant;
the existing glyph readback tests protect nearest/repeat. Global anisotropy
configuration and measured skin pixel parity remain unqualified.

Native packet image retention update (2026-09-11, NV-00/01/06/13/14/17):
Q1 GL texture binding retains implicit driver use, while native submitted
descriptors and images must outlive all consuming command buffers. Q2 reuse the
independently owned LLVKGlyphUpload RGBA image publication (fence, flushed staging,
transfer-to-shader barrier) and retain its immutable resource in each packet's
frame slot. Q3 same-device/allocator/queue/family check before recording, compatible
descriptor layout (fragment+compute), release only after that slot's successful
fence wait or checked device completion at destruction. Raw descriptor callers
remain legacy caller-lifetime debt, not the new native owner path. Context test8
now submits one published image in both slots, removes producer references, and
checks retirement only after both slots complete. This is resource/presentation
qualification, not native login boot or visual parity. Default image sampling
still nearest/repeat; skin linear filtering requires an explicit follow-up.

Browser dependency decision (2026-09-11): the packaged Dullahan API at
build-vc170-64/packages/include/cef/dullahan.h fixes host_process_filename to
dullahan_host.exe and exposes no single-process setting. The viewer wrapper
MediaPluginCEF additionally runs under LLPluginProcessParent/SLPlugin. Upstream
Dullahan initCEF sets CEF browser_subprocess_path; direct embedding removes the
viewer plugin wrapper but not CEF subprocesses. The user subsequently clarified
on 2026-09-11 that the one-process rule applies strictly to the viewer's 3D
portion and that Dullahan/CEF helper processes are allowed. The user also
explicitly permitted the slvoice helper. The earlier browser
blocker was an overly broad interpretation and is removed. Native browser
ownership, callbacks, CPU pixel publication, input, shutdown and Vulkan upload
still require implementation and verification. No blank browser or static
screenshot is substituted as completion.

The active milestone is booting the actual login UI natively, not completing the
entire widget registry before startup integration. Existing UI-START records stay
open where stated. The present LLVKSession entry is too late because application
initialization has already constructed GL-owned UI; the native boot owner must be
selected before LLAppViewerWin32 construction, after the existing updater hook.

Frame prerequisite (NV-00/03/13/14/15/17): source contract is the existing native
LLVKContext::begin2DFrame/end2DFrame versus the GL window swap lifecycle. Q1 GL
presentation hides acquisition/fence ownership, whereas the native path explicitly
waits/acquires/records/submits/presents. Q2 a failed recording must not reset the
fence that a future frame waits on, and a failed submit cannot reuse its reset
fence or acquired semaphore. Q3 reset the fence only immediately before submit,
check all result-returning operations and make fatal state sticky; expose
Unavailable/OutOfDate/Fatal distinctly to the future boot loop. Recreate is only
appropriate for out-of-date/suboptimal WSI, not device loss. Focused checks are
native compilation and fault-injected frame transitions before runtime boot.
No login boot or GPU runtime success is claimed by this frame change.

Native packet prerequisite (NV-00/01/06/11/13/14/17): Q1 UI output is a painter-
ordered set of textured triangles with clip/blend state; the historical sink's
ambient dispatch and single shared vertex buffer are not native login ownership.
The existing ui2d shaders require position/UV/RGBA and a top-left pixel projection.
Q2 recordUiPacket consumes explicit native vertices/ranges/descriptors once per
acquired frame. Q3 each fence-owned frame slot retains its own mapped VMA buffer;
begin waits that slot before reuse/growth, allocation flush precedes submission,
and shutdown waits device work before releasing buffers. A second packet cannot
overwrite a previously recorded packet. Geometry/clip validation precedes any
allocation or draw. Texture descriptors must be retained by the boot resource
owner through all consuming fences; their integration remains open. Fault tests
cover invalid clipping and duplicate packet refusal. Actual presentation and
native login consumers are not established by these CPU failure tests.

Source: [llappviewerwin32.cpp](../../../indra/newview/llappviewerwin32.cpp#L521).
WINMAIN names wWinMain normally, DebuggingWinMain under DEBUGGING_SEH_FILTER.
**1.** Under LL_VELOPACK call velopack_initialize FIRST; false returns0 before viewer
construction. Profiling frame/thread initialization next. Local heap diagnostic
arrays/count; WINDOWS_CRT_MEM_CHECKS&&!INCLUDE_VLD sets CRT debug allocation/leak flags.
Alternative heap-configuration body is under #elif0 and is not an active feature
mandate. Set global large/small icon resource identifiers.
Convert wide command line to narrow string and pass its c_str to new LLAppViewerWin32;
temporary string lifetime requires constructor copy audit. Install exception terminate
handler saving prior. create_app_mutex result inverted -> FoundOtherInstanceAtStartup
in gDebugInfo. viewer.init false: if LLApp::isQuitting return0 (URL handoff path),
else warn and return-1. Neither branch calls cleanup/delete or later NVAPI teardown
in this body.
Init succeeded: hSession0; static cached NvAPICreateApplicationProfile default true.
Enabled -> NvAPI_Initialize; status OK -> DRS_CreateSession; failure logs nvapi_error,
success ll_nvapi_init(session). Non-OK initial NVAPI status has no error branch here.
Heap diagnostic loop runs only if num_heaps>0; active initialization leaves it0.
Loop calls viewer.frame until true. If !LLApp::isError: optional CRT memory check,
set gGLActive=true, viewer.cleanup, optional post-cleanup CRT check. Error skips
cleanup but still deletes viewer. Delete/null viewer pointer; nonzero DRS session
destroy/null. Return0; previous terminate handler is not restored locally.

**2.** One executable/process entry with audited common process services, then one
selected renderer lifecycle. Updater hooks, command-line/settings policy, multi-instance
handoff and crash reporting remain CPU/platform duties, not Vulkan commands.
**3.** Choose a lifecycle owner selected before incompatible application/UI construction,
with explicit partial-init cleanup and backend-specific driver/context/resource setup.
Alternatives rejected by current contract: second viewer executable/process, late
GL draw dispatch, shared low-level RHI or GL-frame interop. Preserve existing GL/Zink
provider behavior; NVAPI profile work must be audited before sharing with native route.
No new selector code until constructor/configuration/entry helper closure.
Checks: updater early exit,init false/quitting,error versus normal cleanup,constructor
command-line retention,mutex handoff,NVAPI init/session failures,frame termination,
native route no GL calls from construction through teardown. These are unexecuted
lifecycle checks, not implied by successful CPU unit tests.
Outgoing: updater,profiling/CRT,wide conversion,LLAppViewerWin32/base constructors,
terminate handler,mutex/debug LLSD,init/frame/cleanup/destructor,LLApp global state,
cached settings,NVAPI helpers and all static initialization before entry. OPEN.

## UI-START-002: LLAppViewer::init pre-window UI block

Source: [llappviewer.cpp](../../../indra/newview/llappviewer.cpp#L1027),
[later window call](../../../indra/newview/llappviewer.cpp#L1205).
This is an ordering-block record, NOT closure of the full init function.
**1.** Following configuration,HTTP/machine/asset statistics and initThreads:
settings map config=gSavedSettings,ignores=gWarningSettings,floater=gSavedSettings,
account=gSavedPerAccountSettings. Copy FSLegacyNotificationWell into internal setting.
LLUI.createInstance(map,LLUIImageList instance,immediate/deferred UI audio callbacks).
SpellCheck enabled: split SpellCheckDictionary by commas; nonempty list -> primary
dictionary first,pop then secondary dictionaries. Set skin folder/theme with UI
language; initStrings. Construct shared UI translation bridge; initialize wearable
and settings type singletons. Construct notifications singleton; log J2C/curl info.
Increment NumSessions setting. Install keyboard translation callback; URL actions:
LLWeb.loadURL with empty targets,loadURLInternal(...false),loadURLExternal(...true),
LLURLDispatcher.dispatchFromTextEditor. Set LLUI help implementation,LLFloater.initClass,
register URL floater dispatcher,construct tool manager. Intervening work before cache
and window remains OPEN, not omitted from lifecycle.
Later initCache failure uses translated OS message,profiling end and returns false.
Create event recorder/watchdog; set gGLActive=true; selectGLBackend then initWindow,
ignoring returned bool. Post-window hardware override/system info,feature lookup,
registered init callbacks and folder-view init run before conditional GL-info gate.
Thus renderer selection in initWindow is not prior to UI/service construction.
**2.** CPU settings/localization/assets/action services required by native controls
must have independently audited neutral owners and explicit initialization order.
**3.** Split configuration resolution from backend-specific service/UI construction;
do not use skipping GL context creation as proof of GL-free startup. Registered
settings/actions may create UI or fetch resources transitively, requiring their own
closure and cancellation. Native data preparation begins after those owners are valid.
Checks: first UI/default/font lookup timing,settings callbacks during map setup,
dictionary/skin failures,notification construction,initWindow failure continuation,
registered init target inventory and partial cleanup. No runtime GL-free claim.
Outgoing: each named helper/singleton/callback,init function preceding/intervening/
following branches,configuration precedence and selectGLBackend provider lifecycle.

## UI-START-003: LLAppViewer::initWindow

Source: [llappviewer.cpp](../../../indra/newview/llappviewer.cpp#L3767).
**1.** Log; set global headless from settings,read IgnorePixelDepth. Read RenderBackend.
Vulkan: Windows probe.hasVulkanDevice false -> warn/local OpenGL fallback without
persisted setting change; true log firstDeviceName. Non-Windows -> warn/OpenGL.
Zink accepted as GL path; prior selectGLBackend is responsible for actual provider.
Unknown except OpenGL -> warn/local OpenGL. Log effective backend.
vulkanBoot=effective=='Vulkan'; true -> process-wide setSkipGLContext(true), false
does NOT explicitly clear it here. Build LLViewerWindow Params from global title/
class,settings x/y,width/height/minima; #ifdef LL_DARWIN fullscreen=false,otherwise
saved fullscreen;ignoreDepth,firstRun. new LLViewerWindow; no local allocation guard.

Windows native: session.start(window,RenderVulkanDebug) false warns that frames will
not present but DOES NOT fail/return/cleanup. resolveGPUClassAndApply runs regardless
of that result. WatchdogEnabled==-1 -> inverse feature WatchdogDisabled; else bool
setting. useWatchdog OR !secondInstance -> initialize watchdog with four callbacks:

1. Marker callback(final): lookup app; final -> logoutRequestSent selects logout-freeze
   versus freeze error marker; nonfinal -> watchdog marker.
2. Recovery callback: lookup app/remove watchdog marker.
3. Report callback(desc): LL_WINDOWS&&LL_BUGSPLAT -> lookup app/writeDebugInfo/report
   custom BugSplat return; other builds return false.
4. Freeze action: lookup app/sendLogoutRequest,translated OSMessageBox freeze/fatal.

Set internal group notice position setting from external; construct notification
manager. WindowMaximized -> maximize now. ForceGraphicsLevel present AND valid ->
setGraphicsLevel(level,false),save RenderQualityPerformance. Set RenderInitError=true,
save ClientSettingsFile. Non-native -> gPipeline.init,error check,window.initGLDefaults;
native skips those only. Set RenderInitError=false,save settings again even when
native session start failed. CrashOnStartup -> forceErrorLLError. FirstLoginThisInstall
AND meetsRequirementsForMaximizedStart -> log,set WindowMaximized true. If saved
WindowMaximized -> maximize (possibly second call). Assign LLUI.mWindow; watch cursor;
window.initBase(); log done,return true. No native success check at return.

**2.** Selected native window/surface/device/UI service initialization with actual
capability negotiation, explicit errors and owned partial teardown. Watchdog,
notifications,settings and layout are CPU/services; GL pipeline setup belongs only
to GL/Zink lifecycle, never native.
**3.** A complete native lifecycle must stage backend choice before CPU UI constructors,
publish window/device readiness only after success, and propagate recoverable/fatal
failure without a permanently blank success-shaped session. Full fallback before
rendering is permissible only with complete cleanup/selection semantics; late implicit
GL recovery is forbidden. Probe-first device name is not selected-surface device
capability evidence (NV-15). Existing settings callback/side-effect ordering must be
audited rather than reordered wholesale.
Checks: every renderer string/platform branch,probe versus actual device mismatch,
session start/feature resolution failure,watchdog settings/second instance,callback
during shutdown,quality overrides,settings-save failure,RenderInitError state,maximize
twice/first-run,base-view construction and GL API execution audit. All runtime open.
Outgoing: settings Params/converters,probe,skip-context flag,LLViewerWindow constructor/
initBase/window methods,session.start,feature manager,watchdog/callback services,
notification manager,settings persistence,gPipeline/GL defaults,crash helper and
maximum-start predicate. Full local body inspected; every transitive edge remains OPEN.

## UI-START-004: LLUI constructor

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175).
**1.** Copy settings-group map,immediate/deferred audio callbacks;window/root/help
null. Construct Render2D(image_provider) then spellchecker BEFORE checking required
config/floater/ignores pointers; missing group fatal, account not checked here.
Assign LLFontGL.sShadowColor from UIColorTable ColorDropShadow. This is a GL-owner
global write, not evidence of a GL API call by itself. Get commit default registrar;
register Floater.Toggle,ToggleOrBringToFront,Show,ShowOrBringToFront,Hide;
Button.SetFloaterToggle,SetDockableFloaterToggle,ShowHelp,ToggleFloater. Return values
from registrar.add ignored. Enable default registrar gets Floater.Visible,IsOpen,
CanShow. Load master commands via LLCommandManager::load; result ignored here.
No root widget/window construction in local body; their later calls remain open.
**2.** CPU native settings/localization/style/action services and image asset provider
with device publication separate. **3.** Explicit lifecycle-owned service bundle,
audited neutral command/actions,shadow style value independent of LLFontGL global.
Do not simply wrap LLUI or invoke its registered GL-owning floater actions from
native controls. Fail partial service initialization with owned cleanup.
Check required group missing,provider absent,duplicate actions,color/command load
failure,early callbacks and teardown. Outgoing: Render2D/spellchecker constructors,
map lookup,UIColorTable,registrars,each callable below,command load and base/members.

## UI-START-005: LLUI destructor

Source: [llui.cpp](../../../indra/llui/llui.cpp#L231).
**1.** Delete spellchecker singleton,then Render2D singleton; members/base later.
No unregister of constructor-added action names or explicit root/help deletion here.
**2.** CPU service teardown; native GPU retirement separate. **3.** Disconnect action/
asset subscriptions and stop preparation before destroying CPU owners/device resources;
map and callback lifetime must be audited, not assume singleton deletion clears all.
Check late callbacks,provider cleanup,partial init and root lifetime.
Outgoing: singleton deletion,Render2D/provider,spellchecker workers,registry owners.

## UI-START-006: LLRender2D constructor

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1746).
**1.** Store raw provider;nonnull -> provider.addOnRemovalCallback(resetProvider).
No GPU call in body; provider callback registration implementation still open.
**2.** CPU image lookup service subscription. **3.** Audited asset service may be
shared at semantic level, not GL LLUIImage ownership. Explicit lifetime registration
needs cancellation and readiness identities.
Check null/provider removal before bridge destruction and callback duplication.
Outgoing: provider callback API,static resetProvider,base/member lifetimes.

## UI-START-007: LLRender2D destructor

Source: [llrender2dutils.cpp](../../../indra/llrender/llrender2dutils.cpp#L1755).
**1.** Nonnull provider -> cleanUp() THEN deleteOnRemovalCallback(resetProvider).
cleanUp virtual can mutate provider/service state; pointer checked only before both
calls. No local GPU completion or exception handling. **2.** CPU asset detachment
and backend resource cleanup split. **3.** Native teardown must stop callbacks and
retain actual provider lifetime across cleanup, then completion-retire versions.
Check cleanup invoking removal callback (sets pointer null),provider deletion and
partial resource failure. Outgoing: concrete cleanUp,callback removal and provider owner.

## UI-ACTION-001: registered Floater.Toggle lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), constructor registration by name.
**1.** Ignore control; toggleInstance(param.asStringRef()),discard return. **2.** CPU
native window/panel action. **3.** Native floater manager must preserve identity and
visibility/creation policy, not call GL-owning reference constructor.
Check absent/existing floater and parameter type. Outgoing: asStringRef,toggleInstance.

## UI-ACTION-002: registered Floater.ToggleOrBringToFront lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** Ignore control; toggleInstanceOrBringToFront(param.asStringRef()),discard result.
**2.** CPU focus/z-order/visibility action. **3.** Preserve distinction from plain
toggle; native painter/focus state prepared after action. Check hidden/obscured/front.
Outgoing: conversion and floater target/default arguments.

## UI-ACTION-003: registered Floater.Show lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** showInstance(param.asStringRef(),empty LLSD,false),ignore control/result.
**2.** CPU native floater show with explicit false focus argument subject to target
signature. **3.** Preserve create-versus-show and key defaults; no generic toggle.
Check missing floater,key and focus policy. Outgoing: showInstance/LLSD/conversion.

## UI-ACTION-004: registered Floater.ShowOrBringToFront lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** showInstanceOrBringToFront(name,empty LLSD),ignore control/result.
**2.** CPU native visibility/z-order action. **3.** Typed native action with same
instance-key and focus policy after target closure. Check already visible behind.
Outgoing: target/defaults and LLSD conversion.

## UI-ACTION-005: registered Floater.Hide lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** hideInstance(param.asStringRef()),ignore control/result. **2.** CPU hide action.
**3.** Native hide must preserve close callbacks and persisted visibility distinctions;
not assume destruction. Check existing/missing instance. Outgoing: hideInstance.

## UI-ACTION-006: registered Floater.Visible lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** Return instanceVisible(name,empty LLSD),control unused. **2.** CPU predicate.
**3.** Resolve native instance state without constructing reference UI as a side effect;
target must be audited before assuming read-only. Check missing/hidden instance.
Outgoing: instanceVisible/LLSD.

## UI-ACTION-007: registered Floater.IsOpen lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** Same instanceVisible(name,empty LLSD),not a separate 'exists' query.
**2.** CPU predicate alias. **3.** Preserve actual alias rather than infer semantics
from 'IsOpen' name. Check constructed-but-hidden floater. Outgoing: instanceVisible.

## UI-ACTION-008: registered Floater.CanShow lambda

Source: [llui.cpp](../../../indra/llui/llui.cpp#L175), named registration.
**1.** Return canShowInstance(name,empty LLSD),control unused. **2.** CPU eligibility
including policy callbacks. **3.** Native actions must preserve restrictions and
availability, not always return true because graphics exists. Check policy denial.
Outgoing: canShowInstance and all registered/policy targets.

## UI-START-008: LLAppViewerWin32 constructor

Source: [llappviewerwin32.cpp](../../../indra/newview/llappviewerwin32.cpp#L874).
**1.** Base LLAppViewer constructs first; mCmdLine initialized from input char*,
consoleAllocated=false,body empty. mCmdLine member type must be verified before
claiming temporary command-line lifetime safety. **2.** CPU platform app construction.
**3.** Backend selection before this class avoids inheriting unaudited base effects;
alternatively split base after constructor/helper closure. No second process needed.
Check argument retention,base failure and console state. Outgoing: base/member ctors.

## UI-START-009: LLAppViewerWin32 destructor

Source: [llappviewerwin32.cpp](../../../indra/newview/llappviewerwin32.cpp#L880).
**1.** Empty body; members/base destruct. **2.** CPU platform teardown.
**3.** Native window/device/resources need independent completed-use teardown before
base services disappear, not an empty platform destructor as cleanup proof.
Check partial init and base destructor. Outgoing: members/base.

## UI-START-010: LLAppViewer constructor

Source: [llappviewer.cpp](../../../indra/newview/llappviewer.cpp#L762).
**1.** Marker-file members default; reportedCrash=false,numSessions0,threadPool null,
purge cache/exit/userData flags false,secondInstance false,snapshotSaved false,
savePerAccountSettings false,quit/closing/logout flags false,mainloopTimeout null,
regionAlive false. Construct cached Randomize Framerate and Periodic Slow Frame
settings with fallback false BEFORE body. Timer-log/CEF-purge threads null,settings
location null,firstRun false,saveSettingsOnExit true,purgeTextures false. Implicit
other base/member initialization remains separately open.
Existing sInstance nonnull logs fatal. dumpPath empty; directory init APP_NAME_x64
if ADDRESS_SIZE64 else APP_NAME. Set sInstance=this,stop gLoggedInTime,processMarkerFiles
BEFORE normal startup logging. LoginInstance.setPlatformInfo(gPlatform,OS version,
simple OS string). #ifdef LL_BUGSPLAT log directory and CrashContext.xml setup;
else per-run dump directory. Assign dumpPath,setDebugFileNames.
No backend decision in body; initialization can access settings/login/filesystem
services and register callbacks before init() runs.
**2.** CPU app/config/diagnostic owners; native route must audit these independently
of GPU initialization. **3.** Prefer selected lifecycle owner before incompatible
construction, with explicit neutral process context for settings/paths/diagnostics.
Do not instantiate reference app then hope late flags neutralize constructor effects.
Checks: singleton duplicate,settings cache subscriptions,marker files/second instance,
login singleton callbacks,platform macros,path failures and partial construction.
Outgoing: LLApp/implicit members,cached controls,dir init,markers,login/OS singletons,
debug filenames and crash context. No runtime isolation configured yet.

## UI-START-011: LLAppViewer destructor

Source: [llappviewer.cpp](../../../indra/newview/llappviewer.cpp#L843).
**1.** Delete settings-location list,destroyMainloopTimeout,removeMarkerFiles.
Does not call full cleanup(),clear sInstance or shut down all services in body.
Members/base follow. **2.** CPU partial/normal teardown. **3.** Native lifecycle needs
explicit partial-init owner graph; deleting app is not equivalent to cleanup.
Check failed init entry returns without deletion,error path skipping cleanup,
marker removal and late callbacks. Outgoing: member deletion,timeout/marker helpers.

## UI-DIR-001: LLDir_Win32 constructor

Source: [lldir_win32.cpp](../../../indra/llfilesystem/lldir_win32.cpp#L95).
**1.** Delimiter backslash. Read optional APPDATA into OSUserDir; empty or !fileExists
-> SHGetKnownFolderPath(RoamingAppData),on success convert/store,update child-process
APPDATA via _wputenv_s,free returned memory. Read LOCALAPPDATA into OSCacheDir;
same fallback to LocalAppData and environment update. PRELOG calls may write early
diagnostics if configured. Thus nonexistent isolation directories FALL BACK to real
profile; setting environment strings alone is insufficient.
GetTempPath(MAX_PATH): success removes last wchar if nonempty (assumed trailing slash),
convert to temp dir,fill empty user/cache dirs from temp. Failure temp=userDir.
Active #if1 branch GetModuleFileName: success terminates at returned size,converts,
splits on last backslash into executable directory/name,GetCurrentDirectory->workingDir.
Failure warns,uses current directory as executable dir; workingDir not assigned
in that branch locally. Buffer/truncation handling remains an API precondition risk.
AppRODataDir=workingDir; if either skins or app_settings is not directory, use
executable dir instead. SkinBaseDir=ROData/skins; defaultCacheDir=buildSLOSCacheDir;
mkdir cache,warn on-1. LLPluginDir=executable/llplugin. Constructor runs at static
initialization via gDirUtil; before viewer settings/command line.
**2.** CPU filesystem/environment services for native/GL lifecycle and isolated tests.
**3.** Share audited directory policy, with precreated process-local environment
overrides for tests and explicit error handling. No runtime test may rely on late
settings flags to undo earlier profile writes. Native service split must retain
resource path/skin behavior without introducing a second viewer process.
Checks: existing/missing/empty overrides,known-folder failure,temp fallback,module
path truncation,resource cwd fallback,cache creation failure and child environment.
Outgoing: getoptenv,fileExists,Windows APIs/conversions,PRELOG,add/isdir/mkdir,
buildSLOSCacheDir and base/global initialization. Local body inspected; helpers open.

## UI-DIR-002: LLDir_Win32::initAppDirs

Source: [lldir_win32.cpp](../../../indra/llfilesystem/lldir_win32.cpp#L302).
**1.** Nonempty RO-data override sets AppRODataDir and skin base. Set appName,
OSUserAppDir=OSUserDir/appName. mkdir returns-1 -> warn and fall back to OSUserDir.
mkdir expanded LOGS,USER_SETTINGS,CACHE each; failures warn but continue. Set CAFile=
executable/ca-bundle.crt. Does not process command-line settings or alternate renderer.
**2.** CPU app directory identity/creation. **3.** Test parent directory must itself
be isolated even if app-subdirectory creation fails; native initialization needs
explicit IO result policy without changing normal GL data locations.
Check existing parent,app-dir mkdir failure,logs/settings/cache write paths and RO data.
Outgoing: mkdir/add/getExpandedFilename and base directory fields.

## UI-CAPTURE-002: gl_capture_frame_once

Portability review, 2026-09-21, source `78d2c624eb` (NV-00/NV-01): the
only call in `display_startup` is guarded by `LL_WINDOWS`, but the static
definition was unconditional. Guard the definition with the same condition.
This preserves the Windows GL reference capture and its ordering, leaves the
existing Linux execution path unchanged, and removes the unused Linux function.
No GPU ownership, synchronization or retirement contract changes. Verification:
inspect both guards and confirm that the function body and call are unchanged;
full Linux compilation remains a CI check.

Source: [llviewerdisplay.cpp](../../../indra/newview/llviewerdisplay.cpp#L261).
**1.** Static getenv(VULKANSTORM_CAPTURE); absent/empty returns. Startup state below
LOGIN_SHOW returns without frame increment. Otherwise increment static frame counter;
only frame90 proceeds. Read raw window width/height; nonpositive warns/returns with
no retry. Allocate width*height*4 bytes; glReadPixels RGBA/UNSIGNED_BYTE at0,0. Open
specified output wb; success fwrite native U32[width,height] then pixel bytes,close,
log; failure warns. Does not check GL read error or fwrite return,read-buffer/pack
state,alpha contract or row orientation conversion in body. On Windows header is
little endian; rows follow GL readback bottom origin.
**2.** Native capture must read application-owned output images under explicit GPU
completion/layout/usage,not GL readback. This existing GL hook is bounded current-
checkpoint observation only,not an approved golden-baseline replacement.
**3.** For current GL startup checks use unique isolated output and verify header,
byte count,device/driver/log identity; later parity requires pinned oracle and
predetermined reference-derived tolerances. Do not infer successful pixels from
file existence alone or use GL-produced capture as native rendering.
Checks: startup gate/frame90,size/output failure,readback content and exact file size.
Outgoing: call site/frame ordering,window extent,state getters,GL readback state,
allocator/file APIs and actual reference inputs. No execution yet in this record.

## UI-CAPTURE-003: automatic quit block in LLAppViewer::idle

Source: [llappviewer.cpp](../../../indra/newview/llappviewer.cpp#L6091).
Local block only; full idle remains open. **1.** Static cached QuitAfterSeconds;
positive AND renderStartTime.elapsed>value -> log and app.forceQuit. No login-state
gate in this block. Following AFK policy is independent and not used for test timeout.
**2.** CPU bounded process test/shutdown trigger. **3.** Use existing timed quit for
isolated startup observation plus an external deadline limited to the process launched
by the test. Timer does not guarantee init cannot hang before idle.
Check early init failure,login state,deadline and cleanup exit code.
Outgoing: cached control,render-start timer,forceQuit and frame/cleanup loop.

## Runtime isolation requirement

The inspected beginning of LLAppViewer::init checks a CLEAR marker and can remove
settings,account configuration and password files before initConfiguration(). No
startup/capture test will invoke this path against the user's normal data as an
experiment. First close directory/environment/command-line precedence and establish
isolated test paths with normal settings/secrets untouched. This is a test-safety
prerequisite, not a reduction of required runtime validation or UI functionality.
The reset branch itself and complete initConfiguration remain OPEN. Inspected
directory code supplies existing APPDATA/LOCALAPPDATA overrides before construction;
TEMP/TMP and capture/diagnostic paths must likewise be confined to the test directory.
Tests must explicitly disable autologin and use no credentials/account-data copies.
The owning mCmdLine member is std::string in
[llappviewerwin32.h](../../../indra/newview/llappviewerwin32.h#L69), confirming the
constructor copies the temporary converted command line.

## UI-NATIVEBOOT-001: LLVKSession::start

Source: [llvksession.cpp](../../../indra/llvulkan/llvksession.cpp#L109).
**1.** Existing global context warns/returns true without checking requested window.
Null window ->warn/false; getNativeHandle/getNativeInstance,null either ->warn/false.
new Context; createInstance(validation,error) false ->warn/delete context/false.
Create surface into GLOBAL s_surface; null ->warn/delete context/false. Physical
selection OR device creation failure (short-circuit) ->warn,destroy surface,clear
global surface,delete context,false. queryClientSize into global width/height;
createSwapchain failure ->warn/delete context,clear global surface,false (comment
claims context adopts surface on entry; consumer ownership still needs audit).
Assign global context BEFORE logging selected device and measureMemoryBandwidthGBps;
publish bandwidth into GPU facts; return true. Exception handling absent locally.
Caller UI-START-003 continues even if false; existing context makes isRunning true
before benchmark/fact publication completes.
**2.** Native selected-device/surface startup and CPU capability publication.
**3.** Transactional lifecycle owner should retain unpublished resources until
complete success and separately validate actual selected-device capabilities; logical
running flag must not disguise partial readiness. Global prior-session reuse and
surface ownership transfer require explicit contracts, not comments as proof.
Checks: every failure point,existing context/different window,surface adoption,client
query failure,benchmark failure,selected device versus early probe and teardown.
Outgoing: window platform handles,Context ctor/create/destroy methods,queryClientSize,
benchmark and facts publication. Local body only; no native correctness closure.

## UI-NATIVEBOOT-002: LLVKContext::createInstance

Source: [llvkcontext.cpp](../../../indra/llvulkan/llvkcontext.cpp#L57).
**1.** volkInitialize non-success ->error/warn/false. mValidation=request. App/engine
Vulkanstorm1.0,Vulkan API1.3. Extensions surface plus Win32 surface under platform
macro. If validation requested: enumerate layers count/data (results ignored),scan
for Khronos validation and LunarG api_dump. Available api_dump appended first;
validation present ->append it and debug-utils extension; absent ->log downgrade and
set mValidation=false (api_dump can remain enabled independently). Build instance info.
mValidation true chains debug messenger create info with ALL four severity levels,
general/validation/performance types and vkDebugCallback. vkCreateInstance through
LL_VK_CHECK failure sets error/warns/false. volkLoadInstance; if validation AND debug
messenger function pointer,create persistent messenger; failure only nulls handle,
still returns true. No extension capability enumeration,API-version fallback or
layer enumeration VK_INCOMPLETE/error handling in local body.
**2.** Native instance/layer negotiation and diagnostics. **3.** Explicit selected
capability/error reporting and actual layer execution evidence, not debug-setting
truthiness. Separate optional API dump from validation in eventual reviewed design;
do not let diagnostic configuration silently exhaust logs or claim validation when
missing. Test environment must capture loader/layer output and report downgrades.
Checks: loader/layer absent,api_dump only,validation available,unsupported extension/
API version,instance failure,messenger failure and validation callback delivery.
Outgoing: Volk functions,enumeration/instance/messenger Vulkan API contracts,
LL_VK_CHECK,debug callback,Context destroy and enabled layer implementations.

## UI-NATIVEBOOT-003: queryClientSize

Source: [llvksession.cpp](../../../indra/llvulkan/llvksession.cpp#L93).
**1.** Outputs0; window nonnull and getSize true -> unsigned max(1,size.x/y).
Failure preserves0. Does not query swapchain-negotiated extent. **2.** CPU surface
size request. **3.** Keep requested client and actual swapchain extents distinct
through native view/clip/capture,as current GL test's outer/client difference shows.
Check minimized/zero/getSize failure and surface extent clamping.
Outgoing: platform getSize,math conversions and createSwapchain consumption.

## Executed current-GL startup observation

2026-09-10, original viewer executable in one test viewer process (PID24624),
fresh isolated roaming/local/temp directories under logs/native-ui-startup-20260910.
No credentials/account settings copied; AutoLogin=false. NVAPI application-profile
modification disabled, crash logger disabled. Environment overrides were child-process
specific; the agent's/normal user's environment was not changed. PRELOG verifies
the isolated APPDATA and LOCALAPPDATA, and viewer log paths confirm isolated user/
cache directories. This is checkpoint-derived current-code evidence, NOT execution
of the historical GL oracle and NOT an oracle/tolerance update.

| Observation | Result |
|---|---|
| Executable | build-vc170-64/newview/RelWithDebInfo/vulkanstorm-bin.exe |
| Executable SHA256 | 98D5AB8729792CC7F4BF51F1F7EAE8B1CF439673AD1EA461A64F4DF02E9B75FB |
| Requested backend | OpenGL |
| Effective provider | Native OpenGL, System32 (viewer log) |
| GL vendor/device | ATI Technologies Inc. / AMD Radeon RX 9070 XT |
| GL version | 4.6.0 Compatibility Profile Context 26.9.1.260826 |
| Requested window | 1024x768, windowed, not maximized |
| Captured raw client extent | 1008x729 |
| Capture time | 2026-09-10T16:52:24Z |
| Raw format/size | little-endian U32 width,height + bottom-origin RGBA8; 2,939,336 bytes |
| Capture SHA256 | 8B4FACA1F7B1EF46A715CE2AD2DCF730304475CBB9400446D835C3E3072DB50B |
| RGB/alpha ranges | RGB0..255; alpha0..255; nonconstant, not a clear-only buffer |
| Startup state | LOGIN_SHOW then LOGIN_WAIT, no account login |
| Termination | QuitAfterSeconds45 fired16:52:58Z; normal cleanup/Goodbye16:52:59Z; exit0 |
| External deadline | 180 seconds, not reached; no forced process termination |

Artifacts (local generated evidence, not approved/committed golden assets):
[raw capture](../../../logs/native-ui-startup-20260910/gl-startup.rgba),
[RGB preview](../../../logs/native-ui-startup-20260910/gl-startup-rgb.png),
[directory prelog](../../../logs/native-ui-startup-20260910/directory-prelog.txt),
[viewer log](../../../logs/native-ui-startup-20260910/roaming/Vulkanstorm_x64/logs/Vulkanstorm.log).
The PNG flips rows and visualizes RGB with opaque preview alpha; original RGBA and
its hash are unchanged. Preview inspection shows menu,login fields/button/logo and
first-run antivirus notice with styled text/link/icon/button. Web-login background
is black at capture. This is not an unobstructed settled-login golden or proof of
web/media completion. No interaction or authentication scenario is qualified.

Exact arguments:

```text
--set RenderBackend OpenGL --set AutoLogin false
--set NvAPICreateApplicationProfile false --set DisableCrashLogger true
--set AllowMultipleViewers true --set FullScreen false --set WindowMaximized false
--set FirstLoginThisInstall false --set WindowWidth 1024 --set WindowHeight 768
--set ShowConsoleWindow false --quitafter 45
```

VULKANSTORM_CAPTURE points at the unique raw output; PRELOG at directory-prelog.txt;
APPDATA,LOCALAPPDATA,TEMP,TMP point at the precreated isolated directories.
No --set unknown-name warnings appeared for these arguments. Ordinary startup did
attempt its default external resources; the log reports missing first-run optional
settings,duplicated absolute user-settings paths,certificate-expiration warnings,
HTTP403 for the defaults fetch,a proxy warning and no-region audio asset errors.
These are observed limitations, not hidden by the successful exit or changed here.
Cleanup reports no per-account save because no account name is known.

First preview conversion using interpreted per-pixel PowerShell was interrupted;
a compiled byte-conversion loop then completed and the preview was actually inspected.
No native GPU validation-layer execution or GL/native image comparison occurred.
Native implementation/lifecycle and historical-oracle qualification remain required.

## UI-NATIVEBOOT-004: LLViewerWindow constructor pre-session font block

Source: [llviewerwindow.cpp](../../../indra/newview/llviewerwindow.cpp#L1979),
[unconditional font setup](../../../indra/newview/llviewerwindow.cpp#L2114).
Block record only: remainder of constructor/Params/member initialization is OPEN.
**1.** Construct window/viewer/stats listeners and notification channels BEFORE
platform window; set notification ignore policy. CreateWindow with dimensions,
fullscreen/headless/vsync,pixel-depth/core/version/cursor settings. Null -> translated
splash warnings,5-second sleep,fastQuit1; otherwise shader setup only if not already
initialized AND !skipGLContext. Restore error trap,set min size,get size. Optional
first-run scale reset; compute clamped settings*system UI scale,aspect-ratio scale,
set LLUI scale and increment FontGL resolution generation. Query actual window
size,assign raw and rounded scaled rects. UNCONDITIONALLY LLFontManager::initClass,
LLImageGL::allocateConversionBuffer,LLFontGL::initClass(DPI,scaleX,scaleY,RO dir,
font settings file,size adjustment). Only following VBO/gGL initialization is gated
by !skipGLContext. Font setup can therefore create glyph GL textures with no GL
context and before initWindow assigns completed gViewerWindow or starts native session.
Runtime below reaches SansSerif Small creation then GLImage initialization assertion.
**2.** Native window/input coordinate setup plus independent CPU font/default service
and completion-safe native glyph publication. UI declaration construction cannot
depend on GL font ownership or a not-yet-created native device.
**3.** Lifecycle selection must establish neutral CPU font metrics/fallback before
native controls, then native resource uploads after device readiness. Do not suppress
the assertion,mark GL initialized,set mIsDisabled or skip font loading as the requested
solution. Those leave default Params/measurement/widget behavior unimplemented.
Tests must cover actual constructor/default-font/text measurement with GL API traps
and later native glyph publication, not just successful window creation.
Checks: native route first default font request,GL route unchanged,first-run DPI,
empty font/missing file,allocation failure and partial window/service teardown.
Outgoing: listeners/channels,window manager/constructor,input sizing,settings callbacks,
scale setters,FontManager,ImageGL conversion buffer,FontGL init/registry/FreeType/atlas
chain already locally recorded in font reports; complete transitive closure still open.

## Executed native startup failure

2026-09-10T16:57:23Z started same executable SHA256 as GL observation,PID24868,
fresh isolated profile under logs/native-ui-native-20260910,with native backend and
RenderVulkanDebug=true. Autologin and NVAPI profile modification remained disabled;
same window settings/45-second quit request. No VULKANSTORM_CAPTURE: existing native
swapchain readback ownership remains unverified and was not exercised.

Vulkan SDK1.4.357.0 explicit layer manifests were present in Windows registry.
Process-local VK_LOADER_LAYERS_DISABLE=VK_LAYER_LUNARG_api_dump and
VK_LOADER_DEBUG=error,warn,layer; removed inherited layer-enable/allow lists for this
child only. Loader's documented explicit-layer filtering was consulted; output
confirms api_dump was forced disabled. No system loader/registry changes made.

Observed sequence:

1. Native OpenGL provider selection still logged System32 before native backend
   selection. This is library/provider selection, not proof a GL draw/context ran.
2. Vulkan probe detected AMD Radeon RX9070XT among2 devices; effective backend Vulkan.
3. Win32 window explicitly logged skipping GL context creation.
4. FontRegistry began SansSerif Small style0 creation,then
   LLImageGL::createGLTexture asserted gGLManager.mInited at line1543.
5. OSMessageBox displayed fatal crash notice; startup did not reach frame/idle timed
   quit or LLVKSession::start. External180-second deadline was reached; the test
   process tree alone was killed,exit-1. This was NOT normal cleanup or a passed test.

Artifacts:
[viewer log](../../../logs/native-ui-native-20260910/roaming/Vulkanstorm_x64/logs/Vulkanstorm.log),
[loader/stderr](../../../logs/native-ui-native-20260910/loader-stderr.txt),
[stdout](../../../logs/native-ui-native-20260910/process-stdout.txt),
[directory prelog](../../../logs/native-ui-native-20260910/directory-prelog.txt).
Layer manifest discovery in the early probe is NOT execution of the Khronos
validation layer for a native session. No native device/session/rendering or native
GPU validation gate passed. The failure provides runtime evidence for the source
font-construction coupling, not a complete stack trace or transitive coverage claim.
Future bounded failure probes should use the existing QAModeTermCode facility after
its callback configuration is verified,avoiding an unattended crash dialog; that
diagnostic exit policy is not a functionality fix.

## Required direction: native equivalent of LLFontGL

User clarification, 2026-09-10: do not touch the OpenGL implementation. Build a
native-Vulkan equivalent of LLFontGL with independently owned dependencies.
Affected invariants: NV-00,NV-01,NV-03,NV-06,NV-12,NV-14,NV-17.

The proposed shared CPU atlas extraction and lazy GL page mirrors are REJECTED.
The attempted patch failed; a subsequent diff confirmed no changes to LLFontGL,
LLFontFreetype or LLFontBitmapCache implementation/header files. Do not resume that
patch or modify GL upload timing,atlas ownership,texture identity,retained-buffer
behavior,default-font construction or assertions as part of native development.

### Three architectural questions

1. **What is the OpenGL function doing?** Use the individual source-backed records
   in [font dependencies](native-ui-dependencies.md),
   [font lifecycle](native-ui-font-lifecycle.md) and
   [submission](native-ui-submission-dependencies.md). Recover font resolution,
   fallback,metrics,source indices,rasterization,color/alpha,layout,decorations,
   caching and lifetime. The GL implementation remains the unchanged reference;
   unresolved callees and runtime behavior stay explicitly open.
2. **How is this done in Vulkan?** An independent native font service owns font
   bytes,faces,metrics,fallback and CPU glyph preparation. Native atlas/image owners
   handle uploads,readiness,descriptor lifetime and completion-based retirement.
   Native rendering consumes prepared text and native resource versions without
   invoking GL draw callbacks or constructing GL font/image/atlas owners.
3. **What is the cleanest implementation of question 2 in terms of Vulkan?** Build
   a native equivalent of LLFontGL's observable responsibilities,not a subclass,
   wrapper,backend switch inside LLFontGL or call-for-call GL translation. Separate
   CPU font/layout ownership from native GPU publication inside the native design.
   Under clarified NV-01, the existing GL-exclusive font/layout/rasterization
   functions cannot implement the native path. FreeType itself must be assessed
   separately from LLFontFreetype and LLFontBitmapCache: independently audited
   API-independent library functionality is not categorically forbidden. This is
   neither approval of a particular integration nor permission to share GL-owning
   wrappers, global font state or visual callbacks. Nonvisual services may be shared
   after their own audit. Do not modify GL to create shared font infrastructure.
   Function boundaries need not match GL when native ownership is clearer. Concrete
   APIs remain to be established through per-function NV-00 work.

Required checks: native font construction/measurement without a GL context or GL
owner dependency; font fallback,metrics,glyph/atlas contents and source-index fixtures;
native upload/publication/retirement and descriptor reuse under actual validation;
native startup through control construction and teardown in the original viewer
process. Preserve GL source and verify its behavior independently. A window opening,
a missing-font guard or disappearance of one assertion is not completion. This
direction is a requirement,not a claim that the native equivalent is implemented.

## Architecture boundary still to close

### Current verification status (2026-09-10)

The requested full dependency replacement is NOT closed. Independent native widget
construction, application startup routing, presentation integration, underline,
remaining layout/asset failure cases and end-to-end reference qualification remain
open. Passing the tests below must not be used to mark those requirements complete.

Latest executed checks (MSVC 14.44, RelWithDebInfo):

- `INTEGRATION_TEST_llvkfontface`: eighteen CPU cases passed. The real packaged
   Twemoji test rendered every distinct glyph reached by its active character map:
   1423 mapped glyphs, including 1413 color rasters, with premultiplied channel bounds.
   This covers the packaged asset at the tested size, not arbitrary SVG documents or
   byte-exact parity with the historical GL oracle. Packaged Inter weight tests and
   independent native font-registry construction tests also passed.
- `INTEGRATION_TEST_llvkglyphupload`: six cases passed on RX 9070 XT with Khronos
   validation and synchronization validation. Graphics cases verify analytic pixel
   results for scissor/projection, texture transform/repeat addressing, RGB/alpha
   blending, depth rejection/disabled depth writes, hard/soft shadows and synthetic
   bold suppressing shadows. Caller pipeline/image references are released before
   submission; the fenced owner retains draw resources through completion.
- A completion-observation test intermittently returned Pending immediately after
   queue-idle success with the OBS implicit layer present. A run without OBS passed,
   which is not proof of causation. Tests now wait on each owner's actual fence via
   explicit timeout-aware wait(), rather than infer completion from queue-idle.
   All six cases then passed with OBS present. Streaming poll() remains nonblocking;
   explicit waits are for bootstrap/tests/teardown, not per-frame streaming policy.
- Graphics SPIR-V is generated from source and embedded in llvkglyphgpu; builds now
   require glslangValidator for this native target. No external runtime shader path
   can select a stale graphics variant. Integration into supported build/packaging
   environments still needs full project qualification.

These results advance native components and do not fix the existing LLViewerWindow
startup assertion: the application still constructs GL-owned visual consumers.

### Native font declaration resolution (2026-09-10)

NV-00/01/03/12/17; reference as below. Q1 roots: LLFontDescriptor::normalize,
font_desc_init_from_xml/init_from_xml, getClosestFontTemplate, createFont/getFont,
LLFontGL::getStyleFromString and LLXMLNode::getTextContents. Recover sequential
legacy-name size/style stripping, exact platform sections, later-file prepending,
style subset selection with bold tie-break, default/ultimate fallback appending,
per-file directory search and size modification/delta. Fallback scale is 1 in the
active source. File predicates are the finite emoji policies already implemented.
Q2: LLVKFontRegistry owns parsed declarations, configuration and native font cache;
it receives declaration bytes and ordered search directories from native startup.
Q3: Expat performs nonvisual XML parsing into native-owned nodes; interpretation is
independent of LLXMLNode/LLInitParam/GL registry. No external entities/DTDs; bounded
document/node/depth limits and callback exception containment. Standard filesystem
IO reads font bytes, then native faces validate them; fonts are published only after
native glyph priming succeeds. Registry destruction does not invalidate retained
native font owners. No static default-font construction or settings callbacks.
Checks: platform choice, missing-file fallback, later-document precedence, size
adjustment, legacy normalization, cache identity, XML rejection and owner lifetime.

Remaining details requiring explicit closure: the source collection loop repeatedly
opens face zero because loadFace ignores face_n; this native resolver loads that
face once, preserving ordering of distinct files but not duplicate owners.
Filename resolution concatenates UTF-8 directory and declaration text literally,
preserving the source search boundary even for absolute-looking filenames.
Malformed numeric declarations fail transactionally rather than retaining source
defaults. Alias-face sharing is deliberately not reproduced as mutable shared
tabular state. These are recorded differences, not a blanket parity claim.

### Native graphics text consumer (2026-09-10)

NV-00/01/06/11/12/13/14/16/17, same reference/configuration. Q1 roots:
LLFontGL::renderTriangle/drawGlyph, LLRender::setSceneBlendType(BT_ALPHA)/blendFunc,
and interface/uiV.glsl/uiF.glsl. Position uses MVP, UV uses texture matrix, normalized
byte color multiplies the texture. Both RGB and alpha use SRC_ALPHA and
ONE_MINUS_SRC_ALPHA. Synthetic bold emits two quads and suppresses shadows; soft
shadow emits five offsets then normal, hard emits one then normal. Color glyphs
use white RGB with the supplied alpha. The source triangle helper ignores slant.
Q2: LLVKTextPipeline owns an explicit render pass/pipeline and embedded SPIR-V;
LLVKTextDraw prepares bounded immutable vertex data from native atlas placements.
Q3: explicit projection/UV matrix, target extent/scissor, quantized color, depth
coordinate, synthetic style and shadow strength. The fenced native submission owns
draw buffers, framebuffer, pipeline and glyph images through completion. The caller
must retain the target image/view and establish COLOR_ATTACHMENT_OPTIMAL/read-write
readiness before recording. This version is the UI overlay pass without depth
attachment; world-label depth and underline remain to be implemented, not silently
claimed. Source-controlled shaders compile and embed in the native library.
Check: actual graphics rendering/readback under validation, analytic color/alpha,
clip/coordinate tests and early release of caller pipeline/font/atlas owners.

Depth policy extension: source depth is inherited from the explicit view (NV-05,
NV-11/12), not a property of a glyph. The pipeline now accepts a selected-device
depth format, comparison and write policy; absence of a depth attachment disables
depth testing. Record rejects pipeline/target mismatches. The caller establishes
DEPTH_STENCIL_ATTACHMENT_OPTIMAL and retains the attachment through completion.
Render-pass dependencies include depth readers/writers; no global GL depth state
is consulted. Test depth rejection and preserved depth-write policy separately
from the already-passing overlay test. Underline still needs its native primitive.

### Connected native glyph validation (2026-09-10)

Latest implementation evidence supersedes earlier statements that no native GPU
glyph operation exists, but does not close application startup or full UI parity.

- `INTEGRATION_TEST_llvkfontface`: fifteen CPU TUT cases pass, including native
   SVG, source-derived measurement, draw alignment/ellipsis, wrapping/hit testing
   and immutable atlas preparation. Locale/CJK and mixed-face edge cases, complete
   SVG support, and allocation failure injection remain qualification gaps.
- `INTEGRATION_TEST_llvkglyphupload`: one GPU TUT case passes on AMD Radeon RX 9070
   XT. It constructs a font from packaged Roboto bytes, lays out `A VA`, prepares a
   32x32 immutable atlas, uploads it, releases producer data, and samples all pixels
   off-center using the production nearest sampler and a compute consumer.
   All 4096 readback bytes match the prepared page. A second uploaded image is
   retired without affecting the first. Two native LLVKGlyphSubmission owners retain
   the first image independently; first-owner retirement does not release the
   second owner's image. Duplicate submission and malformed payload/queue inputs
   are rejected. No validation errors were reported, including teardown.
- Vulkan SDK 1.4.357.0 loader diagnostics explicitly show instance and device
   insertion of VK_LAYER_KHRONOS_validation. The test enables synchronization
   validation through VkValidationFeaturesEXT. API-dump was disabled in the child
   process; AMD switchable-graphics and OBS implicit layers were also present.
   This is actual native GPU execution, not validation manifest discovery.
- GPU tests are opt-in via `-DLL_VULKAN_GPU_TESTS=ON` with LL_TESTS=ON. Build
   `INTEGRATION_TEST_llvkglyphupload` in RelWithDebInfo to compile its SPIR-V from
   [glyph_readback.comp](../../../indra/llvulkan/shaders/glyph_readback.comp) using
   glslangValidator with Vulkan 1.1 target, then execute the test through the existing
   TUT harness. The test requires a graphics+compute queue and validation layers.
- Both native font and glyph GPU libraries compile. `llvulkan` also builds using
   `/p:BuildProjectReferences=false`; the previously recorded unrelated llui test
   compilation failures still prevent claiming a full dependency/viewer build.

This does NOT establish final graphics blending, text decoration, depth/scissor
semantics, presentation/WSI, historical-oracle image parity or native control
construction. The test uses queue-idle waits as bounded diagnostics, not a proposed
streaming schedule. VMA flush/invalidate calls execute, but this device's chosen
host memory is not proven noncoherent; that capability-specific case remains open.
Residency/descriptor pooling across many pages and upload batching remain required
performance/resource work. The application still constructs GL-owned widgets and
LLVKText still accepts LLFontGL pointers; no late conversion or skipped GL init
has been introduced to conceal those remaining startup dependencies.

### Native text range consumers (2026-09-10)

NV-00/01/12/17, same reference/configuration. Q1 roots: LLFontGL::maxDrawableChars,
firstDrawableChar and charFromPixelOffset, plus iswindividual in llstring.cpp:86.
Fit tracks source word-start state (including NBSP and CJK/Hangul range handling),
clips overhang before kerning and rolls back according to three wrap policies.
Backward visibility uses last-glyph ink width, previous advances and unrestricted
pair kerning; no-fit preserves the supplied start. Hit testing uses half/full
advance thresholds, strict comparisons, relative returned offsets and count-1
iteration bounds; unlike measurement it does not exclude next characters >=255
from kerning. Q2: native font methods return source ranges from owned native glyphs,
with explicit scale, width and tabular policy. Q3: bounded string views and checked
inputs; no GL wrapper, widget or registry callbacks. Fixed individual-character
ranges are native data. Standard iswspace/iswpunct preserve the platform predicate
semantics, including wint_t conversion; locale must be pinned for parity. Invalid
backward start positions fail explicitly instead of returning out-of-range indices.
Check: exact ink boundary, half-advance equality, count-1 bound, relative offsets,
backward selection and word-boundary rollback. CJK/punctuation, locale variants and
cross-face fixtures remain qualification obligations, not inferred coverage.

### Immutable atlas preparation (2026-09-10)

NV-00/01/06/11/12/14/17; same reference/configuration. Q1 roots:
LLFontBitmapCache::nextOpenPos, grayscale clear(255,0), color row-copy and
LLFontGL::render UV calculation (PAD_UVY=.5). Separate glyph types, one-pixel
packing gaps, white-RGB/zero-alpha coverage background and half-pixel vertical UV
extension affect sampling. Q2: LLVKGlyphAtlas prepares immutable RGBA pages from
native LineLayout, with ordered placements and retained glyph ownership. Q3:
encoding-separated shelf packing, duplicate-owner reuse, checked payloads/dimensions,
caller-specified byte budget capped at 64 MiB, no live-page updates or repacking.
Color gutters are deterministically transparent black; source uninitialized color
padding is not promoted to a parity contract. Coordinates/orientation and point
sampling stay explicit. Check coverage expansion, gutters, repeated glyph UVs,
zero-ink placements, budget failure and lifetime; Vulkan test consumes these pages.
Packing representation differs from the source and still requires visual parity
qualification; a stable prepared page is not proof of full native UI completion.

### Immutable glyph uploads (2026-09-10)

NV-00/01/03/06/13/14/15/17. Q1 source roots: LLFontFreetype::addGlyphFromFont
publishes CPU atlas contents through LLImageGL::setSubImage; existing native
LLVKContext::createTexture2D/updateTexture2D wait per upload, omit explicit mapped
flush and expose mutable images and unreclaimed descriptor slots. The dependency
is initialized, sampleable image data with valid resource lifetime, not GL calls.
Q2: LLVKGlyphUpload owns one immutable RGBA8 image version, staging allocation,
command pool and fence. It queues a copy without a host wait, flushes via VMA,
transitions UNDEFINED -> TRANSFER_DST -> SHADER_READ_ONLY, and publishes an owning
image reference only after fence completion. Image/view/sampler/descriptor ownership
is one unit; the descriptor pool has one set and is reclaimed with that version.
Q3: small explicit upload owner, actual selected-device format/extent checks,
transactional partial-failure cleanup and no mutation API. Caller must externally
synchronize the supplied graphics/compute queue; device and allocator outlive all
owners. Normal poll retires staging/pool only after successful completion. Pending
owner destruction waits only as cancellation/teardown; unexpected wait failures
attempt device-idle recovery, then terminate rather than free live resources.
Device-loss cleanup remains distinct from successful publication.

The future draw submission MUST retain the published image until every consuming
submission completes; shared ownership alone is not that proof. This uploader
does not yet implement frame retention, an atlas budget, upload batching, WSI or
native UI routing. The per-upload bound is 64 MiB, not a global residency budget.
RGBA8 is a declared byte payload (coverage expansion belongs to its producer),
not automatic sRGB conversion. TRANSFER_SRC usage supports diagnostic readback.
Sampler uses nearest min/mag with no mip levels beyond zero, matching
LLFontBitmapCache::nextOpenPos's TFO_POINT contract (llrender.h:89). The test samples
off-center so linear filtering cannot pass merely by hitting exact texel centers.
Addressing is REPEAT, matching LLImageGL initialization's TAM_WRAP (llimagegl.cpp:566),
which font atlas creation does not override. Both sampling and graphics tests add
whole texture periods to UVs, distinguishing repeat from accidental clamp-to-edge.
Check: independent Vulkan test with actual validation, immutable upload/readback,
caller-byte release, unpublished-before-completion, lifetime and invalid inputs.

Consumer-retention operation: Q1 source retained GL draws keep raw texture names,
whereas Vulkan objects must remain alive through all commands referencing them.
Q2: LLVKGlyphSubmission owns command pool/buffer/fence and immutable image references
from recording through completion. Q3: begin accepts only published same-device,
same-queue images; cross-queue semaphore/ownership support is not implemented.
One-shot submit closes recording, forbids resubmission, and
poll releases images only after its own fence signals. Multiple submissions hold
independent references. Queue access remains externally synchronized; the queue
family must support both graphics and compute, as checked against the physical
device. No WSI semaphores are supplied by this operation: it currently serves
offscreen consumers, not presentation. Pipelines, target images and non-glyph
buffers remain the recording caller's lifetime responsibility. The runtime test
now uses this production owner instead of manually retaining glyph references.
Two recorded and submitted consumers sample the same glyph atlas; retiring the
first owner must leave the image alive until the second completion is observed.

### Native draw layout (2026-09-10)

NV-00/01/06/11/12/17; same source/configuration as the face record. Q1 roots:
LLFontGL::render(float x/y), lines 159-461, getWidthF32, getXAdvance(pointer),
getXKerning and llmath.h's active ll_round = floor(value + .5). Draw layout scales
coordinates, floors the explicit origin, applies ceil-based vertical metrics,
clamps alignment width and uses integer division for horizontal center. It clips
ink against rounded start plus available width before digit centering, snaps glyph
rectangles, advances and kerns using the next source character even outside the
requested range. Ellipsis overflow reserves the rounded logical width of FOUR
dots from the physical budget, then lays out THREE at the resulting pen with the
original width constraint and vertical anchor. Measurement stops at NUL; the draw
loop traverses the supplied length. That distinction is preserved.
Q2: LLVKFont::layoutLine returns owned raster references, absolute source indices,
physical glyph rectangles, source character count, baseline/end pen and logical
rightX. Ellipsis glyphs are marked synthetic and anchored at the omitted range.
Q3: explicit immutable inputs, bounded string view and error-returning preparation;
no GL matrices, texture binding, queue work or widget callbacks. The result is the
native text consumer input, not a GL draw forwarding layer. Depth, clip, color and
decorations belong to explicit downstream draw data. Invalid coordinates/scales
fail instead of reproducing undefined integer conversions.
Check: source-derived snapping, outside-range kerning, odd-width centering,
top/bottom alignment, clipping and ellipsis markers/count. This operation is not
paragraph wrapping, editing/hit-testing or GPU publication; those remain open.

### Native SVG operation (2026-09-10)

NV-00/01/06/12/17; reference `3abd661f498329babaf87b49ffab910fcb5f0e6c`,
Windows RelWithDebInfo. Q1: all five callbacks in
[llfontfreetypesvg.cpp](../../../indra/llrender/llfontfreetypesvg.cpp) parse a
mutable document copy with NanoSVG, fit floored dimensions to x/y ppem using the
minimum scale, center horizontally and use the ascender baseline, then rasterize
straight RGBA and convert to premultiplied BGRA. Preset errors must survive until
render; the source's glyph finalizer does not free a still-parsed image.
Q2: native FreeType libraries install independently owned native hooks. NanoSVG
is an independently assessed CPU parser/rasterizer, not the GL-owned viewer hook.
Its public symbols are renamed with a native prefix, so they cannot resolve to
llrender's NanoSVG copy. Parsing uses memory, not file IO,
custom streams, external images or viewer callbacks. Q3: library-owned RAII state
holds a parsed image between preset/render, keyed by slot and glyph index. A fresh
preset replaces state; cached preset preserves metrics and errors; shutdown frees
even an unrendered document. Explicit byte-wise premultiplication avoids host
endianness assumptions. Face locking serializes hook use. CPU output feeds the
existing native owned-pixel result; no GPU work is performed in callbacks.

Check: callback tests exercise dimensions, cached metrics, exact half-alpha color,
negative-transform rejection, error propagation, recovery and unrendered cleanup;
an actual SVG font-table test must verify FreeType integration. Unsupported outer
transforms and multi-glyph documents return explicit errors, not blank success.
Documents are limited to 4 MiB and glyph bitmaps to 4096 per axis and 4 Mi pixels.
The native NanoSVG copy routes malloc/realloc/free into a per-library allocation
domain with a 64 MiB live-byte cap (including transient replacement allocations).
Failure throws inside C++-compiled NanoSVG and is caught at each FreeType hook;
the domain releases all allocations, including partially constructed parser or
rasterizer data, before reporting FT_Err_Out_Of_Memory. No blank glyph is published
on a library allocation failure. Thread-local dispatch is scoped to the current
locked face, not a shared font owner. NanoSVG's supported SVG subset and allocation
failure injection remain qualification limits; no full SVG standard coverage or
production closure is claimed. Rejecting
negative translation fixes the source's accidental acceptance, not measured parity.

Verification update: the native font target executes eleven TUT cases successfully
with MSVC 14.44 / RelWithDebInfo / LL_TESTS=ON. The SVG integration case constructs
a checksummed SFNT in memory using FreeType table extraction and an authored SVG
document; it verifies actual FreeType hook invocation through LLVKFontFace,
premultiplied red/opaque blue, bottom-up rows, repeated rendering, two independent
library owners and pixel lifetime after teardown. Hook tests also exercise cached
metrics, invalid dimensions, document/bitmap limits and unsupported transforms and
multi-glyph documents. Measured-run tests exercise source indices, range limits,
NUL, padding, fractional scale, tabular policy and owned glyph lifetime. These are
CPU functional checks, not historical GL image parity or Vulkan layer execution.

The earlier seven-test record below remains dated evidence for the preceding
increment. Native SVG single-glyph identity-transform support and measured runs
now exist; full SVG qualification, full text layout, independent UI construction,
startup consumer routing and GPU glyph publication remain incomplete.

### Native face ownership operation (2026-09-10)

Measured-run operation (2026-09-10, same reference): Q1 roots are
LLFontGL::getWidthF32 (539), LLFontFreetype::getXAdvance(pointer) and getXKerning.
The requested source range terminates at NUL, carries nonnegative overhang padding,
adds advance then kerning only for next characters below 255 within the range,
rounds after kerning and divides by horizontal UI scale. Tabular pointer advances
and disabled digit kerning require a positive weight. Q2: measureRun produces an
owned native run with absolute source indices, physical-pixel pen origins, digit
centering offsets, advance, padding and scaled width, without graphics state.
Q3: immutable font settings, bounded input view and overflow-safe range arithmetic;
prime the complete digit set before tabular measurement. Keep GL's primary-face
kerning-index behavior explicit even for fallback glyphs; invalid face-local indices
fail instead of silently changing the result. Cross-face fixtures remain required.
Check: range limits, NUL, padding, fractional scale, rounding, digit policy and
run lifetime after font destruction. This is measurement preparation, not full
text layout: rendering's outside-range lookahead, alignment, ellipses, line wrapping,
selection, hit testing and glyph decoration still require their own consumers.

Reference: `3abd661f498329babaf87b49ffab910fcb5f0e6c`, Windows RelWithDebInfo;
source roots `LLFontManager` construction/destruction, `LLFontFreetype::loadFace`,
`setVariationAxis` and metric accessors in
[llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp).
Affected NV-00/01/03/06/12/17. This is CPU dependency replacement, not R1 closure.

1. GL behavior: the manager initializes FreeType and installs viewer SVG hooks;
   loadFace opens memory at face zero, applies clamped wght then opsz only when
   weight is nonnegative, truncates point size to 26.6 and DPI to unsigned integers,
   derives metrics from font units, selects an existing/first charmap, classifies
   bold/italic, initializes an atlas and inserts glyph zero for nonfallback faces.
   That last preparation reaches GL texture creation. GL remains untouched.
2. Native design: `LLVKFontFace` owns copied bytes, a private library and face. Its
   immutable metrics are CPU results, not textures; no GL object, global library,
   default-font getter or viewer callback is reachable. Native glyph preparation
   and GPU publication are separate operations, still open at this step.
3. Clean implementation: transactional factory plus RAII, no mutable global state
   or exposed FreeType handle. Destroy face before library and bytes. A library
   per face avoids cross-owner initialization/destruction locks initially; pooling
   requires measured justification. Actual variation API errors fail explicitly;
   absent axes are supported. Invalid inputs and nonscalable metrics fail explicitly
   rather than propagating undefined conversions/division. Allocation exceptions
   propagate with RAII cleanup. Existing objects remain valid after another fails.

Independent library audit: FreeType's documented memory-face API uses caller-owned
bytes until face destruction; library instances are independent. This operation
uses default compiled modules, no custom streams/allocators, viewer SVG hooks or
GL callbacks. `FT_Init_FreeType` can read FREETYPE_PROPERTIES, which must be pinned
for parity. See [face ownership](https://freetype.org/freetype2/docs/reference/ft2-face_creation.html)
and [library setup](https://freetype.org/freetype2/docs/reference/ft2-library_setup.html).
This is not a blanket approval of future rasterizer integrations or hooks.

Discriminating check: existing TUT harness links `llvkfont` and FreeType without
llrender/llui/llwindow/Vulkan, opens packaged Roboto bytes, compares source-formula
metrics, tests independent settings and invalid-input recovery. This tests the
CPU operation, not the historical GL oracle or native UI parity. Glyph zero,
fallback resolution, rasterization/SVG, atlas resources and startup consumer
replacement remain open. Native startup must not be declared fixed by this test.

Glyph operation contract (same reference/configuration): `renderGlyph`,
`addGlyphFromFont`, `setSubImageLuminanceAlpha`, `setSubImageBGRA` and `getXKerning`
provide the local source. Q1: load with configured hinting plus requested color,
retry toggled color after load failure, select glyph zero for invalid outlines or
emoji failures, otherwise try question mark; rasterize NORMAL; capture bearings,
advances and side-bearing deltas; expand mono, reverse rows, swizzle BGRA to RGBA.
`NO_HINTING` is actually 0x8000 (FT_LOAD_NO_AUTOHINT), not FT_LOAD_NO_HINTING.
Kerning uses UNFITTED/64 plus strict >32/-31 side-bearing thresholds. The active
LLStringOps::isEmoji predicate is [0x1f000,0x20000), without callbacks.
Q2: native face returns an owned CPU Glyph with bottom-up Coverage8 or
PremultipliedSrgbRgba8, original and rendered indices, metrics and retry status.
No atlas allocation/upload occurs. Q3: serialize access to the private mutable
FreeType face, copy results out and keep metrics immutable. Kerning indices are
face-local; fallback ownership and tabular-digit policy belong to the future font
service. Errors replace fatal GL/UI callbacks; unimplemented/SVG and allocation
errors remain explicit, not disguised by fallback. Negative pitch uses FreeType's
signed downward row offset; padded color rows respect pitch rather than the GL
helper's tightly-packed assumption. These are defined native safety policies, not
claims of parity with unsupported or undefined source inputs.

Check: compare normal rasterization with an independent FreeType face for all
three hinting modes, fractional size/DPI, ASCII, space, accented and missing glyphs;
exercise threshold boundaries and retain results after face destruction. Color,
mono, negative pitch and damaged-font retry branches still require dedicated
fixtures. The existing FreeType binary also requires libpng and zlib-ng, declared
on the native font target without linking viewer image/GL owners. GPU consumers
must preserve the declared encoding and orientation; no GPU parity is claimed.

Native font ownership contract (same reference/configuration): Q1 roots are
`LLFontFreetype::addGlyph` and `LLFontGL::generateASCIIglyphs`, plus the three
registered predicates at llfontregistry.cpp:53-78. Primary glyph wins; for a missing
genuine emoji, eligible predicate fallbacks precede unrestricted fallbacks, then
all predicate fallbacks are retried ignoring predicates. For other missing glyphs,
unrestricted precede predicate fallbacks. Complete miss selects primary glyph zero.
Registry misses prime ASCII 32..126; the primary load also inserts glyph zero.
Q2: `LLVKFont` owns primary/fallback `LLVKFontFace` instances and native glyph cache;
predicate variants and the monochrome preference are explicit data. Constructor
primes glyph zero and printable ASCII entirely on CPU. Cached results own their
pixels, include the producing face index and remain valid after font destruction.
Q3: build all owners transactionally before publishing the font; fixed configuration
and serialized cache access avoid callbacks and mutable global state. The native
settings consumer must create a new font configuration version when policy changes;
that consumer is not yet implemented. File resolution must provide already selected
font bytes; this factory does not silently skip malformed supplied fallback faces.
Tests exercise eager startup glyphs, primary/fallback/last-resort selection using
packaged Roboto and Noto, cache identity and failed construction isolation. Genuine
emoji preference needs a suitable rasterizable fixture. Cross-face text layout,
tabular digits, SVG, image publication and startup routing remain open.

Validation, 2026-09-10, MSVC 14.44, RelWithDebInfo, LL_TESTS=ON:

- `cmake --build build-vc170-64 --config RelWithDebInfo --target INTEGRATION_TEST_llvkfontface`
   builds the separate `llvkfont` library and executes all seven TUT cases: passed.
   These are CPU tests against the same packaged FreeType implementation and
   source-derived formulas, not independent historical GL parity measurements.
- Generated test link dependencies exclude llrender, llui, llwindow, llimage,
   llvulkan, OpenGL and Vulkan loader libraries. `dumpbin /dependents` confirms no
   OpenGL/Vulkan loader DLL import. The native font source includes only its native
   headers, standard C++ and FreeType; no viewer visual callbacks are installed.
- The initial isolated link exposed FreeType's PNG dependency, now explicitly
   declared together with zlib-ng on `llvkfont`. Rebuild and tests then succeeded.
- Full `llvulkan` dependency build is blocked by existing llui test compilation
   failures (markdown test definitions and LLUrlMatch::setValues argument mismatch).
   No GL code/test was changed and LL_TESTS remains enabled. Building `llvulkan`
   itself with `/p:BuildProjectReferences=false` against existing dependencies
   succeeded; that is not a successful full dependency build or viewer relink.
- Editor diagnostics report no errors in the new code. Diff check confirms
   llrender, llui and llviewerwindow.cpp remain unchanged.

Integration status: this library is available to the native target but no startup
or UI consumer has been redirected. Existing LLVKText still takes LLFontGL pointers;
its callers receive them from GL-owned widget state. LLViewerWindow's unconditional
font initialization still reaches the previously reproduced GL assertion. Fixing
that requires native construction/startup consumers, not skipping the call or
converting existing GL font owners. The CPU owner is implemented; startup is not
fixed. Cache budgeting/eviction, variable/color/mono/negative-pitch/error fixtures,
full text layout and renderer publication remain required production work.

No separate executable is proposed. The existing main executable's entry must select
an independent lifecycle before LLAppViewer's incompatible initialization, or that
initialization must be split into audited neutral and backend-owned phases. Choosing
between these two designs requires the actual application constructors, configuration
source precedence, static initialization, frame loop, partial failure and cleanup
contracts. This report identifies the boundary; it does not authorize a new late
dispatch wrapper or mark R1 complete. Native image/geometry defects already recorded
remain required work after ownership/capability prerequisites, not reasons to reduce UI
coverage or present a scaffold as the requested production viewer.
