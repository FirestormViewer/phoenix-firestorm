# -*- cmake -*-
include(Prebuilt)

# There are three possible solutions to provide the llphysicsextensions:
# - The full source package, selected by -DHAVOK:BOOL=ON
# - The stub source package, selected by -DHAVOK:BOOL=OFF 
# - The prebuilt package available to those with sublicenses, selected by -DHAVOK_TPV:BOOL=ON

if (INSTALL_PROPRIETARY)
   set(HAVOK ON CACHE BOOL "Use Havok physics library")
endif (INSTALL_PROPRIETARY)

include_guard()
add_library( llphysicsextensions_impl INTERFACE IMPORTED )


# Note that the use_prebuilt_binary macros below do not in fact include binaries;
# the llphysicsextensions_* packages are source only and are built here.
# The source package and the stub package both build libraries of the same name.

if (HAVOK)
   include(Havok)
   use_prebuilt_binary(llphysicsextensions_source)
   set(LLPHYSICSEXTENSIONS_SRC_DIR ${LIBS_PREBUILT_DIR}/llphysicsextensions/src)
   if(DARWIN)
      set(LLPHYSICSEXTENSIONS_STUB_DIR ${LIBS_PREBUILT_DIR}/llphysicsextensions/stub)
      # can't set these library dependencies per-arch here, need to do it using XCODE_ATTRIBUTE_OTHER_LDFLAGS[arch=*] in newview/CMakeLists.txt
      #target_link_libraries( llphysicsextensions_impl INTERFACE llphysicsextensions)
      #target_link_libraries( llphysicsextensions_impl INTERFACE llphysicsextensionsstub)
   else()
     target_link_libraries( llphysicsextensions_impl INTERFACE llphysicsextensions)
     target_compile_definitions( llphysicsextensions_impl INTERFACE LL_HAVOK=1 )
   endif()
elseif (HAVOK_TPV)
   use_prebuilt_binary(llphysicsextensions_tpv)
   # <FS:TJ> Done in newview/CMakeLists.txt for darwin if Havok is enabled
   if (NOT DARWIN)
      target_link_libraries( llphysicsextensions_impl INTERFACE llphysicsextensions_tpv)
      target_compile_definitions( llphysicsextensions_impl INTERFACE LL_HAVOK=1 )
   endif()
   # </FS:TJ>
   # <FS:ND> include paths for LLs version and ours are different.
   target_include_directories( llphysicsextensions_impl INTERFACE ${LIBS_PREBUILT_DIR}/include/llphysicsextensions)
   # </FS:ND>

   # <FS:ND> havok lib get installed to packages/lib
   link_directories( ${LIBS_PREBUILT_DIR}/lib )
   # </FS:ND>
endif ()

if ((NOT HAVOK AND NOT HAVOK_TPV) OR DARWIN) # <FS:TJ/> ARM64 requires ndPhysicsStub
   use_prebuilt_binary( ndPhysicsStub )

# <FS:ND> Don't set this variable, there is no need to build any stub source if using ndPhysicsStub
#   set(LLPHYSICSEXTENSIONS_SRC_DIR ${LIBS_PREBUILT_DIR}/llphysicsextensions/stub)
# </FS:ND>

   # <FS:TJ> Use find_library to make our lives easier
   find_library(ND_HACDCONVEXDECOMPOSITION_LIBRARY
      NAMES
      nd_hacdConvexDecomposition.lib
      libnd_hacdConvexDecomposition.a
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

   find_library(HACD_LIBRARY
      NAMES
      hacd.lib
      libhacd.a
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

   find_library(ND_PATHING_LIBRARY
      NAMES
      nd_pathing.lib
      libnd_pathing.a
      libnd_Pathing.a
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

   if (NOT HAVOK AND NOT HAVOK_TPV) # <FS:TJ/> Done in newview/CMakeLists.txt for darwin if Havok is enabled
      target_link_libraries(llphysicsextensions_impl INTERFACE ${ND_HACDCONVEXDECOMPOSITION_LIBRARY} ${HACD_LIBRARY} ${ND_PATHING_LIBRARY})
   endif()
   # </FS:TJ>

   # <FS:ND> include paths for LLs version and ours are different.
   target_include_directories( llphysicsextensions_impl INTERFACE ${LIBS_PREBUILT_DIR}/include/ )
   # </FS:ND>

endif ()

# <FS:ND> include paths for LLs version and ours are different.
#target_include_directories( llphysicsextensions_impl INTERFACE   ${LIBS_PREBUILT_DIR}/include/llphysicsextensions)
# </FS:ND>
