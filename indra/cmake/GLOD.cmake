# -*- cmake -*-

if (STANDALONE)
  set(GLOD_FIND_REQUIRED true)
  include(FindGLOD)
else (STANDALONE)
  include(Prebuilt)
  use_prebuilt_binary(GLOD)
set(GLOD_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
set(GLOD_LIBRARIES GLOD)
endif (STANDALONE)
