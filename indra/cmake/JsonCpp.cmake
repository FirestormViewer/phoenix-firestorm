# -*- cmake -*-

include(Prebuilt)

set(JSONCPP_FIND_QUIETLY OFF)
set(JSONCPP_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindJsonCpp)
else (USESYSTEMLIBS)
  use_prebuilt_binary(jsoncpp)
  if (WINDOWS)
    if (ND_BUILD64BIT_ARCH)
      set(JSONCPP_LIBRARIES
        debug json_libmtd.lib
        optimized json_libmt.lib)
    else (ND_BUILD64BIT_ARCH)
      set(JSONCPP_LIBRARIES
        debug json_libmdd.lib
        optimized json_libmd.lib)
    endif (ND_BUILD64BIT_ARCH)
  elseif (DARWIN)
    set(JSONCPP_LIBRARIES libjson_darwin_libmt.a)
  elseif (LINUX)
    if (ND_BUILD64BIT_ARCH)
      set(JSONCPP_LIBRARIES libjson_linux-gcc-4.6_libmt.a)
    else (ND_BUILD64BIT_ARCH)
      set(JSONCPP_LIBRARIES libjson_linux-gcc-4.1.3_libmt.a)
    endif (ND_BUILD64BIT_ARCH)
  endif (WINDOWS)
  set(JSONCPP_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include/jsoncpp" "${LIBS_PREBUILT_DIR}/include/json")
endif (USESYSTEMLIBS)
