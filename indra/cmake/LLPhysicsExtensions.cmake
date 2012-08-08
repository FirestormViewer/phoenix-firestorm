# -*- cmake -*-
include(Prebuilt)

# Note that the use_prebuilt_binary macros below do not in fact include binaries;
# the llphysicsextensions_* packages are source only and are built here.
# The source package and the stub package both build libraries of the same name.

# Using the real wrapper or the stub can be controlled with -DHAVOK:BOOL={ON,OFF}
# the default
if (INSTALL_PROPRIETARY)
   set(HAVOK ON CACHE BOOL "Use Havok physics library")
endif (INSTALL_PROPRIETARY)

if (HAVOK)
   include(Havok)
   use_prebuilt_binary(llphysicsextensions_source)
   set(LLPHYSICSEXTENSIONS_SRC_DIR ${LIBS_PREBUILT_DIR}/llphysicsextensions/src)
   set(LLPHYSICSEXTENSIONS_LIBRARIES    llphysicsextensions)

   # <FS:ND> include paths for LLs version and ours are different.
   set(LLPHYSICSEXTENSIONS_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/llphysicsextensions)
   # </FS:ND>

else (HAVOK)
   use_prebuilt_binary( ndPhysicsStub )

# <FS:ND> Don't set this variable, there is no need to build any stub source if using ndPhysicsStub
#   set(LLPHYSICSEXTENSIONS_SRC_DIR ${LIBS_PREBUILT_DIR}/llphysicsextensions/stub)
# </FS:ND>

   set(LLPHYSICSEXTENSIONS_LIBRARIES nd_hacdConvexDecomposition hacd nd_Pathing )

   # <FS:ND> include paths for LLs version and ours are different.
   set(LLPHYSICSEXTENSIONS_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/ )
   # </FS:ND>

endif (HAVOK)

# <FS:ND> include paths for LLs version and ours are different.
#set(LLPHYSICSEXTENSIONS_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/llphysicsextensions) 
# </FS:ND>