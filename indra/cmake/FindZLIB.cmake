# -*- cmake -*-

include(FindPkgConfig)
pkg_check_modules( ZLIB REQUIRED zlib )
set( ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIRS} )
