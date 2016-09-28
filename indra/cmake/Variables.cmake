# -*- cmake -*-
#
# Definitions of variables used throughout the Second Life build
# process.
#
# Platform variables:
#
#   DARWIN  - Mac OS X
#   LINUX   - Linux
#   WINDOWS - Windows

set(NDTARGET_ARCH "x86" CACHE STRING "Build 64 or 32 bit viewer. Defaults to 32 bit.")
set(ND_USE_OPENJPEG2 OFF CACHE BOOL "Use OpenJPEG 2.1 instead of 1.4. Default off.")

if( ${NDTARGET_ARCH} STREQUAL "x64" )
  set( ND_BUILD64BIT_ARCH ON )
elseif( ${NDTARGET_ARCH} STREQUAL "universal" )
  set( ND_BUILD64BIT_ARCH ON )
  set( OSX_UNIVERSAL_ARCH ON )
endif()

# Relative and absolute paths to subtrees.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

if(NOT DEFINED COMMON_CMAKE_DIR)
    set(COMMON_CMAKE_DIR "${CMAKE_SOURCE_DIR}/cmake")
endif(NOT DEFINED COMMON_CMAKE_DIR)

set(LIBS_CLOSED_PREFIX)
set(LIBS_OPEN_PREFIX)
set(SCRIPTS_PREFIX ../scripts)
set(VIEWER_PREFIX)
set(INTEGRATION_TESTS_PREFIX)
set(LL_TESTS OFF CACHE BOOL "Build and run unit and integration tests (disable for build timing runs to reduce variation")
set(INCREMENTAL_LINK OFF CACHE BOOL "Use incremental linking on win32 builds (enable for faster links on some machines)")

# <FS:ND> When building for Linux x64 we enable building the media plugins, in all other cases we use the prebuild 32 bit packages
# set(ENABLE_MEDIA_PLUGINS ON CACHE BOOL "Turn off building media plugins if they are imported by third-party library mechanism")

if (ND_BUILD64BIT_ARCH AND ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
  set( ENABLE_MEDIA_PLUGINS OFF CACHE FORCE "Build with media plugins" )
else (ND_BUILD64BIT_ARCH AND ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
  set(ENABLE_MEDIA_PLUGINS ON CACHE BOOL "Turn off building media plugins if they are imported by third-party library mechanism")
endif (ND_BUILD64BIT_ARCH AND ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
# </FS:ND>

if(LIBS_CLOSED_DIR)
  file(TO_CMAKE_PATH "${LIBS_CLOSED_DIR}" LIBS_CLOSED_DIR)
else(LIBS_CLOSED_DIR)
  set(LIBS_CLOSED_DIR ${CMAKE_SOURCE_DIR}/${LIBS_CLOSED_PREFIX})
endif(LIBS_CLOSED_DIR)
if(LIBS_COMMON_DIR)
  file(TO_CMAKE_PATH "${LIBS_COMMON_DIR}" LIBS_COMMON_DIR)
else(LIBS_COMMON_DIR)
  set(LIBS_COMMON_DIR ${CMAKE_SOURCE_DIR}/${LIBS_OPEN_PREFIX})
endif(LIBS_COMMON_DIR)
set(LIBS_OPEN_DIR ${LIBS_COMMON_DIR})

set(SCRIPTS_DIR ${CMAKE_SOURCE_DIR}/${SCRIPTS_PREFIX})
set(VIEWER_DIR ${CMAKE_SOURCE_DIR}/${VIEWER_PREFIX})

set(AUTOBUILD_INSTALL_DIR ${CMAKE_BINARY_DIR}/packages)

set(LIBS_PREBUILT_DIR ${AUTOBUILD_INSTALL_DIR} CACHE PATH
    "Location of prebuilt libraries.")

if (EXISTS ${CMAKE_SOURCE_DIR}/Server.cmake)
  # We use this as a marker that you can try to use the proprietary libraries.
  set(INSTALL_PROPRIETARY ON CACHE BOOL "Install proprietary binaries")
endif (EXISTS ${CMAKE_SOURCE_DIR}/Server.cmake)
set(TEMPLATE_VERIFIER_OPTIONS "" CACHE STRING "Options for scripts/template_verifier.py")
set(TEMPLATE_VERIFIER_MASTER_URL "http://bitbucket.org/lindenlab/master-message-template/raw/tip/message_template.msg" CACHE STRING "Location of the master message template")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type.  One of: Debug Release RelWithDebInfo" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(WINDOWS ON BOOL FORCE)
  set(ARCH i686)
  set(LL_ARCH ${ARCH}_win32)
  set(LL_ARCH_DIR ${ARCH}-win32)
  set(WORD_SIZE 32)
  if( ND_BUILD64BIT_ARCH )
    set(WORD_SIZE 64)
  endif( ND_BUILD64BIT_ARCH )
endif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(LINUX ON BOOl FORCE)

  # If someone has specified a word size, use that to determine the
  # architecture.  Otherwise, let the architecture specify the word size.
  if (WORD_SIZE EQUAL 32)
    #message(STATUS "WORD_SIZE is 32")
    set(ARCH i686)
  elseif (WORD_SIZE EQUAL 64)
    #message(STATUS "WORD_SIZE is 64")
    set(ARCH x86_64)
  else (WORD_SIZE EQUAL 32)
    #message(STATUS "WORD_SIZE is UNDEFINED")
    execute_process(COMMAND uname -m COMMAND sed s/i.86/i686/
                    OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (ARCH STREQUAL x86_64)
      #message(STATUS "ARCH is detected as 64; ARCH is ${ARCH}")
      set(WORD_SIZE 64)
    else (ARCH STREQUAL x86_64)
      #message(STATUS "ARCH is detected as 32; ARCH is ${ARCH}")
      set(WORD_SIZE 32)
    endif (ARCH STREQUAL x86_64)
  endif (WORD_SIZE EQUAL 32)

  if (WORD_SIZE EQUAL 32)
    set(DEB_ARCHITECTURE i386)
    set(FIND_LIBRARY_USE_LIB64_PATHS OFF)
    set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib32 ${CMAKE_SYSTEM_LIBRARY_PATH})
  else (WORD_SIZE EQUAL 32)
    set(DEB_ARCHITECTURE amd64)
    set(FIND_LIBRARY_USE_LIB64_PATHS ON)
  endif (WORD_SIZE EQUAL 32)

  execute_process(COMMAND dpkg-architecture -a${DEB_ARCHITECTURE} -qDEB_HOST_MULTIARCH 
      RESULT_VARIABLE DPKG_RESULT
      OUTPUT_VARIABLE DPKG_ARCH
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  #message (STATUS "DPKG_RESULT ${DPKG_RESULT}, DPKG_ARCH ${DPKG_ARCH}")
  if (DPKG_RESULT EQUAL 0)
    set(CMAKE_LIBRARY_ARCHITECTURE ${DPKG_ARCH})
    set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib/${DPKG_ARCH} /usr/local/lib/${DPKG_ARCH} ${CMAKE_SYSTEM_LIBRARY_PATH})
  endif (DPKG_RESULT EQUAL 0)

  include(ConfigurePkgConfig)

  set(LL_ARCH ${ARCH}_linux)
  set(LL_ARCH_DIR ${ARCH}-linux)

  if (INSTALL_PROPRIETARY)
    # Only turn on headless if we can find osmesa libraries.
    include(FindPkgConfig)
    #pkg_check_modules(OSMESA osmesa)
    #if (OSMESA_FOUND)
    #  set(BUILD_HEADLESS ON CACHE BOOL "Build headless libraries.")
    #endif (OSMESA_FOUND)
  endif (INSTALL_PROPRIETARY)

endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(DARWIN 1)
  
  # now we only support Xcode 6.0 using 10.9 (Mavericks), minimum OS 10.7 (Lion)
  #<FS:TS> Be a bit more flexible: support Xcode 6 or 7 and SDKs from 10.9 to 10.11
  #set(XCODE_VERSION 6.0)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 10.7)
  #set(CMAKE_OSX_SYSROOT macosx10.9)
  if(IS_DIRECTORY "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk")
    # Assume Xcode 7 if Sierra SDK is present
    set(XCODE_VERSION 7.3)
    set(CMAKE_OSX_SYSROOT macosx10.12)
    message(STATUS "OS X SDK 10.12 found.")
  elseif(IS_DIRECTORY "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk")
    # Assume Xcode 7 if El Capitan SDK is present
    set(XCODE_VERSION 7.3)
    set(CMAKE_OSX_SYSROOT macosx10.11)
    message(STATUS "OS X SDK 10.11 found.")
  elseif(IS_DIRECTORY "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk")
    # Assume Xcode 7 if Yosemite SDK is present
    set(XCODE_VERSION 7.0)
    set(CMAKE_OSX_SYSROOT macosx10.10)
    message(STATUS "OS X SDK 10.10 found.")
  elseif(IS_DIRECTORY "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk")
    # Assume Xcode 6 if only Mavericks SDK is present
    set(XCODE_VERSION 6.0)
    set(CMAKE_OSX_SYSROOT macosx10.9)
    message(STATUS "OS X SDK 10.9 found.")
  else(IS_DIRECTORY "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk")
    error("Unable to determine which OS X SDK to use. Giving up.")
  endif(IS_DIRECTORY "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk")

  set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL 3)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_STRICT_ALIASING NO)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_FAST_MATH NO)
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS ssse3)
  # <FS:TS> Need libc++ for C++11 and Boost
  #set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libstdc++")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym)

  # Build only for i386 by default, system default on MacOSX 10.6+ is x86_64
  if (NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "i386")
    if(ND_BUILD64BIT_ARCH)
      set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif(ND_BUILD64BIT_ARCH)
    if(OSX_UNIVERSAL_ARCH)
      set(CMAKE_OSX_ARCHITECTURES "i386;x86_64")
    endif(OSX_UNIVERSAL_ARCH)
  endif (NOT CMAKE_OSX_ARCHITECTURES)

  set(ARCH ${CMAKE_OSX_ARCHITECTURES})
  if(ND_BUILD64BIT_ARCH)
    set(ARCH x86_64)
  endif(ND_BUILD64BIT_ARCH)
  if(OSX_UNIVERSAL_ARCH)
    set(ARCH universal)
  endif(OSX_UNIVERSAL_ARCH)
  set(LL_ARCH ${ARCH}_darwin)
  set(LL_ARCH_DIR universal-darwin)
  if(ND_BUILD64BIT_ARCH)
    set(WORD_SIZE 64)
  else (ND_BUILD64BIT_ARCH)
    set(WORD_SIZE 32)
  endif(ND_BUILD64BIT_ARCH)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Default deploy grid
set(GRID agni CACHE STRING "Target Grid")

#set(VIEWER_CHANNEL "Second Life Test" CACHE STRING "Viewer Channel Name")

# Flickr API keys.
set(FLICKR_API_KEY "daaabff93a967e0f37fa18863bb43b29")
set(FLICKR_API_SECRET "846f0958020b553e") 

set(ENABLE_SIGNING OFF CACHE BOOL "Enable signing the viewer")
set(SIGNING_IDENTITY "" CACHE STRING "Specifies the signing identity to use, if necessary.")

set(VERSION_BUILD "0" CACHE STRING "Revision number passed in from the outside")
set(USESYSTEMLIBS OFF CACHE BOOL "Use libraries from your system rather than Linden-supplied prebuilt libraries.")
set(UNATTENDED OFF CACHE BOOL "Should be set to ON for building with VC Express editions.")

set(USE_PRECOMPILED_HEADERS ON CACHE BOOL "Enable use of precompiled header directives where supported.")
# <FS:ND> When using Havok, we have to turn OpenSim support off
if( HAVOK_TPV )
 if( OPENSIM )
  message( "Compiling with Havok libraries, disabling OpenSim support" )
 endif( OPENSIM )
  
 set( OPENSIM OFF )
endif( HAVOK_TPV )
# </FS:ND>

source_group("CMake Rules" FILES CMakeLists.txt)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
