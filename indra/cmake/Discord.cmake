# -*- cmake -*-




include(Prebuilt)
use_prebuilt_binary(discord-rpc)
set(DISCORD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/discord-rpc)
if (ADDRESS_SIZE EQUAL 32 AND WINDOWS)
	set(DISCORD_LIBRARY discord-rpc)
elseif (WINDOWS)
	set(DISCORD_LIBRARY discord-rpc_x64)
elseif (LINUX || DARWIN)
	set(DISCORD_LIBRARY libdiscord-rpc)
endif (ADDRESS_SIZE EQUAL 32 AND WINDOWS)
if (DISCORD_API_KEY)
	add_definitions( -DDISCORD_API_KEY=\"${DISCORD_API_KEY}\")
endif (DISCORD_API_KEY)
