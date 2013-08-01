# -*- cmake -*-

# - Find Apache Portable Runtime
# Find the APR includes and libraries
# This module defines
#  APR_INCLUDE_DIR and APRUTIL_INCLUDE_DIR, where to find apr.h, etc.
#  APR_LIBRARIES and APRUTIL_LIBRARIES, the libraries needed to use APR.
#  APR_FOUND and APRUTIL_FOUND, If false, do not try to use APR.
# also defined, but not for general use are
#  APR_LIBRARY and APRUTIL_LIBRARY, where to find the APR library.

# APR first.

# This version has been modified to search the OS X SDKs first. This file
#  is included for both STANDALONE builds and OS X anything builds. We
#  need to find the OS X SDK first, if it's present, to avoid conflicts
#  between building on one release and pulling in a library that's
#  incompatible with the target release. -- TS, 22 Nov 2011

FIND_PATH(APR_INCLUDE_DIR apr.h
/Developer/SDKs/MacOSX10.5.sdk/usr/include/apr-1
/Developer/SDKs/MacOSX10.6.sdk/usr/include/apr-1
/usr/local/include/apr-1
/usr/local/include/apr-1.0
/usr/include/apr-1
/usr/include/apr-1.0
)

SET(APR_NAMES ${APR_NAMES} apr-1)
# Gah, what a grody hack. We do this twice, the first time ignoring any system
#  libraries to let the OS X SDK be found first, the second in case the first
#  didn't turn up the libraries. The second search is not expected to be
#  needed; it's a belt-and-suspenders check. -- TS, 22 Nov 2011
FIND_LIBRARY(APR_LIBRARY
  NAMES ${APR_NAMES}
  PATHS /Developer/SDKs/MacOSX10.5.sdk/usr/lib
        /Developer/SDKs/MacOSX10.6.sdk/usr/lib
        /usr/lib /usr/local/lib
  NO_DEFAULT_PATH
  )
IF (NOT APR_LIBRARY)
  FIND_LIBRARY(APR_LIBRARY
    NAMES ${APR_NAMES}
    PATHS /usr/lib /usr/local/lib
  )
ENDIF (NOT APR_LIBRARY)

IF (APR_LIBRARY AND APR_INCLUDE_DIR)
    SET(APR_LIBRARIES ${APR_LIBRARY})
    SET(APR_FOUND "YES")
ELSE (APR_LIBRARY AND APR_INCLUDE_DIR)
  SET(APR_FOUND "NO")
ENDIF (APR_LIBRARY AND APR_INCLUDE_DIR)


IF (APR_FOUND)
   IF (NOT APR_FIND_QUIETLY)
      MESSAGE(STATUS "Found APR: ${APR_LIBRARIES}")
   ENDIF (NOT APR_FIND_QUIETLY)
ELSE (APR_FOUND)
   IF (APR_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find APR library")
   ENDIF (APR_FIND_REQUIRED)
ENDIF (APR_FOUND)

# Deprecated declarations.
SET (NATIVE_APR_INCLUDE_PATH ${APR_INCLUDE_DIR} )
GET_FILENAME_COMPONENT (NATIVE_APR_LIB_PATH ${APR_LIBRARY} PATH)

MARK_AS_ADVANCED(
  APR_LIBRARY
  APR_INCLUDE_DIR
  )

# Next, APRUTIL.

FIND_PATH(APRUTIL_INCLUDE_DIR apu.h
/Developer/SDKs/MacOSX10.5.sdk/usr/include/apr-1
/Developer/SDKs/MacOSX10.6.sdk/usr/include/apr-1
/usr/local/include/apr-1
/usr/local/include/apr-1.0
/usr/include/apr-1
/usr/include/apr-1.0
)

SET(APRUTIL_NAMES ${APRUTIL_NAMES} aprutil-1)
FIND_LIBRARY(APRUTIL_LIBRARY
  NAMES ${APRUTIL_NAMES}
  PATHS /Developer/SDKs/MacOSX10.5.sdk/usr/lib
        /Developer/SDKs/MacOSX10.6.sdk/usr/lib
        /usr/lib /usr/local/lib
  NO_DEFAULT_PATH
  )
IF (NOT APRUTIL_LIBRARY)
  FIND_LIBRARY(APRUTIL_LIBRARY
    NAMES ${APRUTIL_NAMES}
    PATHS /usr/lib /usr/local/lib
  )
ENDIF (NOT APRUTIL_LIBRARY)

IF (APRUTIL_LIBRARY AND APRUTIL_INCLUDE_DIR)
    SET(APRUTIL_LIBRARIES ${APRUTIL_LIBRARY})
    SET(APRUTIL_FOUND "YES")
ELSE (APRUTIL_LIBRARY AND APRUTIL_INCLUDE_DIR)
  SET(APRUTIL_FOUND "NO")
ENDIF (APRUTIL_LIBRARY AND APRUTIL_INCLUDE_DIR)


IF (APRUTIL_FOUND)
   IF (NOT APRUTIL_FIND_QUIETLY)
      MESSAGE(STATUS "Found APRUTIL: ${APRUTIL_LIBRARIES}")
   ENDIF (NOT APRUTIL_FIND_QUIETLY)
ELSE (APRUTIL_FOUND)
   IF (APRUTIL_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find APRUTIL library")
   ENDIF (APRUTIL_FIND_REQUIRED)
ENDIF (APRUTIL_FOUND)

# Deprecated declarations.
SET (NATIVE_APRUTIL_INCLUDE_PATH ${APRUTIL_INCLUDE_DIR} )
GET_FILENAME_COMPONENT (NATIVE_APRUTIL_LIB_PATH ${APRUTIL_LIBRARY} PATH)

MARK_AS_ADVANCED(
  APRUTIL_LIBRARY
  APRUTIL_INCLUDE_DIR
  )
