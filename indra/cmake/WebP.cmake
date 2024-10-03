# -*- cmake -*-

include_guard()

include(Prebuilt)

add_library( ll::libwebp INTERFACE IMPORTED )

use_system_binary( libwebp )

use_prebuilt_binary(libwebp)
if(WINDOWS)
  target_link_libraries( ll::libwebp INTERFACE
    debug ${ARCH_PREBUILT_DIRS_DEBUG}/libwebp_debug.lib
    optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libwebp.lib
    )
else()
  target_link_libraries( ll::libwebp INTERFACE
    debug ${ARCH_PREBUILT_DIRS_DEBUG}/libwebp.a
    optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libwebp.a
    debug ${ARCH_PREBUILT_DIRS_DEBUG}/libsharpyuv.a
    optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libsharpyuv.a
    )
endif()

target_include_directories( ll::libwebp SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/webp)
