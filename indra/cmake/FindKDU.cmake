# -*- cmake -*-

# - Find KDU
# This module defines
#  KDU_INCLUDE_DIR, where to find kdu.h, etc.
#  KDU_LIBRARIES, the library needed to use KDU.
#  KDU_FOUND, If false, do not try to use KDU.

# LL coded the #include with a glod path in llfloatermodelpreview.cpp
find_path(KDU_INCLUDE_DIR kdu_image.h
  PATH_SUFFIXES kdu
  )

set(KDU_NAMES ${KDU_NAMES} kdu)
find_library(KDU_LIBRARIES
  NAMES ${KDU_NAMES}
  )

if (KDU_LIBRARIES AND KDU_INCLUDE_DIR)
  set(KDU_FOUND "YES")
  set(KDU_INCLUDE_DIRS ${KDU_INCLUDE_DIR})
  set(KDU_LIBRARY ${KDU_LIBRARIES})
else (KDU_LIBRARIES AND KDU_INCLUDE_DIR)
  set(KDU_FOUND "NO")
endif (KDU_LIBRARIES AND KDU_INCLUDE_DIR)

if (KDU_FOUND)
  if (NOT KDU_FIND_QUIETLY)
    message(STATUS "Found KDU: Library in '${KDU_LIBRARY}' and header in '${KDU_INCLUDE_DIR}' ")
  endif (NOT KDU_FIND_QUIETLY)
else (KDU_FOUND)
  if (KDU_FIND_REQUIRED)
    message(FATAL_ERROR " * * * * *\nCould not find KDU library!\n* * * * *")
  endif (KDU_FIND_REQUIRED)
endif (KDU_FOUND)

mark_as_advanced(
  KDU_LIBRARIES
  KDU_INCLUDE_DIR
  )
