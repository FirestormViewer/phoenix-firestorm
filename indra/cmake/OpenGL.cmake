# -*- cmake -*-

include(Variables)
include(Prebuilt)

# <FS:TS> Shut cmake up about OpenGL library preference. This can
# be either LEGACY or GLVND.
if (LINUX)
  SET(OpenGL_GL_PREFERENCE LEGACY)
endif (LINUX)

if (BUILD_HEADLESS)
  SET(OPENGL_glu_LIBRARY GLU)
  SET(OPENGL_HEADLESS_LIBRARIES OSMesa16 dl GLU)
endif (BUILD_HEADLESS)

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
