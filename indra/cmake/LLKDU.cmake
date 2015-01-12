# -*- cmake -*-

# USE_KDU can be set when launching cmake as an option using the argument -DUSE_KDU:BOOL=ON
# When building using proprietary binaries though (i.e. having access to LL private servers), 
# we always build with KDU
if (INSTALL_PROPRIETARY)
  set(USE_KDU ON CACHE BOOL "Use Kakadu library.")
endif (INSTALL_PROPRIETARY)

set( ND_KDU_SUFFIX "" )
if( ND_BUILD64BIT_ARCH )
  if( WINDOWS OR LINUX )
    set( ND_KDU_SUFFIX "_x64" )
  endif( WINDOWS OR LINUX )
endif( ND_BUILD64BIT_ARCH )
    

if (USE_KDU)
  if (USESYSTEMLIBS)
    include(FindKDU)
  else (USESYSTEMLIBS)
    include(Prebuilt)
    use_prebuilt_binary(kdu)
    if (WINDOWS)
      set(KDU_LIBRARY kdu${ND_KDU_SUFFIX}.lib)
    else (WINDOWS)
      set(KDU_LIBRARY libkdu${ND_KDU_SUFFIX}.a)
    endif (WINDOWS)
    set(KDU_INCLUDE_DIR ${AUTOBUILD_INSTALL_DIR}/include/kdu)
  endif (USESYSTEMLIBS)
  set(LLKDU_INCLUDE_DIRS ${LIBS_OPEN_DIR}/llkdu)
  set(LLKDU_LIBRARIES llkdu)
endif (USE_KDU)
