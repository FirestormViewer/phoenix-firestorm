# -*- cmake -*-

project(media_plugin_libvlc)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLWindow)
include(Linking)
include(PluginAPI)
include(MediaPluginBase)   # <FS:Zi> Linux volume catcher

include(LibVLCPlugin)

### media_plugin_libvlc


set(media_plugin_libvlc_SOURCE_FILES
    media_plugin_libvlc.cpp
    )

add_library(media_plugin_libvlc
    SHARED
    ${media_plugin_libvlc_SOURCE_FILES}
    )

target_link_libraries(media_plugin_libvlc
        media_plugin_base
        ll::libvlc
)

if (WINDOWS AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  use_prebuilt_binary(soloud)
  set(VLC_MINIAUDIO_INPUT "${AUTOBUILD_INSTALL_DIR}/include/soloud/miniaudio.h")
  set(VLC_MINIAUDIO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vlc_miniaudio/llvlc_miniaudio.h")
  include("${CMAKE_CURRENT_SOURCE_DIR}/prepare_miniaudio.cmake")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${VLC_MINIAUDIO_INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_miniaudio.cmake")
  target_sources(media_plugin_libvlc PRIVATE llvlcaudio.cpp llvlcaudiobridge.cpp)
  target_include_directories(media_plugin_libvlc PRIVATE "${AUTOBUILD_INSTALL_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/vlc_miniaudio")
  target_compile_features(media_plugin_libvlc PRIVATE cxx_std_17)
  target_compile_definitions(media_plugin_libvlc PRIVATE LL_VLC_PCM_AUDIO=1)

  option(LL_VLC_AUDIO_BRIDGE_TESTS "Build device-free VLC PCM adapter tests" OFF)
  if (LL_VLC_AUDIO_BRIDGE_TESTS)
    add_executable(vlc_audio_plugin_bridge_test
      tests/llvlcaudiobridge_test.cpp llvlcaudiobridge.cpp llvlcaudio.cpp)
    target_include_directories(vlc_audio_plugin_bridge_test PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}" "${AUTOBUILD_INSTALL_DIR}/include"
      "${CMAKE_CURRENT_BINARY_DIR}/vlc_miniaudio")
    target_compile_features(vlc_audio_plugin_bridge_test PRIVATE cxx_std_17)
    target_link_libraries(vlc_audio_plugin_bridge_test PRIVATE ll::libvlc)
    add_test(NAME vlc_audio_plugin_bridge COMMAND vlc_audio_plugin_bridge_test)
    set_tests_properties(vlc_audio_plugin_bridge PROPERTIES TIMEOUT 60)
  endif ()
else ()
  target_compile_definitions(media_plugin_libvlc PRIVATE LL_VLC_PCM_AUDIO=0)
endif ()

if (WINDOWS)
  set_target_properties(
    media_plugin_libvlc
    PROPERTIES
    LINK_FLAGS "/MANIFEST:NO /NODEFAULTLIB:LIBCMT"
    )
endif (WINDOWS)

if (DARWIN)
  # Don't prepend 'lib' to the executable name, and don't embed a full path in the library's install name
  set_target_properties(
    media_plugin_libvlc
    PROPERTIES
    PREFIX ""
    BUILD_WITH_INSTALL_RPATH 1
    INSTALL_RPATH "@executable_path/../Frameworks"
    LINK_FLAGS "-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/../base/media_plugin_base.exp"
  )

endif (DARWIN)

if( LINUX )
  set(CMAKE_SHARED_LINKER_FLAGS  "-Wl,--build-id -Wl,-rpath,'$ORIGIN:$ORIGIN/../../lib'")
endif()
