# -*- cmake -*-
include(Linking)
include(Prebuilt)

if (LINUX)
  use_prebuilt_binary(openal) #Always need the .so for voice
  set(OPENAL OFF CACHE BOOL "Enable OpenAL")
else (LINUX)
  set(OPENAL OFF CACHE BOOL "Enable OpenAL")
endif (LINUX)

if (OPENAL)
  message( WARNING "Using OpenAL is discouraged due to no maintenance of the viewers openal integration, possible memory leaks and no support for streaming audio. Switch to fmodstudio if possible" )
  set(OPENAL_LIB_INCLUDE_DIRS "${LIBS_PREBUILT_DIR}/include/AL")
  if (USESYSTEMLIBS)
    include(FindPkgConfig)
    include(FindOpenAL)
    pkg_check_modules(OPENAL_LIB REQUIRED openal)
    pkg_check_modules(FREEALUT_LIB REQUIRED freealut)
  else (USESYSTEMLIBS)
    use_prebuilt_binary(openal)
  endif (USESYSTEMLIBS)
  if(WINDOWS)
    set(OPENAL_LIBRARIES 
      OpenAL32
      alut
    )
  else()
    set(OPENAL_LIBRARIES 
      openal
      alut
    )
  endif()
endif (OPENAL)
