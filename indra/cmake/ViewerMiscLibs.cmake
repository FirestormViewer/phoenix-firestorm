# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  if (LINUX)
    use_prebuilt_binary(libuuid)
    use_prebuilt_binary(fontconfig)
  endif (LINUX)
  use_prebuilt_binary(libhunspell)
  use_prebuilt_binary(slvoice)
#  use_prebuilt_binary(libidn)

  if( ADDRESS_SIZE EQUAL 64 )
    if( DARWIN )
      use_prebuilt_binary( slplugin_x86 )
    endif( DARWIN )
  endif( )
endif(NOT USESYSTEMLIBS)

