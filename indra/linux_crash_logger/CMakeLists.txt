# -*- cmake -*-
project(linux_crash_logger)

# <FS:Beq> Reuse the viewer's bundled SDL3; no additional GUI toolkit.
include(00-Common)
include(Boost)
include(OpenSSL)
include(NGHTTP2)
include(ZLIBNG)
include(CURL)
include(SDL3)

add_library(linux-crash-report STATIC linux_crash_report.cpp)
target_include_directories(linux-crash-report PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linux-crash-report PUBLIC ll::libcurl ll::boost)
add_executable(linux-crash-logger linux_crash_logger.cpp)
target_link_libraries(linux-crash-logger PRIVATE linux-crash-report ll::SDL3)
add_custom_target(linux-crash-logger-target ALL DEPENDS linux-crash-logger)

if(LL_TESTS)
  add_executable(linux-crash-report-test-driver tests/report_driver.cpp)
  target_link_libraries(linux-crash-report-test-driver PRIVATE linux-crash-report)
  add_dependencies(BUILD_TESTS linux-crash-report-test-driver linux-crash-logger)
  add_test(NAME linux-crash-report
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_report.py
            $<TARGET_FILE:linux-crash-report-test-driver> $<TARGET_FILE:linux-crash-logger>)
  set_tests_properties(linux-crash-report PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${LIBS_PREBUILT_DIR}/lib/release:$ENV{LD_LIBRARY_PATH}")
endif()
# </FS:Beq>
