# Internal plugins are maintained in PluginAPI and still include the old server paths.
# Keep forwarding headers in the build/SDK, rather than restoring the old source tree.
set(FALCON_STRUCTURE_INVENTORY "${CMAKE_CURRENT_SOURCE_DIR}/structure/bds-1.26.10.json")
set(FALCON_LEGACY_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/legacy-include")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FALCON_STRUCTURE_INVENTORY}")

# The inventory writes old/path as consecutive scalar fields. Read those lines rather
# than requiring string(JSON), which is newer than the project's CMake 3.16 minimum.
file(STRINGS "${FALCON_STRUCTURE_INVENTORY}" inventoryPaths REGEX "^      \"(old|path)\":")
set(oldPath "")
set(expectedLegacyHeaders "")
foreach(entry IN LISTS inventoryPaths)
    if(entry MATCHES "\"old\": \"([^\"]+)\"")
        set(oldPath "${CMAKE_MATCH_1}")
    elseif(entry MATCHES "\"path\": \"([^\"]+)\"")
        set(newPath "${CMAKE_MATCH_1}")
        if(oldPath MATCHES "^Falcon.Server/include/(.+)$")
            set(legacyHeader "${CMAKE_MATCH_1}")
            if(NOT newPath MATCHES "^Falcon.Server/include/(.+)$")
                message(FATAL_ERROR "Header moved outside include/: ${oldPath}")
            endif()
            set(FALCON_CURRENT_SERVER_HEADER "${CMAKE_MATCH_1}")
            if(NOT legacyHeader STREQUAL FALCON_CURRENT_SERVER_HEADER)
                list(APPEND expectedLegacyHeaders "${legacyHeader}")
                if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/${FALCON_CURRENT_SERVER_HEADER}")
                    message(FATAL_ERROR "Missing relocated server header: ${FALCON_CURRENT_SERVER_HEADER}")
                endif()
                configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LegacyServerHeader.h.in"
                        "${FALCON_LEGACY_INCLUDE_DIR}/${legacyHeader}" @ONLY)
            endif()
        endif()
        set(oldPath "")
    endif()
endforeach()

file(GLOB_RECURSE generatedLegacyHeaders RELATIVE "${FALCON_LEGACY_INCLUDE_DIR}"
        "${FALCON_LEGACY_INCLUDE_DIR}/*.h")
foreach(header IN LISTS generatedLegacyHeaders)
    if(NOT header IN_LIST expectedLegacyHeaders)
        file(REMOVE "${FALCON_LEGACY_INCLUDE_DIR}/${header}")
    endif()
endforeach()
