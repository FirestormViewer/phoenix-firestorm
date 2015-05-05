# -*- cmake -*-
include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindPkgConfig)

  pkg_check_modules(FREETYPE REQUIRED freetype2)
else (USESYSTEMLIBS)
  use_prebuilt_binary(freetype)
  set(FREETYPE_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
  set(FREETYPE_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/freetype2) #<FS:ND/> Also add freetype2 to search dir, or some includes will fail.
  set(FREETYPE_LIBRARIES freetype)
endif (USESYSTEMLIBS)

link_directories(${FREETYPE_LIBRARY_DIRS})
