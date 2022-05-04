# -*- cmake -*-

include(Variables)
include(GLEXT)
include(Prebuilt)

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
    if( NOT USE_SDL2 )
      use_prebuilt_binary(SDL)
      set (SDL_FOUND TRUE)
      set (SDL_LIBRARY SDL directfb fusion direct X11)
    else()
      use_prebuilt_binary(SDL2)
      set (SDL2_FOUND TRUE)
      set (SDL_LIBRARY SDL2 SDL2_mixer X11)
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
