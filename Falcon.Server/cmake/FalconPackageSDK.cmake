cmake_minimum_required(VERSION 3.19)

foreach(requiredVariable FALCON_BUILD_DIR FALCON_SOURCE_DIR FALCON_SDK_OUTPUT FALCON_SDK_PLATFORM FALCON_SDK_VERSION)
    if(NOT DEFINED ${requiredVariable})
        message(FATAL_ERROR "${requiredVariable} is not set")
    endif()
endforeach()

file(REAL_PATH "${FALCON_SOURCE_DIR}" sourceRoot)
file(REAL_PATH "${FALCON_BUILD_DIR}" buildRoot)
get_filename_component(FALCON_SDK_OUTPUT "${FALCON_SDK_OUTPUT}" ABSOLUTE)

set(sdkDescription "${buildRoot}/FalconInternalSDK.cmake")
if(NOT EXISTS "${sdkDescription}")
    message(FATAL_ERROR "${sdkDescription} is missing: build Falcon with FALCON_EXPORT_SYMBOLS=ON first")
endif()
include("${sdkDescription}")

file(REMOVE_RECURSE "${FALCON_SDK_OUTPUT}")
file(MAKE_DIRECTORY "${FALCON_SDK_OUTPUT}/include")

set(packagedDirectories "")
set(seenDirectories "")
set(index 0)
foreach(directory IN LISTS FALCON_INTERNAL_INCLUDE_DIRECTORIES)
    if(directory STREQUAL "" OR NOT IS_DIRECTORY "${directory}")
        continue()
    endif()

    file(REAL_PATH "${directory}" resolved)
    if(resolved IN_LIST seenDirectories OR resolved STREQUAL sourceRoot OR resolved STREQUAL buildRoot)
        continue()
    endif()
    list(APPEND seenDirectories "${resolved}")

    string(FIND "${resolved}/" "${sourceRoot}/" sourcePosition)
    string(FIND "${resolved}/" "${buildRoot}/" buildPosition)
    if(NOT sourcePosition EQUAL 0 AND NOT buildPosition EQUAL 0)
        message(STATUS "Leaving out the system include directory ${resolved}")
        continue()
    endif()

    file(COPY "${resolved}/" DESTINATION "${FALCON_SDK_OUTPUT}/include/${index}"
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN "*.hpp" PATTERN "*.inl" PATTERN "*.ipp")
    list(APPEND packagedDirectories "\${CMAKE_CURRENT_LIST_DIR}/include/${index}")
    math(EXPR index "${index} + 1")
endforeach()

set(executable "")
set(linkerFile "")
if(FALCON_SDK_PLATFORM STREQUAL "windows")
    get_filename_component(linkerName "${FALCON_INTERNAL_LINKER_FILE}" NAME)
    file(COPY "${FALCON_INTERNAL_LINKER_FILE}" DESTINATION "${FALCON_SDK_OUTPUT}/lib")
    set(linkerFile "\${CMAKE_CURRENT_LIST_DIR}/lib/${linkerName}")
elseif(FALCON_SDK_PLATFORM STREQUAL "macos")
    get_filename_component(executableName "${FALCON_INTERNAL_EXECUTABLE}" NAME)
    file(COPY "${FALCON_INTERNAL_EXECUTABLE}" DESTINATION "${FALCON_SDK_OUTPUT}/bin")
    set(executable "\${CMAKE_CURRENT_LIST_DIR}/bin/${executableName}")
endif()

string(CONCAT sdkContent
        "set(FALCON_INTERNAL_SDK_VERSION [==[${FALCON_SDK_VERSION}]==])\n"
        "set(FALCON_INTERNAL_SDK_PLATFORM [==[${FALCON_SDK_PLATFORM}]==])\n"
        "set(FALCON_INTERNAL_EXECUTABLE \"${executable}\")\n"
        "set(FALCON_INTERNAL_LINKER_FILE \"${linkerFile}\")\n"
        "set(FALCON_INTERNAL_INCLUDE_DIRECTORIES \"${packagedDirectories}\")\n"
        "set(FALCON_INTERNAL_COMPILE_DEFINITIONS [==[${FALCON_INTERNAL_COMPILE_DEFINITIONS}]==])\n"
        "set(FALCON_INTERNAL_CXX_COMPILER_ID [==[${FALCON_INTERNAL_CXX_COMPILER_ID}]==])\n"
        "set(FALCON_INTERNAL_CXX_COMPILER_VERSION [==[${FALCON_INTERNAL_CXX_COMPILER_VERSION}]==])\n")
file(WRITE "${FALCON_SDK_OUTPUT}/FalconInternalSDK.cmake" "${sdkContent}")

string(CONCAT readme
        "Falcon internal plugin SDK ${FALCON_SDK_VERSION} for ${FALCON_SDK_PLATFORM}\n\n"
        "Compiler: ${FALCON_INTERNAL_CXX_COMPILER_ID} ${FALCON_INTERNAL_CXX_COMPILER_VERSION}\n\n"
        "Internal plugins only load on the Falcon ${FALCON_SDK_VERSION} release for ${FALCON_SDK_PLATFORM},\n"
        "and must be built with the same compiler and version. Point falcon_add_internal_plugin at this folder:\n\n"
        "    cmake -B build -DFALCON_SDK_DIR=/path/to/this/folder\n\n"
        "See https://github.com/Falcon-MC/PluginAPI for a complete example.\n")
file(WRITE "${FALCON_SDK_OUTPUT}/README.txt" "${readme}")

message(STATUS "Packaged ${index} include directories into ${FALCON_SDK_OUTPUT}")
