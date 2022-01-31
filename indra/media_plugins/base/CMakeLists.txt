# -*- cmake -*-

project(media_plugin_base)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLPlugin)
include(LLMath)
include(LLRender)
include(LLWindow)
include(Linking)
include(PluginAPI)
include(OpenGL)

include_directories(
    ${LLPLUGIN_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
)
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    )


### media_plugin_base

if(NOT ADDRESS_SIZE EQUAL 32)
  if(WINDOWS)
    ##add_definitions(/FIXED:NO)
  else(WINDOWS) # not windows therefore gcc LINUX and DARWIN
    add_definitions(-fPIC)
  endif(WINDOWS)
endif(NOT ADDRESS_SIZE EQUAL 32)

set(media_plugin_base_SOURCE_FILES
    media_plugin_base.cpp
)

set(media_plugin_base_HEADER_FILES
    CMakeLists.txt

    media_plugin_base.h
)

add_library(media_plugin_base
    ${media_plugin_base_SOURCE_FILES}
    )


# <FS:ND> Try to find pulse header, if we got them we can use the linux volume catcher
if (LINUX)
  include(GLIB)

  include_directories( ${GLIB_INCLUDE_DIRS} )

  set( PULSE_AUDIO True )
  foreach( PULSE_FILE pulse/introspect.h pulse/context.h pulse/subscribe.h pulse/glib-mainloop.h )
	find_path( PULSE_FILE_FOUND ${PULSE_FILE} NO_CACHE)
	if( NOT PULSE_FILE_FOUND )
	  message( "Looking for ${PULSE_FILE} ... not found")
	  set( PULSE_AUDIO False )
	else()
	  message( "Looking for ${PULSE_FILE} ... found")
    endif()
  endforeach()

  if( NOT PULSE_AUDIO )
	message( "Building without linux volume catcher" )
    set(LINUX_VOLUME_CATCHER dummy_volume_catcher.cpp)
  else()
	message( "Building with linux volume catcher" )
    set(LINUX_VOLUME_CATCHER linux_volume_catcher.cpp)
  endif()

endif()
