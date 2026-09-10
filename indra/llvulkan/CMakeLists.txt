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
include(FreeType)
include(PNG)
include(ZLIBNG)
include(EXPAT)

add_library(llvkfont llvkfont.cpp llvkfont.h llvkfontface.cpp llvkfontface.h llvkfontsvg.cpp llvkfontsvg.h
    llvkglyphatlas.cpp llvkglyphatlas.h llvkfontregistry.cpp llvkfontregistry.h)
target_include_directories(llvkfont SYSTEM PRIVATE ${LIBS_PREBUILT_DIR}/include)
target_include_directories(llvkfont PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(llvkfont PRIVATE ll::freetype ll::libpng ll::zlib-ng ll::expat)
set_property(TARGET llvkfont PROPERTY POSITION_INDEPENDENT_CODE ON)

find_program(LLVK_GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
set(glyph_draw_headers)
foreach(stage vert frag)
    set(shader_binary "${CMAKE_CURRENT_BINARY_DIR}/glyph_draw.${stage}.spv")
    set(shader_header "${CMAKE_CURRENT_BINARY_DIR}/glyph_draw_${stage}_spv.h")
    add_custom_command(OUTPUT "${shader_header}" BYPRODUCTS "${shader_binary}"
        COMMAND "${LLVK_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glyph_draw.${stage}" -o "${shader_binary}"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${shader_binary}" "-DOUTPUT=${shader_header}"
            "-DNAME=glyph_draw_${stage}_spv" -P "${CMAKE_CURRENT_SOURCE_DIR}/shaders/embed_spirv.cmake"
        DEPENDS "shaders/glyph_draw.${stage}" shaders/embed_spirv.cmake VERBATIM)
    list(APPEND glyph_draw_headers "${shader_header}")
endforeach()
add_library(llvkglyphgpu llvkglyphupload.cpp llvkglyphupload.h llvktextdraw.cpp llvktextdraw.h ${glyph_draw_headers})
target_include_directories(llvkglyphgpu PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(llvkglyphgpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(llvkglyphgpu PUBLIC llvkfont ll::vulkan_sdk)
target_compile_definitions(llvkglyphgpu PUBLIC VK_NO_PROTOTYPES
    VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
set_property(TARGET llvkglyphgpu PROPERTY POSITION_INDEPENDENT_CODE ON)

option(LL_VULKAN_GPU_TESTS "Run native glyph GPU tests requiring Vulkan validation layers" OFF)
if (LL_TESTS)
    include(LLAddBuildTest)
    LL_ADD_INTEGRATION_TEST(llvkfontface "" "llvkfont;llcommon;ll::freetype")
    target_compile_definitions(INTEGRATION_TEST_llvkfontface PRIVATE
        LLVK_FONT_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/Roboto-Regular.ttf"
        LLVK_FALLBACK_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/NotoSansCombined-Regular.ttf"
        LLVK_EMOJI_FIXTURE="${LIBS_PREBUILT_DIR}/fonts/TwemojiSVG.ttf"
        LLVK_VARIABLE_FIXTURE="${LIBS_PREBUILT_DIR}/fonts/InterVariableFont.ttf")
    if (LL_VULKAN_GPU_TESTS)
        find_program(LLVK_GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
        set(glyph_readback_spv "${CMAKE_CURRENT_BINARY_DIR}/glyph_readback.comp.spv")
        add_custom_command(OUTPUT "${glyph_readback_spv}"
            COMMAND "${LLVK_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glyph_readback.comp" -o "${glyph_readback_spv}"
            DEPENDS shaders/glyph_readback.comp VERBATIM)
        add_custom_target(llvk_glyph_readback_shader DEPENDS "${glyph_readback_spv}")
        LL_ADD_INTEGRATION_TEST(llvkglyphupload "llvk_vma_impl.cpp;${VULKAN_SDK_VOLK_SOURCE}" "llvkglyphgpu;llvkfont;llcommon")
        add_dependencies(INTEGRATION_TEST_llvkglyphupload llvk_glyph_readback_shader)
        target_compile_definitions(INTEGRATION_TEST_llvkglyphupload PRIVATE
            LLVK_GLYPH_READBACK_SHADER="${glyph_readback_spv}"
            LLVK_FONT_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/Roboto-Regular.ttf")
    endif ()
endif ()

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
    llvkuiwidgets.cpp
    llvkuifolder.cpp
    llvktext.cpp
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
    llvkuirenderinternal.h
    llvkuiwidgets.h
    llvkuifolder.h
    llvktext.h
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
    llvkfont
    llvkglyphgpu
        llcommon
        llmath
        llwindow
        llimage
        llfilesystem
        llxml
        llrender
        llui
        ll::freetype
        ll::vulkan_sdk
        )
