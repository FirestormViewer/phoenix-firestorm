# -*- cmake -*-
include(Prebuilt)

if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  set(NVAPI OFF CACHE BOOL "Use NVAPI." FORCE)
else()
  set(NVAPI ON CACHE BOOL "Use NVAPI.")
endif()

if (NVAPI)
  if (WINDOWS)
    add_library( ll::nvapi INTERFACE IMPORTED )
    target_link_libraries( ll::nvapi INTERFACE nvapi)
    use_prebuilt_binary(nvapi)
  endif (WINDOWS)
endif (NVAPI)

