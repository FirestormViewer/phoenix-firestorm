# -*- cmake -*-

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


set(MEDIA_PLUGIN_BASE_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/media_plugins/base/
    )

set(MEDIA_PLUGIN_BASE_LIBRARIES media_plugin_base)
