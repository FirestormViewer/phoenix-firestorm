# -*- cmake -*-

if (WINDOWS)
  if (DEFINED ENV{PROGRAMFILES\(X86\)})
    set (PROGRAMFILES $ENV{PROGRAMFILES\(X86\)})
  else (DEFINED ENV{PROGRAMFILES\(X86\)})
    set (PROGRAMFILES $ENV{PROGRAMFILES})
  endif (DEFINED ENV{PROGRAMFILES\(X86\)})

  find_path(DIRECTX_INCLUDE_DIR dxdiag.h
            "$ENV{DXSDK_DIR}/Include"
            "${PROGRAMFILES}/Windows Kits/8.1/Include/um"
            "${PROGRAMFILES}/Microsoft DirectX SDK (June 2010)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (August 2009)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (March 2009)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (August 2008)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (June 2008)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (March 2008)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (November 2007)/Include"
            "${PROGRAMFILES}/Microsoft DirectX SDK (August 2007)/Include"
            "C:/DX90SDK/Include"
            "${PROGRAMFILES}/DX90SDK/Include"
            )
  if (DIRECTX_INCLUDE_DIR)
    include_directories(${DIRECTX_INCLUDE_DIR})
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_INCLUDE_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Include")
  endif (DIRECTX_INCLUDE_DIR)

  if(ADDRESS_SIZE EQUAL 32)
    set(DIRECTX_ARCHITECTURE "x86")
  else(ADDRESS_SIZE EQUAL 32)
    set(DIRECTX_ARCHITECTURE "x64")
  endif(ADDRESS_SIZE EQUAL 32)

  find_path(DIRECTX_LIBRARY_DIR dxguid.lib
            "$ENV{DXSDK_DIR}/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Windows Kits/8.1/Lib/winv6.3/um/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (June 2010)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (August 2009)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (March 2009)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (August 2008)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (June 2008)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (March 2008)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (November 2007)/Lib/${DIRECTX_ARCHITECTURE}"
            "${PROGRAMFILES}/Microsoft DirectX SDK (August 2007)/Lib/${DIRECTX_ARCHITECTURE}"
            "C:/DX90SDK/Lib"
            "${PROGRAMFILES}/DX90SDK/Lib"
            )
  if (DIRECTX_LIBRARY_DIR)
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX library: ${DIRECTX_LIBRARY_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_LIBRARY_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Libraries")
  endif (DIRECTX_LIBRARY_DIR)

endif (WINDOWS)
