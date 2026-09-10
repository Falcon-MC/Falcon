cmake_minimum_required(VERSION 3.16)
project(Falcon.Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FALCON_RECIPES_JSON "${CMAKE_CURRENT_SOURCE_DIR}/resources/recipes.json")
set(FALCON_ITEM_TAGS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/resources/item_tags.json")
set(FALCON_BIOME_DEFINITIONS_NBT "${CMAKE_CURRENT_SOURCE_DIR}/resources/biome_definitions.nbt")
set(FALCON_BLOCK_PALETTE_NBT "${CMAKE_CURRENT_SOURCE_DIR}/resources/block_palette.nbt")
if(NOT EXISTS "${FALCON_RECIPES_JSON}" OR NOT EXISTS "${FALCON_ITEM_TAGS_JSON}")
    message(FATAL_ERROR "Falcon crafting data files are missing")
endif()
if(NOT EXISTS "${FALCON_BIOME_DEFINITIONS_NBT}")
    message(FATAL_ERROR "Falcon biome definitions file is missing")
endif()
if(NOT EXISTS "${FALCON_BLOCK_PALETTE_NBT}")
    message(FATAL_ERROR "Falcon block palette file is missing")
endif()

file(READ "${FALCON_RECIPES_JSON}" FALCON_RECIPES_JSON_CONTENT)
file(READ "${FALCON_ITEM_TAGS_JSON}" FALCON_ITEM_TAGS_JSON_CONTENT)
set(FALCON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FALCON_GENERATED_DIR}")
configure_file(cmake/CraftingRecipeJson.h.in "${FALCON_GENERATED_DIR}/CraftingRecipeJson.h" @ONLY)

file(READ "${FALCON_BIOME_DEFINITIONS_NBT}" FALCON_BIOME_DEFINITIONS_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FALCON_BIOME_DEFINITIONS_BYTES "${FALCON_BIOME_DEFINITIONS_HEX}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n"
        FALCON_BIOME_DEFINITIONS_BYTES "${FALCON_BIOME_DEFINITIONS_BYTES}")
configure_file(cmake/BiomeDefinitionsNbt.h.in "${FALCON_GENERATED_DIR}/BiomeDefinitionsNbt.h" @ONLY)

file(READ "${FALCON_BLOCK_PALETTE_NBT}" FALCON_BLOCK_PALETTE_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FALCON_BLOCK_PALETTE_BYTES "${FALCON_BLOCK_PALETTE_HEX}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n"
        FALCON_BLOCK_PALETTE_BYTES "${FALCON_BLOCK_PALETTE_BYTES}")
configure_file(cmake/BlockPaletteNbt.h.in "${FALCON_GENERATED_DIR}/BlockPaletteNbt.h" @ONLY)

if(NOT FALCON_BUILD_VERSION)
    set(FALCON_BUILD_VERSION "1.26.50.01")
endif()
if(NOT FALCON_BUILD_ID)
    set(FALCON_BUILD_ID "0")
endif()
set(FALCON_BUILD_BRANCH "unknown")
set(FALCON_BUILD_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE FALCON_BUILD_ID
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE FALCON_BUILD_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE FALCON_BUILD_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(CMAKE_BUILD_TYPE)
    set(FALCON_BUILD_CONFIGURATION "${CMAKE_BUILD_TYPE}")
else()
    set(FALCON_BUILD_CONFIGURATION "Publish")
endif()
configure_file(cmake/BuildInfo.h.in "${FALCON_GENERATED_DIR}/BuildInfo.h" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${FALCON_RECIPES_JSON}" "${FALCON_ITEM_TAGS_JSON}" "${FALCON_BIOME_DEFINITIONS_NBT}"
        "${FALCON_BLOCK_PALETTE_NBT}")

file(GLOB_RECURSE FALCON_SERVER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(FalconServer ${FALCON_SERVER_SOURCES})

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

target_include_directories(FalconServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FALCON_GENERATED_DIR} ${ZLIB_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR})
target_link_libraries(FalconServer PRIVATE FalconNetwork FalconProtocol leveldb qjs LibDataChannel::LibDataChannelStatic)

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(FalconServer PRIVATE -static -fuse-ld=lld)
    target_link_libraries(FalconServer PRIVATE z ssl crypto ws2_32 crypt32)
else()
    target_link_libraries(FalconServer PRIVATE ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)
endif()
