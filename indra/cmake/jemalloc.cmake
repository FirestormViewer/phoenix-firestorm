# -*- cmake -*-
include(Prebuilt)

set( USE_JEMALLOC ON CACHE BOOL "Ship prebuild jemalloc library with packaged viewer" )

if( USE_JEMALLOC )
  if (USESYSTEMLIBS)
    message( WARNING "Search for jemalloc not implemented for standalone builds" )
  else (USESYSTEMLIBS)
    use_prebuilt_binary(jemalloc)
  endif (USESYSTEMLIBS)
endif()
