# -*- cmake -*-
add_subdirectory(base)

if (LINUX)
    add_subdirectory(gstreamer010)
    add_subdirectory(cef)
    add_subdirectory(libvlc)
endif (LINUX)

if (DARWIN)
    add_subdirectory(quicktime)
    add_subdirectory(cef)
endif (DARWIN)

if (WINDOWS)
    if( WORD_SIZE EQUAL 32 )
       add_subdirectory(quicktime)
    endif()

    add_subdirectory(cef)
    add_subdirectory(winmmshim)
    add_subdirectory(libvlc)
endif (WINDOWS)

### add_subdirectory(example)

if (WINDOWS OR LINUX)
    add_subdirectory(gstreamer10)
endif (WINDOWS OR LINUX)

