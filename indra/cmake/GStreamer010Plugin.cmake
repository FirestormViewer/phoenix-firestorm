# -*- cmake -*-
include(Prebuilt)
if (NOT LINUX)
  return()
endif()

add_library( ll::gstreamer INTERFACE IMPORTED )
target_compile_definitions( ll::gstreamer INTERFACE LL_GSTREAMER010_ENABLED=1)
use_system_binary(gstreamer)

use_prebuilt_binary(gstreamer)

# <FS:Zi> Not sure if this is the correct place to add this, but it works
target_include_directories( ll::gstreamer SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/gstreamer-0.10)
