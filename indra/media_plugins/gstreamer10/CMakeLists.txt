# -*- cmake -*-

project(media_plugin_gstreamer10)

include(00-Common)
include(LLCommon)
include(LLImage)

include(LLMath)

include(LLWindow)
include(Linking)
include(PluginAPI)

include(OpenGL)
include(GLIB)

include(GStreamer10Plugin)


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
        media_plugin_base
        ll::gstreamer
)


