cmake_minimum_required(VERSION 3.16)
project(Falcon.Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FALCON_BEDROCK_DATA_PROTOCOL 2225)
include("${falcon_bedrock_data_SOURCE_DIR}/cmake/FalconBedrockData.cmake")

falcon_bedrock_data_file(recipes.json FALCON_RECIPES_JSON)
falcon_bedrock_data_file(item_tags.json FALCON_ITEM_TAGS_JSON)
falcon_bedrock_data_file(biome_definitions.nbt FALCON_BIOME_DEFINITIONS_NBT)
falcon_bedrock_data_file(block_palette.nbt FALCON_BLOCK_PALETTE_NBT)
falcon_bedrock_data_file(voxel_shapes.json FALCON_VOXEL_SHAPES_JSON)
falcon_bedrock_data_file(block_collision_shapes.json FALCON_BLOCK_COLLISION_SHAPES_JSON)
falcon_bedrock_data_file(creative_items.json FALCON_CREATIVE_ITEMS_JSON)
falcon_bedrock_data_embed_text(FILE block_tags.json NAMESPACE FalconBlockTagsData SYMBOL kBlockTagsJson
        DELIMITER falcontags OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/BlockTagsJson.h")
falcon_bedrock_data_embed_text(FILE block_solid.json NAMESPACE FalconBlockSolidData SYMBOL kBlockSolidJson
        DELIMITER falconsolid OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/BlockSolidJson.h")
falcon_bedrock_data_embed_text(FILE aim_assist.json NAMESPACE FalconAimAssistData SYMBOL kAimAssistJson
        DELIMITER falconaim OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/AimAssistJson.h")
falcon_bedrock_data_embed_text(FILE command_hidden_items.json NAMESPACE FalconCommandData SYMBOL kCommandHiddenItemsJson
        DELIMITER falconitems OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/CommandHiddenItemsJson.h")
falcon_bedrock_data_embed_text(FILE command_hidden_blocks.json NAMESPACE FalconCommandData SYMBOL kCommandHiddenBlocksJson
        DELIMITER falconblocks OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/CommandHiddenBlocksJson.h")
falcon_bedrock_data_embed_text(FILE loot_tables.json NAMESPACE FalconLootData SYMBOL kLootTablesJson
        DELIMITER falconloot OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/LootTablesJson.h")
falcon_bedrock_data_embed_text(FILE trade_tables.json NAMESPACE FalconTradeData SYMBOL kTradeTablesJson
        DELIMITER falcontrade OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/TradeTablesJson.h")
falcon_bedrock_data_embed_text(FILE worldgen.json NAMESPACE FalconWorldgenData SYMBOL kWorldgenJson
        DELIMITER falconworldgen OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/WorldgenJson.h")
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

file(READ "${FALCON_BLOCK_COLLISION_SHAPES_JSON}" FALCON_BLOCK_COLLISION_SHAPES_JSON_CONTENT)
configure_file(cmake/BlockCollisionShapesJson.h.in "${FALCON_GENERATED_DIR}/BlockCollisionShapesJson.h" @ONLY)

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
# built as its own library further down: the transport library links against it
set(FALCON_TRANSPORT_TRANSCRIPT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Network/TransportTranscript.cpp")
list(REMOVE_ITEM FALCON_CORE_SOURCES ${FALCON_TRANSPORT_TRANSCRIPT_SOURCE})

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

# the transport library keeps its secure contexts to itself: it is built to pass them through this
# small library, which keeps what a peer sent while its session was set up
add_library(FalconTransportTranscript STATIC ${FALCON_TRANSPORT_TRANSCRIPT_SOURCE})
target_compile_features(FalconTransportTranscript PRIVATE cxx_std_20)
target_include_directories(FalconTransportTranscript PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OPENSSL_INCLUDE_DIR})
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(FalconTransportTranscript PUBLIC ssl crypto)
else()
    target_link_libraries(FalconTransportTranscript PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()
target_compile_definitions(datachannel-static PRIVATE SSL_CTX_set_info_callback=falcon_transport_install)
target_link_libraries(datachannel-static PUBLIC FalconTransportTranscript)
target_link_libraries(FalconCore PUBLIC FalconTransportTranscript)

option(FALCON_BUILD_SERVER_EXECUTABLE "Build the dedicated server executable" ON)

option(FALCON_BUILD_EMBEDDED_TESTS "Build embedded server lifecycle checks" OFF)
if(FALCON_BUILD_EMBEDDED_TESTS)
    enable_testing()
    # one shared copy of the server for every test: linking the objects into each of them made about
    # 150 executables of 60 MB, 10 GB of build folder for the same code
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # not on mingw: a dll of the whole server passes the 65535 exports of a pe file, and the -static
        # tests would each carry a C++ runtime of their own beside the dll's
        add_library(FalconTestCore INTERFACE)
        target_sources(FalconTestCore INTERFACE $<TARGET_OBJECTS:FalconCore>)
        target_link_libraries(FalconTestCore INTERFACE FalconCore)
    else()
        add_library(FalconTestCore SHARED)
        target_link_libraries(FalconTestCore PUBLIC FalconCore)
    endif()
    # every test gets a timeout: a server that never stops would hold the ci job until its own limit
    function(falcon_register_test name target)
        cmake_parse_arguments(PARSE_ARGV 2 FALCON_TEST "" "TIMEOUT" "ARGS")
        if(FALCON_TEST_UNPARSED_ARGUMENTS)
            message(FATAL_ERROR "falcon_register_test(${name}): unknown arguments ${FALCON_TEST_UNPARSED_ARGUMENTS}")
        endif()
        if(NOT FALCON_TEST_TIMEOUT)
            set(FALCON_TEST_TIMEOUT 60)
        endif()
        add_test(NAME ${name} COMMAND ${target} ${FALCON_TEST_ARGS})
        set_tests_properties(${name} PROPERTIES TIMEOUT ${FALCON_TEST_TIMEOUT})
    endfunction()
    function(falcon_add_test name target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE FalconTestCore)
        if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(${target} PRIVATE -static -fuse-ld=lld)
        endif()
        falcon_register_test(${name} ${target} ${ARGN})
    endfunction()
    falcon_add_test(embedded-lifecycle FalconEmbeddedTests tests/EmbeddedServerTests.cpp TIMEOUT 120)
    falcon_add_test(startup-persistence FalconStartupTests tests/StartupTests.cpp)
    falcon_add_test(movement-input-timeline FalconMovementInputTests tests/MovementInputTests.cpp)
    falcon_add_test(block-breaking-settings FalconBlockBreakingTests tests/BlockBreakingTests.cpp)
    falcon_add_test(ride-control-regressions FalconRideControlTests tests/RideControlTests.cpp)
    falcon_add_test(gameplay-26.60 FalconGameplay2660Tests tests/Gameplay2660Tests.cpp)
    falcon_add_test(item-pickup-partial FalconItemPickupTests tests/ItemPickupTests.cpp)
    falcon_add_test(item-stack-sizes FalconItemStackSizeTests tests/ItemStackSizeTests.cpp)
    falcon_add_test(item-cooldowns FalconItemCooldownTests tests/ItemCooldownTests.cpp)
    falcon_add_test(boats FalconBoatTests tests/BoatTests.cpp)
    falcon_add_test(minecarts FalconMinecartTests tests/MinecartTests.cpp)
    falcon_add_test(item-durability FalconItemDurabilityTests tests/ItemDurabilityTests.cpp)
    falcon_add_test(anvil-repair FalconAnvilTests tests/AnvilTests.cpp)
    falcon_add_test(dragon-fight FalconDragonFightTests tests/DragonFightTests.cpp)
    falcon_add_test(sulfur-caves-generation FalconSulfurCavesTests tests/SulfurCavesTests.cpp)
    falcon_add_test(end-gateway FalconEndGatewayTests tests/EndGatewayTests.cpp)
    falcon_add_test(script-api-foundation FalconScriptApiTests tests/ScriptApiTests.cpp)
    falcon_add_test(script-api-blocks FalconScriptBlockTests tests/ScriptBlockTests.cpp)
    falcon_add_test(script-api-debug FalconScriptDebugTests tests/ScriptDebugTests.cpp)
    falcon_add_test(script-api-diagnostics FalconScriptDiagnosticsTests tests/ScriptDiagnosticsTests.cpp)
    falcon_add_test(script-api-gametest FalconScriptGameTestTests tests/ScriptGameTestTests.cpp)
    falcon_add_test(script-api-beta FalconScriptBetaTests tests/ScriptBetaTests.cpp)
    falcon_add_test(script-api-npc FalconScriptNpcTests tests/ScriptNpcTests.cpp)
    falcon_add_test(npc-dialogues FalconNpcDialogueTests tests/NpcDialogueTests.cpp)
    falcon_add_test(script-api-poi FalconScriptPoiTests tests/ScriptPoiTests.cpp)
    falcon_add_test(script-api-block-beta FalconScriptBlockBetaTests tests/ScriptBlockBetaTests.cpp)
    falcon_add_test(script-api-block-recipe FalconScriptBlockRecipeTests tests/ScriptBlockRecipeTests.cpp)
    falcon_add_test(script-api-recipe-crafting FalconScriptRecipeCraftingTests tests/ScriptRecipeCraftingTests.cpp)
    falcon_add_test(script-api-block-storage FalconScriptBlockStorageTests tests/ScriptBlockStorageTests.cpp)
    falcon_add_test(script-api-event-filters FalconScriptEventFilterTests tests/ScriptEventFilterTests.cpp)
    falcon_add_test(script-api-container-views FalconScriptContainerViewTests tests/ScriptContainerViewTests.cpp)
    falcon_add_test(script-api-callback-lifetime FalconScriptCallbackLifetimeTests tests/ScriptCallbackLifetimeTests.cpp)
    falcon_add_test(script-api-watchdog FalconScriptWatchdogTests tests/ScriptWatchdogTests.cpp)
    falcon_add_test(script-api-projectile FalconScriptProjectileTests tests/ScriptProjectileTests.cpp)
    falcon_add_test(script-api-aim-assist FalconScriptAimAssistTests tests/ScriptAimAssistTests.cpp)
    falcon_add_test(script-api-surface FalconScriptSurfaceTests tests/ScriptSurfaceTests.cpp TIMEOUT 300)
    falcon_add_test(scoreboard FalconScoreboardTests tests/ScoreboardTests.cpp)
    falcon_add_test(hostile-melee FalconHostileMeleeTests tests/HostileMeleeTests.cpp)
    falcon_add_test(spawn-egg-pick FalconSpawnEggPickTests tests/SpawnEggPickTests.cpp)
    falcon_add_test(zero-damage-hit FalconZeroDamageHitTests tests/ZeroDamageHitTests.cpp)
    falcon_add_test(player-attack-facing FalconPlayerAttackFacingTests tests/PlayerAttackFacingTests.cpp)
    falcon_add_test(resistance FalconResistanceTests tests/ResistanceTests.cpp)
    falcon_add_test(charge-attack FalconChargeAttackTests tests/ChargeAttackTests.cpp)
    falcon_add_test(patrol-goals FalconPatrolGoalTests tests/PatrolGoalTests.cpp)
    falcon_add_test(mob-components FalconMobComponentTests tests/MobComponentTests.cpp)
    falcon_add_test(drink-potion FalconDrinkPotionTests tests/DrinkPotionTests.cpp)
    falcon_add_test(cpu-count FalconCpuCountTests tests/CpuCountTests.cpp)
    falcon_add_test(iron-golem FalconIronGolemTests tests/IronGolemTests.cpp)
    falcon_add_test(mob-spawner FalconMobSpawnerTests tests/MobSpawnerTests.cpp)
    falcon_add_test(villager FalconVillagerTests tests/VillagerTests.cpp)
    falcon_add_test(villager-life FalconVillagerLifeTests tests/VillagerLifeTests.cpp)
    falcon_add_test(villager-breeding FalconVillagerBreedingTests tests/VillagerBreedingTests.cpp)
    falcon_add_test(villager-components FalconVillagerComponentTests tests/VillagerComponentTests.cpp)
    falcon_add_test(villager-interaction FalconVillagerInteractionTests tests/VillagerInteractionTests.cpp)
    falcon_add_test(trade FalconTradeTests tests/TradeTests.cpp)
    falcon_add_test(mount-inventory FalconMountInventoryTests tests/MountInventoryTests.cpp)
    falcon_add_test(hunger FalconHungerTests tests/HungerTests.cpp)
    falcon_add_test(difficulty-damage FalconDifficultyDamageTests tests/DifficultyDamageTests.cpp)
    falcon_add_test(mount-armor FalconMountArmorTests tests/MountArmorTests.cpp)
    falcon_add_test(taming FalconTamingTests tests/TamingTests.cpp)
    falcon_add_test(ocelot-sit-on-block FalconOcelotSitOnBlockTests tests/OcelotSitOnBlockTests.cpp)
    falcon_add_test(cat-gifts FalconCatGiftTests tests/CatGiftTests.cpp)
    falcon_add_test(ocelot-attack FalconOcelotAttackTests tests/OcelotAttackTests.cpp)
    falcon_add_test(sheep FalconSheepTests tests/SheepTests.cpp)
    falcon_add_test(ranged-mobs FalconRangedMobTests tests/RangedMobTests.cpp)
    falcon_add_test(structure-generation FalconStructureTests tests/StructureTests.cpp TIMEOUT 120)
    falcon_add_test(nether-fortress FalconNetherFortressTests tests/NetherFortressTests.cpp)
    falcon_add_test(behavior-pack-data FalconBehaviorPackDataTests tests/BehaviorPackDataTests.cpp)
    falcon_add_test(nether-fortress-spawns FalconNetherFortressSpawnTests tests/NetherFortressSpawnTests.cpp)
    falcon_add_test(monument FalconMonumentTests tests/MonumentTests.cpp)
    falcon_add_test(saved-data FalconSavedDataTests tests/SavedDataTests.cpp)
    falcon_add_test(stronghold-generation FalconStrongholdTests tests/StrongholdTests.cpp TIMEOUT 120)
    falcon_add_test(stonecutter FalconStonecutterTests tests/StonecutterTests.cpp)
    falcon_add_test(smithing FalconSmithingTests tests/SmithingTests.cpp)
    falcon_add_test(grindstone FalconGrindstoneTests tests/GrindstoneTests.cpp)
    falcon_add_test(loom FalconLoomTests tests/LoomTests.cpp)
    falcon_add_test(jukebox FalconJukeboxTests tests/JukeboxTests.cpp)
    falcon_add_test(note-block FalconNoteBlockTests tests/NoteBlockTests.cpp)
    falcon_add_test(recipe-unlocking FalconRecipeUnlockingTests tests/RecipeUnlockingTests.cpp)
    falcon_add_test(beacon FalconBeaconTests tests/BeaconTests.cpp)
    falcon_add_test(conduit FalconConduitTests tests/ConduitTests.cpp)
    falcon_add_test(pumpkin-carving FalconPumpkinCarvingTests tests/PumpkinCarvingTests.cpp)
    falcon_add_test(dripleaf FalconDripleafTests tests/DripleafTests.cpp)
    falcon_add_test(player-craft-recipe FalconPlayerCraftRecipeTests tests/PlayerCraftRecipeTests.cpp)
    falcon_add_test(ghast FalconGhastTests tests/GhastTests.cpp)
    falcon_add_test(zombified-piglins FalconZombifiedPiglinTests tests/ZombifiedPiglinTests.cpp)
    falcon_add_test(piglins FalconPiglinTests tests/PiglinTests.cpp)
    falcon_add_test(blazes FalconBlazeTests tests/BlazeTests.cpp)
    falcon_add_test(hoglins FalconHoglinTests tests/HoglinTests.cpp)
    falcon_add_test(wither-skeletons FalconWitherSkeletonTests tests/WitherSkeletonTests.cpp)
    falcon_add_test(striders FalconStriderTests tests/StriderTests.cpp)
    falcon_add_test(script-admin FalconScriptAdminTests tests/ScriptAdminTests.cpp)
    falcon_add_test(script-net FalconScriptNetTests tests/ScriptNetTests.cpp TIMEOUT 90)
    falcon_add_test(maps FalconMapTests tests/MapTests.cpp)
    falcon_add_test(account-binding FalconAccountBindingTests tests/AccountBindingTests.cpp)
    falcon_add_test(xp-command FalconXpCommandTests tests/XpCommandTests.cpp)
    falcon_add_test(shelf FalconShelfTests tests/ShelfTests.cpp)
    falcon_add_test(chiseled-bookshelf FalconChiseledBookshelfTests tests/ChiseledBookshelfTests.cpp)
    falcon_add_test(clone-command FalconCloneCommandTests tests/CloneCommandTests.cpp)
    falcon_add_test(functions FalconFunctionTests tests/FunctionTests.cpp)
    falcon_add_test(playanimation-command FalconPlayAnimationCommandTests tests/PlayAnimationCommandTests.cpp)
    falcon_add_test(command-list FalconCommandListTests tests/CommandListTests.cpp)
    falcon_add_test(execute-command FalconExecuteCommandTests tests/ExecuteCommandTests.cpp)
    falcon_add_test(bundle FalconBundleTests tests/BundleTests.cpp)
    falcon_add_test(copper-golem FalconCopperGolemTests tests/CopperGolemTests.cpp)
    falcon_add_test(player-effect-save FalconPlayerEffectSaveTests tests/PlayerEffectSaveTests.cpp)
    falcon_add_test(lingering-cloud FalconLingeringCloudTests tests/LingeringCloudTests.cpp)
    falcon_add_test(container-leave FalconContainerLeaveTests tests/ContainerLeaveTests.cpp)
    falcon_add_test(achievements FalconAchievementsTests tests/AchievementsTests.cpp)
    falcon_add_test(ender-chest-viewers FalconEnderChestViewerTests tests/EnderChestViewerTests.cpp)
    falcon_add_test(item-use-slot FalconItemUseSlotTests tests/ItemUseSlotTests.cpp)
    falcon_add_test(projectile-launch FalconProjectileLaunchTests tests/ProjectileLaunchTests.cpp)
    falcon_add_test(enderman-teleport FalconEndermanTeleportTests tests/EndermanTeleportTests.cpp)
    falcon_add_test(crossbow FalconCrossbowTests tests/CrossbowTests.cpp)
    falcon_add_test(pack-item-rules FalconPackItemRulesTests tests/PackItemRulesTests.cpp)
    falcon_add_test(campfire-native-save FalconCampfireTests tests/CampfireTests.cpp ARGS native-save)
    falcon_register_test(campfire-cooking FalconCampfireTests ARGS cooking)
    falcon_register_test(campfire-extinguished FalconCampfireTests ARGS extinguished)
    falcon_add_test(furnace-fuels FalconFurnaceParityTests tests/FurnaceParityTests.cpp ARGS fuels)
    falcon_register_test(furnace-plant-fuels FalconFurnaceParityTests ARGS plants)
    falcon_add_test(recipe-item-identifiers FalconRecipeItemIdentifierTests tests/RecipeItemIdentifierTests.cpp
            ARGS identifiers)
    falcon_register_test(furnace-sand-smelts FalconRecipeItemIdentifierTests ARGS sand-smelts)
    falcon_register_test(crafting-bed-metadata FalconRecipeItemIdentifierTests ARGS bed-metadata)
    falcon_add_test(crafting-output-metadata FalconCraftingOutputMetadataTests tests/CraftingOutputMetadataTests.cpp
            ARGS metadata)
    falcon_register_test(crafting-output-nbt FalconCraftingOutputMetadataTests ARGS nbt)
    falcon_add_test(dispenser FalconDispenserTests tests/DispenserTests.cpp)
    falcon_add_test(projectile-sweep FalconProjectileSweepTests tests/ProjectileSweepTests.cpp)
    falcon_add_test(projectile-flight FalconProjectileFlightTests tests/ProjectileFlightTests.cpp)
    falcon_add_test(projectile-sync FalconProjectileSyncTests tests/ProjectileSyncTests.cpp)
    falcon_add_test(phantom-insomnia FalconPhantomInsomniaTests tests/PhantomInsomniaTests.cpp)
    falcon_add_test(lodestone-tracking FalconLodestoneTrackingTests tests/LodestoneTrackingTests.cpp)
    falcon_add_test(wither FalconWitherTests tests/WitherTests.cpp TIMEOUT 120)
    falcon_add_test(projectile-impacts FalconProjectileImpactTests tests/ProjectileImpactTests.cpp)
    falcon_add_test(shulkers FalconShulkerTests tests/ShulkerTests.cpp)
    falcon_add_test(heads FalconHeadTests tests/HeadTests.cpp)
    falcon_add_test(bells FalconBellTests tests/BellTests.cpp)
    falcon_add_test(spreadplayers FalconSpreadPlayersTests tests/SpreadPlayersTests.cpp)
    falcon_add_test(banner-recipes FalconBannerRecipeTests tests/BannerRecipeTests.cpp)
    falcon_add_test(grid-recipes FalconGridRecipeTests tests/GridRecipeTests.cpp)
    falcon_add_test(observers FalconObserverTests tests/ObserverTests.cpp)
    falcon_add_test(cakes FalconCakeTests tests/CakeTests.cpp)
    falcon_add_test(flower-pots FalconFlowerPotTests tests/FlowerPotTests.cpp)
    falcon_add_test(world-dynamic-properties FalconWorldDynamicPropertiesTests tests/WorldDynamicPropertiesTests.cpp)
    falcon_add_test(client-probes FalconClientProbeTests tests/ClientProbeTests.cpp)
    falcon_add_test(image-maps FalconImageMapTests tests/ImageMapTests.cpp)
    falcon_add_test(plugin-api FalconPluginApiTests tests/PluginApiTests.cpp)
    falcon_add_test(client-attestation FalconClientAttestationTests tests/ClientAttestationTests.cpp)
    falcon_add_test(wasm-files FalconWasmFilesTests tests/WasmFilesTests.cpp)
    falcon_add_test(client-side-generation FalconClientSideGenerationTests tests/ClientSideGenerationTests.cpp
            TIMEOUT 120)
    falcon_add_test(tree-generation FalconTreeGenerationTests tests/TreeGenerationTests.cpp)
    falcon_add_test(nether-ore-veins FalconNetherOreTests tests/NetherOreTests.cpp)
    falcon_add_test(bed FalconBedTests tests/BedTests.cpp)
    falcon_add_test(cauldron-dye FalconCauldronDyeTests tests/CauldronDyeTests.cpp)
    falcon_add_test(pack-optimizer FalconPackOptimizerTests tests/PackOptimizerTests.cpp)
    falcon_add_test(respawn-anchor FalconRespawnAnchorTests tests/RespawnAnchorTests.cpp)
    falcon_add_test(redstone FalconRedstoneTests tests/RedstoneTests.cpp)
    falcon_add_test(circuit FalconCircuitTests tests/CircuitTests.cpp)
    falcon_add_test(vibrations FalconVibrationTests tests/VibrationTests.cpp)
    falcon_add_test(block-drops FalconBlockDropTests tests/BlockDropTests.cpp)
    falcon_add_test(furnace-recipe-kinds FalconFurnaceRecipeKindTests tests/FurnaceRecipeKindTests.cpp)
    falcon_add_test(signs FalconSignTests tests/SignTests.cpp)
    falcon_add_test(eye-of-ender-walls FalconEyeOfEnderWallTests tests/EyeOfEnderWallTests.cpp)
    falcon_add_test(optimization-regressions FalconOptimizationTests tests/OptimizationTests.cpp TIMEOUT 120)
    falcon_add_test(chunk-stream FalconChunkStreamTests tests/ChunkStreamTests.cpp)
    falcon_add_test(warden-anger FalconWardenAngerTests tests/WardenAngerTests.cpp)
    falcon_add_test(float-goal FalconFloatGoalTests tests/FloatGoalTests.cpp)
    falcon_add_test(warden-goals FalconWardenGoalTests tests/WardenGoalTests.cpp)
    falcon_add_test(warden-senses FalconWardenSensesTests tests/WardenSensesTests.cpp)
    falcon_add_test(sculk-shrieker FalconSculkShriekerTests tests/SculkShriekerTests.cpp)
    falcon_add_test(sculk-catalyst FalconSculkCatalystTests tests/SculkCatalystTests.cpp)
    falcon_add_test(sculk-spreader FalconSculkSpreaderTests tests/SculkSpreaderTests.cpp)
    falcon_add_test(multiface-block FalconMultifaceBlockTests tests/MultifaceBlockTests.cpp)
    falcon_add_test(bone-meal-spread FalconBoneMealSpreadTests tests/BoneMealSpreadTests.cpp)
    falcon_add_test(player-ride-tamed FalconPlayerRideTamedTests tests/PlayerRideTamedTests.cpp TIMEOUT 120)
    falcon_add_test(mob-falls FalconMobFallTests tests/MobFallTests.cpp TIMEOUT 300)
    falcon_add_test(generation-caches FalconGenerationCacheTests tests/GenerationCacheTests.cpp TIMEOUT 120)
    falcon_add_test(ancient-city FalconAncientCityTests tests/AncientCityTests.cpp TIMEOUT 120)
    falcon_add_test(infested-nylium FalconInfestedNyliumTests tests/InfestedNyliumTests.cpp TIMEOUT 120)
    falcon_add_test(decoration-order FalconDecorationOrderTests tests/DecorationOrderTests.cpp TIMEOUT 120)
    falcon_add_test(kelp FalconKelpTests tests/KelpTests.cpp TIMEOUT 120)
    falcon_add_test(terrain-beard FalconTerrainBeardTests tests/TerrainBeardTests.cpp)
    falcon_add_test(legacy-trees FalconLegacyTreeTests tests/LegacyTreeTests.cpp TIMEOUT 120)
    falcon_add_test(deep-dark-sculk FalconDeepDarkSculkTests tests/DeepDarkSculkTests.cpp TIMEOUT 120)
    falcon_add_test(block-placement-filter FalconBlockPlacementFilterTests tests/BlockPlacementFilterTests.cpp)
    falcon_add_test(block-ticking-queue FalconBlockTickingQueueTests tests/BlockTickingQueueTests.cpp)
    falcon_add_test(signaling-properties FalconSignalingPropertiesTests tests/SignalingPropertiesTests.cpp)
    falcon_add_test(fluid-parity FalconFluidParityTests tests/FluidParityTests.cpp
            TIMEOUT 300 ARGS platform drain stairs twosrc twosrcair hole hole2 holefar waterfall ledge still placeflow
            lavaplat lavawater basalt drops waterlog waterlogout waterlogslab sourceabove netherlava ~sponge@41
            concrete lavafall waterfalllava)
    target_compile_definitions(FalconFluidParityTests PRIVATE
            FALCON_FLUID_TRACE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fluids")
    falcon_add_test(bubble-columns FalconBubbleColumnTests tests/BubbleColumnTests.cpp)
    falcon_add_test(liquid-flow FalconLiquidFlowTests tests/LiquidFlowTests.cpp)
    falcon_add_test(fast-join FalconFastJoinTests tests/FastJoinTests.cpp)
    falcon_add_test(actor-visibility FalconActorVisibilityTests tests/ActorVisibilityTests.cpp)
    # the queue alone: no server to link
    add_executable(FalconTaskQueueTests tests/TaskQueueTests.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(FalconTaskQueueTests PRIVATE FalconShared Threads::Threads)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconTaskQueueTests PRIVATE -static -fuse-ld=lld)
    endif()
    falcon_register_test(task-queue FalconTaskQueueTests)
    falcon_add_test(crowd FalconCrowdTests tests/CrowdTests.cpp)
    falcon_add_test(snow-layer-collision FalconSnowLayerCollisionTests tests/SnowLayerCollisionTests.cpp)
    falcon_add_test(snow-logging FalconSnowLoggingTests tests/SnowLoggingTests.cpp)
    falcon_add_test(tnt-blast FalconTntBlastTests tests/TntBlastTests.cpp)
    falcon_add_test(jigsaw-structures FalconJigsawStructureTests tests/JigsawStructureTests.cpp TIMEOUT 120)
    falcon_add_test(paintings FalconPaintingTests tests/PaintingTests.cpp)
    falcon_add_test(replaceitem-command FalconReplaceItemCommandTests tests/ReplaceItemCommandTests.cpp)
    falcon_add_test(experience FalconExperienceTests tests/ExperienceTests.cpp)
    falcon_add_test(hopper-horizontal FalconHopperTransferTests tests/HopperTransferTests.cpp ARGS horizontal)
    falcon_register_test(hopper-fuel FalconHopperTransferTests ARGS fuel)
    falcon_register_test(hopper-composter FalconHopperTransferTests ARGS composter)
    falcon_add_test(brewing-idle-save FalconBrewingStandTests tests/BrewingStandTests.cpp ARGS idle-save)
    falcon_register_test(brewing-ingredient-change FalconBrewingStandTests ARGS ingredient-change)
    falcon_register_test(brewing-countdown FalconBrewingStandTests ARGS countdown)
    falcon_register_test(brewing-plugin-events FalconBrewingStandTests ARGS plugin-events)
    falcon_add_test(send-event FalconSendEventTests tests/SendEventTests.cpp)
    falcon_add_test(summon-entity FalconSummonEntityTests tests/SummonEntityTests.cpp)
    falcon_add_test(vexes FalconVexTests tests/VexTests.cpp)
    falcon_add_test(illager-captains FalconIllagerCaptainTests tests/IllagerCaptainTests.cpp)
    falcon_add_test(evokers FalconEvokerTests tests/EvokerTests.cpp)
    falcon_add_test(vindicators FalconVindicatorTests tests/VindicatorTests.cpp)
    falcon_add_test(damage-over-time FalconDamageOverTimeTests tests/DamageOverTimeTests.cpp)
    falcon_add_test(break-blocks FalconBreakBlocksTests tests/BreakBlocksTests.cpp)
    falcon_add_test(knockback-roar FalconKnockbackRoarTests tests/KnockbackRoarTests.cpp)
    falcon_add_test(ravagers FalconRavagerTests tests/RavagerTests.cpp)
    falcon_add_test(breeze-goals FalconBreezeGoalsTests tests/BreezeGoalsTests.cpp)
    falcon_add_test(wind-burst FalconWindBurstTests tests/WindBurstTests.cpp)
    falcon_add_test(delayed-attack FalconDelayedAttackTests tests/DelayedAttackTests.cpp)
    falcon_add_test(fog-stack FalconFogStackTests tests/FogStackTests.cpp)
    falcon_add_test(music-command FalconMusicCommandTests tests/MusicCommandTests.cpp)
    falcon_add_test(hud-command FalconHudCommandTests tests/HudCommandTests.cpp)
    falcon_add_test(command-errors FalconCommandErrorTests tests/CommandErrorTests.cpp)
    falcon_add_test(inputpermission-command FalconInputPermissionCommandTests tests/InputPermissionCommandTests.cpp)
    falcon_add_test(sulfur-cube FalconSulfurCubeTests tests/SulfurCubeTests.cpp TIMEOUT 120)
    falcon_add_test(entity-filter-parity FalconEntityFilterParityTests tests/EntityFilterParityTests.cpp TIMEOUT 300)
    target_sources(FalconEntityFilterParityTests PRIVATE tests/JsonEntityFilter.cpp)
endif()

option(FALCON_BUILD_WORLDGEN_PARITY "Build the tool comparing generated terrain with a world saved by BDS" OFF)
if(FALCON_BUILD_WORLDGEN_PARITY)
    add_executable(FalconWorldGenParity tools/WorldGenParity.cpp)
    target_link_libraries(FalconWorldGenParity PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconWorldGenParity PRIVATE -static -fuse-ld=lld)
    endif()
endif()

option(FALCON_BUILD_GEN_SPEED "Build the tool timing chunk generation on one thread" OFF)
if(FALCON_BUILD_GEN_SPEED)
    add_executable(FalconGenSpeed tools/GenSpeed.cpp)
    target_link_libraries(FalconGenSpeed PRIVATE FalconCore)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(FalconGenSpeed PRIVATE -static -fuse-ld=lld)
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
    # scripts run on the main thread with a 1 MB stack cap: the 1 MB windows default leaves them no room and a
    # heavy behavior pack stops loading part way; 8 MB as on linux
    target_link_options(FalconServer PRIVATE -static -fuse-ld=lld -Wl,--stack,8388608)
endif()

option(FALCON_EXPORT_SYMBOLS "Export the server symbols so internal plugins can link against the executable" ON)

if(FALCON_EXPORT_SYMBOLS)
    set_target_properties(FalconServer PROPERTIES ENABLE_EXPORTS ON)

    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(FALCON_EXPORT_INPUTS "${CMAKE_CURRENT_BINARY_DIR}/FalconServerExports.txt")
        set(FALCON_EXPORT_DEFINITION "${CMAKE_CURRENT_BINARY_DIR}/FalconServer.def")
        set(FALCON_EXPORT_SOURCES "$<TARGET_OBJECTS:FalconServer>" "$<TARGET_OBJECTS:FalconCore>"
                "$<TARGET_FILE:FalconProtocol>"
                "$<TARGET_FILE:FalconNetwork>" "$<TARGET_FILE:FalconShared>" "$<TARGET_FILE:FalconBlockStateUpdater>")
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
