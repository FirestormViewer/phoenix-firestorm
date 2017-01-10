# -*- cmake -*-
# Construct the version and copyright information based on package data.
include(Python)

# run_build_test.py.
add_custom_command(OUTPUT packages-info.txt
  COMMENT Generating packages-info.txt for the about box
  MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/../autobuild.xml
  DEPENDS ${CMAKE_SOURCE_DIR}/../scripts/packages-formatter.py
  COMMAND ${PYTHON_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/cmake/run_build_test.py -DAUTOBUILD_ADDRSIZE=${ADDRESS_SIZE}
          ${PYTHON_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/../scripts/packages-formatter.py "${VIEWER_CHANNEL}" "${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION}" > packages-info.txt
  )
