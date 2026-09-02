/**
 * @file llvkprobe.cpp
 * @brief Implementation of the Vulkan availability probe.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "llvkprobe.h"

// volk provides the global Vulkan function table and dynamically loads the
// loader (vulkan-1.dll) at runtime. VK_NO_PROTOTYPES is set by CMake.
#include "volk/volk.h"

#include "llerror.h"
#include "llvkgpufacts.h"

#include <vector>

namespace
{
    bool        g_probed = false;
    bool        g_has_device = false;
    std::string g_device_name;

    void probe()
    {
        if (g_probed)
        {
            return;
        }
        g_probed = true;

        // volkInitialize loads vulkan-1.dll and resolves the global entry
        // points. Fails (returns error) when no Vulkan loader is installed.
        if (volkInitialize() != VK_SUCCESS)
        {
            LL_INFOS("Vulkan") << "No Vulkan loader present (volkInitialize failed); Vulkan unavailable." << LL_ENDL;
            return;
        }

        // Minimal instance sufficient for physical-device enumeration.
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Vulkanstorm-Probe";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Vulkanstorm";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        VkInstance instance = VK_NULL_HANDLE;
        if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS)
        {
            LL_INFOS("Vulkan") << "vkCreateInstance failed; no usable Vulkan ICD." << LL_ENDL;
            volkFinalize();
            return;
        }

        volkLoadInstanceOnly(instance);

        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if (device_count > 0)
        {
            g_has_device = true;

            // <VulkanStorm> Prefer a discrete GPU for the facts snapshot (the
            // viewer renders on the high-performance device). Fall back to the
            // first device if none is discrete.
            std::vector<VkPhysicalDevice> devices(device_count);
            vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
            VkPhysicalDevice chosen = devices.front();
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(chosen, &props);
            for (VkPhysicalDevice dev : devices)
            {
                VkPhysicalDeviceProperties p{};
                vkGetPhysicalDeviceProperties(dev, &p);
                if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    chosen = dev;
                    props = p;
                    break;
                }
            }

            g_device_name = props.deviceName ? props.deviceName : "";

            // <VulkanStorm> Stage 1: capture the static GPU facts while the
            // instance/device handle is live (this runs before LLFeatureManager
            // first reads). Bandwidth is Stage 2 (needs a logical device).
            {
                LLVKGpuFacts::Facts facts;
                facts.valid        = true;
                facts.vendorID     = props.vendorID;
                facts.deviceID     = props.deviceID;
                facts.deviceName   = g_device_name;
                facts.deviceType   = (uint32_t)props.deviceType;
                facts.apiVersion   = props.apiVersion;
                facts.maxImageDimension2D  = props.limits.maxImageDimension2D;
                facts.maxSamplerAnisotropy = props.limits.maxSamplerAnisotropy;
                const VkSampleCountFlags sc = props.limits.framebufferColorSampleCounts;
                facts.maxSamples   = (sc & VK_SAMPLE_COUNT_64_BIT) ? 64
                                   : (sc & VK_SAMPLE_COUNT_32_BIT) ? 32
                                   : (sc & VK_SAMPLE_COUNT_16_BIT) ? 16
                                   : (sc & VK_SAMPLE_COUNT_8_BIT)  ? 8
                                   : (sc & VK_SAMPLE_COUNT_4_BIT)  ? 4
                                   : (sc & VK_SAMPLE_COUNT_2_BIT)  ? 2 : 1;

                VkPhysicalDeviceMemoryProperties mem{};
                vkGetPhysicalDeviceMemoryProperties(chosen, &mem);
                uint64_t vram = 0;
                for (uint32_t i = 0; i < mem.memoryHeapCount; ++i)
                {
                    if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    {
                        vram += (uint64_t)mem.memoryHeaps[i].size;
                    }
                }
                facts.vramBytes = vram;

                LLVKGpuFacts::setStaticFacts(facts);
            }
            // </VulkanStorm>

            LL_INFOS("Vulkan") << "Vulkan device detected: " << g_device_name
                               << " (" << device_count << " device(s))"
                               << " vendorID=0x" << std::hex << props.vendorID << std::dec
                               << " VRAM=" << LLVKGpuFacts::vramMB() << "MB"
                               << " maxTex=" << LLVKGpuFacts::get().maxImageDimension2D
                               << LL_ENDL;
        }
        else
        {
            LL_INFOS("Vulkan") << "Vulkan loader present but no physical devices enumerated." << LL_ENDL;
        }

        vkDestroyInstance(instance, nullptr);
        volkFinalize();
    }
}

namespace LLVKProbe
{
    bool hasVulkanDevice()
    {
        probe();
        return g_has_device;
    }

    std::string firstDeviceName()
    {
        probe();
        return g_device_name;
    }
}
