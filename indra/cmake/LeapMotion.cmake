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
    set(LEAP_MOTION_LIBRARY
        debug Leapd.dll
        optimized Leap.dll)
   elseif (LINUX)
     set(LEAP_MOTION_LIBRARY libLeap.so)
   endif()
    set(LEAP_MOTION_INCLUDE_DIR ${LIBS_OPEN_DIR}/leap-motion)
  endif (STANDALONE)
endif( LEAPMOTION )