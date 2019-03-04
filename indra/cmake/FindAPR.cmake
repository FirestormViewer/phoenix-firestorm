# -*- cmake -*-

include(FindPkgConfig)
pkg_check_modules( APR REQUIRED apr-1 )
set( APR_INCLUDE_DIR ${APR_INCLUDE_DIRS} )

pkg_check_modules( APRUTIL REQUIRED apr-util-1 )
set( APRUTIL_INCLUDE_DIR ${APRUTIL_INCLUDE_DIRS} )
