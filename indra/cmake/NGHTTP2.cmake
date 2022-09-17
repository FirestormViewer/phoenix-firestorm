include(Linking)
include(Prebuilt)

include_guard()
add_library( ll::nghttp2 INTERFACE IMPORTED )

use_system_binary(nghttp2)
use_prebuilt_binary(nghttp2)
if (WINDOWS)
  # <FS:Ansariel> ARCH_PREBUILT_DIRS_RELEASE is "." and would cause searching for the lib in the wrong place when not using VS
  ##target_link_libraries( ll::nghttp2 INTERFACE ${ARCH_PREBUILT_DIRS_RELEASE}/nghttp2.lib)
  target_link_libraries( ll::nghttp2 INTERFACE nghttp2.lib)
elseif (DARWIN)
  target_link_libraries( ll::nghttp2 INTERFACE libnghttp2.dylib)
else (WINDOWS)
  target_link_libraries( ll::nghttp2 INTERFACE libnghttp2.a )
endif (WINDOWS)
target_include_directories( ll::nghttp2 SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/nghttp2)
