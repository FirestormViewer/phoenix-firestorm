# -*- cmake -*-
include (Prebuilt)

if (USESYSTEMLIBS)
  include(FindPkgConfig)
  pkg_check_modules( MINIZIP REQUIRED minizip )
  pkg_check_modules( LIBXML2 REQUIRED libxml-2.0 )
  pkg_check_modules( LIBPCRECPP REQUIRED libpcrecpp )

  find_library( COLLADADOM_LIBRARY collada14dom )
  find_path( COLLADADOM_INCLUDE_DIR colladadom/dae.h )

  if( COLLADADOM_INCLUDE_DIR STREQUAL "COLLADADOM_INCLUDE_DIR-NOTFOUND" )
    message( FATAL_ERROR "Cannot find colladadom include dir" )
  endif()
  
  set( COLLADA_INCLUDE_DIRS ${COLLADADOM_INCLUDE_DIR}/colladadom ${COLLADADOM_INCLUDE_DIR}/colladadom/1.4 )

else (USESYSTEMLIBS)
  set(COLLADA_INCLUDE_DIRS
    ${LIBS_PREBUILT_DIR}/include/collada
    ${LIBS_PREBUILT_DIR}/include/collada/1.4
    )
endif (USESYSTEMLIBS)
