# -*- cmake -*-

# <FS:Ansariel> Prefer FS-specific Discord implementation
#include(Prebuilt)
#
#include_guard()
#
#add_library(ll::discord_sdk INTERFACE IMPORTED)
#target_compile_definitions(ll::discord_sdk INTERFACE LL_DISCORD=1)
#
#use_prebuilt_binary(discord_sdk)
#
#target_include_directories(ll::discord_sdk SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/discord_sdk)
#target_link_libraries(ll::discord_sdk INTERFACE discord_partner_sdk)
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
