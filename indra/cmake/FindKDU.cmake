find_path( KDU_INCLUDE_DIR kdu/kdu_image.h )
find_library( KDU_LIBRARY kdu_x64 )

if ( NOT KDU_LIBRARY OR NOT KDU_INCLUDE_DIR )
  message(FATAL_ERROR "Cannot find KDU: Library '${KDU_LIBRARY}'; header '${KDU_INCLUDE_DIR}' ")
endif()

set( KDU_INCLUDE_DIR ${KDU_INCLUDE_DIR}/kdu )
set( LLKDU_LIBRARIES llkdu )
