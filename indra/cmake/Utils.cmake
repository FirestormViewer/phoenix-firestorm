# -*- cmake -*-
#
# Miscellaneous utility macros

include(VersionControl)

macro(load_config_file _config_filename)
  # FIXME: should use this when we deprecate cmake 2.4.8 support
  #  file(STRINGS ${_config_filename} _config_file_lines 
  #       REGEX "[a-zA-Z_][a-zA-Z0-9_]*\s*=\s*[a-zA-Z0-9_- ]*")
  file(READ ${_config_filename} _config_file_contents)
  set (config_entry_re "[ ]*([a-zA-Z][a-zA-Z0-9_]*)[ ]*=([a-zA-Z0-9.]+)[ ]*")

  # HACK: This is the easiest way to iterate over lines in a file with 2.4-8
  string(REGEX MATCHALL 
         ${config_entry_re}
         _all_config_values 
         ${_config_file_contents}
         )
  foreach(_config_value ${_all_config_values})
    # Extract the key (variable name) and value and assign to a cmake variable.
    string(REGEX REPLACE "${config_entry_re}" "\\1" _key "${_config_value}")
    string(REGEX REPLACE "${config_entry_re}" "\\2" _value "${_config_value}")
    set("${_key}" "${_value}")
  endforeach(_config_value)
endmacro(load_config_file _config_filename)

# Loads data from indra/Version, and sets the following variables for both the
# server and viewer
# (VIEWER|SERVER)_VERSION
# (VIEWER|SERVER)_VERSION_MAJOR
# (VIEWER|SERVER)_VERSION_MINOR
# (VIEWER|SERVER)_VERSION_PATCH
# 
# And the build version
# VERSION_BUILD
macro(load_version_data)
  load_config_file(Version)
  vcs_get_revision(VERSION_BUILD)

  if (SERVER)
    if (NOT "${VERSION_SERVER}")
      message(FATAL_ERROR "VERSION_SERVER not set in indra/Version")
    endif (NOT "${VERSION_SERVER}")
    split_version_parts(
      "${VERSION_SERVER}"
      VERSION_SERVER_MAJOR
      VERSION_SERVER_MINOR
      VERSION_SERVER_PATCH
      )
    # append the build number to the server version
    set(VERSION_SERVER "${VERSION_SERVER}.${VERSION_BUILD}")
    message(STATUS "Server version ${VERSION_SERVER}")
  endif (SERVER)

  if (VIEWER)
	# We probably don't care enough to get a cmake warning about this. [Disc]
#    if (NOT ${VERSION_SERVER})
#      message(FATAL_ERROR "VERSION_SERVER not set in indra/Version")
#    endif (NOT ${VERSION_SERVER})
    split_version_parts(
      "${VERSION_VIEWER}"
      VERSION_VIEWER_MAJOR
      VERSION_VIEWER_MINOR
      VERSION_VIEWER_PATCH
      )
    # append the build number to the viewer version
    set(VERSION_VIEWER "${VERSION_VIEWER}.${VERSION_BUILD}")
    message(STATUS "Viewer version ${VERSION_VIEWER}")
  endif (VIEWER)
endmacro(load_version_data)

# Split the version into major, minor and patch parts, and assign them to the
# variable names supplied to the macro.
macro(split_version_parts _version _major _minor _patch)
  set(INDRA_VERSION_RE "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  if (NOT "${_version}" MATCHES "${INDRA_VERSION_RE}")
    message(FATAL_ERROR "${_version} is not a valid version string. It should be a 3 part version, MAJOR.MINOR.PATCH. Example: 2.1.49")
  endif (NOT "${_version}" MATCHES "${INDRA_VERSION_RE}")
  string(REGEX REPLACE "${INDRA_VERSION_RE}" "\\1" ${_major} "${_version}")
  string(REGEX REPLACE "${INDRA_VERSION_RE}" "\\2" ${_minor} "${_version}")
  string(REGEX REPLACE "${INDRA_VERSION_RE}" "\\3" ${_patch} "${_version}")
endmacro(split_version_parts)

# configure_files - macro to simplify calling configure_file on source files
#
# arguments:
#   GENERATED FILES - variable to hold a list of generated files
#   FILES           - names of files to be configured.
macro(configure_files GENERATED_FILES)
  set(_files_to_add "")
  foreach (_template_file ${ARGV})
    if (NOT "${GENERATED_FILES}" STREQUAL "${_template_file}")
      configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${_template_file}.in
        ${CMAKE_CURRENT_SOURCE_DIR}/${_template_file}
        @ONLY
        )
      set(_files_to_add 
          ${_files_to_add} 
          ${CMAKE_CURRENT_SOURCE_DIR}/${_template_file}
          )
    endif (NOT "${GENERATED_FILES}" STREQUAL "${_template_file}")
  endforeach(_template_file)
  set(${GENERATED_FILES} ${_files_to_add})
endmacro(configure_files)
