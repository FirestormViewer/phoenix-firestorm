# -*- cmake -*-

include(FindPkgConfig)
pkg_check_modules( HUNSPELL REQUIRED hunspell )
set( HUNSPELL_INCLUDE_DIR ${HUNSPELL_INCLUDE_DIRS} )
set( HUNSPELL_LIBRARY ${HUNSPELL_LIBRARIES} )