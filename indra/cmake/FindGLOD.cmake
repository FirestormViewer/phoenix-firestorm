# -*- cmake -*-

# - Find GLOD
# This module defines
#  GLOD_INCLUDE_DIR, where to find glod.h, etc.
#  GLOD_LIBRARIES, the library needed to use GLOD.
#  GLOD_FOUND, If false, do not try to use GLOD.

# LL coded the #include with a glod path in llfloatermodelpreview.cpp
find_path(GLOD_INCLUDE_DIR glod/glod.h)

set(GLOD_NAMES ${GLOD_NAMES} glod)
find_library(GLOD_LIBRARIES
  NAMES ${GLOD_NAMES}
  )

if (GLOD_LIBRARIES AND GLOD_INCLUDE_DIR)
  set(GLOD_FOUND "YES")
else (GLOD_LIBRARIES AND GLOD_INCLUDE_DIR)
  set(GLOD_FOUND "NO")
endif (GLOD_LIBRARIES AND GLOD_INCLUDE_DIR)

if (GLOD_FOUND)
  if (NOT GLOD_FIND_QUIETLY)
    message(STATUS "Found GLOD: Library in '${GLOD_LIBRARY}' and header in '${GLOD_INCLUDE_DIR}' ")
  endif (NOT GLOD_FIND_QUIETLY)
else (GLOD_FOUND)
  if (GLOD_FIND_REQUIRED)
    message(FATAL_ERROR " * * * * *\nCould not find GLOD library!\n* * * * *")
  endif (GLOD_FIND_REQUIRED)
endif (GLOD_FOUND)

mark_as_advanced(
  GLOD_LIBRARIES
  GLOD_INCLUDE_DIR
  )
