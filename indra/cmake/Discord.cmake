# -*- cmake -*-

include(Prebuilt)
use_prebuilt_binary(discord-rpc)
set(DISCORD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/discord-rpc)

if (WINDOWS)
  set(DISCORD_LIBRARY discord-rpc)
elseif (LINUX)
set(DISCORD_LIBRARY discord-rpc)
elseif (DARWIN)
set(DISCORD_LIBRARY discord-rpc)
endif (WINDOWS)
