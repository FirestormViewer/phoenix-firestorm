# -*- cmake -*-

# - Find HUNSPELL
# This module defines
#  HACD_INCLUDE_DIR, where to find hacdHACD.h, etc.
#  HACD_LIBRARY, the library needed to use HACD.
#  HACD_FOUND, If false, do not try to use HACD.

find_path(HACD_INCLUDE_DIR hacdHACD.h
  PATH_SUFFIXES hacd
  )

set(HACD_NAMES ${HACD_NAMES} hacd)
find_library(HACD_LIBRARY
  NAMES ${HACD_NAMES}
  )

find_library( LLCONVEXDECOMP_LIBRARY NAMES  nd_hacdConvexDecomposition )

if (HACD_LIBRARY AND HACD_INCLUDE_DIR AND LLCONVEXDECOMP_LIBRARY)
  set(HACD_FOUND "YES")
else (HACD_LIBRARY AND HACD_INCLUDE_DIR AND LLCONVEXDECOMP_LIBRARY)
  set(HACD_FOUND "NO")
endif (HACD_LIBRARY AND HACD_INCLUDE_DIR AND LLCONVEXDECOMP_LIBRARY)


if (HACD_FOUND)
  if (NOT HACD_FIND_QUIETLY)
    message(STATUS "Found HACD: Library in '${HACD_LIBRARY}' and header in '${HACD_INCLUDE_DIR}' ")
  endif (NOT HACD_FIND_QUIETLY)
else (HACD_FOUND)
  if (HACD_FIND_REQUIRED)
    message(FATAL_ERROR " * * *\nCould not find HACD library! * * *")
  endif (HACD_FIND_REQUIRED)
endif (HACD_FOUND)

mark_as_advanced(
  HACD_LIBRARY
  HACD_DECOMP_LIB
  HACD_INCLUDE_DIR
  )
