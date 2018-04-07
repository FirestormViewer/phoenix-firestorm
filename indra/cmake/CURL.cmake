# -*- cmake -*-
include(Prebuilt)
include(NGHTTP2)

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
      if (LINUX)
          list(APPEND CURL_LIBRARIES
               pthread ${NGHTTP2_LIBRARIES}
              )
      endif (LINUX)
  endif (WINDOWS)
  set(CURL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (USESYSTEMLIBS)
