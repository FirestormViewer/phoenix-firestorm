# -*- cmake -*-

include(FindPkgConfig)
pkg_check_modules( URIPARSER REQUIRED liburiparser )
set( URIPARSER_INCLUDE_DIR ${URIPARSER_INCLUDE_DIRS} )