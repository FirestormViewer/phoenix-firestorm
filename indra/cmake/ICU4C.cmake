# -*- cmake -*-
include(Prebuilt)

include_guard()

add_library( ll::icu4c INTERFACE IMPORTED )


use_system_binary(icu4c)
use_prebuilt_binary(icu4c)
if (WINDOWS)
  target_link_libraries( ll::icu4c INTERFACE  icuuc)
elseif(DARWIN)
  target_link_libraries( ll::icu4c INTERFACE  icuuc)
elseif(LINUX)
  #<FS:PC>
  #The icu4c 3p puts the libraries in "lib" rather than the normal "release/lib".
  #Add this to the link search path (otherwise link to the library fails)
  target_link_directories( ll::icu4c INTERFACE ${LIBS_PREBUILT_DIR}/lib)

  target_link_libraries( ll::icu4c INTERFACE  icuuc)
else()
  message(FATAL_ERROR "Invalid platform")
endif()

target_include_directories( ll::icu4c SYSTEM INTERFACE  ${LIBS_PREBUILT_DIR}/include/unicode )

use_prebuilt_binary(dictionaries)
