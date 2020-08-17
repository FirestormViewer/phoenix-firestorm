
include(Prebuilt)

if( LINUX )
  use_prebuilt_binary(glib)
  set(GLIB_FOUND ON CACHE BOOL "Build against glib 2")
  set(GLIB_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/glib-2.0 ${LIBS_PREBUILT_DIR}/lib/release/glib-2.0/include ) 
  set(GLIB_LIBRARIES libgobject-2.0.a libglib-2.0.a libffi.a libpcre.a)

  add_definitions(-DLL_GLIB=1)
endif()
