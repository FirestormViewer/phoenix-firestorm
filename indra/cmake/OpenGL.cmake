# -*- cmake -*-

if(LINUX)
  # <FS:Zi> make sure to link to libGL.so instead of libOpenGL.so so the linker does not
  # complain about libGL.so.1: error adding symbols: DSO missing from command line
  set(OpenGL_GL_PREFERENCE "LEGACY")
endif()

include(Variables)
include(Prebuilt)
include(FindOpenGL)

if(LINUX)
  if( NOT OPENGL_FOUND )
	message( FATAL_ERROR "OpenGL not found, install mesa-common-dev libgl1-mesa-dev" )
  endif()

  find_file( GLUH "GL/glu.h" )
  if( NOT GLUH )
	message( FATAL_ERROR "GL/glu.h not found, install libglu1-mesa-dev" )
  endif()
  find_library( XINERAMA Xinerama )
  if( NOT XINERAMA )
	message( FATAL_ERROR "Cannot link with -lXinerame, install libxinerama-dev" )
  endif()
  find_library( XRANDR Xrandr)
  if( NOT XRANDR )
	message( FATAL_ERROR "Cannot link with -lXrandr, install libxrandr-dev" )
  endif()
endif()
