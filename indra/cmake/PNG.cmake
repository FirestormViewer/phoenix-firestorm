# -*- cmake -*-

if(${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 8 AND ${CMAKE_PATCH_VERSION} GREATER 3) 
# Silence warning about overriding findZLIB.cmake with our own copy.
 cmake_policy(SET CMP0017 OLD)
# Silence warning about using policies to silence warnings about using policies.
 cmake_policy(SET CMP0011 NEW)
endif(${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 8 AND ${CMAKE_PATCH_VERSION} GREATER 3) 

include(Prebuilt)

set(PNG_FIND_QUIETLY ON)
set(PNG_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindPNG)
else (STANDALONE)
  use_prebuilt_binary(libpng)
  if (WINDOWS)
    set(PNG_LIBRARIES libpng15)
    set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng15)
  elseif(DARWIN)
    set(PNG_LIBRARIES png15)
    set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng15)
  else()
    set(PNG_LIBRARIES png15)
    set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng15)
  endif()
endif (STANDALONE)
