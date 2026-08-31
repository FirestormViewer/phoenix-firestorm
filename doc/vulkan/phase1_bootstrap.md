# VulkanStorm — Phase 1 bring-up notes and raw-GL audit

Status: Phase 1 validated on hardware (2026-08-31)
Related: [design_overview.md](design_overview.md)

## What Phase 1 delivered

- **`indra/llvulkan`** library (CMake target `llvulkan`): the Vulkan render
  backend root. `LLVKContext` owns the instance, physical/logical device,
  graphics+present queues, swapchain, VMA allocator, and a minimal frame loop
  (dynamic-rendering clear). `LLVKProbe` is the real `vkEnumeratePhysicalDevices`
  probe.
- **`3p-vulkan-sdk`** prebuilt package (`anne-skydancer/3p-vulkan-sdk`):
  Vulkan-Headers `vulkan-sdk-1.4.350.1` + volk `1.4.350` + VMA `v3.4.0`,
  headers/source only. volk loads `vulkan-1.dll` from the user's driver at
  runtime, so the viewer still starts on systems with no Vulkan.
- **Selector gating**: the Renderer selector (Hardware Settings) is offered only
  when a Vulkan ICD + physical device is present; otherwise it is hidden and the
  viewer renders OpenGL directly.
- **Startup rule**: `LLAppViewer::initWindow()` re-probes. If `RenderBackend` is
  `Vulkan` but no device is available, the viewer runs OpenGL **for that session
  only** (the persisted setting is left untouched) — no lockout, not silent.
- **Self-test** (`RenderVulkanSelfTest`, diagnostic): creates a dedicated window
  and drives the full Vulkan chain. Validated on an AMD Radeon RX 9070 XT:
  device enumerated, Win32 surface created, swapchain 624x441, 120 clear-color
  frames presented.

## Key architectural finding (corrects the plan)

**A window is bound to one graphics API for its lifetime.** The viewer's main
window is created with an OpenGL surface, and `vkCreateSwapchainKHR` on it fails
with `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR`. Consequences:

- The Phase-1 self-test cannot present on the viewer's GL window; it uses a
  dedicated window instead.
- **Phase 2 must create the viewer window Vulkan-native** when
  `RenderBackend=Vulkan` (no GL context is created on it at all), rather than
  trying to convert an existing GL window. `LLWindow` already exposes
  `getNativeHandle()`/`getNativeInstance()` for surface creation; the Vulkan
  branch must skip `LLVertexBuffer::initClass`/`gGL.init`/`LLImageGL::initClass`
  in `LLViewerWindow::init()`.

## Raw-GL callsite audit (newview/)

Not all rendering goes through `LLRender`; many call sites invoke raw GL. These
must be routed through the backend abstraction or gated per backend before the
Vulkan path can reach parity. Top offenders by raw `gl*` call count:

| File | raw gl* calls |
|---|---|
| `pipeline.cpp` | 70 |
| `lldrawpoolterrain.cpp` | 62 |
| `llviewerwindow.cpp` | 51 |
| `llspatialpartition.cpp` | 24 |
| `llviewerdisplay.cpp` | 21 |
| `llreflectionmapmanager.cpp` | 18 |
| `gltfscenemanager.cpp` | 17 |
| `llmaniptranslate.cpp` | 13 |
| `llmodelpreview.cpp` | 12 |
| `llscenemonitor.cpp` | 11 |

Most-frequent raw calls (need Vulkan twins / state mapping): `glPolygonMode`
(67), `glPolygonOffset` (30), `glClearColor`/`glClear` (43), `glTexGen*`,
`glEnable`/`glDisable`, `glViewport`, `glTexParameteri`, `glReadPixels`, the
occlusion-query family (`glBeginQuery`/`glEndQuery`/`glGetQueryObjectuiv`), and
the stencil functions. These map to PSO state, render-pass clears, descriptor
updates, and `VK_KHR` query objects respectively.

## Next: Phase 2

Decouple the GL init sequence so a `RenderBackend=Vulkan` launch creates a
Vulkan-native window and runs the frame loop through `LLVKContext`. 2D/UI on
Vulkan (the design doc's Phase 2) is the first real content milestone.
