# -*- cmake -*-

include(FindPkgConfig)
pkg_check_modules( JSONCPP REQUIRED jsoncpp )
set( JSONCPP_INCLUDE_DIR ${JSONCPP_INCLUDE_DIRS} )

