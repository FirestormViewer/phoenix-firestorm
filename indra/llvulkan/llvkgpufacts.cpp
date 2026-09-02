/**
 * @file llvkgpufacts.cpp
 * @brief Implementation of the backend-neutral Vulkan GPU facts snapshot.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "llvkgpufacts.h"

namespace
{
    LLVKGpuFacts::Facts g_facts;

    // PCI vendor IDs as reported by the driver via VkPhysicalDeviceProperties.
    constexpr uint32_t VENDOR_AMD    = 0x1002;
    constexpr uint32_t VENDOR_NVIDIA = 0x10de;
    constexpr uint32_t VENDOR_INTEL  = 0x8086;
    constexpr uint32_t VENDOR_APPLE  = 0x106b;
}

namespace LLVKGpuFacts
{
    void setStaticFacts(const Facts& facts)
    {
        g_facts = facts;
        g_facts.bandwidthValid = false; // bandwidth is Stage 2
        g_facts.bandwidthGBps = 0.f;
    }

    void setBandwidth(float gbps)
    {
        g_facts.bandwidthGBps = gbps;
        g_facts.bandwidthValid = (gbps >= 0.f);
    }

    const Facts& get()
    {
        return g_facts;
    }

    Vendor vendor()
    {
        switch (g_facts.vendorID)
        {
        case VENDOR_AMD:    return Vendor::AMD;
        case VENDOR_NVIDIA: return Vendor::NVIDIA;
        case VENDOR_INTEL:  return Vendor::Intel;
        case VENDOR_APPLE:  return Vendor::Apple;
        default:            return g_facts.valid ? Vendor::Other : Vendor::Unknown;
        }
    }

    std::string vendorName()
    {
        switch (vendor())
        {
        case Vendor::AMD:    return "AMD";
        case Vendor::NVIDIA: return "NVIDIA";
        case Vendor::Intel:  return "Intel";
        case Vendor::Apple:  return "Apple";
        case Vendor::Other:  return "Unknown";
        default:             return "";
        }
    }

    int vramMB()
    {
        return (int)(g_facts.vramBytes / (1024ull * 1024ull));
    }
}
