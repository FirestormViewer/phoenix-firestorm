# -*- cmake -*-

include_guard()
add_library( fs::glod INTERFACE IMPORTED )

include(Prebuilt)
use_prebuilt_binary(glod)
if (WINDOWS)
  target_link_libraries( fs::glod INTERFACE glod.lib)
elseif (DARWIN)
  target_link_libraries( fs::glod INTERFACE libGLOD.dylib)
elseif (LINUX)
  target_link_libraries( fs::glod INTERFACE libGLOD.a libvds.a)
endif (WINDOWS)

target_include_directories( fs::glod SYSTEM INTERFACE
        ${AUTOBUILD_INSTALL_DIR}/include/glod
        )