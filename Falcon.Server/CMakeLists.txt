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
falcon_bedrock_data_embed_text(FILE loot_tables.json NAMESPACE FalconLootData SYMBOL kLootTablesJson
        DELIMITER falconloot OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/LootTablesJson.h")
falcon_bedrock_data_embed_text(FILE item_palette.json NAMESPACE FalconItemData SYMBOL kItemPaletteJson
        DELIMITER falconitems OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/ItemPaletteJson.h")
falcon_bedrock_data_embed_binary(FILE item_components.nbt NAMESPACE FalconItemData SYMBOL kItemComponentsNbt
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/ItemComponentsNbt.h")
falcon_bedrock_data_embed_binary(FILE block_definitions.nbt NAMESPACE FalconBlockDefinitionData
        SYMBOL kBlockDefinitionsNbt OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/BlockDefinitionsNbt.h")
falcon_bedrock_data_embed_text(FILE entity_loot_tables.json NAMESPACE FalconLootData SYMBOL kEntityLootTablesJson
        DELIMITER falconentityloot OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/EntityLootTablesJson.h")
falcon_bedrock_data_embed_text(FILE r16_to_current_item_map.json NAMESPACE FalconLegacyItemData
        SYMBOL kLegacyItemMapJson DELIMITER falconlegacy
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/LegacyItemMapJson.h")

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

file(GLOB FALCON_LANGUAGE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Languages/*.lang")
list(SORT FALCON_LANGUAGE_SOURCES)
set(FALCON_LANGUAGE_FILES "")
foreach(languageFile IN LISTS FALCON_LANGUAGE_SOURCES)
    get_filename_component(languageLocale "${languageFile}" NAME_WE)
    file(READ "${languageFile}" languageContent)
    string(APPEND FALCON_LANGUAGE_FILES "        {\"${languageLocale}\", R\"falconlang(${languageContent})falconlang\"},\n")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FALCON_LANGUAGE_SOURCES})
configure_file(cmake/LanguageFiles.h.in "${FALCON_GENERATED_DIR}/LanguageFiles.h" @ONLY)

if(NOT FALCON_BUILD_VERSION)
    set(FALCON_BUILD_VERSION "1.26.51.01")
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
string(MAKE_C_IDENTIFIER "falcon_build_${FALCON_BUILD_COMMIT}_${FALCON_BUILD_ID}" FALCON_BUILD_GUARD)
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
target_link_libraries(FalconServer PRIVATE FalconNetwork FalconProtocol FalconBlockStateUpdater leveldb qjs LibDataChannel::LibDataChannelStatic
        Falcon::PluginAPI ${CMAKE_DL_LIBS})

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(FalconServer PRIVATE -static -fuse-ld=lld)
    target_link_libraries(FalconServer PRIVATE z ssl crypto ws2_32 crypt32)
else()
    target_link_libraries(FalconServer PRIVATE ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)
endif()

option(FALCON_EXPORT_SYMBOLS "Export the server symbols so internal plugins can link against the executable" ON)

if(FALCON_EXPORT_SYMBOLS)
    set_target_properties(FalconServer PROPERTIES ENABLE_EXPORTS ON)

    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(FALCON_EXPORT_INPUTS "${CMAKE_CURRENT_BINARY_DIR}/FalconServerExports.txt")
        set(FALCON_EXPORT_DEFINITION "${CMAKE_CURRENT_BINARY_DIR}/FalconServer.def")
        set(FALCON_EXPORT_SOURCES "$<TARGET_OBJECTS:FalconServer>" "$<TARGET_FILE:FalconProtocol>"
                "$<TARGET_FILE:FalconNetwork>" "$<TARGET_FILE:FalconNBT>" "$<TARGET_FILE:FalconBlockStateUpdater>")
        file(GENERATE OUTPUT "${FALCON_EXPORT_INPUTS}" CONTENT "$<JOIN:${FALCON_EXPORT_SOURCES},\n>\n")
        add_custom_command(TARGET FalconServer PRE_LINK
                COMMAND "${CMAKE_COMMAND}" "-DFALCON_NM=${CMAKE_NM}" "-DFALCON_EXPORT_INPUTS=${FALCON_EXPORT_INPUTS}"
                "-DFALCON_EXPORT_DEFINITION=${FALCON_EXPORT_DEFINITION}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FalconExports.cmake"
                VERBATIM)
        target_link_options(FalconServer PRIVATE "${FALCON_EXPORT_DEFINITION}")
    endif()

    string(CONCAT FALCON_INTERNAL_SDK
            "set(FALCON_INTERNAL_EXECUTABLE \"$<TARGET_FILE:FalconServer>\")\n"
            "set(FALCON_INTERNAL_LINKER_FILE \"$<TARGET_LINKER_FILE:FalconServer>\")\n"
            "set(FALCON_INTERNAL_INCLUDE_DIRECTORIES \"$<TARGET_PROPERTY:FalconServer,INCLUDE_DIRECTORIES>\")\n"
            "set(FALCON_INTERNAL_COMPILE_DEFINITIONS \"$<TARGET_PROPERTY:FalconServer,COMPILE_DEFINITIONS>\")\n"
            "set(FALCON_INTERNAL_CXX_COMPILER_ID \"${CMAKE_CXX_COMPILER_ID}\")\n"
            "set(FALCON_INTERNAL_CXX_COMPILER_VERSION \"${CMAKE_CXX_COMPILER_VERSION}\")\n")
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/FalconInternalSDK.cmake" CONTENT "${FALCON_INTERNAL_SDK}")
endif()
