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
include(LLWindow)
include(LLImage)
include(VulkanSdk)

set(llvulkan_SOURCE_FILES
    llvk_vma_impl.cpp
    llvkcontext.cpp
    llvkgpufacts.cpp
    llvkprobe.cpp
    llvkselftest.cpp
    llvksession.cpp
    llvkui2d.cpp
    llvkuiimage.cpp
    llvkuirender.cpp
    llvkuitestscene.cpp
    ${VULKAN_SDK_VOLK_SOURCE}
    )

set(llvulkan_HEADER_FILES
    CMakeLists.txt

    llvkcontext.h
    llvkgpufacts.h
    llvkprobe.h
    llvkselftest.h
    llvksession.h
    llvkui2d.h
    llvkuiimage.h
    llvkuirender.h
    llvkuitestscene.h
    )

# volk.c is C; the rest is C++. Give volk its required define so it exposes the
# global Vulkan function table without linking the static loader.
set_source_files_properties(${VULKAN_SDK_VOLK_SOURCE} PROPERTIES
    COMPILE_DEFINITIONS "VK_NO_PROTOTYPES")

add_library (llvulkan ${llvulkan_SOURCE_FILES})
target_include_directories(llvulkan INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(llvulkan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# <VulkanStorm> llxml + llfilesystem don't export an include() module here;
# add their dirs explicitly for the GL-free XML parse + skin-dir resolution.
target_include_directories(llvulkan PRIVATE
    ${CMAKE_SOURCE_DIR}/llxml
    ${CMAKE_SOURCE_DIR}/llfilesystem)
# </VulkanStorm>

set_property(TARGET llvulkan PROPERTY POSITION_INDEPENDENT_CODE ON)

# volk requires VK_NO_PROTOTYPES so it owns all Vulkan function pointers.
# VK_USE_PLATFORM_WIN32_KHR enables the Win32 surface extension declarations.
target_compile_definitions(llvulkan PUBLIC VK_NO_PROTOTYPES)
if (WINDOWS)
    target_compile_definitions(llvulkan PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif ()

# VMA configuration, shared by every TU that includes vk_mem_alloc.h: dynamic
# Vulkan functions come from volk's loaded table (set at allocator creation).
target_compile_definitions(llvulkan PUBLIC
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1)

target_link_libraries(llvulkan
        llcommon
        llmath
        llwindow
        llimage
        llfilesystem
        llxml
        llrender
        llui
        ll::vulkan_sdk
        )
