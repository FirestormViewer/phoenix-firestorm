# -*- cmake -*-

include(Prebuilt)

set(JSONCPP_FIND_QUIETLY ON)
set(JSONCPP_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindJsonCpp)
else (STANDALONE)
  use_prebuilt_binary(jsoncpp)
  if (WINDOWS)
    set(JSONCPP_LIBRARIES 
      debug json_vc100debug_libmt.lib
      optimized json_vc100_libmt)
  elseif (DARWIN)
    set(JSONCPP_LIBRARIES libjson_darwin_libmt.a)
  elseif (LINUX)
    if (ND_BUILD64BIT_ARCH)
      set(JSONCPP_LIBRARIES libjson_linux-gcc-4.4.7_libmt.a)
    else (ND_BUILD64BIT_ARCH)
      set(JSONCPP_LIBRARIES libjson_linux-gcc-4.1.3_libmt.a)
    endif (ND_BUILD64BIT_ARCH
  endif (WINDOWS)
  set(JSONCPP_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include/jsoncpp" "${LIBS_PREBUILT_DIR}/include/json")
endif (STANDALONE)
