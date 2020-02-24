# -*- cmake -*-
include(Prebuilt)

if (LINUX AND NOT SYSTEMLIBS )
  set(USE_JEMALLOC ON)
endif ()

if( USE_JEMALLOC )
  if (USESYSTEMLIBS)
    message( WARNING "Search for jemalloc not implemented for standalone builds" )
  else (USESYSTEMLIBS)
    use_prebuilt_binary(jemalloc)
  endif (USESYSTEMLIBS)
endif()
