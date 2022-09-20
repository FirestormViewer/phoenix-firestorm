# -*- cmake -*-

include(Variables)
include(GLEXT)
include(Prebuilt)

include_guard()
add_library( ll::SDL INTERFACE IMPORTED )


if (LINUX)
  #Must come first as use_system_binary can exit this file early
  #target_compile_definitions( ll::SDL INTERFACE LL_SDL=1)

  #use_system_binary(SDL)
  #use_prebuilt_binary(SDL)
  
  target_include_directories( ll::SDL SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include)

  if( USE_SDL1 )
    target_compile_definitions( ll::SDL INTERFACE LL_SDL=1 )

    use_system_binary(SDL)
    use_prebuilt_binary(SDL)
    set (SDL_FOUND TRUE)

    target_link_libraries (ll::SDL INTERFACE SDL directfb fusion direct X11)

  else()
    target_compile_definitions( ll::SDL INTERFACE LL_SDL2=1 LL_SDL=1 )

    use_system_binary(SDL2)
    use_prebuilt_binary(SDL2)
    set (SDL2_FOUND TRUE)

    target_link_libraries( ll::SDL INTERFACE SDL2 X11 )
  endif()
endif (LINUX)


