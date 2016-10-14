# -*- cmake -*-

project(media_plugin_gstreamer10)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLPlugin)
include(LLMath)
include(LLRender)
include(LLWindow)
include(Linking)
include(PluginAPI)
include(MediaPluginBase)
include(OpenGL)

include(GStreamer10Plugin)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${MEDIA_PLUGIN_BASE_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    ${GSTREAMER10_INCLUDE_DIRS}
    ${GSTREAMER10_PLUGINS_BASE_INCLUDE_DIRS}
)
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    )

### media_plugin_gstreamer10

if(NOT WORD_SIZE EQUAL 32)
  if(NOT WINDOWS) # not windows therefore gcc LINUX and DARWIN
    add_definitions(-fPIC)
  endif()
endif(NOT WORD_SIZE EQUAL 32)

set(media_plugin_gstreamer10_SOURCE_FILES
    media_plugin_gstreamer10.cpp
    llmediaimplgstreamer_syms.cpp
    )

set(media_plugin_gstreamer10_HEADER_FILES
    llmediaimplgstreamer_syms.h
    llmediaimplgstreamertriviallogging.h
    )

add_library(media_plugin_gstreamer10
    SHARED
    ${media_plugin_gstreamer10_SOURCE_FILES}
)

target_link_libraries(media_plugin_gstreamer10
  ${LLPLUGIN_LIBRARIES}
  ${MEDIA_PLUGIN_BASE_LIBRARIES}
  ${LLCOMMON_LIBRARIES}
  ${PLUGIN_API_WINDOWS_LIBRARIES}
  ${GSTREAMER10_LIBRARIES}
)

#add_dependencies(media_plugin_gstreamer10
#  ${LLPLUGIN_LIBRARIES}
#  ${MEDIA_PLUGIN_BASE_LIBRARIES}
#  ${LLCOMMON_LIBRARIES}
#)

if (WINDOWS)
  set_target_properties(
    media_plugin_gstreamer10
    PROPERTIES
    LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO /NODEFAULTLIB:LIBCMT"
    )
endif (WINDOWS)
