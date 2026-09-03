# -*- cmake -*-
# <VulkanStorm>
# Mesa Zink (OpenGL-over-Vulkan) runtime, bundled as a prebuilt 3p package
# (3p-mesazink, built from the patched Mesa devel tree: AMD RX9000-series
# support + viewer crash-region fixes). The package ships the Gallium WGL
# opengl32.dll and libgallium_wgl.dll only; they are staged into the viewer's
# mesa\ subdirectory and selected via RenderBackend=Zink (delay-loaded
# opengl32 import redirected at runtime). OFF by default.
include(Prebuilt)

if (WINDOWS)
    option(USE_MESAZINK "Bundle the Mesa Zink OpenGL-over-Vulkan runtime" OFF)
else ()
    set(USE_MESAZINK OFF CACHE BOOL "Bundle the Mesa Zink OpenGL-over-Vulkan runtime" FORCE)
endif ()

if (USE_MESAZINK)
    use_prebuilt_binary(mesazink)
    foreach(mesazink_file opengl32.dll libgallium_wgl.dll)
        if (NOT EXISTS "${AUTOBUILD_INSTALL_DIR}/bin/release/${mesazink_file}")
            message(FATAL_ERROR "Missing Mesa Zink runtime file: ${mesazink_file} (re-run autobuild install)")
        endif ()
    endforeach()
endif ()
# </VulkanStorm>
