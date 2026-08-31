# -*- cmake -*-
# Vulkan SDK for the Vulkanstorm viewer: Vulkan-Headers + volk meta-loader +
# Vulkan Memory Allocator. Header/source only (the vulkan_sdk prebuilt package
# ships no compiled binaries). volk.c is compiled into the llvulkan library and
# loads vulkan-1.dll dynamically at runtime via volkInitialize(), so the viewer
# still starts on systems without a Vulkan driver/ICD.
include(Prebuilt)

include_guard()

use_system_binary(vulkan_sdk)
use_prebuilt_binary(vulkan_sdk)

# Header-only interface target: Vulkan-Headers + volk + VMA includes.
#   include/vulkan, include/vk_video  -> ${LIBS_PREBUILT_DIR}/include
#   include/volk/volk.h               -> volk/volk.h
#   include/vma/vk_mem_alloc.h        -> vma/vk_mem_alloc.h
add_library(ll::vulkan_sdk INTERFACE IMPORTED)
target_include_directories(ll::vulkan_sdk SYSTEM INTERFACE
    ${LIBS_PREBUILT_DIR}/include
    ${LIBS_PREBUILT_DIR}/include/volk   # volk.c does #include "volk.h"
    ${LIBS_PREBUILT_DIR}/include/vma    # VMA: vk_mem_alloc.h
    )

# Absolute path to the volk source file, for the llvulkan library to compile.
set(VULKAN_SDK_VOLK_SOURCE ${LIBS_PREBUILT_DIR}/source/volk/volk.c)
