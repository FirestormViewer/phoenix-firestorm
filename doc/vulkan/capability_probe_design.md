# Vulkan Capability Probe — Design

**Status:** DRAFT for review.
**Branch:** `vulkan-capability-probe` (from master baseline @ `8d0e03293d`).
**Scope:** get the Vulkan backend off GPU_CLASS_0 / Low by supplying the GPU
facts the feature classifier needs — **without an OpenGL context.** Not on the
UI critical path, but critical for the viewer to run *well* once the 3D pipeline
lands (feature/GPU-class selection drives shaders, shadows, draw distance,
texture memory budget).

---

## 0. The problem (agent-traced, 2026-09-02)

GPU classification (`GPU_CLASS_0..5`) is driven by `LLFeatureManager::
loadGPUClass()` ([llfeaturemanager.cpp:440](../../../indra/newview/llfeaturemanager.cpp)),
which reads `gGLManager` fields and runs a GL memory-bandwidth benchmark
(`gpu_benchmark()`, [llglsandbox.cpp:1046](../../../indra/newview/llglsandbox.cpp)).
On the Vulkan path there is **no GL context**, so `gGLManager` is never
initialized: the benchmark can't run, all GL fields are zero → the classifier
falls through to `GPU_CLASS_0` (Low) forever.

**Timing constraint:** `LLFeatureManager::getInstance()` first fires at
[llviewerwindow.cpp:2139](../../../indra/newview/llviewerwindow.cpp) during window
init, **before** `LLVKSession::start()` ([llappviewer.cpp:3851](../../../indra/newview/llappviewer.cpp)).
So a Vulkan probe cannot simply run "after the session starts" — either it runs
earlier, or the classification result is supplied/overridden after the fact.

---

## 1. What the classifier actually consumes → Vulkan equivalents

Most inputs map cleanly onto `VkPhysicalDevice*` structs (already available once
`LLVKContext` selects a physical device). One input has **no** Vulkan source.

| Classification input | GL source | Vulkan source | Clean? |
|---|---|---|---|
| Vendor (AMD/NVIDIA/Intel/Apple) | `mGLVendor` string | `VkPhysicalDeviceProperties.vendorID` (0x1002/0x10de/0x8086/0x106b…) | ✓ |
| Device name | `mGLRenderer` | `deviceName` | ✓ |
| Device type (discrete/iGPU) | — | `deviceType` (prefer DISCRETE) | ✓ |
| Max texture size | `GL_MAX_TEXTURE_SIZE` | `limits.maxImageDimension2D` | ✓ |
| VRAM (MB) | vendor mem extensions | `memoryProperties` DEVICE_LOCAL heaps sum | ✓ |
| MSAA samples | `GL_MAX_SAMPLES` | `limits.framebufferColorSampleCounts` (popcount) | ✓ |
| Anisotropy | `GL_MAX_TEXTURE_MAX_ANISOTROPY` | `limits.maxSamplerAnisotropy` | ✓ |
| Apple cap (CLASS_3) | `mIsApple` | `vendorID == 0x106b` | ✓ |
| System RAM <8GB down-step | `LLMemory` | unchanged (platform API) | ✓ |
| **Memory bandwidth (GB/s)** | `gpu_benchmark()` — GL render+texture loop + ARB_timer_query | **NONE** in `VkPhysicalDevice*` | ✗ |
| **GPU-class thresholds** | bandwidth + CPU bias → CLASS_0..5 | needs bandwidth | ✗ |

**The single blocker is memory bandwidth.** Everything else is a field read.

---

## 2. Bandwidth: the decision that shapes the probe

The GL benchmark times a texture-fetch loop with `ARB_timer_query`. There is no
`VkPhysicalDevice` field for it — it must be *measured* or *estimated*. Options:

- **A — Vulkan-native micro-benchmark (preferred).** A tiny, self-contained
  Vulkan pass that mirrors the GL benchmark's *result* (GB/s), not its GL logic:
  a large device-local buffer, a simple copy/shader read loop, timed with
  `VkQueryPool` (`VK_QUERY_TYPE_TIMESTAMP`) or calibrated host timing around a
  fence. Runs once, off the UI path, on the probe's own command buffer. This is
  the only option that produces a *real* number for high-end cards (RX 9070 XT
  → CLASS_5), and it's Vulkan-native (policy-clean). Needs a device + queue +
  a small buffer + timestamp query support (`limits.timestampComputeAndGraphics`
  / `timestampPeriod`).
- **B — Heuristic table.** vendorID + deviceType + deviceName → assumed class
  (discrete modern GPU → CLASS_4/5). Cheap, no GPU work, but it's a guess that
  will mis-rate edge cases and needs maintenance.
- **C — Static fallback (CLASS_3).** Safest, but permanently mis-rates high-end
  hardware — defeats the purpose.

**Recommendation: A, with C as the failure fallback.** Measure bandwidth with a
Vulkan-native timestamped micro-benchmark; if timestamp queries are unsupported
or the benchmark fails to run, fall back to CLASS_3 (never CLASS_0). B is a
maintainability trap; avoid.

---

## 3. Probe design

New, self-contained — `indra/llvulkan/llvkprobe.{h,cpp}` (extends the existing
`LLVKProbe` concept from Phase 1) + a bandwidth micro-benchmark. **Policy-clean:
no `gGL`, no GL context.** It reads `VkPhysicalDevice*` and runs its own tiny
Vulkan workload.

```mermaid
flowchart TD
    A["LLVKContext: physical device selected"] --> B["LLVKProbe::gather()"]
    B --> C["Read VkPhysicalDeviceProperties<br/>vendorID/deviceName/deviceType/apiVersion"]
    B --> D["Read VkPhysicalDeviceMemoryProperties<br/>sum DEVICE_LOCAL heaps → VRAM"]
    B --> E["Read VkPhysicalDeviceLimits<br/>maxImageDimension2D / aniso / MSAA / UBO range"]
    B --> F["Bandwidth micro-benchmark<br/>device-local buffer + timestamp query<br/>(fallback: CLASS_3 if unsupported)"]
    C & D & E & F --> G["Populate the classifier's inputs"]
    G --> H["LLFeatureManager gets a real GPU class<br/>(off CLASS_0/Low)"]
```

**Two sub-problems to solve in implementation:**

1. **Where the facts land.** The classifier reads `gGLManager` fields. The probe
   must supply those facts **without** `gGLManager.initGL()` (which needs a GL
   context). Two honest routes — decide in implementation:
   - *(a)* Populate the specific `gGLManager` *fields* the classifier reads
     (vendor/renderer/VRAM/max-texture) from the Vulkan probe — a narrow,
     field-level fill, not a GL init. Pragmatic, but touches the GL struct.
   - *(b)* Add a backend-abstracted "GPU facts" provider that
     `LLFeatureManager` reads, with a GL provider (unchanged behavior) and a
     Vulkan provider (the probe). Cleaner separation, but touches
     `LLFeatureManager`'s input path.
   **Lean (b)** for policy cleanliness — `LLFeatureManager` reads a
   backend-neutral facts struct; GL fills it from `gGLManager` (unchanged
   result), Vulkan fills it from the probe. Keeps the Vulkan path from poking
   the GL manager at all.

2. **Timing — beat the first read.** `LLFeatureManager` first reads during
   window init ([llviewerwindow.cpp:2139]), before the session. Options:
   - *(i)* Run the probe during `LLVKContext` physical-device selection (which
     happens before window init completes on the Vulkan path) and cache the
     facts, so the first `LLFeatureManager` read already sees them.
   - *(ii)* Defer/override classification: let it default, then re-classify once
     the probe has run (a `LLFeatureManager::setGPUClass` backdoor or re-entry).
   **Lean (i)** — populate before the first read — because it avoids a
   re-classification path and any settings written from the wrong class. Confirm
   the exact ordering during implementation (the device must exist before
   [llviewerwindow.cpp:2139] fires).

---

## 4. Acceptance

1. On the Vulkan path, the viewer logs a real GPU class (not CLASS_0/Low) and
   the corresponding feature tier (shaders/shadows/draw-distance/texture budget)
   appropriate to the hardware (RX 9070 XT → high class).
2. Facts logged match the physical device (vendor, deviceName, VRAM, max texture
   size) — verifiable against the Vulkan device log.
3. Bandwidth benchmark produces a plausible GB/s on supported hardware; on
   timestamp-unsupported/failed runs it falls back to CLASS_3 (never CLASS_0).
4. The GL path is **unchanged** (its facts still come from `gGLManager`; the
   classification result for GL is byte-identical to before).
5. No GL context is created on the Vulkan path; clean shutdown.

## 5. Explicitly out of scope

The UI/render pipeline (separate `vulkanui` effort); 3D rendering; actually
*consuming* the GPU class for Vulkan render features (that comes with the 3D
pipeline). This branch only gets the classification *correct*.

## 6. Open questions for review

1. **Facts landing:** (a) narrow `gGLManager` field-fill vs (b) backend-neutral
   GPU-facts provider read by `LLFeatureManager`. I lean (b). Preference?
2. **Timing:** (i) populate-before-first-read vs (ii) re-classify-after. I lean
   (i). Preference?
3. **Bandwidth:** Vulkan-native micro-benchmark (A) with CLASS_3 fallback (C) —
   confirm, vs the heuristic table (B). I lean A+C.
