# -*- cmake -*-
include(Prebuilt)

if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  set(NDOF OFF CACHE BOOL "Use NDOF space navigator joystick library." FORCE)
else()
  set(NDOF ON CACHE BOOL "Use NDOF space navigator joystick library.")
endif()

include_guard()
add_library( ll::ndof INTERFACE IMPORTED )

if (NDOF)
  if (WINDOWS OR DARWIN)
    use_prebuilt_binary(libndofdev)
  elseif (LINUX)
    use_prebuilt_binary(open-libndofdev)
  endif (WINDOWS OR DARWIN)

  find_library(NDOF_LIBRARY
      NAMES
      libndofdev
      ndofdev
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  target_link_libraries(ll::ndof INTERFACE ${NDOF_LIBRARY})

  target_compile_definitions(ll::ndof INTERFACE LIB_NDOF=1)
endif (NDOF)


