# -*- cmake -*-

include(Python)

macro (build_version _target)
  execute_process(
      COMMAND ${PYTHON_EXECUTABLE} ${SCRIPTS_DIR}/build_version.py
        llversion${_target}.h ${LLCOMMON_INCLUDE_DIRS}
      OUTPUT_VARIABLE ${_target}_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )

  if (${_target}_VERSION)
    message(STATUS "Version of ${_target} is ${${_target}_VERSION}")
  else (${_target}_VERSION)
    message(SEND_ERROR "Could not determine ${_target} version")
  endif (${_target}_VERSION)
endmacro (build_version)


macro (build_channel _target)
  execute_process(
      COMMAND ${PYTHON_EXECUTABLE} ${SCRIPTS_DIR}/build_channel.py
        llversion${_target}.h ${LLCOMMON_INCLUDE_DIRS}
      OUTPUT_VARIABLE VIEWER_CHANNEL
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      
  if (VIEWER_CHANNEL)
    message(STATUS "Channel is ${VIEWER_CHANNEL}")
  else (VIEWER_CHANNEL)
    message(SEND_ERROR "Could not determine channel")
  endif (VIEWER_CHANNEL)
endmacro (build_channel)
