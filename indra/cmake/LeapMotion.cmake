# -*- cmake -*-

include(Linking)

if (INSTALL_PROPRIETARY)
  set(LEAPMOTION ON CACHE BOOL "Building with Leap Motion Controller support.")
endif (INSTALL_PROPRIETARY)

if( LEAPMOTION )
  if (STANDALONE)
    # *TODO: Standalone support
  else (STANDALONE)
    include(Prebuilt)
    use_prebuilt_binary(leap-motion)
  if (DARWIN)
    set(LEAP_MOTION_LIBRARY libLeap.dylib)
  elseif (WINDOWS)
    if (ND_BUILD64BIT_ARCH)
        set(LEAP_MOTION_LIBRARY
            debug ${LIBS_PREBUILT_DIR}/lib/debug/x64/Leapd.lib
            optimized ${LIBS_PREBUILT_DIR}/lib/release/x64/Leap.lib)
    else (ND_BUILD64BIT_ARCH)
        set(LEAP_MOTION_LIBRARY
            debug ${LIBS_PREBUILT_DIR}/lib/debug/x86/Leapd.lib
            optimized ${LIBS_PREBUILT_DIR}/lib/release/x86/Leap.lib)
    endif (ND_BUILD64BIT_ARCH)
   elseif (LINUX)
    if (ND_BUILD64BIT_ARCH)
        set(LEAP_MOTION_LIBRARY ${LIBS_PREBUILT_DIR}/lib/release/x64/libLeap.so)
    else (ND_BUILD64BIT_ARCH)
        set(LEAP_MOTION_LIBRARY ${LIBS_PREBUILT_DIR}/lib/release/x86/libLeap.so)
    endif (ND_BUILD64BIT_ARCH)
   endif()
    set(LEAP_MOTION_INCLUDE_DIR ${LIBS_OPEN_DIR}/leap-motion)
  endif (STANDALONE)
endif( LEAPMOTION )