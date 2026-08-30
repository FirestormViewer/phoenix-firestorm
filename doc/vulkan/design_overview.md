# VulkanStorm — Native Vulkan Render Pipeline Design

Status: Draft (design phase)
Target codebase: Firestorm viewer (`indra/`), Windows-first, SDL2/macOS to follow.

## 1. Objective

Provide a **parallel, selectable, end-to-end Vulkan render pipeline** alongside the
existing OpenGL renderer. The user selects the backend in
**Preferences → Graphics → Hardware Settings**; switching backends requires a
viewer shutdown and restart (backend is fixed per process).

Non-goals for the mainline effort: the archived side projects; dynamic in-process
backend switching; mobile/Metal/D3D backends.

## 2. Existing architecture (as found)

| Layer | Files | Notes |
|---|---|---|
| Immediate-mode render/state API | `indra/llrender/llrender.{h,cpp}` | `LLRender` — matrix stacks, vertex submission (`begin`/`vertex*`/`end`), fixed state (blend, depth, cull), render targets. Almost every caller goes through this or raw GL. |
| GL object wrappers | `indra/llrender/llglslshader.*`, `llimagegl.*`, `llvertexbuffer.*`, `llrendertarget.*`, `llcubemap*.*`, `lltexture.*` | Own GL object lifetime; these must gain Vulkan twins. |
| GL context/window | `indra/llwindow/llwindow*.{h,cpp}` | `LLWindow` carries GL-context APIs (`createSharedContext`, `switchContext`, `swapBuffers`). Win32 impl uses WGL; SDL2 and Cocoa similarly. |
| Scene pipeline | `indra/newview/pipeline.cpp`, `lldrawpool*.*` | `LLPipeline::init()`; draw pools submit via `LLRender`/raw GL. Deferred + forward passes, shadow maps, post FX. |
| Startup sequence | `indra/newview/llappviewer.cpp` `LLAppViewer::initWindow()` | Creates `LLViewerWindow` → `gPipeline.init()` → `initGLDefaults()`. **Backend branch point.** |
| Preferences UI | `indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml` ("Hardware Settings" panel) + `LLPanelPreferenceGraphics` in `llfloaterpreference.cpp` | Restart-required UX precedent exists (`textRestartRequired`, skin-change restart dialog). |
| Settings store | `indra/newview/app_settings/settings.xml` | LLSD map entries; accessed via `gSavedSettings`. |
| Vulkan precedent | `indra/cmake/VulkanGltf.cmake` | A `vulkan_gltf` prebuilt binary is already linked — Vulkan SDK/loader presence in the build pipeline is established. |

## 3. Design

### 3.1 Backend selection model

- New persistent setting `RenderBackend` (String, values `"OpenGL"` | `"Vulkan"`, default `"OpenGL"`), declared in `settings.xml`.
- Read once, **before** window creation, in `LLAppViewer::initWindow()`. The backend is immutable for the process lifetime; changing it sets a "restart required" flag and the user must quit (viewer shutdown, not relog).
- If Vulkan initialization fails at startup (no ICD, no device, swapchain failure), the viewer falls back to OpenGL for that session, records the failure reason in the log, and surfaces a one-time notification. The setting itself is only rewritten on explicit user action.

### 3.2 Selector UI (Hardware Settings tab)

- Add a combo box **Renderer:** (items: `OpenGL`, `Vulkan`) bound to a *shadow* control `RenderBackendPending` (so cancel/apply semantics stay clean), in the "Hardware Settings" panel of `panel_preferences_graphics1.xml`.
- On commit of a value different from the active backend: show the existing restart-required text and, on OK/Apply, raise a restart confirmation notification (pattern: `LLPanelPreferenceSkins::callbackRestart`). On confirm, write `RenderBackend` and initiate normal viewer shutdown (`LLAppViewer::requestQuit`).
- Vulkan item is shown but disabled with a tooltip when the system reports no Vulkan-capable device (detected once at panel build via a cheap `vkEnumeratePhysicalDevices` probe).

### 3.3 Code architecture

```
┌───────────────────────── newview (pipeline, drawpools, UI) ─────────────────────────┐
│                        LLRender (backend-neutral API surface)                        │
├───────────────────────────┬─────────────────────────────────────────────────────────┤
│      GL backend           │                Vulkan backend                            │
│  (existing llrender impl) │  indra/llvulkan/                                         │
│                           │   llvkcontext    instance/device/queues/swapchain        │
│                           │   llvkshader     SPIR-V modules, pipelines, descriptors  │
│                           │   llvkbuffers    vertex/index/uniform buffers (VMA)      │
│                           │   llvkimage      textures, samplers, cubemaps            │
│                           │   llvkrendertarget  offscreen images + render passes     │
│                           │   llvkpipeline   pipeline-state cache (PSO key)          │
└───────────────────────────┴─────────────────────────────────────────────────────────┘
              ▲                                                     ▲
        LLWindow (WGL context)                              LLWindow (VkSurfaceKHR)
```

Key decisions:

1. **Runtime-dispatched, not compile-time.** One binary ships both backends; selection
   is the persisted setting. `LLRender` keeps its current API but forwards to a
   backend interface (`LLRenderBackend`). This bounds the blast radius: draw pools,
   UI, and pipeline code change minimally or not at all.
2. **Vulkan lives in a new library `indra/llvulkan`** (CMake target `llvulkan`),
   keeping GL headers out of it and vice versa. Use Vulkan Memory Allocator (VMA)
   and `volk` (or the SDK loader) as new prebuilt dependencies — the `vulkan_gltf`
   prebuilt precedent shows the 3p pipeline supports this.
3. **Shaders**: GLSL sources stay the single source of truth initially, cross-compiled
   to SPIR-V with `glslang` at build time (CMake rule + committed fallback binaries).
   GLSL 4.x → Vulkan semantics adjustments (descriptor sets, push constants,
   `gl_VertexIndex`) are handled by a thin compat header prepended per stage.
4. **State mapping**: `LLRender`'s immediate-mode calls map to a recorded
   command buffer per frame; blend/depth/stencil state changes become PSO-key
   changes resolved from `llvkpipeline` cache; uniform/texture binding maps to
   descriptor sets with per-frame descriptor pools.
5. **Windowing**: `LLWindow` gains a backend-neutral surface hook:
   `createVulkanSurface(VkInstance)` implemented per platform
   (`vkCreateWin32SurfaceKHR` on Win32, SDL_Vulkan_CreateSurface on SDL2, CAMetalLayer
   on macOS later). GL-only APIs (`createSharedContext` etc.) become no-ops/unsupported
   under Vulkan; the one GL shared-context consumer (texture decode threads) gets a
   Vulkan transfer-queue path instead.
6. **Swap**: `swapBuffers()` → `vkQueuePresentKHR` with acquire/present semaphores and
   frame-in-flight fencing (2–3 frames). VSync toggle maps to present-mode selection
   (FIFO vs IMMEDIATE/MAILBOX).

### 3.4 Phased implementation plan

| Phase | Deliverable | Exit criteria |
|---|---|---|
| **0** | `RenderBackend` setting + Hardware-tab selector + restart flow + startup branch that still always takes GL | Selector visible, persists, restart prompt works, GL path unchanged |
| **1** | `llvulkan` library; window backend produces a `VkSurfaceKHR`; frame loop renders a **clear color + trilinear test triangle** through the new abstraction; fallback-to-GL on failure | Viewer launches with `RenderBackend=Vulkan`, renders test frame, clean shutdown |
| **2** | 2D/UI path on Vulkan (UI textures, `LLFontGL` glyph cache, `gPipeline` 2D draws) | Full UI interactive on Vulkan; 3D world view may be blank |
| **3** | Forward-render 3D world parity pass-by-pass (sky, terrain, prims, avatars, alpha) | World visible; per-pass visual diff vs GL on test scenes |
| **4** | Deferred rendering, shadows, PBR materials, post FX (`llpostprocess`), water, reflection probes (`llcubemap*`) | Deferred feature parity; testplans in `doc/testplans/` pass on Vulkan |
| **5** | Hardening: crash handling, `RenderInitError` marker semantics, watchdog interplay, perf tuning, feature-manager gating per backend | Soak testing; no regressions on GL path |

Each phase lands behind the selector; the GL path must remain bit-identical in
behavior at every merge point.

### 3.5 Settings to add (Phase 0)

- `RenderBackend` — String `"OpenGL"|"Vulkan"`, persist 1, default `"OpenGL"`.
- `RenderBackendPending` — String, persist 0, shadow for the combo box.
- `RenderVulkanDebug` — Boolean, persist 1, default 0 — enables validation layers + `VK_EXT_debug_utils` markers.

### 3.6 Risks / watch items

- **Raw GL call sites**: not everything goes through `LLRender`. A codebase-wide audit
  (`gl[A-Z]`, `LLGLSLShader::bind`, direct `glBindTexture` etc. in `newview/`) is a
  Phase-1 deliverable; offenders must be routed through the abstraction or
  backend-gated.
- **Texture threading**: `LLImageGL` + GL shared contexts on decode threads. Vulkan
  has no shared contexts; use a dedicated transfer queue + staging buffers and a
  per-thread command pool.
- **Feature manager**: `llfeaturemanager.cpp` keys capabilities off `gGLManager`
  (vendor, GL version). A parallel `LLVKManager` capability block is needed so
  recommended-graphics-level logic works under Vulkan.
- **Crash/markers**: `RenderInitError` startup marker semantics must cover Vulkan
  init too (Phase 1).
- **macOS**: Vulkan via MoltenVK; out of initial scope, but keep the abstraction
  honest about it (surface hook + portability subset).
- **UI acceptance bar**: exact visual parity per test policy — per-pass
  screenshot diffs, not approximations.

## 4. Immediate next steps (Phase 0 implementation)

1. Add the three settings to `settings.xml`.
2. Add the "Renderer" combo box + restart text to `panel_preferences_graphics1.xml`.
3. Wire commit/apply logic in `LLPanelPreferenceGraphics` (write-through to
   `RenderBackend` only on confirmed restart).
4. Branch point in `LLAppViewer::initWindow()` reading `RenderBackend` (GL-only
   behavior for now; logs selection).
5. Probe function `LLVKProbe::hasVulkanDevice()` in a new `indra/llvulkan` stub library.
