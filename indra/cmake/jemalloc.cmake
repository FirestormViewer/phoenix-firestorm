# -*- cmake -*-
include(Prebuilt)

if( USE_JEMALLOC )
  if (USESYSTEMLIBS)
    message( WARNING "Not implemented" )
  else (USESYSTEMLIBS)
    use_prebuilt_binary(jemalloc)
  endif (USESYSTEMLIBS)
endif()
