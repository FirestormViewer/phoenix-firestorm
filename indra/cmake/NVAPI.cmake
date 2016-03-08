# -*- cmake -*-
include(Prebuilt)

set(NVAPI ON CACHE BOOL "Use NVAPI.")

if (NVAPI)
  if (WINDOWS)
    use_prebuilt_binary(nvapi)
    if( NOT ND_BUILD64BIT_ARCH )
      set(NVAPI_LIBRARY nvapi)
    else( NOT ND_BUILD64BIT_ARCH )
      set(NVAPI_LIBRARY nvapi64 )
    endif( NOT ND_BUILD64BIT_ARCH )
  else (WINDOWS)
    set(NVAPI_LIBRARY "")
  endif (WINDOWS)
else (NVAPI)
  set(NVAPI_LIBRARY "")
endif (NVAPI)

