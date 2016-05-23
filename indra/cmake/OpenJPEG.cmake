# -*- cmake -*-
include(Prebuilt)

if( ND_USE_OPENJPEG2 )
  message( "Using OpenJPEG2" )
  include( OpenJPEG2 )
  return()
endif( ND_USE_OPENJPEG2 )

set(OPENJPEG_FIND_QUIETLY ON)
set(OPENJPEG_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindOpenJPEG)
else (USESYSTEMLIBS)
  use_prebuilt_binary(openjpeg)
  
  if(WINDOWS)
    # Windows has differently named release and debug openjpeg(d) libs.
    set(OPENJPEG_LIBRARIES 
        debug openjpegd
        optimized openjpeg)
  else(WINDOWS)
    set(OPENJPEG_LIBRARIES openjpeg)
  endif(WINDOWS)
  
    set(OPENJPEG_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/openjpeg)
endif (USESYSTEMLIBS)
