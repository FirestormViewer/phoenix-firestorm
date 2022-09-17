# -*- cmake -*-

# Growl is actually libnotify on linux systems.

include_guard()
add_library( fs::growl INTERFACE IMPORTED )

include(Prebuilt)
use_prebuilt_binary(gntp-growl)
if (WINDOWS)
  target_link_libraries( fs::growl INTERFACE growl.lib growl++.lib)
elseif (DARWIN)
  target_link_libraries( fs:growl INTERFACE libgrowl.dylib libgrowl++.dylib)
endif (WINDOWS)

add_compile_definitions(HAS_GROWL)

target_include_directories( fs::growl SYSTEM INTERFACE
        ${AUTOBUILD_INSTALL_DIR}/include/Growl
        )