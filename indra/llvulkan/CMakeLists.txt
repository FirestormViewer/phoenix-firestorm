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
include(JPEG)
include(OpenJPEG)
include(ZLIBNG)
include(EXPAT)
include(Hunspell)

include(FetchContent)
FetchContent_Declare(llvk_stb
    URL https://codeload.github.com/nothings/stb/zip/2c980bb59875b0d32144a71867fbdebb2f77cd20
    URL_HASH SHA256=8e59f72b0780690cda64726804269f638a3be77b9d1506ea95f443f7964bccf0
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(llvk_stb)

add_library(llvksessionowner llvksessionowner.cpp llvksessionowner.h)
target_include_directories(llvksessionowner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET llvksessionowner PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(llvklogin llvkloginprotocol.cpp llvkloginprotocol.h llvkregioncircuit.cpp llvkregioncircuit.h)
target_sources(llvklogin PRIVATE llvkchatprotocol.cpp llvkchatprotocol.h)
target_include_directories(llvklogin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(llvklogin PUBLIC llcommon PRIVATE llxml ll::expat)
set_property(TARGET llvklogin PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(llvkerror llvkerror.cpp llvkerror.h)
target_include_directories(llvkerror PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET llvkerror PROPERTY POSITION_INDEPENDENT_CODE ON)
if (WINDOWS)
    target_sources(llvkerror PRIVATE llvkerrorwin32.cpp)
    target_link_libraries(llvkerror PRIVATE user32)
endif ()

add_library(llvkwidgets llvkwidgettree.cpp llvkwidgettree.h llvkwidgetlayout.cpp llvkwidgetlayout.h
    llvkwidgetfactory.cpp llvkwidgetfactory.h llvkcontrol.cpp llvkcontrol.h
    llvkwidgetimage.cpp llvkwidgetimage.h llvkicon.cpp llvkicon.h llvkbutton.cpp llvkbutton.h
    llvkwidgetinput.cpp llvkbadge.cpp llvkbadge.h llvklabel.cpp llvklabel.h
    llvktext.cpp llvkplaintextlayout.h llvkplaincontrol.cpp llvkplaincontrol.h llvkcheckbox.cpp llvkcolorswatch.cpp llvkscrolllist.cpp
    llvkcolor.cpp llvkcolor.h llvkborder.h llvkpanel.cpp llvkpanel.h llvkxmllayers.cpp llvkxmllayers.h
    llvkskinfiles.cpp llvkskinfiles.h llvkskinimages.cpp llvkskinimages.h llvkwidgetjpeg.cpp llvkwidgettga.cpp llvkwidgetj2c.cpp
    llvklineeditor.cpp llvklineeditor.h llvkclipboard.cpp llvkclipboard.h llvkstyledtext.h
    llvkscroll.cpp llvkscroll.h llvkcombo.cpp llvkbrowsersurface.cpp llvkbrowsersurface.h llvkwidgetpaint.cpp llvkwidgetpaint.h
    llvkviewerui.cpp llvkviewerui.h llvksettingsmgr.cpp llvksettingsmgr.h llvkmenu.cpp llvkmenu.h llvkfloater.cpp llvkfloater.h llvkdialogs.cpp llvkspinner.cpp llvkradiogroup.cpp)
target_link_libraries(llvkwidgets PRIVATE llmath PUBLIC llxml llvkerror llvksessionowner)
target_sources(llvkwidgets PRIVATE llvktext.h)
target_sources(llvkwidgets PRIVATE llvkslider.cpp llvkspellcheck.cpp llvkspellcheck.h llvktranslation.cpp llvktranslation.h)
target_sources(llvkwidgets PRIVATE llvkkeybindings.cpp llvkkeybindings.h)
target_sources(llvkwidgets PRIVATE llvkmediafilter.cpp llvkmediafilter.h)
target_sources(llvkwidgets PRIVATE llvkmutelist.cpp llvkmutelist.h)
target_sources(llvkwidgets PRIVATE llvkproxy.cpp llvkproxy.h)
target_sources(llvkwidgets PRIVATE llvkbeamcolor.cpp llvkbeamcolor.h)
target_sources(llvkwidgets PRIVATE llvkcommunications.cpp)
target_sources(llvkwidgets PRIVATE llvkconnectedshell.cpp)
target_sources(llvkwidgets PRIVATE llvkbeamshape.cpp llvkbeamshape.h)
target_sources(llvkwidgets PRIVATE llvkgraphicpresets.cpp llvkgraphicpresets.h)
target_sources(llvkwidgets PRIVATE llvkpreferencesbackup.cpp llvkpreferencesbackup.h)
target_sources(llvkwidgets PRIVATE llvkgraphicspolicy.cpp llvkgraphicspolicy.h)
target_sources(llvkwidgets PRIVATE llvktexturectrl.cpp llvktexturectrl.h)
target_include_directories(llvkwidgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LIBS_OPEN_DIR}/llwebrtc)
target_include_directories(llvkwidgets SYSTEM PRIVATE ${llvk_stb_SOURCE_DIR})
target_link_libraries(llvkwidgets PRIVATE ll::expat ll::libpng ll::zlib-ng ll::libjpeg ll::openjpeg ll::hunspell)
target_link_libraries(llvkwidgets PUBLIC llcommon)
if (WINDOWS)
    include(OpenSSL)
    include(NGHTTP2)
    include(CURL)
    target_sources(llvklogin PRIVATE llvkloginhttp.cpp llvkloginhttp.h llvklogintransport.cpp llvklogintransport.h)
    target_link_libraries(llvklogin PRIVATE ll::libcurl PUBLIC llvksessionowner)
    target_link_libraries(llvkwidgets PRIVATE user32)
endif ()
set_property(TARGET llvkwidgets PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(llvkfont llvkfont.cpp llvkfont.h llvkfontface.cpp llvkfontface.h llvkfontsvg.cpp llvkfontsvg.h
    llvkglyphatlas.cpp llvkglyphatlas.h llvkfontregistry.cpp llvkfontregistry.h)
target_include_directories(llvkfont SYSTEM PRIVATE ${LIBS_PREBUILT_DIR}/include)
target_include_directories(llvkfont PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(llvkfont PRIVATE ll::freetype ll::libpng ll::zlib-ng ll::expat)
set_property(TARGET llvkfont PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(llvkwidgets PUBLIC llvkfont)

find_program(LLVK_GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
set(native_ui_shaders)
foreach(stage vert frag)
    set(shader "${CMAKE_CURRENT_BINARY_DIR}/compiled_ui/ui2d.${stage}.spv")
    add_custom_command(OUTPUT "${shader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/compiled_ui"
        COMMAND "${LLVK_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui2d.${stage}" -o "${shader}"
        DEPENDS "shaders/ui2d.${stage}" VERBATIM)
    list(APPEND native_ui_shaders "${shader}")
endforeach()
add_custom_target(llvk_ui_shaders DEPENDS ${native_ui_shaders})
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

add_library(llvkuipacket llvkuipacket.cpp llvkuipacket.h llvkimagepublication.cpp llvkimagepublication.h llvkwidgetgpu.cpp llvkwidgetgpu.h)
target_link_libraries(llvkuipacket PUBLIC llvkwidgets llvkglyphgpu)
set_property(TARGET llvkuipacket PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(llvkscene llvksceneselection.cpp llvksceneselection.h)
target_include_directories(llvkscene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(llvkscene PUBLIC llmath llcommon)
set_property(TARGET llvkscene PROPERTY POSITION_INDEPENDENT_CODE ON)

if (WINDOWS)
    include(CEFPlugin)
    include(${CMAKE_CURRENT_SOURCE_DIR}/native_dullahan.cmake)
    add_library(vk_media_plugin_cef llvkbrowser.cpp llvkbrowser.h)
    target_include_directories(vk_media_plugin_cef SYSTEM PRIVATE "${LIBS_PREBUILT_DIR}/include/cef")
    target_link_libraries(vk_media_plugin_cef PUBLIC llvkwidgets PRIVATE vkdullahan user32)
    add_library(llvkwindowmgr llvkwindowmgr.cpp llvkwindowmgr.h llvkstartup.cpp llvkstartup.h)
    target_link_libraries(llvkwindowmgr PUBLIC llvkerror PRIVATE llvklogin)
    target_sources(llvkwindowmgr PRIVATE llvkaudio.cpp llvkaudio.h)
    target_sources(llvkwindowmgr PRIVATE llvkstartupstatus.cpp llvkstartupstatus.h)
    target_sources(llvkwindowmgr PRIVATE llvkvoice.cpp llvkvoice.h)
    target_sources(llvkwindowmgr PRIVATE llvktexturecache.cpp llvktexturecache.h ../newview/lltexturecache.cpp)
    target_sources(llvkwindowmgr PRIVATE llvktexturepreview.cpp llvktexturepreview.h)
    target_include_directories(llvkwindowmgr PRIVATE ${CMAKE_SOURCE_DIR}/newview)
    target_link_libraries(llvkwindowmgr PRIVATE llimage)
    target_sources(llvkwindowmgr PRIVATE llvkjoystick.cpp llvkjoystick.h)
    target_link_libraries(llvkwindowmgr PRIVATE dinput8 dxguid winmm)
    target_link_libraries(llvkwindowmgr PRIVATE llwebrtc)
    include(OPENAL)
    if (TARGET ll::openal)
        target_link_libraries(llvkwindowmgr PRIVATE ll::openal)
    endif ()
    target_compile_definitions(llvkwindowmgr PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    target_compile_definitions(llvkwindowmgr PRIVATE
        LLVK_VIEWER_MAJOR="${VIEWER_VERSION_MAJOR}" LLVK_VIEWER_MINOR="${VIEWER_VERSION_MINOR}"
        LLVK_VIEWER_PATCH="${VIEWER_VERSION_PATCH}" LLVK_VIEWER_BUILD="${VIEWER_VERSION_REVISION}"
        LLVK_VIEWER_UPSTREAM="${VIEWER_VERSION_LL}" LLVK_VIEWER_CHANNEL="${VIEWER_CHANNEL}")
    target_link_libraries(llvkwindowmgr PUBLIC llvkuipacket vk_media_plugin_cef PRIVATE ll::libcurl user32 shell32 ole32 psapi comdlg32)
    add_dependencies(llvkwindowmgr llvk_ui_shaders)
endif ()

option(LL_VULKAN_GPU_TESTS "Run native glyph GPU tests requiring Vulkan validation layers" OFF)
option(LL_VULKAN_BROWSER_TESTS "Run native browser lifecycle tests requiring packaged CEF helpers" OFF)
if (LL_TESTS)
    include(LLAddBuildTest)
    if (WINDOWS)
        add_executable(notification_capture ../../tools/vulkan/notification_capture.cpp)
        target_link_libraries(notification_capture PRIVATE d3d11 dxgi dwmapi windowsapp)
        add_executable(notification_leap ../../tools/vulkan/notification_leap.cpp)
        target_link_libraries(notification_leap PRIVATE llxml llcommon)
        add_custom_command(TARGET notification_leap POST_BUILD
            COMMAND "$<TARGET_FILE:notification_leap>" --self-test VERBATIM)
    endif ()
    add_executable(INTEGRATION_TEST_llvksessionowner tests/llvksessionowner_test.cpp)
    target_link_libraries(INTEGRATION_TEST_llvksessionowner PRIVATE llvksessionowner)
    add_custom_command(TARGET INTEGRATION_TEST_llvksessionowner POST_BUILD
        COMMAND "$<TARGET_FILE:INTEGRATION_TEST_llvksessionowner>" VERBATIM)
    add_test(NAME llvksessionowner COMMAND INTEGRATION_TEST_llvksessionowner)
    if (WINDOWS)
        LL_ADD_INTEGRATION_TEST(llvkloginprotocol "" "llvklogin;llxml;llcommon;llmessage;ll::openssl")
        target_compile_definitions(INTEGRATION_TEST_llvkloginprotocol PRIVATE
            LLVK_LOGIN_MESSAGE_TEMPLATE="${CMAKE_SOURCE_DIR}/../scripts/messages/message_template.msg")
    endif ()
    add_dependencies(llvkwidgets INTEGRATION_TEST_llvksessionowner)
    add_executable(INTEGRATION_TEST_llvkerror tests/llvkerror_test.cpp)
    target_link_libraries(INTEGRATION_TEST_llvkerror PRIVATE llvkerror)
    add_custom_command(TARGET INTEGRATION_TEST_llvkerror POST_BUILD
        COMMAND "$<TARGET_FILE:INTEGRATION_TEST_llvkerror>" VERBATIM)
    if (WINDOWS)
        add_dependencies(llvkwindowmgr INTEGRATION_TEST_llvkerror)
    endif ()
    if (WINDOWS AND LL_VULKAN_BROWSER_TESTS)
        LL_ADD_INTEGRATION_TEST(llvkbrowser "" "vk_media_plugin_cef;llcommon")
        set(native_browser_test_targets INTEGRATION_TEST_llvkbrowser)
        if (LL_VULKAN_GPU_TESTS)
            add_executable(INTEGRATION_TEST_llvktexturecache_startup tests/llvktexturecache_startup_test.cpp)
            target_link_libraries(INTEGRATION_TEST_llvktexturecache_startup PRIVATE llvkwindowmgr llcommon)
            set_target_properties(INTEGRATION_TEST_llvktexturecache_startup PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sharedlibs")
            add_custom_command(TARGET INTEGRATION_TEST_llvktexturecache_startup POST_BUILD
                COMMAND "$<TARGET_FILE:INTEGRATION_TEST_llvktexturecache_startup>" VERBATIM)
            LL_ADD_INTEGRATION_TEST(llvkwindowmgr "llvkcontext.cpp;llvk_vma_impl.cpp;${VULKAN_SDK_VOLK_SOURCE}" "llvkwindowmgr;llcommon;ll::vulkan_sdk")
            add_dependencies(INTEGRATION_TEST_llvkwindowmgr INTEGRATION_TEST_llvktexturecache_startup)
            configure_file(${CMAKE_SOURCE_DIR}/newview/res/viewerRes.rc
                ${CMAKE_CURRENT_BINARY_DIR}/native_status_test.rc)
            target_sources(INTEGRATION_TEST_llvkwindowmgr PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/native_status_test.rc)
            target_include_directories(INTEGRATION_TEST_llvkwindowmgr PRIVATE ${CMAKE_SOURCE_DIR}/newview/res)
            target_compile_definitions(INTEGRATION_TEST_llvkwindowmgr PRIVATE VK_USE_PLATFORM_WIN32_KHR
                LLVK_LOGIN_SOURCE="${CMAKE_SOURCE_DIR}/newview" LLVK_LOGIN_PACKAGED_FONTS="${LIBS_PREBUILT_DIR}/fonts"
                LLVK_NOTIFICATION_CAPTURE_EXE="$<TARGET_FILE:notification_capture>")
            add_dependencies(INTEGRATION_TEST_llvkwindowmgr notification_capture)
            list(APPEND native_browser_test_targets INTEGRATION_TEST_llvkwindowmgr)
            foreach(stage vert frag)
                add_custom_command(TARGET INTEGRATION_TEST_llvkwindowmgr PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:INTEGRATION_TEST_llvkwindowmgr>/shaders/compiled"
                    COMMAND "${LLVK_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1
                        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui2d.${stage}"
                        -o "$<TARGET_FILE_DIR:INTEGRATION_TEST_llvkwindowmgr>/shaders/compiled/ui2d.${stage}.spv"
                    VERBATIM)
            endforeach()
        endif()
        foreach(browser_target IN LISTS native_browser_test_targets)
          foreach(runtime libcef.dll chrome_elf.dll dullahan_host.exe libEGL.dll libGLESv2.dll d3dcompiler_47.dll dxcompiler.dll dxil.dll v8_context_snapshot.bin)
            add_custom_command(TARGET ${browser_target} PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LIBS_PREBUILT_DIR}/bin/release/${runtime}" "$<TARGET_FILE_DIR:${browser_target}>/${runtime}"
                VERBATIM)
          endforeach()
          add_custom_command(TARGET ${browser_target} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${LIBS_PREBUILT_DIR}/resources" "$<TARGET_FILE_DIR:${browser_target}>"
            VERBATIM)
        endforeach()
    endif ()
    LL_ADD_INTEGRATION_TEST(llvkcontext "llvkcontext.cpp;llvk_vma_impl.cpp;${VULKAN_SDK_VOLK_SOURCE}" "llcommon;ll::vulkan_sdk;llvkuipacket;llvkscene")
    target_include_directories(INTEGRATION_TEST_llvkcontext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(INTEGRATION_TEST_llvkcontext PRIVATE VK_NO_PROTOTYPES
        LLVK_CONTEXT_SKIN_FIXTURE="${CMAKE_SOURCE_DIR}/newview/skins/default"
        LLVK_CONTEXT_FONT_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/Roboto-Regular.ttf"
        VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
    if (WINDOWS)
        target_compile_definitions(INTEGRATION_TEST_llvkcontext PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    endif ()
    LL_ADD_INTEGRATION_TEST(llvkwidgettree "" "llvkwidgets;llvkfont;llcommon;ll::libpng;ll::zlib-ng;ll::libjpeg")
    if (MSVC)
        target_compile_options(INTEGRATION_TEST_llvkwidgettree PRIVATE /bigobj)
    endif ()
    target_compile_definitions(INTEGRATION_TEST_llvkwidgettree PRIVATE
        LLVK_WIDGET_FONT_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/Roboto-Regular.ttf"
        LLVK_WIDGET_PACKAGED_FONTS="${LIBS_PREBUILT_DIR}/fonts"
        LLVK_WIDGET_SKIN_FIXTURE="${CMAKE_SOURCE_DIR}/newview/skins/default")
    LL_ADD_INTEGRATION_TEST(llvkfontface "" "llvkfont;llcommon;ll::freetype")
    target_compile_definitions(INTEGRATION_TEST_llvkfontface PRIVATE
        LLVK_FONT_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/Roboto-Regular.ttf"
        LLVK_FALLBACK_FIXTURE="${CMAKE_SOURCE_DIR}/newview/fonts/NotoSansCombined-Regular.ttf"
        LLVK_EMOJI_FIXTURE="${LIBS_PREBUILT_DIR}/fonts/TwemojiSVG.ttf"
        LLVK_VARIABLE_FIXTURE="${LIBS_PREBUILT_DIR}/fonts/InterVariableFont.ttf")
    if (LL_VULKAN_GPU_TESTS)
        find_program(LLVK_GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
        if (WINDOWS)
            target_compile_definitions(INTEGRATION_TEST_llvkcontext PRIVATE LLVK_CONTEXT_PRESENT_TEST)
            target_link_libraries(INTEGRATION_TEST_llvkcontext user32)
            add_custom_command(TARGET INTEGRATION_TEST_llvkcontext PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:INTEGRATION_TEST_llvkcontext>/shaders/compiled"
                COMMAND "${LLVK_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1
                    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui2d.vert"
                    -o "$<TARGET_FILE_DIR:INTEGRATION_TEST_llvkcontext>/shaders/compiled/ui2d.vert.spv"
                COMMAND "${LLVK_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1
                    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui2d.frag"
                    -o "$<TARGET_FILE_DIR:INTEGRATION_TEST_llvkcontext>/shaders/compiled/ui2d.frag.spv"
                VERBATIM)
        endif ()
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
    llvklegacytext.cpp
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
    llvklegacytext.h
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
    llvkwidgets
    llvkfont
    llvkglyphgpu
    llvkuipacket
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
