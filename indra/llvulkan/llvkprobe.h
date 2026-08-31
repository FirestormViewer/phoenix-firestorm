/**
 * @file llvkprobe.h
 * @brief Vulkan availability / device-capability probe.
 *
 * @details
 * A single, cheap probe used in two places:
 *   - the Preferences "Renderer" selector gate (offer Vulkan only when a real
 *     ICD + physical device is present), and
 *   - the startup path in LLAppViewer::initWindow() (fall back to OpenGL for
 *     the session if RenderBackend=Vulkan but no device is available).
 *
 * It deliberately does NOT create a full LLVKContext; it only creates a minimal
 * instance and enumerates physical devices. volk loads vulkan-1.dll at runtime,
 * so this is safe to call on systems with no Vulkan driver.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKPROBE_H
#define LLVKPROBE_H

#include <string>

namespace LLVKProbe
{
    // Returns true if a Vulkan loader AND at least one physical device are
    // present. On first call it initializes volk and enumerates devices; the
    // result is cached for the process lifetime (probe is one-shot).
    bool hasVulkanDevice();

    // Human-readable name of the first enumerated physical device (e.g. the
    // GPU name), or an empty string if none. Valid after hasVulkanDevice().
    std::string firstDeviceName();
}

#endif // LLVKPROBE_H
