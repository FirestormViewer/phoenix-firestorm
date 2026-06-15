# -*- cmake -*-
include(Linking)
include(Prebuilt)

include_guard()

# ND: Turn this off by default, the openal code in the viewer isn't very well maintained, seems
# to have memory leaks, has no option to play music streams
# It probably makes sense to to completely remove it

set(USE_OPENAL ON CACHE BOOL "Enable OpenAL")

# <FS:Zi> Always download the libopenal.so library on Linux for SLVoice
if (LINUX)
  use_prebuilt_binary(openal)
endif (LINUX)

# ND: To streamline arguments passed, switch from OPENAL to USE_OPENAL
# To not break all old build scripts convert old arguments but warn about it
if(OPENAL)
  message( WARNING "Use of the OPENAL argument is deprecated, please switch to USE_OPENAL")
  set(USE_OPENAL ${OPENAL})
endif()

if (USE_OPENAL)
  add_library( ll::openal INTERFACE IMPORTED )
  target_include_directories( ll::openal SYSTEM INTERFACE "${LIBS_PREBUILT_DIR}/include")
  target_compile_definitions( ll::openal INTERFACE LL_OPENAL=1)
  # ARM64: OpenAL Soft is built locally; the prebuilt (x64) package isn't applicable.
  if (NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    use_prebuilt_binary(openal)
  endif()

  find_library(OPENAL_LIBRARY
      NAMES
      OpenAL32
      openal
      libopenal.dylib
      libopenal.so
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  # ALUT dependency removed: the OpenAL backend now uses raw ALC + a built-in WAV
  # loader, so freealut (alut) is no longer linked on any platform.
  target_link_libraries(ll::openal INTERFACE ${OPENAL_LIBRARY})

endif ()
