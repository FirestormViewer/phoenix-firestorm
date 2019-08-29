
find_library( GLOD_LIBRARY GLOD )
find_library( VDS_LIBRARY vds )
find_path( GLOD_INCLUDE_DIR glod/glod.h )

if( GLOD_INCLUDE_DIR STREQUAL "GLOD_INCLUDE_DIR-NOTFOUND" )
  message( FATAL_ERROR "Cannot find glod include dir" )
endif()
if( GLOD_LIBRARY STREQUAL "GLOD_LIBRARY-NOTFOUND" )
  message( FATAL_ERROR "Cannot find library GLOD.a" )
endif()
if( VDS_LIBRARY STREQUAL "VDS_LIBRARY-NOTFOUND" )
  message( FATAL_ERROR "Cannot find library vds.a" )
endif()
set(GLOD_LIBRARIES ${GLOD_LIBRARY} ${VDS_LIBRARY} )
  
