# -*- cmake -*-

if(${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 8 AND ${CMAKE_PATCH_VERSION} GREATER 3) 
# Silence warning about overriding findZLIB.cmake with our own copy.
 cmake_policy(SET CMP0017 OLD)
# Silence warning about using policies to silence warnings about using policies.
 cmake_policy(SET CMP0011 NEW)
endif(${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 8 AND ${CMAKE_PATCH_VERSION} GREATER 3) 

include(Prebuilt)

include_guard()
add_library( ll::libpng INTERFACE IMPORTED )

use_system_binary(libpng)
use_prebuilt_binary(libpng)
if (WINDOWS)
  target_link_libraries(ll::libpng INTERFACE libpng16)
else()
  target_link_libraries(ll::libpng INTERFACE png16 )
endif()
target_include_directories( ll::libpng SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/libpng16)
