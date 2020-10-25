# -*- cmake -*-
include(Prebuilt)

set(OPENJPEG_FIND_QUIETLY ON)
set(OPENJPEG_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindOpenJPEG)
else (USESYSTEMLIBS)
  use_prebuilt_binary(openjpeg)
  
  if(DARWIN)
    set(OPENJPEG_LIBRARIES openjpeg)
  else(DARWIN)
    set(OPENJPEG_LIBRARIES openjp2)
  endif(DARWIN)

  set(OPENJPEG_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/openjpeg)
endif (USESYSTEMLIBS)
