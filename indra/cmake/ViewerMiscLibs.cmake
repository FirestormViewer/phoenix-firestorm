# -*- cmake -*-
include(Prebuilt)

if (LINUX)
  use_prebuilt_binary(libuuid)
  add_library( ll::fontconfig INTERFACE IMPORTED )

  if( NOT USE_CONAN )
    find_package(Fontconfig REQUIRED) # <FS:PC> fontconfig and freetype should be taken from the
    # use_prebuilt_binary(fontconfig) #         user's system, and not be packaged with the viewer
  else()
    target_link_libraries( ll::fontconfig INTERFACE CONAN_PKG::fontconfig )
  endif()
endif (LINUX)

if( NOT USE_CONAN )
  use_prebuilt_binary(libhunspell)
endif()

use_prebuilt_binary(slvoice)

