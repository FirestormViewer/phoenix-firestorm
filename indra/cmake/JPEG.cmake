# -*- cmake -*-
include(Prebuilt)

include(Linking)

include_guard()
add_library( ll::libjpeg INTERFACE IMPORTED )

use_system_binary(libjpeg)
use_prebuilt_binary(libjpeg-turbo)
if (LINUX)
  target_link_libraries( ll::libjpeg INTERFACE jpeg)
elseif (DARWIN)
  target_link_libraries( ll::libjpeg INTERFACE jpeg)
elseif (WINDOWS)
    target_link_libraries( ll::libjpeg INTERFACE
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/jpeg.lib
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/jpeg.lib)
endif (LINUX)
target_include_directories( ll::libjpeg SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include)
