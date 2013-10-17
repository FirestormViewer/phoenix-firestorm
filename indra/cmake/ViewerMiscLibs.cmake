# -*- cmake -*-
include(Prebuilt)

if (NOT STANDALONE)
  use_prebuilt_binary(libhunspell)
  use_prebuilt_binary(libuuid)
  use_prebuilt_binary(slvoice)
  use_prebuilt_binary(fontconfig)

  if( WINDOWS )
    if( ND_BUILD64BIT_ARCH )
      use_prebuilt_binary( slplugin_x86 )
      use_prebuilt_binary( wix )
    endif( ND_BUILD64BIT_ARCH )
  endif( WINDOWS )
endif(NOT STANDALONE)

