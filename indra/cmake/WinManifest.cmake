# - Embeds a specific manifest file in a Windows binary
# Defines the following:
# EMBED_MANIFEST - embed manifest in a windows binary with mt
# Parameters - _target is the target file, type - 1 for EXE, 2 for DLL

  MACRO(EMBED_MANIFEST _target type)
    ADD_CUSTOM_COMMAND(
      TARGET ${_target}
      POST_BUILD
      COMMAND "mt.exe"
      ARGS
        -manifest \"${CMAKE_SOURCE_DIR}\\tools\\manifests\\compatibility.manifest\"
        -inputresource:\"$<TARGET_FILE:${_target}>\"\;\#${type}
        -outputresource:\"$<TARGET_FILE:${_target}>\"\;\#${type}
      COMMENT "Adding compatibility manifest to ${_target}"
    )
  ENDMACRO(EMBED_MANIFEST _target type)
