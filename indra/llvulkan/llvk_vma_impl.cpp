/**
 * @file llvk_vma_impl.cpp
 * @brief Vulkan Memory Allocator implementation translation unit.
 *
 * VMA is header-only; exactly one translation unit must define
 * VMA_IMPLEMENTATION to emit its function bodies. This is that TU.
 * volk supplies the Vulkan function table, so we use dynamic Vulkan functions
 * and tell VMA the headers are already included via volk.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

// volk provides Vulkan types + the loaded function table; VMA builds on it.
// VMA_STATIC_VULKAN_FUNCTIONS / VMA_DYNAMIC_VULKAN_FUNCTIONS are set
// project-wide by CMake so every TU that includes vk_mem_alloc.h agrees.

#define VMA_IMPLEMENTATION
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
