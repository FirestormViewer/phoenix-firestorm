# -*- cmake -*-

project(llfilesystem)

include(00-Common)
include(LLCommon)

set(llfilesystem_SOURCE_FILES
    lldir.cpp
    lldiriterator.cpp
    lllfsthread.cpp
    lldiskcache.cpp
    llfilesystem.cpp
    )

set(llfilesystem_HEADER_FILES
    CMakeLists.txt
    lldir.h
    lldirguard.h
    lldiriterator.h
    lllfsthread.h
    lldiskcache.h
    llfilesystem.h
    )

# Per-platform source/header lists, kept separate so that files belonging to
# platforms other than the one currently being built can still be listed in
# the project (for IDE visibility) while being excluded from compilation.
set(macosx_SOURCE_FILES
    lldir_utils_objc.mm
    lldir_mac.cpp
    )
set(macosx_HEADER_FILES
    lldir_utils_objc.h
    lldir_mac.h
    )

set(linux_SOURCE_FILES
    lldir_linux.cpp
    )
set(linux_HEADER_FILES
    lldir_linux.h
    )

set(windows_SOURCE_FILES
    lldir_win32.cpp
    )
set(windows_HEADER_FILES
    lldir_win32.h
    )

if (DARWIN)
  LIST(APPEND llfilesystem_SOURCE_FILES ${macosx_SOURCE_FILES})
  LIST(APPEND llfilesystem_HEADER_FILES ${macosx_HEADER_FILES})

  set_source_files_properties(
    lldir_utils_objc.mm
    PROPERTIES
    SKIP_PRECOMPILE_HEADERS TRUE
  )
endif (DARWIN)

if (LINUX)
  LIST(APPEND llfilesystem_SOURCE_FILES ${linux_SOURCE_FILES})
  LIST(APPEND llfilesystem_HEADER_FILES ${linux_HEADER_FILES})

  if (INSTALL)
    set_source_files_properties(lldir_linux.cpp
                                PROPERTIES COMPILE_FLAGS
                                "-DAPP_RO_DATA_DIR=\\\"${APP_SHARE_DIR}\\\""
                                )
  endif (INSTALL)
endif (LINUX)

if (WINDOWS)
  LIST(APPEND llfilesystem_SOURCE_FILES ${windows_SOURCE_FILES})
  LIST(APPEND llfilesystem_HEADER_FILES ${windows_HEADER_FILES})
endif (WINDOWS)

list(APPEND llfilesystem_SOURCE_FILES ${llfilesystem_HEADER_FILES})

# Collect all platform files for IDE visibility, excluding the files already
# added above for the platform actually being built.
set(ide_visibility_files "")
if (NOT DARWIN)
  list(APPEND ide_visibility_files ${macosx_SOURCE_FILES} ${macosx_HEADER_FILES})
endif()
if (NOT LINUX)
  list(APPEND ide_visibility_files ${linux_SOURCE_FILES} ${linux_HEADER_FILES})
endif()
if (NOT WINDOWS)
  list(APPEND ide_visibility_files ${windows_SOURCE_FILES} ${windows_HEADER_FILES})
endif()

add_library (llfilesystem
    ${llfilesystem_SOURCE_FILES}
    # Add non-platform files for IDE visibility only
    ${ide_visibility_files}
    )

# Mark non-platform files as excluded from build
if (WINDOWS)
  set_source_files_properties(
    ${macosx_SOURCE_FILES}
    ${macosx_HEADER_FILES}
    ${linux_SOURCE_FILES}
    ${linux_HEADER_FILES}
    PROPERTIES
    HEADER_FILE_ONLY TRUE
    )
elseif (DARWIN)
  set_source_files_properties(
    ${windows_SOURCE_FILES}
    ${windows_HEADER_FILES}
    ${linux_SOURCE_FILES}
    ${linux_HEADER_FILES}
    PROPERTIES
    HEADER_FILE_ONLY TRUE
    )
elseif (LINUX)
  set_source_files_properties(
    ${macosx_SOURCE_FILES}
    ${macosx_HEADER_FILES}
    ${windows_SOURCE_FILES}
    ${windows_HEADER_FILES}
    PROPERTIES
    HEADER_FILE_ONLY TRUE
    )
endif()

target_link_libraries(llfilesystem
        llmath
        llcommon
    )
target_include_directories( llfilesystem  INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR})

# Portable-install detection (".portable" marker) only needs the
# LL_VELOPACK compile definition, not the actual Velopack library/headers.
if (TARGET ll::velopack)
    target_compile_definitions(llfilesystem PRIVATE LL_VELOPACK=1)
endif ()


if (USE_PRECOMPILED_HEADERS)
    target_precompile_headers(llfilesystem REUSE_FROM llprecompiled)
endif ()

# Add tests
if (LL_TESTS)
    include(LLAddBuildTest)
    # UNIT TESTS
    SET(llfilesystem_TEST_SOURCE_FILES
    lldiriterator.cpp
    )

    LL_ADD_PROJECT_UNIT_TESTS(llfilesystem "${llfilesystem_TEST_SOURCE_FILES}")

    # INTEGRATION TESTS
    set(test_libs llmath llcommon llfilesystem )

    # TODO: Some of these need refactoring to be proper Unit tests rather than Integration tests.
    LL_ADD_INTEGRATION_TEST(lldir "" "${test_libs}")
    # <FS:Beq> FIRE-37081: Exercise startup cache operations with Unicode profile paths.
    LL_ADD_INTEGRATION_TEST(lldiskcache "" "${test_libs}")
    set_tests_properties(INTEGRATION_TEST_RUNNER_lldiskcache PROPERTIES TIMEOUT 30)
    # </FS:Beq>
    # <FS:Beq> Extended non-ASCII path test coverage; strict assertions expose existing defects.
    LL_ADD_INTEGRATION_TEST(llnonasciipaths "" "${test_libs}")
    LL_ADD_INTEGRATION_TEST(llstaticassetpaths "" "${test_libs}")
    # A separate process exercises constructor watermarks before any settings callback.
    LL_ADD_INTEGRATION_TEST(llcachewatermarks "" "${test_libs}")
    set_tests_properties(INTEGRATION_TEST_RUNNER_llcachewatermarks PROPERTIES TIMEOUT 30 LABELS "nonascii")
    # A second process permits another singleton lifecycle and distinguishes
    # Unicode-specific failures from general protected-cache accounting bugs.
    SET_TEST_PATH(STATIC_ASSET_TEST_PATH)
    LL_TEST_COMMAND(STATIC_ASSET_ASCII_COMMAND "${STATIC_ASSET_TEST_PATH}"
      "${CMAKE_COMMAND}" -E env "LL_TEST_ASCII_PROFILE=1" "$<TARGET_FILE:INTEGRATION_TEST_llstaticassetpaths>")
    add_test(NAME INTEGRATION_TEST_RUNNER_llstaticassetpaths_ascii COMMAND ${STATIC_ASSET_ASCII_COMMAND})
    # Detail logging must not change filesystem outcomes or summary counters.
    LL_TEST_COMMAND(STATIC_ASSET_DEBUG_COMMAND "${STATIC_ASSET_TEST_PATH}"
      "${CMAKE_COMMAND}" -E env "LL_TEST_CACHE_DEBUG=1" "$<TARGET_FILE:INTEGRATION_TEST_llstaticassetpaths>")
    LL_TEST_COMMAND(STATIC_ASSET_ASCII_DEBUG_COMMAND "${STATIC_ASSET_TEST_PATH}"
      "${CMAKE_COMMAND}" -E env "LL_TEST_ASCII_PROFILE=1" "LL_TEST_CACHE_DEBUG=1" "$<TARGET_FILE:INTEGRATION_TEST_llstaticassetpaths>")
    add_test(NAME INTEGRATION_TEST_RUNNER_llstaticassetpaths_debug COMMAND ${STATIC_ASSET_DEBUG_COMMAND})
    add_test(NAME INTEGRATION_TEST_RUNNER_llstaticassetpaths_ascii_debug COMMAND ${STATIC_ASSET_ASCII_DEBUG_COMMAND})
    set_tests_properties(INTEGRATION_TEST_RUNNER_llnonasciipaths
      INTEGRATION_TEST_RUNNER_llstaticassetpaths INTEGRATION_TEST_RUNNER_llstaticassetpaths_ascii
      INTEGRATION_TEST_RUNNER_llstaticassetpaths_debug INTEGRATION_TEST_RUNNER_llstaticassetpaths_ascii_debug
      PROPERTIES TIMEOUT 30 LABELS "nonascii")
    # </FS:Beq>
endif (LL_TESTS)
