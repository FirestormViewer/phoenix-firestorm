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

