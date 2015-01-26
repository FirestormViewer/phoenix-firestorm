# -*- cmake -*-
include(Prebuilt)

set(CURL_FIND_QUIETLY ON)
set(CURL_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindCURL)
else (USESYSTEMLIBS)
  use_prebuilt_binary(curl)
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
      if (LINUX AND ND_BUILD64BIT_ARCH)
          list(APPEND CURL_LIBRARIES
               idn
              )
      endif (LINUX AND ND_BUILD64BIT_ARCH)
  endif (WINDOWS)
  set(CURL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (USESYSTEMLIBS)
