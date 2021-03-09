# -*- cmake -*-
include(Prebuilt)

if (LINUX AND NOT SYSTEMLIBS )
  set(USE_JEMALLOC ON)
endif ()

if( USE_JEMALLOC )
  if (USESYSTEMLIBS)
    message( WARNING "Not implemented" )
  else (USESYSTEMLIBS)
    use_prebuilt_binary(jemalloc)
  endif (USESYSTEMLIBS)
endif()
