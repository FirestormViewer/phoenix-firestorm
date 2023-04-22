# -*- cmake -*-

include_guard()
add_library(fs::discord INTERFACE IMPORTED)

include(Prebuilt)
use_prebuilt_binary(discord-rpc)

if (WINDOWS)
  target_link_libraries(fs::discord INTERFACE discord-rpc)
elseif (LINUX)
  target_link_libraries(fs::discord INTERFACE discord-rpc)
elseif (DARWIN)
  target_link_libraries(fs::discord INTERFACE discord-rpc)
endif (WINDOWS)

target_include_directories(fs::discord SYSTEM INTERFACE
        ${AUTOBUILD_INSTALL_DIR}/include/discord-rpc
        )
