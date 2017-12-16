# -*- cmake -*-

# FMOD can be set when launching the make using the argument -DFMOD:BOOL=ON
# When building using proprietary binaries though (i.e. having access to LL private servers),
# we always build with FMODSTUDIO.
# Open source devs should use the -DFMODSTUDIO:BOOL=ON then if they want to build with FMOD, whether
# they are using USESYSTEMLIBS or not.
if (INSTALL_PROPRIETARY)
  set(FMODSTUDIO ON CACHE BOOL "Using FMOD Studio sound library.")
endif (INSTALL_PROPRIETARY)


if (FMODSTUDIO)
  if (USESYSTEMLIBS)
    # In that case, we use the version of the library installed on the system
    set(FMODSTUDIO_FIND_REQUIRED ON)
    include(FindFMODSTUDIO)
  else (USESYSTEMLIBS)
    if (FMODSTUDIO_LIBRARY AND FMODSTUDIO_INCLUDE_DIR)
      # If the path have been specified in the arguments, use that
      set(FMODSTUDIO_LIBRARIES ${FMODSTUDIO_LIBRARY})
      MESSAGE(STATUS "Using FMODSTUDIO path: ${FMODSTUDIO_LIBRARIES}, ${FMODSTUDIO_INCLUDE_DIR}")
    else (FMODSTUDIO_LIBRARY AND FMODSTUDIO_INCLUDE_DIR)
      # If not, we're going to try to get the package listed in autobuild.xml
      # Note: if you're not using INSTALL_PROPRIETARY, the package URL should be local (file:/// URL) 
      # as accessing the private LL location will fail if you don't have the credential
      include(Prebuilt)
      use_prebuilt_binary(fmodstudio)
      if (WINDOWS)
        if (NOT ND_BUILD64BIT_ARCH)
          set(FMODSTUDIO_LIBRARY 
              debug fmodL_vc
              optimized fmod_vc)
         elseif (ND_BUILD64BIT_ARCH)
           set(FMODSTUDIO_LIBRARY 
              debug fmodL64_vc
              optimized fmod64_vc)
         endif(NOT ND_BUILD64BIT_ARCH)
      elseif (DARWIN)
        set(FMODSTUDIO_LIBRARY 
            debug fmodL
            optimized fmod)
      elseif (LINUX)
        set(FMODSTUDIO_LIBRARY 
            debug fmodL
            optimized fmod)
      endif (WINDOWS)
      set(FMODSTUDIO_LIBRARIES ${FMODSTUDIO_LIBRARY})
      set(FMODSTUDIO_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/fmodstudio)
    endif (FMODSTUDIO_LIBRARY AND FMODSTUDIO_INCLUDE_DIR)
  endif (USESYSTEMLIBS)
endif (FMODSTUDIO)

