# -*- cmake -*-
include(Prebuilt)
include(Linking)

if( NOT USESYSTEMLIBS )
  
  use_prebuilt_binary(googlemock)

  set(GOOGLEMOCK_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include)

  if (LINUX)
      # VWR-24366: gmock is underlinked, it needs gtest.
      set(GOOGLEMOCK_LIBRARIES
          gmock -Wl,--no-as-needed
          gtest -Wl,--as-needed)
  elseif(WINDOWS)
      set(GOOGLEMOCK_LIBRARIES
          gmock)
      set(GOOGLEMOCK_INCLUDE_DIRS
          ${LIBS_PREBUILT_DIR}/include
          ${LIBS_PREBUILT_DIR}/include/gmock
          ${LIBS_PREBUILT_DIR}/include/gmock/boost/tr1/tr1)
  elseif(DARWIN)
      set(GOOGLEMOCK_LIBRARIES
          gmock
          gtest)
  endif(LINUX)
else()
  
  find_library( GOOGLETEST_LIBRARY gtest )
  find_library( GOOGLEMOCK_LIBRARY gmock )

  if( GOOGLETEST_LIBRARY STREQUAL "GOOGLETEST_LIBRARY-NOTFOUND" )
    message( FATAL_ERROR "Cannot find gtest library" )
  endif()

  if( GOOGLEMOCK_LIBRARY STREQUAL "GOOGLEMOCK_LIBRARY-NOTFOUND" )
    message( FATAL_ERROR "Cannot find gmock library" )
  endif()

  set(GOOGLEMOCK_LIBRARIES ${GOOGLEMOCK_LIBRARY} ${GOOGLETEST_LIBRARY} )
  
endif()

