# -*- cmake -*-

if (USESYSTEMLIBS)
  set(GLOD_FIND_REQUIRED true)
  include(FindGLOD)
else (USESYSTEMLIBS)
  include(Prebuilt)
  use_prebuilt_binary(glod)
set(GLOD_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
set(GLOD_LIBRARIES GLOD)
endif (USESYSTEMLIBS)
