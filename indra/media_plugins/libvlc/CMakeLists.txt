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

target_compile_definitions(media_plugin_libvlc PRIVATE LL_VLC_PCM_AUDIO=0)

if (WINDOWS AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  find_program(VLC_MODULE_CC NAMES x86_64-w64-mingw32-gcc gcc REQUIRED)
  set(SPEAKERFILL_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/speakerfill-source")
  include("${CMAKE_CURRENT_SOURCE_DIR}/prepare_speakerfill.cmake")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/prepare_speakerfill.cmake")
  set(SPEAKERFILL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/libspeakerfill_output_plugin.dll")
  add_custom_command(OUTPUT "${SPEAKERFILL_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
    COMMAND "${VLC_MODULE_CC}" -shared -std=gnu11 -O2 -g -DHAVE_ATTRIBUTE_PACKED -D__PLUGIN__ -DMODULE_STRING="speakerfill_output"
      -I "${AUTOBUILD_INSTALL_DIR}/include/vlc/plugins" -I "${SPEAKERFILL_SOURCE_DIR}"
      "${SPEAKERFILL_SOURCE_DIR}/speakerfill_output.c" "${VLCCORE_LIBRARY}"
      -lole32 -luuid -lksuser -lwinmm -lws2_32 -o "${SPEAKERFILL_OUTPUT}"
    DEPENDS "${SPEAKERFILL_SOURCE_DIR}/speakerfill_output.c" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_speakerfill.cmake"
    VERBATIM)
  add_custom_target(vlc_speakerfill_output DEPENDS "${SPEAKERFILL_OUTPUT}")
  add_library(vlc_speakerfill MODULE vlc_speakerfill.cpp)
  target_include_directories(vlc_speakerfill SYSTEM PRIVATE "${AUTOBUILD_INSTALL_DIR}/include/vlc/plugins")
  target_compile_definitions(vlc_speakerfill PRIVATE __PLUGIN__ MODULE_STRING="speakerfill")
  target_link_libraries(vlc_speakerfill PRIVATE ll::libvlc)
  set_target_properties(vlc_speakerfill PROPERTIES PREFIX "lib" OUTPUT_NAME "speakerfill_plugin")
  add_executable(vlc_speakerfill_test tests/vlc_speakerfill_test.cpp)
  target_sources(vlc_speakerfill_test PRIVATE llvlcspeakerfillconfig.cpp)
  target_sources(media_plugin_libvlc PRIVATE llvlcspeakerfillconfig.cpp)
  target_include_directories(media_plugin_libvlc SYSTEM PRIVATE "${AUTOBUILD_INSTALL_DIR}/include/vlc/plugins")
  target_include_directories(vlc_speakerfill_test SYSTEM PRIVATE "${AUTOBUILD_INSTALL_DIR}/include/vlc/plugins")
  target_compile_options(vlc_speakerfill_test PRIVATE /wd4191)
  target_link_libraries(vlc_speakerfill_test PRIVATE ll::libvlc)
  target_include_directories(vlc_speakerfill_test SYSTEM PRIVATE "${AUTOBUILD_INSTALL_DIR}/include")
  target_include_directories(vlc_speakerfill_test PRIVATE "${SPEAKERFILL_SOURCE_DIR}")
  target_link_libraries(vlc_speakerfill_test PRIVATE ole32)
  add_dependencies(vlc_speakerfill_test vlc_speakerfill vlc_speakerfill_output)
  add_dependencies(media_plugin_libvlc vlc_speakerfill vlc_speakerfill_output)
  add_test(NAME vlc_speakerfill COMMAND ${CMAKE_COMMAND} -E env
    "PATH=${AUTOBUILD_INSTALL_DIR}/bin/release;$ENV{PATH}"
    "VLC_PLUGIN_PATH=$<TARGET_FILE_DIR:vlc_speakerfill>"
    $<TARGET_FILE:vlc_speakerfill_test> $<TARGET_FILE:vlc_speakerfill> "${SPEAKERFILL_OUTPUT}")
endif ()

option(LL_VLC_AUDIO_BRIDGE_TESTS "Build device-free VLC PCM adapter tests" OFF)
if (WINDOWS AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND LL_VLC_AUDIO_BRIDGE_TESTS)
  use_prebuilt_binary(soloud)
  set(VLC_MINIAUDIO_INPUT "${AUTOBUILD_INSTALL_DIR}/include/soloud/miniaudio.h")
  set(VLC_MINIAUDIO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vlc_miniaudio/llvlc_miniaudio.h")
  include("${CMAKE_CURRENT_SOURCE_DIR}/prepare_miniaudio.cmake")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${VLC_MINIAUDIO_INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_miniaudio.cmake")
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
