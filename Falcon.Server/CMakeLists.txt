cmake_minimum_required(VERSION 3.16)
project(Falcon.Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FALCON_BEDROCK_DATA_PROTOCOL 2193)
include("${falcon_bedrock_data_SOURCE_DIR}/cmake/FalconBedrockData.cmake")

falcon_bedrock_data_file(recipes.json FALCON_RECIPES_JSON)
falcon_bedrock_data_file(item_tags.json FALCON_ITEM_TAGS_JSON)
falcon_bedrock_data_file(biome_definitions.nbt FALCON_BIOME_DEFINITIONS_NBT)
falcon_bedrock_data_file(block_palette.nbt FALCON_BLOCK_PALETTE_NBT)
falcon_bedrock_data_file(voxel_shapes.json FALCON_VOXEL_SHAPES_JSON)
falcon_bedrock_data_file(creative_items.json FALCON_CREATIVE_ITEMS_JSON)

file(READ "${FALCON_RECIPES_JSON}" FALCON_RECIPES_JSON_CONTENT)
file(READ "${FALCON_ITEM_TAGS_JSON}" FALCON_ITEM_TAGS_JSON_CONTENT)
set(FALCON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FALCON_GENERATED_DIR}")
configure_file(cmake/CraftingRecipeJson.h.in "${FALCON_GENERATED_DIR}/CraftingRecipeJson.h" @ONLY)

file(READ "${FALCON_VOXEL_SHAPES_JSON}" FALCON_VOXEL_SHAPES_JSON_CONTENT)
configure_file(cmake/VoxelShapesJson.h.in "${FALCON_GENERATED_DIR}/VoxelShapesJson.h" @ONLY)

file(READ "${FALCON_CREATIVE_ITEMS_JSON}" FALCON_CREATIVE_ITEMS_JSON_CONTENT)
configure_file(cmake/CreativeItemsJson.h.in "${FALCON_GENERATED_DIR}/CreativeItemsJson.h" @ONLY)

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

file(GLOB FALCON_BLOCK_UPGRADE_SCHEMA_FILES "${FALCON_BLOCK_UPGRADE_SCHEMA_DIR}/*.json")
list(SORT FALCON_BLOCK_UPGRADE_SCHEMA_FILES)
set(FALCON_BLOCK_UPGRADE_SCHEMAS "")
foreach(schemaFile IN LISTS FALCON_BLOCK_UPGRADE_SCHEMA_FILES)
    get_filename_component(schemaName "${schemaFile}" NAME)
    string(REGEX MATCH "^[0-9]+" schemaId "${schemaName}")
    if(schemaId STREQUAL "")
        continue()
    endif()
    math(EXPR schemaId "${schemaId}")
    file(READ "${schemaFile}" schemaContent)
    string(APPEND FALCON_BLOCK_UPGRADE_SCHEMAS "        {${schemaId}, R\"falconschema(${schemaContent})falconschema\"},\n")
endforeach()
configure_file(cmake/BlockUpgradeSchemas.h.in "${FALCON_GENERATED_DIR}/BlockUpgradeSchemas.h" @ONLY)

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
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FALCON_BLOCK_UPGRADE_SCHEMA_FILES})

file(GLOB_RECURSE FALCON_SERVER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(FalconServer ${FALCON_SERVER_SOURCES})

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

target_include_directories(FalconServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FALCON_GENERATED_DIR} ${ZLIB_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR})
target_link_libraries(FalconServer PRIVATE FalconNetwork FalconProtocol FalconBlockStateUpdater leveldb qjs LibDataChannel::LibDataChannelStatic)

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(FalconServer PRIVATE -static -fuse-ld=lld)
    target_link_libraries(FalconServer PRIVATE z ssl crypto ws2_32 crypt32)
else()
    target_link_libraries(FalconServer PRIVATE ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)
endif()
