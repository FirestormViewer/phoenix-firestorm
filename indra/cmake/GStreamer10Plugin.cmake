# -*- cmake -*-
include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindPkgConfig)

  pkg_check_modules(GSTREAMER10 REQUIRED gstreamer-1.0)
  pkg_check_modules(GSTREAMER10_PLUGINS_BASE REQUIRED gstreamer-plugins-base-1.0)
elseif (LINUX OR WINDOWS)
  use_prebuilt_binary(gstreamer10)
  use_prebuilt_binary(libxml2)
  set(GSTREAMER10_FOUND ON FORCE BOOL)
  set(GSTREAMER10_PLUGINS_BASE_FOUND ON FORCE BOOL)
  set(GSTREAMER10_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include/gstreamer-1.0
      ${LIBS_PREBUILT_DIR}/include/glib-2.0
      ${LIBS_PREBUILT_DIR}/include/libxml2
      )
  # We don't need to explicitly link against gstreamer itself, because
  # LLMediaImplGStreamer probes for the system's copy at runtime.
  set(GSTREAMER10_LIBRARIES)
endif (USESYSTEMLIBS)

if (GSTREAMER10_FOUND AND GSTREAMER10_PLUGINS_BASE_FOUND)
  set(GSTREAMER10 ON CACHE BOOL "Build with GStreamer-1.0 streaming media support.")
endif (GSTREAMER10_FOUND AND GSTREAMER10_PLUGINS_BASE_FOUND)

if (GSTREAMER10)
  add_definitions(-DLL_GSTREAMER10_ENABLED=1)
endif (GSTREAMER10)

