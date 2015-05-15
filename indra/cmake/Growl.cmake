# -*- cmake -*-

# Growl is actually libnotify on linux systems.

if (USESYSTEMLIBS)
    if( LINUX )
	  add_definitions( -DHAS_GROWL)
    endif( LINUX )
    #set(LIBNOTIFY_FIND_REQUIRED ON)
    #include(FindLibnotify)
    #set(GROWL_INCLUDE_DIRS ${LIBNOTIFY_INCLUDE_DIR})
    #set(GROWL_LIBRARY ${LIBNOTIFY_LIBRARIES})
else (USESYSTEMLIBS)
    if (DARWIN OR WINDOWS)
	# Growl is making some problems still
      include(Prebuilt)
      use_prebuilt_binary(gntp-growl)
      set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
      set(GROWL_LIBRARY growl growl++)
      add_definitions( -DHAS_GROWL)
	elseif (LINUX)
      add_definitions( -DHAS_GROWL)
    endif ()
endif (USESYSTEMLIBS)
