# -*- cmake -*-

# <FS:ND> Try to find pulse header, if we got them we can use the linux volume catcher
if (LINUX)

  foreach( PULSE_FILE pulse/introspect.h pulse/context.h pulse/subscribe.h )
	find_path( PULSE_FILE_${PULSE_FILE}_FOUND ${PULSE_FILE} NO_CACHE)
	if( NOT PULSE_FILE_${PULSE_FILE}_FOUND )
	  message( "Looking for ${PULSE_FILE} ... not found")
	  message( FATAL_ERROR "Pulse header not found" )
	else()
	  message( "Looking for ${PULSE_FILE} ... found")
    endif()
  endforeach()

  message( "Building with linux volume catcher" )
  set(LINUX_VOLUME_CATCHER linux_volume_catcher.cpp)

  set(MEDIA_PLUGIN_BASE_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/media_plugins/base/
    )

  set(MEDIA_PLUGIN_BASE_LIBRARIES media_plugin_base)

endif()    # <FS:Zi> make sure no changes happen to other platforms by moving endif() here
