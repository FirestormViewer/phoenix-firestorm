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

find_library(LIBPNG_LIBRARY
    NAMES
    libpng16.lib
    libpng16.a
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

target_link_libraries(ll::libpng INTERFACE ${LIBPNG_LIBRARY})
target_include_directories(ll::libpng SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/libpng16)
