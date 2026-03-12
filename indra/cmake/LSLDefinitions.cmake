# -*- cmake -*-
include(Prebuilt)

use_prebuilt_binary(lsl_definitions)

configure_file("${AUTOBUILD_INSTALL_DIR}/lsl_definitions/lsl_keywords_pretty.xml"
               "${CMAKE_SOURCE_DIR}/newview/app_settings/keywords_lsl_default.xml" COPYONLY)
configure_file("${AUTOBUILD_INSTALL_DIR}/lsl_definitions/slua_keywords_pretty.xml"
               "${CMAKE_SOURCE_DIR}/newview/app_settings/keywords_lua_default.xml" COPYONLY)
