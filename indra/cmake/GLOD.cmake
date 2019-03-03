# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  use_prebuilt_binary(glod)
  set(GLOD_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
  set(GLOD_LIBRARIES GLOD vds)
else()
  include(FindGLOD)
endif (NOT USESYSTEMLIBS)

