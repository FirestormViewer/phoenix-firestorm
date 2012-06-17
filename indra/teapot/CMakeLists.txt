# -*- cmake -*-

project(teapot)
include(00-Common)
include(LLCommon)
include(LLVFS)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    )

set(teapot_SOURCE_FILES
    tea.cpp
    )

set(teapot_HEADER_FILES
    CMakeLists.txt
    tea.h
    )

set_source_files_properties(${teapot_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND teapot_SOURCE_FILES ${teapot_HEADER_FILES})

add_library (teapot ${teapot_SOURCE_FILES})
target_link_libraries(
    teapot
    )
