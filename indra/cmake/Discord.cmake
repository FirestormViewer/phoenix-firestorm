# -*- cmake -*-

# <FS:Ansariel> Prefer FS-specific Discord implementation
#include(Prebuilt)
#
#add_library(ll::discord INTERFACE IMPORTED)
#target_compile_definitions(ll::discord INTERFACE LL_DISCORD=1)
#
#use_prebuilt_binary(discord)
#
#target_include_directories(ll::discord SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include)
#target_link_libraries(ll::discord INTERFACE discord_partner_sdk)
# </FS:Ansariel>

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
