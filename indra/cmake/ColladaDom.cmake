# -*- cmake -*-
include (Prebuilt)

if (STANDALONE)
  find_path(COLLADA_INCLUDE_DIRS 1.4/dom/domConstants.h
    PATH_SUFFIXES collada)
  set(COLLADA_INCLUDE_DIRS
    ${COLLADA_INCLUDE_DIRS} ${COLLADA_INCLUDE_DIRS}/1.4
    )
else (STANDALONE)
  set(COLLADA_INCLUDE_DIRS
    ${LIBS_PREBUILT_DIR}/include/collada
    ${LIBS_PREBUILT_DIR}/include/collada/1.4
    )
endif (STANDALONE)
