# -*- cmake -*-
include(FindWindowsSDK)

if (WINDOWS)
  include(FindWindowsSDK)

  get_windowssdk_include_dirs(${WINDOWSSDK_PREFERRED_DIR} WINDOWSSDK_INCLUDE_DIRS)
  find_path(DIRECTX_INCLUDE_DIR
      NAMES dxdiag.h
      PATHS ${WINDOWSSDK_INCLUDE_DIRS})
  if (DIRECTX_INCLUDE_DIR)
    include_directories(${DIRECTX_INCLUDE_DIR})
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_INCLUDE_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Include")
  endif (DIRECTX_INCLUDE_DIR)


  get_windowssdk_library_dirs(${WINDOWSSDK_PREFERRED_DIR} WINDOWSSDK_LIBRARY_DIRS)
  find_path(DIRECTX_LIBRARY_DIR
      NAMES dxguid.lib
      PATHS ${WINDOWSSDK_LIBRARY_DIRS})
  if (DIRECTX_LIBRARY_DIR)
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_LIBRARY_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_LIBRARY_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Libraries")
  endif (DIRECTX_LIBRARY_DIR)

endif (WINDOWS)
