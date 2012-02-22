# -*- cmake -*-

# Growl is actually libnotify on linux systems.

# LO - Mind the comment mess, I can only test on windows so im using the comments to make it do nothing on ohter systems
if (STANDALONE)
    #set(LIBNOTIFY_FIND_REQUIRED ON)
    #include(FindLibnotify)
    #set(GROWL_INCLUDE_DIRS ${LIBNOTIFY_INCLUDE_DIR})
    #set(GROWL_LIBRARY ${LIBNOTIFY_LIBRARIES})
else (STANDALONE)
    #include(Prebuilt)
    #use_prebuilt_binary(Growl)
    if (DARWIN)
	#set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
	#set(GROWL_LIBRARY growl)
    elseif (WINDOWS)
	include(Prebuilt)
    use_prebuilt_binary(Growl)
	set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
	set(GROWL_LIBRARY growl++)
    elseif (LINUX)
	# Everything glib-2.0 and GTK-specific is pulled in by UI.cmake.. Ugh.
	#set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libnotify)
	#set(GROWL_LIBRARY notify)
    endif (DARWIN)
endif (STANDALONE)
