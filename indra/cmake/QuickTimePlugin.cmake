# -*- cmake -*-

if(INSTALL_PROPRIETARY)
  include(Prebuilt)
  use_prebuilt_binary(quicktime)
endif(INSTALL_PROPRIETARY)

if (DARWIN)
  include(CMakeFindFrameworks)
  find_library(QUICKTIME_LIBRARY QuickTime)
elseif (WINDOWS)
  set(QUICKTIME_SDK_DIR "$ENV{PROGRAMFILES}/QuickTime SDK"
      CACHE PATH "Location of the QuickTime SDK.")

  find_library(DEBUG_QUICKTIME_LIBRARY qtmlclient.lib
               PATHS
               "${QUICKTIME_SDK_DIR}\\libraries"
               )

  find_library(RELEASE_QUICKTIME_LIBRARY qtmlclient.lib
               PATHS
               "${QUICKTIME_SDK_DIR}\\libraries"
               )

  if (DEBUG_QUICKTIME_LIBRARY AND RELEASE_QUICKTIME_LIBRARY)
    message ("Using QuickTime SDK libraries.")
  else (DEBUG_QUICKTIME_LIBRARY AND RELEASE_QUICKTIME_LIBRARY)
    message ("Using QuickTime prebuilt libraries.")
    set (USE_QUICKTIME_PREBUILT ON CACHE BOOL "Using QuickTime prebuilt libraries.")

    if (NOT INSTALL_PROPRIETARY)
      include(Prebuilt)
      use_prebuilt_binary(quicktime)
    endif (NOT INSTALL_PROPRIETARY)

    find_library(DEBUG_QUICKTIME_LIBRARY qtmlclient.lib
                 PATHS
                 ${ARCH_PREBUILT_DIRS_RELEASE}
                 )

    find_library(RELEASE_QUICKTIME_LIBRARY qtmlclient.lib
                 PATHS
                 ${ARCH_PREBUILT_DIRS_RELEASE}
                 )
  endif (DEBUG_QUICKTIME_LIBRARY AND RELEASE_QUICKTIME_LIBRARY)

  if (DEBUG_QUICKTIME_LIBRARY AND RELEASE_QUICKTIME_LIBRARY)
    set(QUICKTIME_LIBRARY 
        optimized ${RELEASE_QUICKTIME_LIBRARY}
        debug ${DEBUG_QUICKTIME_LIBRARY}
        )
        
  endif (DEBUG_QUICKTIME_LIBRARY AND RELEASE_QUICKTIME_LIBRARY)
  
  if (USE_QUICKTIME_PREBUILT)
    include_directories(
      ${LIBS_PREBUILT_DIR}/include/quicktime
    )
  else (USE_QUICKTIME_PREBUILT)
    include_directories(
      "${QUICKTIME_SDK_DIR}\\CIncludes"
    )
  endif (USE_QUICKTIME_PREBUILT)
endif (DARWIN)

mark_as_advanced(QUICKTIME_LIBRARY)

if (QUICKTIME_LIBRARY)
  set(QUICKTIME ON CACHE BOOL "Build with QuickTime streaming media support.")
endif (QUICKTIME_LIBRARY)

