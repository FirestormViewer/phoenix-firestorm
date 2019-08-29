# -*- cmake -*-

if (USESYSTEMLIBS)
  include(FindGLOD)
else (USESYSTEMLIBS)
  include(Prebuilt)
  use_prebuilt_binary(glod)
set(GLOD_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
if(LINUX)
  set(GLOD_LIBRARIES GLOD vds)
else()
  set(GLOD_LIBRARIES GLOD)
endif()
endif (USESYSTEMLIBS)
