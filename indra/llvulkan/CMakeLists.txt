# -*- cmake -*-

# llvulkan — the Vulkan render backend for the Vulkanstorm viewer.
#
# This library is the Vulkan twin of llrender's GL object wrappers. It owns the
# Vulkan instance/device/queues/swapchain (LLVKContext), shader/pipeline objects,
# buffers (via VMA), and images. It links against the header/source-only
# vulkan_sdk prebuilt package (Vulkan-Headers + volk + VMA); volk.c is compiled
# in here and loads vulkan-1.dll dynamically at runtime, so the viewer still
# starts on systems without a Vulkan driver.

project(llvulkan)

include(00-Common)
include(LLCommon)
include(VulkanSdk)

set(llvulkan_SOURCE_FILES
    llvkcontext.cpp
    llvkprobe.cpp
    ${VULKAN_SDK_VOLK_SOURCE}
    )

set(llvulkan_HEADER_FILES
    CMakeLists.txt

    llvkcontext.h
    llvkprobe.h
    )

# volk.c is C; the rest is C++. Give volk its required define so it exposes the
# global Vulkan function table without linking the static loader.
set_source_files_properties(${VULKAN_SDK_VOLK_SOURCE} PROPERTIES
    COMPILE_DEFINITIONS "VK_NO_PROTOTYPES")

add_library (llvulkan ${llvulkan_SOURCE_FILES})
target_include_directories(llvulkan INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(llvulkan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET llvulkan PROPERTY POSITION_INDEPENDENT_CODE ON)

# volk requires VK_NO_PROTOTYPES so it owns all Vulkan function pointers.
target_compile_definitions(llvulkan PUBLIC VK_NO_PROTOTYPES)

target_link_libraries(llvulkan
        llcommon
        ll::vulkan_sdk
        )
