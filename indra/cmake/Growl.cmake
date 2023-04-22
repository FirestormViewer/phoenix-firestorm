# -*- cmake -*-

# Growl is actually libnotify on linux systems.

if (DARWIN OR WINDOWS) # <FS:Zi> no need to do these things on Linux

include_guard()
add_library( fs::growl INTERFACE IMPORTED )

include(Prebuilt)
use_prebuilt_binary(gntp-growl)
if (WINDOWS)
  target_link_libraries( fs::growl INTERFACE growl.lib growl++.lib)
elseif (DARWIN)
  target_link_libraries( fs::growl INTERFACE libgrowl.dylib libgrowl++.dylib)
endif (WINDOWS)

target_include_directories( fs::growl SYSTEM INTERFACE
        ${AUTOBUILD_INSTALL_DIR}/include/Growl
        )
endif (DARWIN OR WINDOWS) # <FS:Zi> no need to do these things on Linux
