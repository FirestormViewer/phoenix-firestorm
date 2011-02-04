# -*- cmake -*-

# Growl is actually libnotify on linux systems.
if (STANDALONE)
    set(LIBNOTIFY_FIND_REQUIRED ON)
    include(FindLibnotify)
    set(GROWL_INCLUDE_DIRS ${LIBNOTIFY_INCLUDE_DIR})
    set(GROWL_LIBRARY ${LIBNOTIFY_LIBRARIES})
else (STANDALONE)
    include(Prebuilt)
    use_prebuilt_binary(Growl)
    if (DARWIN)
	set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
	set(GROWL_LIBRARY growl)
    elseif (WINDOWS)
	set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
	set(GROWL_LIBRARY lgggrowl++)
    elseif (LINUX)
	# Everything glib-2.0 and GTK-specific is pulled in by UI.cmake.. Ugh.
	set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libnotify)
	set(GROWL_LIBRARY notify)
    endif (DARWIN)
endif (STANDALONE)
