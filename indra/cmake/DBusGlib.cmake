# -*- cmake -*-
include(Prebuilt)
include(GLIB)

if( GLIB_FOUND )
if (USESYSTEMLIBS)
  include(FindPkgConfig)

  pkg_check_modules(DBUSGLIB REQUIRED dbus-glib-1)

elseif (LINUX)
  use_prebuilt_binary(dbus_glib)
  set(DBUSGLIB_FOUND ON FORCE BOOL)
  set(DBUSGLIB_INCLUDE_DIRS
	${GLIB_INCLUDE_DIRS}
    ${LIBS_PREBUILT_DIR}/include/dbus
    )
  # We don't need to explicitly link against dbus-glib itself, because
  # the viewer probes for the system's copy at runtime.
endif (USESYSTEMLIBS)

if (DBUSGLIB_FOUND)
  set(DBUSGLIB ON CACHE BOOL "Build with dbus-glib message bus support.")
endif (DBUSGLIB_FOUND)

if (DBUSGLIB)
  add_definitions(-DLL_DBUS_ENABLED=1)
endif (DBUSGLIB)

endif()
