# -*- cmake -*-

include_guard()

option(USE_FMODSTUDIO "Enable the optional FMOD Studio audio engine" OFF)

# ND: To streamline arguments passed, switch from FMODSTUDIO to USE_FMODSTUDIO
# To not break all old build scripts convert old arguments but warn about it
if(FMODSTUDIO)
  message( WARNING "Use of the FMODSTUDIO argument is deprecated, please switch to USE_FMODSTUDIO")
  set(USE_FMODSTUDIO ${FMODSTUDIO})
endif()

if (USE_FMODSTUDIO)
  add_library( ll::fmodstudio INTERFACE IMPORTED )
  target_compile_definitions( ll::fmodstudio INTERFACE LL_FMODSTUDIO=1)

  if (FMODSTUDIO_LIBRARY AND FMODSTUDIO_INCLUDE_DIR)
    # If the path have been specified in the arguments, use that

    target_link_libraries(ll::fmodstudio INTERFACE ${FMODSTUDIO_LIBRARY})
    target_include_directories( ll::fmodstudio SYSTEM INTERFACE  ${FMODSTUDIO_INCLUDE_DIR})
  else (FMODSTUDIO_LIBRARY AND FMODSTUDIO_INCLUDE_DIR)
    # If not, we're going to try to get the package listed in autobuild.xml
    # Note: if you're not using INSTALL_PROPRIETARY, the package URL should be local (file:/// URL)
    # as accessing the private LL location will fail if you don't have the credential
    include(Prebuilt)
    use_prebuilt_binary(fmodstudio)

    find_library(FMOD_LIBRARY
      NAMES
      fmod_vc.lib
      libfmod.dylib
      libfmod.so
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

    target_link_libraries(ll::fmodstudio INTERFACE ${FMOD_LIBRARY})

    target_include_directories( ll::fmodstudio SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/fmodstudio)
  endif (FMODSTUDIO_LIBRARY AND FMODSTUDIO_INCLUDE_DIR)
else()
  set( USE_FMODSTUDIO "OFF")
endif ()

