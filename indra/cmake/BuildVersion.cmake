# -*- cmake -*-
# Construct the viewer version number based on the indra/VIEWER_VERSION file

if (NOT DEFINED VIEWER_SHORT_VERSION) # will be true in indra/, false in indra/newview/
    set(VIEWER_VERSION_BASE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/newview/VIEWER_VERSION.txt")

    if ( EXISTS ${VIEWER_VERSION_BASE_FILE} )
        file(STRINGS ${VIEWER_VERSION_BASE_FILE} VIEWER_SHORT_VERSION REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+")
        string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" VIEWER_VERSION_MAJOR ${VIEWER_SHORT_VERSION})
        string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1" VIEWER_VERSION_MINOR ${VIEWER_SHORT_VERSION})
        string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" VIEWER_VERSION_PATCH ${VIEWER_SHORT_VERSION})

        if (DEFINED ENV{revision})
           set(VIEWER_VERSION_REVISION $ENV{revision})
           message("Revision (from environment): ${VIEWER_VERSION_REVISION}")

        elseif (DEFINED ENV{AUTOBUILD_BUILD_ID})
           set(VIEWER_VERSION_REVISION $ENV{AUTOBUILD_BUILD_ID})
           message(STATUS "Revision (from autobuild environment): ${VIEWER_VERSION_REVISION}")

        else (DEFINED ENV{revision})
           find_program(MERCURIAL hg)
           find_program(SED sed)
           if (DEFINED MERCURIAL AND DEFINED SED)
              execute_process(
                 # <FS:TS> FIRE-11737: Reverting to old revisions shows tip in build string
                 #         This command gets the revision number of the current
                 #         repository tip. This leads to confusion when
                 #         building an earlier revision. Instead, we use
                 #         "hg identify -n" to get the local revision number
                 #         of the actual state of the repository.
                 #COMMAND ${MERCURIAL} log -r tip:0 --template '\\n'
                 #COMMAND ${WORDCOUNT} -l
                 #COMMAND ${SED} "s/ //g"
                 COMMAND ${MERCURIAL} identify -n
                 COMMAND ${SED} "s/+//"	# [CR] Strip off any + from the revision number
                 OUTPUT_VARIABLE VIEWER_VERSION_REVISION
                 OUTPUT_STRIP_TRAILING_WHITESPACE
                 )
              if ("${VIEWER_VERSION_REVISION}" MATCHES "^[0-9]+$")
                 message("Revision (from hg) ${VIEWER_VERSION_REVISION}")
              else ("${VIEWER_VERSION_REVISION}" MATCHES "^[0-9]+$")
                 message("Revision not set (repository not found?); using 0")
                 set(VIEWER_VERSION_REVISION 0 )
              endif ("${VIEWER_VERSION_REVISION}" MATCHES "^[0-9]+$")
           else (DEFINED MERCURIAL AND DEFINED SED)
              message("Revision not set: 'hg' or 'sed' not found; using 0")
              set(VIEWER_VERSION_REVISION 0)
           endif (DEFINED MERCURIAL AND DEFINED SED)
        endif (DEFINED ENV{revision})
        message("Building '${VIEWER_CHANNEL}' Version ${VIEWER_SHORT_VERSION}.${VIEWER_VERSION_REVISION}")
    else ( EXISTS ${VIEWER_VERSION_BASE_FILE} )
        message(SEND_ERROR "Cannot get viewer version from '${VIEWER_VERSION_BASE_FILE}'") 
    endif ( EXISTS ${VIEWER_VERSION_BASE_FILE} )

    if ("${VIEWER_VERSION_REVISION}" STREQUAL "")
      message("Ultimate fallback, revision was blank or not set: will use 0")
      set(VIEWER_VERSION_REVISION 0)
    endif ("${VIEWER_VERSION_REVISION}" STREQUAL "")

    set(VIEWER_CHANNEL_VERSION_DEFINES
        "LL_VIEWER_CHANNEL=${VIEWER_CHANNEL}"
        "LL_VIEWER_VERSION_MAJOR=${VIEWER_VERSION_MAJOR}"
        "LL_VIEWER_VERSION_MINOR=${VIEWER_VERSION_MINOR}"
        "LL_VIEWER_VERSION_PATCH=${VIEWER_VERSION_PATCH}"
        "LL_VIEWER_VERSION_BUILD=${VIEWER_VERSION_REVISION}"
        "LLBUILD_CONFIG=\"${CMAKE_BUILD_TYPE}\""
        )
endif (NOT DEFINED VIEWER_SHORT_VERSION)
