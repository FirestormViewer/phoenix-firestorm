# -*- cmake -*-
include(Prebuilt)

set(HACD_FIND_QUIETLY ON)
set(HACD_FIND_REQUIRED ON)

message("test AAAAAAAAAAAAAAAAAAAAAAAAAAAAA")

if (STANDALONE)
  include(FindHACD)
else (STANDALONE)
  use_prebuilt_binary(hacd)
  if (WINDOWS)
    set(HACD_LIBRARY hacd)
    set(HACD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hacd)
  elseif(DARWIN)
    set(HACD_LIBRARY HACD_LIB)
    set(HACD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hacd)
  else()
    set(HACD_LIBRARY HACD_LIB)
    set(HACD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hacd)
  endif()
endif (STANDALONE)
