# -*- cmake -*-

# Growl is actually libnotify on linux systems.

if (DARWIN OR WINDOWS) # <FS:Zi> no need to do these things on Linux

include_guard()
add_library( fs::growl INTERFACE IMPORTED )

# Growl has no ARM64 build; disable on Windows ARM64.
if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  include(Prebuilt)
  use_prebuilt_binary(gntp-growl)

  find_library(GROWL_LIBRARY
    NAMES
    growl.lib
    libgrowl.dylib
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  find_library(GROWL_PLUSPLUS_LIBRARY
    NAMES
    growl++.lib
    libgrowl++.dylib
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  target_link_libraries(fs::growl INTERFACE ${GROWL_LIBRARY} ${GROWL_PLUSPLUS_LIBRARY})

  target_include_directories( fs::growl SYSTEM INTERFACE
          ${AUTOBUILD_INSTALL_DIR}/include/Growl
          )
endif()
endif (DARWIN OR WINDOWS) # <FS:Zi> no need to do these things on Linux
