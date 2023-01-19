# -*- cmake -*-
include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindPkgConfig)

  pkg_check_modules(FREETYPE REQUIRED freetype2)
else (USESYSTEMLIBS)
  if (LINUX)                          # <FS:PC> linux fontconfig and freetype should come
    find_package(Freetype REQUIRED)   #         from the user's system
  else (LINUX)                        #         Linux links this via llwindow/CMakeLists
    use_prebuilt_binary(freetype)
    set(FREETYPE_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
    set(FREETYPE_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/freetype2) #<FS:ND/> Also add freetype2 to search dir, or some includes will fail.
    set(FREETYPE_LIBRARIES freetype)
  endif()
endif (USESYSTEMLIBS)

link_directories(${FREETYPE_LIBRARY_DIRS})
