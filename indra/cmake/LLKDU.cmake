# -*- cmake -*-

# USE_KDU can be set when launching cmake as an option using the argument -DUSE_KDU:BOOL=ON
# When building using proprietary binaries though (i.e. having access to LL private servers), 
# we always build with KDU
if (INSTALL_PROPRIETARY)
  set(USE_KDU ON CACHE BOOL "Use Kakadu library.")
endif (INSTALL_PROPRIETARY)

if (USE_KDU)
  if (USESYSTEMLIBS)
    find_path( KDU_INCLUDE_DIR kdu/kdu_image.h )
    find_library( KDU_LIBRARY kdu_x64 )
    if ( NOT KDU_LIBRARY OR NOT KDU_INCLUDE_DIR )
      message(FATAL_ERROR "Cannot find KDU: Library '${KDU_LIBRARY}'; header '${KDU_INCLUDE_DIR}' ")
    endif()
    set( KDU_INCLUDE_DIR ${KDU_INCLUDE_DIR}/kdu )
    set( LLKDU_LIBRARIES llkdu )
  else()
    include(Prebuilt)
    use_prebuilt_binary(kdu)
    if (WINDOWS)
      set(KDU_LIBRARY kdu.lib)
    else (WINDOWS)
      set(KDU_LIBRARY libkdu.a)
    endif (WINDOWS)
    set(KDU_INCLUDE_DIR ${AUTOBUILD_INSTALL_DIR}/include/kdu)
    set(LLKDU_INCLUDE_DIRS ${LIBS_OPEN_DIR}/llkdu)
    set(LLKDU_LIBRARIES llkdu)
  endif()
endif (USE_KDU)
