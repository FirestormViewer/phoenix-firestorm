# -*- cmake -*-
include(Prebuilt)

set(CURL_FIND_QUIETLY ON)
set(CURL_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindCURL)
else (USESYSTEMLIBS)
  use_prebuilt_binary(curl)
  if (DARWIN)
    use_prebuilt_libary(libidn)
  endif (DARWIN)
  if (WINDOWS)
    set(CURL_LIBRARIES 
    debug libcurld.lib
    optimized libcurl.lib)
  else (WINDOWS)
    set(CURL_LIBRARIES libcurl.a)
      if (DARWIN)
          list(APPEND CURL_LIBRARIES
               idn
               iconv
              )
      endif (DARWIN)
      if (LINUX)
          list(APPEND CURL_LIBRARIES
               pthread
              )
      endif (LINUX)
      if (LINUX AND ND_BUILD64BIT_ARCH)
          list(APPEND CURL_LIBRARIES
               idn pthread
              )
      endif (LINUX AND ND_BUILD64BIT_ARCH)
  endif (WINDOWS)
  set(CURL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (USESYSTEMLIBS)
