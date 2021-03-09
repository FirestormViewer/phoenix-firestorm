# -*- cmake -*-

# - Find NGHTTP2
# This module defines
#  NGHTTP2_INCLUDE_DIR, where to find glod.h, etc.
#  NGHTTP2_LIBRARIES, the library needed to use NGHTTP2.
#  NGHTTP2_FOUND, If false, do not try to use NGHTTP2.

find_path(NGHTTP2_INCLUDE_DIR nghttp2/nghttp2.h)

set(NGHTTP2_NAMES ${NGHTTP2_NAMES} nghttp2)
find_library(NGHTTP2_LIBRARIES
  NAMES ${NGHTTP2_NAMES}
  )

if (NGHTTP2_LIBRARIES AND NGHTTP2_INCLUDE_DIR)
  set(NGHTTP2_FOUND "YES")
else (NGHTTP2_LIBRARIES AND NGHTTP2_INCLUDE_DIR)
  set(NGHTTP2_FOUND "NO")
endif (NGHTTP2_LIBRARIES AND NGHTTP2_INCLUDE_DIR)

if (NGHTTP2_FOUND)
  if (NOT NGHTTP2_FIND_QUIETLY)
    message(STATUS "Found NGHTTP2: Library in '${NGHTTP2_LIBRARY}' and header in '${NGHTTP2_INCLUDE_DIR}' ")
  endif (NOT NGHTTP2_FIND_QUIETLY)
else (NGHTTP2_FOUND)
  if (NGHTTP2_FIND_REQUIRED)
    message(FATAL_ERROR " * * * * *\nCould not find NGHTTP2 library!\n* * * * *")
  endif (NGHTTP2_FIND_REQUIRED)
endif (NGHTTP2_FOUND)

mark_as_advanced(
  NGHTTP2_LIBRARIES
  NGHTTP2_INCLUDE_DIR
  )
