# -*- cmake -*-

# these should be moved to their own cmake file
include(Prebuilt)
include(Boost)

set(LLPRIMITIVE_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/llprimitive
    )

if( NOT USESYSTEMLIBS )
  use_prebuilt_binary(colladadom)
  use_prebuilt_binary(pcre)
  use_prebuilt_binary(libxml2)

  set( COLLADADOM_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/collada ${LIBS_PREBUILT_DIR}/include/collada/1.4 )
  
  if (WINDOWS)
      set(LLPRIMITIVE_LIBRARIES 
          debug llprimitive
          optimized llprimitive
          debug libcollada14dom23-sd
          optimized libcollada14dom23-s
          libxml2_a
          debug pcrecppd
          optimized pcrecpp
          debug pcred
          optimized pcre
          ${BOOST_SYSTEM_LIBRARIES}
          )
  elseif (DARWIN)
      set(LLPRIMITIVE_LIBRARIES 
          llprimitive
          debug collada14dom-d
          optimized collada14dom
          minizip
          xml2
          pcrecpp
          pcre
          iconv           # Required by libxml2
          )
  elseif (LINUX)
      set(LLPRIMITIVE_LIBRARIES 
          llprimitive
          debug collada14dom-d
          optimized collada14dom
          minizip
          xml2
          pcrecpp
          pcre
          )
  endif (WINDOWS)

else()

  include(FindPkgConfig)
  pkg_check_modules( MINIZIP REQUIRED minizip )
  pkg_check_modules( LIBXML2 REQUIRED libxml-2.0 )
  pkg_check_modules( LIBPCRECPP REQUIRED libpcrecpp )

  find_library( COLLADADOM_LIBRARY collada14dom )
  find_path( COLLADADOM_INCLUDE_DIR colladadom/dae.h )

  if( COLLADADOM_INCLUDE_DIR STREQUAL "COLLADADOM_INCLUDE_DIR-NOTFOUND" )
    message( FATAL_ERROR "Cannot find colladadom include dir" )
  endif()
  
  set( COLLADADOM_INCLUDE_DIRS ${COLLADADOM_INCLUDE_DIR}/colladadom ${COLLADADOM_INCLUDE_DIR}/colladadom/1.4 )

  set(LLPRIMITIVE_LIBRARIES 
      llprimitive
      ${COLLADADOM_LIBRARY}
      ${MINIZIP_LIBRARIES}
      ${LIBXML2_LIBRARIES}
      ${LIBPRCECPP_LIBRARIES}
     )

endif()
