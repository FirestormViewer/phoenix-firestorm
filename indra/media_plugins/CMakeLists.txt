# -*- cmake -*-

add_subdirectory(base)

if (LINUX)
    include(GLIB)
    #add_subdirectory(gstreamer010)
    #add_subdirectory(example)
    if( GLIB_FOUND )
      add_subdirectory(gstreamer10)
	endif()
    add_subdirectory(libvlc)
    add_subdirectory(cef)
endif (LINUX)

if (DARWIN)
    add_subdirectory(cef)
    add_subdirectory(libvlc)
    #add_subdirectory(example)
endif (DARWIN)

if (WINDOWS)
    add_subdirectory(cef)
    add_subdirectory(libvlc)
    #add_subdirectory(example)
endif (WINDOWS)
