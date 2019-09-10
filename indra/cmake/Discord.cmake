# -*- cmake -*-

if (DISCORD_API_KEY)
  include(Prebuilt)
  use_prebuilt_binary(discord-rpc)
  set(DISCORD_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/discord-rpc)

  if (WINDOWS)
    if (ADDRESS_SIZE EQUAL 32)
      set(DISCORD_LIBRARY discord-rpc)
    else ()
      set(DISCORD_LIBRARY discord-rpc_x64)
    endif(ADDRESS_SIZE EQUAL 32)
  elseif (LINUX)
    set(DISCORD_LIBRARY discord-rpc)
  elseif (DARWIN)
    set(DISCORD_LIBRARY discord-rpc)
  endif (WINDOWS)

  add_definitions( -DDISCORD_API_KEY=\"${DISCORD_API_KEY}\")
  add_definitions( -DHAS_DISCORD )
  set(USE_DISCORD TRUE)
endif (DISCORD_API_KEY)
