# -*- cmake -*-
add_subdirectory(base)

if (LINUX)
    add_subdirectory(gstreamer10)
    add_subdirectory(cef)
    #add_subdirectory(example)
endif (LINUX)

if (DARWIN)
    add_subdirectory(cef)
    add_subdirectory(libvlc)
    #add_subdirectory(example)
endif (DARWIN)

if (WINDOWS)
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        add_subdirectory(cef)
        add_subdirectory(libvlc)
    endif()
    #add_subdirectory(example)
endif (WINDOWS)
