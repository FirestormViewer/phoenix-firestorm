# Vulkan telemetry type consistency

Source review: `eaee5eead0`, 2026-09-21; NV-00/NV-01, neutral telemetry only.
`llviewerstats.cpp` declares a reduced global `VkResult` enum, conflicting with
the complete SDK enum used by the native renderer. Linux LTO reports this ODR
violation even though the telemetry loader probe is Windows-only.

Use `vulkan/vulkan_core.h` for the enum, handles, PFN types and version macros.
The viewer already inherits SDK headers and `VK_NO_PROTOTYPES` from its public
llvulkan dependency. Preserve the existing LoadLibrary/GetProcAddress/FreeLibrary
probe, including missing-loader and missing-entry-point behavior. No Vulkan
instance, GPU resource or renderer behavior changes; no loader import is added.

Validate the SDK type use and version macros with a bounded compilation check.
Full Linux LTO validation remains CI work.
