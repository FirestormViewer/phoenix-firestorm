# -*- cmake -*-

include(Variables)
include(GLEXT)
include(Prebuilt)

include_guard()

add_library( sdl INTERFACE IMPORTED )
if (USESYSTEMLIBS)
  include(FindSDL)

  # This should be done by FindSDL.  Sigh.
  mark_as_advanced(
      SDLMAIN_LIBRARY
      SDL_INCLUDE_DIR
      SDL_LIBRARY
      )
else (USESYSTEMLIBS)
  if (LINUX)
    if( USE_SDL1 )
      use_prebuilt_binary(SDL)
      set (SDL_FOUND TRUE)
	  
      target_link_libraries (sdl INTERFACE SDL directfb fusion direct X11)
	  target_compile_definitions( sdl INTERFACE LL_SDL=1 )

    else()
      use_prebuilt_binary(SDL2)
      set (SDL2_FOUND TRUE)

	  target_link_libraries( sdl INTERFACE SDL2 X11 )
	  target_compile_definitions( sdl INTERFACE LL_SDL2=1 LL_SDL=1 )

    endif()

  endif (LINUX)
endif (USESYSTEMLIBS)

set(LLWINDOW_INCLUDE_DIRS
    ${GLEXT_INCLUDE_DIR}
    ${LIBS_OPEN_DIR}/llwindow
    )

if (BUILD_HEADLESS)
  set(LLWINDOW_HEADLESS_LIBRARIES
      llwindowheadless
      )
endif (BUILD_HEADLESS)

  set(LLWINDOW_LIBRARIES
      llwindow
      )
