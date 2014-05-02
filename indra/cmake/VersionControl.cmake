# -*- cmake -*-
#
# Utility macros for getting info from the version control system.

# @return the rev number ie from the change set
macro(vcs_get_revision _output_variable)
	MESSAGE(STATUS "Checking for Mercurial repository ${PROJECT_SOURCE_DIR}")
	
	# Check for the hg directory to see if this is a hg repo
	IF(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/../.hg")
		# Find the hg executable for mac by bundle or by path and system path look everywhere!
		FIND_PROGRAM(_hg hg [PATH CMAKE_SYSTEM_PROGRAM_PATH CMAKE_SYSTEM_APPBUNDLE_PATH])
		
		# Test the return status make sure its found
		IF(NOT ${_hg} MATCHES "-NOTFOUND")
			# Output HG success
			MESSAGE(STATUS "Found hg!")
			
			# Grab the revision number
			EXECUTE_PROCESS(COMMAND ${_hg} summary WORKING_DIRECTORY 
			"${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE _release 
			RESULT_VARIABLE hg_result
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_STRIP_TRAILING_WHITESPACE)
			
			if (hg_result EQUAL 0)
				# So some regex magic
				foreach(_v_l ${_release})
				if(_v_l MATCHES "^parent: *[^0-9]*\([0-9]+\):\([a-z0-9]+\)")
					# not using CPACK yet for installing. This is prep work for CPACK
					#set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_1})
		
					# set Rev number.
					set(PHOENIX_WC_REVISION ${CMAKE_MATCH_1})
				endif()
				endforeach()
			ELSE ()
				# give feedback and set the rev to 0
				MESSAGE("Mercurial had an error. Too old version of hg?")
				SET(PHOENIX_WC_REVISION 0)
			ENDIF ()
		ELSE()
			# give feedback and set the rev to 0
			MESSAGE("Could not parse Mercurial version. Newer version of hg?")
			SET(PHOENIX_WC_REVISION 0)
		ENDIF()
	ELSE()
		# give feedback and set the rev to 0 as teh dirs not found
		MESSAGE(STATUS "Mercurial not used..")
		SET(PHOENIX_WC_REVISION 0)
	ENDIF()
	 
	# CPACK framework
	#SET(PHOENIX_PACKAGE_VERSION ${V_MAJOR}.${V_MINOR}.${V_PATCH})
	
	# Dev build number framework.
	#SET(PHOENIX_DEVELOPMENT_VERSION 1)
	
	# Show the version number and complete / return to utility script.
	MESSAGE(STATUS "Current Mercurial revision is ${PHOENIX_WC_REVISION}")
	set(${_output_variable} ${PHOENIX_WC_REVISION})
endmacro(vcs_get_revision)