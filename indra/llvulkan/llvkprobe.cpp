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

            // Capture the first device's name for logging / UI.
            VkPhysicalDevice first = VK_NULL_HANDLE;
            uint32_t one = 1;
            vkEnumeratePhysicalDevices(instance, &one, &first);
            if (first != VK_NULL_HANDLE)
            {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(first, &props);
                g_device_name = props.deviceName ? props.deviceName : "";
            }
            LL_INFOS("Vulkan") << "Vulkan device detected: " << g_device_name
                               << " (" << device_count << " device(s))" << LL_ENDL;
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
