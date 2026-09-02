/**
 * @file llvkgpufacts.h
 * @brief Backend-neutral GPU facts captured from the Vulkan physical device.
 *
 * @details
 * On the Vulkan path there is no GL context, so gGLManager is never
 * initialized and the GL-derived device facts it normally captures (vendor,
 * renderer name, VRAM, limits) are absent. This module captures the equivalent
 * facts directly from the Vulkan API (VkPhysicalDeviceProperties /
 * VkPhysicalDeviceMemoryProperties / limits) and publishes them as a single
 * snapshot that backend-neutral consumers (LLFeatureManager, the About panel)
 * can read WITHOUT touching gGLManager.
 *
 * Two stages (see doc/vulkan/capability_probe_design.md):
 *   - Static facts (Stage 1): captured in the early LLVKProbe enumeration,
 *     before LLFeatureManager first reads. Everything except bandwidth.
 *   - Bandwidth (Stage 2): measured by a Vulkan-native micro-benchmark once a
 *     logical device exists (LLVKSession::start()); feeds the GPU-class
 *     thresholds. Until then bandwidthValid == false.
 *
 * Policy: this module is GL-free (no gGL / LLRender / GL context).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKGPUFACTS_H
#define LLVKGPUFACTS_H

#include <cstdint>
#include <string>

namespace LLVKGpuFacts
{
    // Vendor identity, derived from the Vulkan vendorID (the driver-reported
    // PCI vendor ID). Mirrors the vendor flags gGLManager would set on GL.
    enum class Vendor : uint8_t { Unknown, AMD, NVIDIA, Intel, Apple, Other };

    struct Facts
    {
        bool        valid = false;          // a physical device was read
        uint32_t    vendorID = 0;           // VkPhysicalDeviceProperties
        uint32_t    deviceID = 0;
        std::string deviceName;
        uint32_t    deviceType = 0;         // VkPhysicalDeviceType
        uint32_t    apiVersion = 0;
        // Limits (Stage 1).
        uint32_t    maxImageDimension2D = 0;
        float       maxSamplerAnisotropy = 0.f;
        uint32_t    maxSamples = 1;
        // Memory (Stage 1): sum of DEVICE_LOCAL heap sizes.
        uint64_t    vramBytes = 0;
        // Bandwidth (Stage 2): measured at logical-device-up.
        bool        bandwidthValid = false;
        float       bandwidthGBps = 0.f;
    };

    // Publish the static facts (called from the early probe, Stage 1).
    void setStaticFacts(const Facts& facts);
    // Publish the measured bandwidth (called at logical-device-up, Stage 2).
    void setBandwidth(float gbps);

    // Read the current snapshot. valid==false until a device has been read.
    const Facts& get();

    Vendor vendor();
    // The vendor name string matching what GL would report (e.g. for the
    // RDNA3.5 guard / About panel). Derived from vendorID.
    std::string vendorName();
    // Convenience: VRAM in whole MB (matches gGLManager.mVRAM units).
    int vramMB();
}

#endif // LLVKGPUFACTS_H
