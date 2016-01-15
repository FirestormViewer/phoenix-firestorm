# -*- cmake -*-
include(Prebuilt)

set(OPENJPEG_FIND_QUIETLY ON)
set(OPENJPEG_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindOpenJPEG)
else (USESYSTEMLIBS)
  use_prebuilt_binary(openjpeg2)
  
  if(WINDOWS)
    set(OPENJPEG_LIBRARIES 
        debug openjp2
        optimized openjp2)
  else(WINDOWS)
    set(OPENJPEG_LIBRARIES openjp2)
  endif(WINDOWS)
  
  # set(OPENJPEG_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/openjpeg)
endif (USESYSTEMLIBS)
