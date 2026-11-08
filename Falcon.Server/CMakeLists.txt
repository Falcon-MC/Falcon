cmake_minimum_required(VERSION 3.16)
project(Falcon.Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FALCON_BEDROCK_DATA_PROTOCOL 2225)
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
falcon_bedrock_data_embed_binary(FILE structures.pack NAMESPACE FalconStructureData SYMBOL kStructuresPack
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/StructuresPack.h")
set(FALCON_OLDER_DATA_PROTOCOLS 2169)
set(FALCON_OLDER_DATA_INCLUDES "")
set(FALCON_OLDER_DATA_ENTRIES "")
foreach(olderProtocol IN LISTS FALCON_OLDER_DATA_PROTOCOLS)
    set(FALCON_BEDROCK_DATA_PROTOCOL ${olderProtocol})
    set(olderDir "${CMAKE_CURRENT_BINARY_DIR}/generated/older/${olderProtocol}")
    set(olderNamespace "FalconOlderData${olderProtocol}")
    falcon_bedrock_data_embed_text(FILE item_palette.json NAMESPACE ${olderNamespace} SYMBOL kItemPaletteJson
            DELIMITER falconitems OUTPUT "${olderDir}/ItemPaletteJson.h")
    falcon_bedrock_data_embed_binary(FILE item_components.nbt NAMESPACE ${olderNamespace} SYMBOL kItemComponentsNbt
            OUTPUT "${olderDir}/ItemComponentsNbt.h")
    falcon_bedrock_data_embed_text(FILE recipes.json NAMESPACE ${olderNamespace} SYMBOL kRecipesJson
            DELIMITER falconrecipes OUTPUT "${olderDir}/RecipesJson.h")
    falcon_bedrock_data_embed_text(FILE creative_items.json NAMESPACE ${olderNamespace} SYMBOL kCreativeItemsJson
            DELIMITER falconcreative OUTPUT "${olderDir}/CreativeItemsJson.h")
    falcon_bedrock_data_embed_binary(FILE block_palette.nbt NAMESPACE ${olderNamespace} SYMBOL kBlockPaletteNbt
            OUTPUT "${olderDir}/BlockPaletteNbt.h")
    foreach(header ItemPaletteJson ItemComponentsNbt RecipesJson CreativeItemsJson BlockPaletteNbt)
        string(APPEND FALCON_OLDER_DATA_INCLUDES "#include \"older/${olderProtocol}/${header}.h\"\n")
    endforeach()
    string(APPEND FALCON_OLDER_DATA_ENTRIES
            "        {${olderProtocol}, ${olderNamespace}::kItemPaletteJson, ${olderNamespace}::kItemComponentsNbt, "
            "${olderNamespace}::kItemComponentsNbtSize, ${olderNamespace}::kRecipesJson, "
            "${olderNamespace}::kCreativeItemsJson, ${olderNamespace}::kBlockPaletteNbt, "
            "${olderNamespace}::kBlockPaletteNbtSize},\n")
endforeach()
set(FALCON_BEDROCK_DATA_PROTOCOL 2225)
configure_file(cmake/OlderProtocolData.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/OlderProtocolData.h" @ONLY)
falcon_bedrock_data_embed_binary(FILE block_definitions.nbt NAMESPACE FalconBlockDefinitionData
        SYMBOL kBlockDefinitionsNbt OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/BlockDefinitionsNbt.h")
falcon_bedrock_data_embed_text(FILE entity_loot_tables.json NAMESPACE FalconLootData SYMBOL kEntityLootTablesJson
        DELIMITER falconentityloot OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/EntityLootTablesJson.h")
falcon_bedrock_data_embed_text(FILE spawn_rules.json NAMESPACE FalconSpawnData SYMBOL kSpawnRulesJson
        DELIMITER falconspawn OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/SpawnRulesJson.h")
falcon_bedrock_data_embed_text(FILE entity_despawn.json NAMESPACE FalconSpawnData SYMBOL kEntityDespawnJson
        DELIMITER falcondespawn OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/EntityDespawnJson.h")
falcon_bedrock_data_embed_text(FILE entity_definitions.json NAMESPACE FalconEntityData
        SYMBOL kEntityDefinitionsJson DELIMITER falconentities
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/EntityDefinitionsJson.h")
falcon_bedrock_data_embed_text(FILE command_translations.json NAMESPACE FalconLanguageData
        SYMBOL kCommandTranslationsJson DELIMITER falcontexts
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/CommandTranslationsJson.h")
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
    set(FALCON_BUILD_VERSION "1.26.60.01")
endif()
if(NOT FALCON_BUILD_ID)
    set(FALCON_BUILD_ID "0")
endif()
set(FALCON_BUILD_BRANCH "unknown")
set(FALCON_BUILD_COMMIT "unknown")
find_package(Git QUIET)
get_filename_component(FALCON_REPOSITORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(GIT_FOUND AND EXISTS "${FALCON_REPOSITORY_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
            WORKING_DIRECTORY "${FALCON_REPOSITORY_DIR}" OUTPUT_VARIABLE FALCON_BUILD_ID
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY "${FALCON_REPOSITORY_DIR}" OUTPUT_VARIABLE FALCON_BUILD_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${FALCON_REPOSITORY_DIR}" OUTPUT_VARIABLE FALCON_BUILD_COMMIT
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

file(GLOB_RECURSE FALCON_CORE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(FALCON_EXECUTABLE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Server/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Server/SetupWizard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Server/VersionChecker.cpp")
list(REMOVE_ITEM FALCON_CORE_SOURCES ${FALCON_EXECUTABLE_SOURCES})

# An object library rather than a static one: blocks, items and actors register themselves from static
# initialisers, and a static library would let the linker drop every object nothing references by name.
add_library(FalconCore OBJECT ${FALCON_CORE_SOURCES})

# no fused multiply-adds anywhere in generation: the game rounds every float step, fma changes the terrain
file(GLOB_RECURSE FALCON_LEVELGEN_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/World/Level/LevelGen/*.cpp")
set_source_files_properties(${FALCON_LEVELGEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
# map shading compares quantised colours, so its float steps must round like the game's
file(GLOB_RECURSE FALCON_MAP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/World/Level/Map/*.cpp")
set_source_files_properties(${FALCON_MAP_SOURCES} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")

include(cmake/FalconLegacyHeaders.cmake)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

target_include_directories(FalconCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FALCON_GENERATED_DIR} ${FALCON_LEGACY_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR})
target_include_directories(FalconCore SYSTEM PRIVATE ${stb_image_SOURCE_DIR})
target_link_libraries(FalconCore PUBLIC FalconNetwork FalconProtocol FalconBlockStateUpdater leveldb qjs LibDataChannel::LibDataChannelStatic
        Falcon::PluginAPI wasmtime ${CMAKE_DL_LIBS})

if(WIN32)
    target_compile_definitions(FalconCore PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(FalconCore PUBLIC z ssl crypto ws2_32 crypt32)
else()
    target_link_libraries(FalconCore PUBLIC ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)
endif()

option(FALCON_BUILD_SERVER_EXECUTABLE "Build the dedicated server executable" ON)

option(FALCON_BUILD_EMBEDDED_TESTS "Build embedded server lifecycle checks" OFF)
if(FALCON_BUILD_EMBEDDED_TESTS)
    enable_testing()
    add_executable(FalconEmbeddedTests tests/EmbeddedServerTests.cpp)
    target_link_libraries(FalconEmbeddedTests PRIVATE FalconCore)
    add_executable(FalconStartupTests tests/StartupTests.cpp)
    target_link_libraries(FalconStartupTests PRIVATE FalconCore)
    add_executable(FalconMovementInputTests tests/MovementInputTests.cpp)
    target_link_libraries(FalconMovementInputTests PRIVATE FalconCore)
    add_executable(FalconBlockBreakingTests tests/BlockBreakingTests.cpp)
    target_link_libraries(FalconBlockBreakingTests PRIVATE FalconCore)
    add_executable(FalconRideControlTests tests/RideControlTests.cpp)
    target_link_libraries(FalconRideControlTests PRIVATE FalconCore)
    add_executable(FalconGameplay2660Tests tests/Gameplay2660Tests.cpp)
    target_link_libraries(FalconGameplay2660Tests PRIVATE FalconCore)
    add_executable(FalconAnvilTests tests/AnvilTests.cpp)
    target_link_libraries(FalconAnvilTests PRIVATE FalconCore)
    add_executable(FalconDragonFightTests tests/DragonFightTests.cpp)
    target_link_libraries(FalconDragonFightTests PRIVATE FalconCore)
    add_executable(FalconSulfurCavesTests tests/SulfurCavesTests.cpp)
    target_link_libraries(FalconSulfurCavesTests PRIVATE FalconCore)
    add_executable(FalconEndGatewayTests tests/EndGatewayTests.cpp)
    target_link_libraries(FalconEndGatewayTests PRIVATE FalconCore)
    add_executable(FalconScriptApiTests tests/ScriptApiTests.cpp)
    target_link_libraries(FalconScriptApiTests PRIVATE FalconCore)
    add_executable(FalconHostileMeleeTests tests/HostileMeleeTests.cpp)
    target_link_libraries(FalconHostileMeleeTests PRIVATE FalconCore)
    add_executable(FalconRangedMobTests tests/RangedMobTests.cpp)
    target_link_libraries(FalconRangedMobTests PRIVATE FalconCore)
    add_executable(FalconStructureTests tests/StructureTests.cpp)
    target_link_libraries(FalconStructureTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconMovementInputTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconBlockBreakingTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconRideControlTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconEmbeddedTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconStartupTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconGameplay2660Tests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconAnvilTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconDragonFightTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconSulfurCavesTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconEndGatewayTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconScriptApiTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconHostileMeleeTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconRangedMobTests PRIVATE -static -fuse-ld=lld)
        target_link_options(FalconStructureTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME movement-input-timeline COMMAND FalconMovementInputTests)
    set_tests_properties(movement-input-timeline PROPERTIES TIMEOUT 60)
    add_test(NAME block-breaking-settings COMMAND FalconBlockBreakingTests)
    set_tests_properties(block-breaking-settings PROPERTIES TIMEOUT 60)
    add_test(NAME ride-control-regressions COMMAND FalconRideControlTests)
    set_tests_properties(ride-control-regressions PROPERTIES TIMEOUT 60)
    add_test(NAME embedded-lifecycle COMMAND FalconEmbeddedTests)
    set_tests_properties(embedded-lifecycle PROPERTIES TIMEOUT 120)
    add_test(NAME startup-persistence COMMAND FalconStartupTests)
    set_tests_properties(startup-persistence PROPERTIES TIMEOUT 60)
    add_test(NAME gameplay-26.60 COMMAND FalconGameplay2660Tests)
    set_tests_properties(gameplay-26.60 PROPERTIES TIMEOUT 60)
    add_test(NAME anvil-repair COMMAND FalconAnvilTests)
    set_tests_properties(anvil-repair PROPERTIES TIMEOUT 60)
    add_test(NAME hostile-melee COMMAND FalconHostileMeleeTests)
    set_tests_properties(hostile-melee PROPERTIES TIMEOUT 60)
    add_test(NAME ranged-mobs COMMAND FalconRangedMobTests)
    set_tests_properties(ranged-mobs PROPERTIES TIMEOUT 60)
    add_executable(FalconMapTests tests/MapTests.cpp)
    target_link_libraries(FalconMapTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconMapTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME maps COMMAND FalconMapTests)
    set_tests_properties(maps PROPERTIES TIMEOUT 60)
    add_executable(FalconImageMapTests tests/ImageMapTests.cpp)
    target_link_libraries(FalconImageMapTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconImageMapTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME image-maps COMMAND FalconImageMapTests)
    set_tests_properties(image-maps PROPERTIES TIMEOUT 60)
    add_test(NAME dragon-fight COMMAND FalconDragonFightTests)
    set_tests_properties(dragon-fight PROPERTIES TIMEOUT 60)
    add_test(NAME sulfur-caves-generation COMMAND FalconSulfurCavesTests)
    set_tests_properties(sulfur-caves-generation PROPERTIES TIMEOUT 60)
    add_test(NAME end-gateway COMMAND FalconEndGatewayTests)
    set_tests_properties(end-gateway PROPERTIES TIMEOUT 60)
    add_test(NAME script-api-foundation COMMAND FalconScriptApiTests)
    set_tests_properties(script-api-foundation PROPERTIES TIMEOUT 60)
    add_executable(FalconClientSideGenerationTests tests/ClientSideGenerationTests.cpp)
    target_link_libraries(FalconClientSideGenerationTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconClientSideGenerationTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME client-side-generation COMMAND FalconClientSideGenerationTests)
    set_tests_properties(client-side-generation PROPERTIES TIMEOUT 120)
    add_executable(FalconTreeGenerationTests tests/TreeGenerationTests.cpp)
    target_link_libraries(FalconTreeGenerationTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconTreeGenerationTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME tree-generation COMMAND FalconTreeGenerationTests)
    set_tests_properties(tree-generation PROPERTIES TIMEOUT 60)
    add_test(NAME structure-generation COMMAND FalconStructureTests)
    set_tests_properties(structure-generation PROPERTIES TIMEOUT 120)
    add_executable(FalconOptimizationTests tests/OptimizationTests.cpp)
    target_link_libraries(FalconOptimizationTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconOptimizationTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME optimization-regressions COMMAND FalconOptimizationTests)
    set_tests_properties(optimization-regressions PROPERTIES TIMEOUT 120)
    add_executable(FalconDecorationOrderTests tests/DecorationOrderTests.cpp)
    target_link_libraries(FalconDecorationOrderTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconDecorationOrderTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME decoration-order COMMAND FalconDecorationOrderTests)
    set_tests_properties(decoration-order PROPERTIES TIMEOUT 120)
    add_executable(FalconBlockTickingQueueTests tests/BlockTickingQueueTests.cpp)
    target_link_libraries(FalconBlockTickingQueueTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconBlockTickingQueueTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME block-ticking-queue COMMAND FalconBlockTickingQueueTests)
    set_tests_properties(block-ticking-queue PROPERTIES TIMEOUT 60)
    add_executable(FalconSignalingPropertiesTests tests/SignalingPropertiesTests.cpp)
    target_link_libraries(FalconSignalingPropertiesTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconSignalingPropertiesTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME signaling-properties COMMAND FalconSignalingPropertiesTests)
    set_tests_properties(signaling-properties PROPERTIES TIMEOUT 60)
    add_executable(FalconFluidParityTests tests/FluidParityTests.cpp)
    target_link_libraries(FalconFluidParityTests PRIVATE FalconCore)
    target_compile_definitions(FalconFluidParityTests PRIVATE
            FALCON_FLUID_TRACE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fluids")
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconFluidParityTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME fluid-parity COMMAND FalconFluidParityTests
            platform drain stairs twosrc twosrcair hole hole2 holefar waterfall ledge still placeflow
            lavaplat lavawater basalt drops waterlog waterlogout waterlogslab sourceabove netherlava
            ~sponge@41 concrete lavafall waterfalllava)
    set_tests_properties(fluid-parity PROPERTIES TIMEOUT 300)
    add_executable(FalconFastJoinTests tests/FastJoinTests.cpp)
    target_link_libraries(FalconFastJoinTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconFastJoinTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME fast-join COMMAND FalconFastJoinTests)
    set_tests_properties(fast-join PROPERTIES TIMEOUT 60)
    add_executable(FalconActorVisibilityTests tests/ActorVisibilityTests.cpp)
    target_link_libraries(FalconActorVisibilityTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconActorVisibilityTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME actor-visibility COMMAND FalconActorVisibilityTests)
    set_tests_properties(actor-visibility PROPERTIES TIMEOUT 60)
    add_executable(FalconSnowLayerCollisionTests tests/SnowLayerCollisionTests.cpp)
    target_link_libraries(FalconSnowLayerCollisionTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconSnowLayerCollisionTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME snow-layer-collision COMMAND FalconSnowLayerCollisionTests)
    set_tests_properties(snow-layer-collision PROPERTIES TIMEOUT 60)
    add_executable(FalconSulfurCubeTests tests/SulfurCubeTests.cpp)
    target_link_libraries(FalconSulfurCubeTests PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconSulfurCubeTests PRIVATE -static -fuse-ld=lld)
    endif()
    add_test(NAME sulfur-cube COMMAND FalconSulfurCubeTests)
    set_tests_properties(sulfur-cube PROPERTIES TIMEOUT 120)
endif()

option(FALCON_BUILD_WORLDGEN_PARITY "Build the tool comparing generated terrain with a world saved by BDS" OFF)
if(FALCON_BUILD_WORLDGEN_PARITY)
    add_executable(FalconWorldGenParity tools/WorldGenParity.cpp)
    target_link_libraries(FalconWorldGenParity PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconWorldGenParity PRIVATE -static -fuse-ld=lld)
    endif()
endif()

if(NOT FALCON_BUILD_SERVER_EXECUTABLE)
    return()
endif()

add_executable(FalconServer ${FALCON_EXECUTABLE_SOURCES})
target_link_libraries(FalconServer PRIVATE FalconCore)

if(WIN32)
    target_compile_definitions(FalconServer PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(FalconServer PRIVATE -static -fuse-ld=lld)
endif()

option(FALCON_EXPORT_SYMBOLS "Export the server symbols so internal plugins can link against the executable" ON)

if(FALCON_EXPORT_SYMBOLS)
    set_target_properties(FalconServer PROPERTIES ENABLE_EXPORTS ON)

    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(FALCON_EXPORT_INPUTS "${CMAKE_CURRENT_BINARY_DIR}/FalconServerExports.txt")
        set(FALCON_EXPORT_DEFINITION "${CMAKE_CURRENT_BINARY_DIR}/FalconServer.def")
        set(FALCON_EXPORT_SOURCES "$<TARGET_OBJECTS:FalconServer>" "$<TARGET_OBJECTS:FalconCore>"
                "$<TARGET_FILE:FalconProtocol>"
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
            "set(FALCON_INTERNAL_INCLUDE_DIRECTORIES \"$<TARGET_PROPERTY:FalconCore,INCLUDE_DIRECTORIES>\")\n"
            "set(FALCON_INTERNAL_COMPILE_DEFINITIONS \"$<TARGET_PROPERTY:FalconCore,COMPILE_DEFINITIONS>\")\n"
            "set(FALCON_INTERNAL_CXX_COMPILER_ID \"${CMAKE_CXX_COMPILER_ID}\")\n"
            "set(FALCON_INTERNAL_CXX_COMPILER_VERSION \"${CMAKE_CXX_COMPILER_VERSION}\")\n")
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/FalconInternalSDK.cmake" CONTENT "${FALCON_INTERNAL_SDK}")
endif()
