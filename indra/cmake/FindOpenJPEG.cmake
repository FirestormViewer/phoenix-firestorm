# -*- cmake -*-

include(FindPkgConfig)

pkg_check_modules( OPENJPEG REQUIRED libopenjpeg1 )

set(OPENJPEG_INCLUDE_DIR ${OPENJPEG_INCLUDE_DIRS})
