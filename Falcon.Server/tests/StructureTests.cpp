#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/ChunkStructureBounds.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LevelGen/Structure/JigsawPlacer.h"
#include "World/Level/LevelGen/Structure/OceanRuinFeature.h"
#include "World/Level/LevelGen/Structure/OceanRuinStructure.h"
#include "World/Level/LevelGen/Structure/PillagerOutpostStructure.h"
#include "World/Level/LevelGen/Structure/RuinedPortalStructure.h"
#include "World/Level/LevelGen/Structure/ScatteredPiece.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"
#include "World/Level/LevelGen/Structure/VillageStructure.h"

#include <array>
#include <initializer_list>
#include <map>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::vector<const StructureProcessors::Rule *> rules(const std::string &name) {
        std::vector<const StructureProcessors::Rule *> out;
        for (const StructureProcessors::Rule &rule: StructureProcessors::get(name))
            out.push_back(&rule);
        return out;
    }

    uint32_t seedDrawing(float probability, bool below) {
        for (uint32_t seed = 1;; ++seed) {
            std::mt19937 random(seed);
            const float value = (float) ((double) random() * 2.3283064365386963e-10);
            if ((value < probability) == below)
                return seed;
        }
    }

    void checkProcessors() {
        require(StructureProcessors::get("minecraft:plains_path_decay").size() == 5, "Plains street rules differ");
        require(StructureProcessors::get("minecraft:no_such_list").empty(), "An unknown list has rules");

        const BlockState air("minecraft:air");
        const auto mossy = rules("minecraft:cobblestone_building_low_decay");

        std::mt19937 untouched(7);
        const BlockState dirt = StructureProcessors::apply(mossy, BlockState("minecraft:dirt"), air, untouched);
        std::mt19937 fresh(7);
        require(dirt.mName == "minecraft:dirt" && untouched() == fresh(), "A non-matching block drew from the random");

        std::mt19937 low(seedDrawing(0.1f, true));
        require(StructureProcessors::apply(mossy, BlockState("minecraft:cobblestone"), air, low).mName
                        == "minecraft:mossy_cobblestone",
                "A draw under the probability kept the cobblestone");
        std::mt19937 high(seedDrawing(0.1f, false));
        require(StructureProcessors::apply(mossy, BlockState("minecraft:cobblestone"), air, high).mName
                        == "minecraft:cobblestone",
                "A draw over the probability replaced the cobblestone");

        const auto decay = rules("minecraft:abandoned_plains_decay");
        std::mt19937 web(seedDrawing(0.5f, true));
        require(StructureProcessors::apply(decay, BlockState("minecraft:glass_pane"), air, web).mName
                        == "minecraft:web",
                "The pane did not turn into a web");
        std::mt19937 brown(seedDrawing(0.5f, false));
        require(StructureProcessors::apply(decay, BlockState("minecraft:glass_pane"), air, brown).mName
                        == "minecraft:brown_stained_glass_pane",
                "The pane kept its colour");

        // bridge over water: the later water rule no longer matches
        std::mt19937 any(1);
        require(StructureProcessors::apply(rules("minecraft:plains_path_decay"), BlockState("minecraft:grass_block"),
                                           BlockState("minecraft:water"), any)
                                .mName
                        == "minecraft:oak_planks",
                "A street block over water is no bridge");

        std::mt19937 crops(1);
        const BlockState beetroot = StructureProcessors::apply(rules("minecraft:wheat_beet_farm"),
                                                               BlockState("minecraft:carrots"), air, crops);
        require(beetroot.mName == "minecraft:beetroot" && beetroot.mStates.getInt("growth", 0) == 7,
                "The farm crop is not fully grown beetroot");
    }

    void checkStyles() {
        require(BiomeChunkGenDataRegistry::getVillageType(BiomeIds::DESERT) == 0, "Desert is no desert village");
        require(BiomeChunkGenDataRegistry::getVillageType(BiomeIds::ICE_PLAINS) == 1, "Ice plains are not snowy");
        require(BiomeChunkGenDataRegistry::getVillageType(BiomeIds::COLD_TAIGA) == 3, "Cold taiga is not taiga");
        require(BiomeChunkGenDataRegistry::getVillageType(BiomeIds::FOREST) == 4, "A forest village is not plains");
    }

    void checkVillage() {
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        const std::mt19937 stream = StructureFeature::createPostProcessRandom(42, -92, -27);
        require(stream == std::mt19937(StructureFeature::getPostProcessSeed(42, -92, -27)),
                "The post process seed does not seed the post process random");

        // BDS 1.26.61: meeting point 2 at -92, -27
        const auto first = VillageStructure::cachedPlan(*generator, -92, -27);
        require(first != nullptr && !first->empty(), "No village at -92, -27");
        require(std::string(first->front().mElement->mName) == "minecraft:plains_meeting_point_2",
                "The village centre differs");
        require(VillageStructure::cachedPlan(*generator, -92, -27) == first, "The plan was not kept");
        require(VillageStructure::plan(*generator, -92, -27).size() == first->size(), "The kept plan differs");
        require(VillageStructure::cachedPlan(*generator, -92, -26) == nullptr, "A plain chunk has a village");

        StructureTerrain terrain(*generator);
        const auto a = VillageStructure::placeInChunk(*generator, *first, -92, -27, terrain);
        const auto b = VillageStructure::placeInChunk(*generator, *first, -92, -27, terrain);
        require(!a.mBlocks.empty() && a.mBlocks.size() == b.mBlocks.size(), "The chunk share is not reproducible");
        for (const auto &entry: a.mBlocks) {
            require(std::get<0>(entry.first) >> 4 == -92 && std::get<2>(entry.first) >> 4 == -27,
                    "A block left its chunk");
        }

        // BDS 1.26.61 saves the meeting point's bell there
        bool bell = false;
        const VillageStructure::Position bellPosition(-1467, 64, -422);
        for (const auto &entity: a.mBlockEntities)
            bell = bell || (entity.mBlock.mName == "minecraft:bell" && entity.mPosition == bellPosition);
        require(bell, "The meeting point bell has no block actor");
        for (const auto &mob: a.mMobs) {
            if (mob.mIdentifier == "minecraft:villager_v2")
                require(mob.mEvent == "minecraft:spawn_from_village", "A village villager lost its spawn event");
        }
    }

    void checkConversions() {
        StructureTemplate::PaletteEntry snow;
        snow.mName = "minecraft:snow";
        snow.mProperties["layers"] = "3";
        BlockState out;
        require(StructureBlockTable::translate(snow, 0, out) && out.mName == "minecraft:snow_layer"
                        && out.mStates.getInt("height", -1) == 2,
                "Snow layers lose their height");

        // BDS 1.26.61: a north stonecutter turned twice faces south
        StructureTemplate::PaletteEntry cutter;
        cutter.mName = "minecraft:stonecutter";
        cutter.mProperties["facing"] = "north";
        require(StructureBlockTable::translate(cutter, 2, out) && out.mName == "minecraft:stonecutter_block"
                        && out.mStates.getString("minecraft:cardinal_direction") == "south",
                "The stonecutter turns wrong");

        StructureTemplate::PaletteEntry pot;
        pot.mName = "minecraft:potted_cactus";
        require(StructureBlockTable::translate(pot, 0, out) && out.mName == "minecraft:flower_pot",
                "A potted plant is no flower pot");
    }

    void checkMirror() {
        // the mirror flips the coordinates before the turn
        int32_t x = 3, z = 5;
        StructureTemplate::mirror(1, x, z);
        StructureTemplate::rotate(1, x, z);
        require(x == 5 && z == 3, "A mirrored template turns from the wrong side");
        x = 3, z = 5;
        StructureTemplate::mirror(2, x, z);
        require(x == -3 && z == 5, "Mirror 2 does not flip x");

        // 1 swaps north and south, 2 east and west, then the turn
        StructureTemplate::PaletteEntry chest;
        chest.mName = "minecraft:chest";
        chest.mProperties = {{"facing", "north"}, {"type", "single"}, {"waterlogged", "false"}};
        BlockState out;
        require(StructureBlockTable::translate(chest, 0, 1, out)
                        && out.mStates.getString("minecraft:cardinal_direction") == "south",
                "A mirrored chest keeps its facing");
        require(StructureBlockTable::translate(chest, 1, 1, out)
                        && out.mStates.getString("minecraft:cardinal_direction") == "west",
                "A mirrored chest turns before the mirror");
        require(StructureBlockTable::translate(chest, 0, 2, out)
                        && out.mStates.getString("minecraft:cardinal_direction") == "north",
                "Mirror 2 moves a north chest");

        StructureTemplate::PaletteEntry stairs;
        stairs.mName = "minecraft:stone_brick_stairs";
        stairs.mProperties = {
                {"facing", "east"}, {"half", "bottom"}, {"shape", "outer_left"}, {"waterlogged", "false"}};
        require(StructureBlockTable::translate(stairs, 0, 2, out) && out.mStates.getInt("weirdo_direction", -1) == 1
                        && out.mStates.getString("minecraft:corner") == "outer_left",
                "Mirrored stairs face the wrong way or change their corner");

        StructureTemplate::PaletteEntry vine;
        vine.mName = "minecraft:vine";
        vine.mProperties = {{"north", "true"}, {"south", "true"}};
        require(StructureBlockTable::translate(vine, 0, 1, out) && out.mStates.getInt("vine_direction_bits", -1) == 4,
                "A mirrored vine does not keep its south face first");

        StructureTemplate::PaletteEntry bars;
        bars.mName = "minecraft:iron_bars";
        bars.mProperties = {{"east", "false"}, {"north", "false"}, {"south", "true"}, {"west", "true"}};
        require(StructureBlockTable::translate(bars, 0, 1, out)
                        && out.mStates.getByte("minecraft:connection_north", -1) == 1
                        && out.mStates.getByte("minecraft:connection_south", -1) == 0
                        && out.mStates.getByte("minecraft:connection_west", -1) == 1,
                "Mirrored iron bars keep their sides");
        require(StructureBlockTable::translate(bars, 1, 0, out)
                        && out.mStates.getByte("minecraft:connection_south", -1) == 0
                        && out.mStates.getByte("minecraft:connection_west", -1) == 1
                        && out.mStates.getByte("minecraft:connection_north", -1) == 1
                        && out.mStates.getByte("minecraft:connection_east", -1) == 0,
                "Turned iron bars do not turn their sides");

        StructureTemplate::PaletteEntry ladder;
        ladder.mName = "minecraft:ladder";
        ladder.mProperties = {{"facing", "north"}};
        require(!StructureBlockTable::translate(ladder, 0, 1, out), "A ladder is mirrored without a known state");
        require(StructureBlockTable::translate(ladder, 0, 0, out), "A ladder is left out without a mirror");
    }

    void checkBlockActors() {
        std::unique_ptr<BlockActor> smoker = BlockActorClassRegistry::create(FurnaceBlockActor::SMOKER_ID);
        const auto *furnace = dynamic_cast<const FurnaceBlockActor *>(smoker.get());
        require(furnace != nullptr && furnace->mKind == FurnaceKind::Smoker, "A smoker loads as another furnace");
        require(std::string(furnace->getBlockActorId()) == FurnaceBlockActor::SMOKER_ID, "A smoker saves as a furnace");

        BannerBlockActor banner;
        banner.setBase(3);
        const Tag saved = banner.saveNbt();
        require(saved.getInt("Base", -1) == 3 && saved.getInt("Type", -1) == 0, "The banner colour is lost");
        require(!saved.contains("Patterns"), "An empty pattern list is saved");

        std::unique_ptr<BlockActor> created = BlockActorClassRegistry::create(BrushableBlockActor::BLOCK_ACTOR_ID);
        auto *brushable = dynamic_cast<BrushableBlockActor *>(created.get());
        require(brushable != nullptr, "BrushableBlock does not load as a brushable block");
        brushable->setType("minecraft:suspicious_gravel");
        brushable->loadLootTable(Tag::ofCompound());
        const Tag fresh = brushable->saveWithPosition();
        require(fresh.getString("LootTable") == BrushableBlockActor::EMPTY_LOOT_TABLE
                        && fresh.getInt("LootTableSeed", -1) == 0,
                "A fresh brushable block lost the empty table");
        require(fresh.getByte("brush_direction", 0) == 6 && fresh.getInt("brush_count", -1) == 0
                        && fresh.getString("type") == "minecraft:suspicious_gravel" && !fresh.contains("item"),
                "A fresh brushable block saves other fields than the game");
        brushable->setLootTable("loot_tables/entities/desert_well_brushable_block.json", -7);
        require(brushable->saveWithPosition().getInt("LootTableSeed", 0) == -7, "The archaeology seed is lost");

        // strokes count every 10 ticks, the cooldown running on from the previous stroke time
        BrushableBlockActor brushed;
        bool rollLoot = false;
        require(brushed.stroke(100, 1, rollLoot) && rollLoot && brushed.getBrushCount() == 1
                        && brushed.getBrushDirection() == 1,
                "The first stroke does not count or roll the loot");
        require(!brushed.stroke(110, 2, rollLoot), "A stroke inside the cooldown counts");
        require(brushed.stroke(111, 2, rollLoot) && !rollLoot && brushed.getBrushDirection() == 1,
                "A later stroke rolls the loot again or turns the face");
        require(!brushed.stroke(120, 1, rollLoot) && brushed.stroke(121, 1, rollLoot) && brushed.getBrushCount() == 3,
                "A stroke past the cooldown does not count");
        require(!brushed.stroke(130, 1, rollLoot) && brushed.stroke(131, 1, rollLoot) && brushed.getBrushCount() == 4,
                "The stroke cooldown restarts from the current tick");
        bool changed = false;
        require(brushed.checkReset(171, changed) == 20 && !changed, "The count falls back before the pause ends");
        require(brushed.checkReset(172, changed) == 10 && changed && brushed.getBrushCount() == 2,
                "A pause does not take two strokes off");
        require(brushed.checkReset(182, changed) == 10 && brushed.getBrushCount() == 0, "The count goes below zero");
        require(brushed.checkReset(192, changed) == 0 && !changed && brushed.getBrushDirection() == 6,
                "A clean block keeps its brushed face");
        const int32_t progress[] = {0, 1, 1, 2, 2, 2, 3, 3, 3, 3};
        for (int32_t count = 0; count < 10; ++count)
            require(BrushableBlockActor::progressOf(count) == progress[count], "The brushed progress differs");
    }
}

namespace {
    void checkMaterials() {
        require(StructureMaterials::isLiquid(BlockState("minecraft:water")), "water is a liquid material");
        require(StructureMaterials::isLiquid(BlockState("minecraft:flowing_lava")), "lava is a liquid material");
        require(StructureMaterials::isLiquid(BlockState("minecraft:bubble_column")), "bubble column is a liquid");
        require(!StructureMaterials::isLiquid(BlockState("minecraft:powder_snow")), "powder snow is no liquid");
        require(StructureMaterials::isPlant(BlockState("minecraft:short_grass")), "short grass is a plant material");
        require(StructureMaterials::isPlant(BlockState("minecraft:seagrass")), "seagrass is a plant material");
        require(!StructureMaterials::isPlant(BlockState("minecraft:waterlily")), "the lily pad is no plant material");
        require(!StructureMaterials::isPlant(BlockState("minecraft:oak_leaves")), "leaves are no plant material");
        require(!StructureMaterials::isPlant(BlockState("minecraft:lava")), "lava is no plant material");

        for (size_t i = 0; i < BlockDataTable::getCount(); i++) {
            BlockMaterialType type;
            if (!BlockMaterial::find(BlockState(BlockDataTable::getEntries()[i].mIdentifier), type))
                throw std::runtime_error(std::string("no material for ") + BlockDataTable::getEntries()[i].mIdentifier);
        }
        BlockMaterialType type;
        require(!BlockMaterial::find(BlockState("custom:block"), type), "an unknown block has no material");
        require(BlockMaterial::find(BlockState("minecraft:cobblestone_wall"), type) && type == BlockMaterialType::Solid,
                "walls take the material of cobblestone");
        require(BlockMaterial::find(BlockState("minecraft:white_wool_stairs"), type)
                        && type == BlockMaterialType::Solid,
                "wool stairs take the material of wool");
        require(BlockMaterial::find(BlockState("minecraft:copper_chest"), type) && type == BlockMaterialType::Metal,
                "copper chests are metal");
        require(BlockMaterial::flagsOf(BlockState("minecraft:water")) == 0x0a, "water flags");
        require(BlockMaterial::flagsOf(BlockState("minecraft:oak_leaves")) == 0x1d, "leaves flags");

        require(StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:stone"), false, false), "stone stops");
        require(StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:wall_sign"), false, false),
                "wooden signs stop the search");
        require(!StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:ice"), true, true),
                "ice has the air material");
        require(!StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:oak_leaves"), true, false),
                "leaves are looked through");
        require(StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:oak_leaves"), false, true),
                "leaves stop when they count");
        require(StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:flowing_water"), true, false),
                "water stops when it counts");
        require(!StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:water"), false, false),
                "water is looked through otherwise");
        // the feature rules' query.above_top_solid passes neither flag: leaves are looked through there too
        require(!StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:oak_leaves"), false, false)
                        && !StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:air"), false, false),
                "the feature query stops on leaves or air");
        require(!StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:short_grass"), true, true),
                "grass never stops");

        require(StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:sand")), "sand buries the treasure");
        require(StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:stone")), "stone buries the treasure");
        require(StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:normal_stone_double_slab")),
                "a double slab keeps the default property");
        require(!StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:normal_stone_slab")),
                "a slab clears the default property");
        require(!StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:water")), "water does not bury");
        require(!StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:glass")), "glass has flag 1");
        require(!StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:chest")), "a chest clears it");
        require(!StructureMaterials::isBuriedTreasureGround(BlockState("custom:block")), "unknown blocks do not bury");
        require(!StructureMaterials::isBuriedTreasureGround(BlockState("minecraft:iron_chain")),
                "chains of 1.26.50 worlds lose the default property");
    }
}

namespace {
    void checkOceanRuins() {
        OceanRuinFeature::Config config;
        require(OceanRuinFeature::getConfig(BiomeIds::DEEP_COLD_OCEAN, config) && !config.mWarm
                        && config.mLargeChance == 0.5f && config.mClusterChance == 0.4f,
                "Deep cold ruins differ");
        require(OceanRuinFeature::getConfig(BiomeIds::LUKEWARM_OCEAN, config) && config.mWarm
                        && config.mLargeChance == 0.3f && config.mClusterChance == 0.5f,
                "Lukewarm ruins are not warm");
        require(!OceanRuinFeature::getConfig(BiomeIds::PLAINS, config), "Plains have ruins");

        // every block the ruins lay has a Bedrock state
        for (const std::string finish: {"_brick", "_cracked", "_mossy"}) {
            for (const char *name: {"ruin1", "ruin2", "ruin3", "ruin4", "ruin5", "ruin6", "ruin7", "ruin8", "big_ruin1",
                                    "big_ruin2", "big_ruin3", "big_ruin8"}) {
                const auto shape = StructureTemplate::load(std::string("ruin/") + name + finish + ".nbt");
                require(shape != nullptr, "A cold ruin template is missing");
                for (const StructureTemplate::PaletteEntry &entry: shape->getPalette()) {
                    BlockState out;
                    require(entry.mName == "minecraft:air" || entry.mName == "minecraft:structure_block"
                                    || StructureBlockTable::translate(entry, 1, out),
                            "A cold ruin block has no state");
                }
            }
        }
        for (const char *name: {"ruin4", "ruin_warm1", "ruin_warm8", "big_ruin_warm4", "big_ruin_warm7"}) {
            const auto shape = StructureTemplate::load(std::string("ruin/") + name + ".nbt");
            require(shape != nullptr, "A ruin template is missing");
            for (const StructureTemplate::PaletteEntry &entry: shape->getPalette()) {
                BlockState out;
                require(entry.mName == "minecraft:air" || entry.mName == "minecraft:structure_block"
                                || StructureBlockTable::translate(entry, 3, out),
                        "A ruin block has no state");
            }
        }

        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        const StructureFeature *ruins = StructureFeature::get("ocean_ruin");
        require(ruins != nullptr, "Ocean ruins are not registered");
        StructureChunkPos start;
        require(ruins->findNearest(*generator, 0, 0, start), "No ocean ruin near the origin");
        const auto pieces = OceanRuinStructure::plan(*generator, start.x, start.z);
        require(!pieces.empty(), "An ocean ruin start has no piece");
        require(OceanRuinStructure::plan(*generator, start.x, start.z).size() == pieces.size(),
                "The ruin plan is not reproducible");
        for (const OceanRuinStructure::Piece &piece: pieces) {
            require(StructureTemplate::load(piece.mTemplate + ".nbt") != nullptr, "A planned ruin has no template");
            require(piece.mIntegrity > 0.0f && piece.mIntegrity < 1.0f, "A ruin keeps all its blocks");
        }
        if (!pieces.front().mWarm)
            require(pieces.size() % 3 == 0, "A cold ruin lacks a finish");
    }
}

namespace {
    void checkPillagerOutposts() {
        // every block the outposts lay has a Bedrock state
        for (const char *name:
             {"watchtower", "watchtower_overgrown", "feature_cage1", "feature_cage2", "feature_cage_with_allays",
              "feature_logs", "feature_targets", "feature_tent1", "feature_tent2"}) {
            const auto shape = StructureTemplate::load(std::string("pillageroutpost/") + name + ".nbt");
            require(shape != nullptr, "An outpost template is missing");
            for (const StructureTemplate::PaletteEntry &entry: shape->getPalette()) {
                BlockState out;
                require(entry.mName == "minecraft:structure_block" || entry.mName == "minecraft:jigsaw"
                                || StructureBlockTable::translate(entry, 2, out),
                        "An outpost block has no state");
            }
        }

        // the last true face wins, up covering all four
        StructureTemplate::PaletteEntry vine;
        vine.mName = "minecraft:vine";
        vine.mProperties = {{"east", "false"}, {"north", "false"}, {"south", "true"}, {"up", "true"}, {"west", "true"}};
        BlockState out;
        require(StructureBlockTable::translate(vine, 0, out) && out.mStates.getInt("vine_direction_bits", -1) == 2,
                "The vine faces differ");
        vine.mProperties = {{"south", "true"}};
        require(StructureBlockTable::translate(vine, 1, out) && out.mStates.getInt("vine_direction_bits", -1) == 2,
                "A turned vine keeps its face");
        // the placement keeps a single face: up stands for all four, south first
        vine.mProperties = {{"up", "true"}};
        require(StructureBlockTable::translate(vine, 0, out) && out.mStates.getInt("vine_direction_bits", -1) == 1,
                "An up vine keeps more than its south face");
        require(StructureBlockTable::translate(vine, 3, out) && out.mStates.getInt("vine_direction_bits", -1) == 8,
                "A turned up vine is not east");
        vine.mProperties = {};
        require(StructureBlockTable::translate(vine, 1, out) && out.mStates.getInt("vine_direction_bits", -1) == 8,
                "A faceless vine is not read as north");

        require(StructureBlockTable::bannerType("minecraft:illager_captain_wall_banner") == 1
                        && StructureBlockTable::bannerType("minecraft:brown_wall_banner") == 0,
                "The captain banner is not ominous");

        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        const StructureFeature *outposts = StructureFeature::get("pillager_outpost");
        require(outposts != nullptr, "Pillager outposts are not registered");
        StructureChunkPos start;
        require(outposts->findNearest(*generator, 0, 0, start), "No outpost near the origin");
        const auto pieces = PillagerOutpostStructure::plan(*generator, start.x, start.z);
        require(pieces.size() >= 2 && pieces.size() <= 6, "An outpost has a wrong piece count");
        require(pieces[0].mTemplate == "pillageroutpost/watchtower"
                        && pieces[1].mTemplate == "pillageroutpost/watchtower_overgrown",
                "The outpost does not open with its towers");
        for (size_t i = 2; i < pieces.size(); ++i)
            require(pieces[i].mFeature && StructureTemplate::load(pieces[i].mTemplate + ".nbt") != nullptr,
                    "An outpost prop has no template");
    }
}

namespace {
    void checkRuinedPortals() {
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        const StructureFeature *portals = StructureFeature::get("ruined_portal");
        require(portals != nullptr && portals->getSpacing() == 40, "Ruined portals are not registered");
        StructureChunkPos start;
        require(portals->findNearest(*generator, 0, 0, start), "No ruined portal near the origin");

        const auto portal = RuinedPortalStructure::plan(*generator, start.x, start.z);
        require(portal != nullptr, "A ruined portal has no plan");
        require(portal == RuinedPortalStructure::plan(*generator, start.x, start.z), "The portal plan is not kept");
        const auto shape = StructureTemplate::load(portal->mTemplate + ".nbt");
        require(shape != nullptr, "The portal template is missing");
        const int32_t spanX = portal->mMaxX - portal->mMinX;
        const int32_t spanZ = portal->mMaxZ - portal->mMinZ;
        require((portal->mQuarterTurns & 1) == 0 ? spanX == shape->getSizeX() && spanZ == shape->getSizeZ()
                                                 : spanX == shape->getSizeZ() && spanZ == shape->getSizeX(),
                "The portal box does not follow the turned template");
        require(portal->mMaxY - portal->mMinY == shape->getSizeY(), "The portal box has the wrong height");

        bool obsidian = false;
        for (const RuinedPortalStructure::Change &change: portal->mChanges)
            obsidian = obsidian || change.mBlock.mName == "minecraft:obsidian"
                       || change.mBlock.mName == "minecraft:crying_obsidian";
        require(obsidian, "The portal frame is missing");
        for (const RuinedPortalStructure::Chest &chest: portal->mChests)
            require(chest.mLootTable == "loot_tables/chests/ruined_portal.json", "The portal chest has another loot");
    }
}

namespace {
    void checkScatteredPieces() {
        Level level("scattered-piece-test", 2, 1);
        LevelChunk chunk(1, 0);
        const BlockState stone("minecraft:stone");
        const BlockState air("minecraft:air");
        require(ScatteredPiece::aboveTopSolid(chunk, 0, 0) == LevelChunk::MIN_Y, "An empty column has no ground");
        chunk.setBlock(0, 70, 0, stone);
        require(ScatteredPiece::aboveTopSolid(chunk, 0, 0) == 71, "The ground is one over the top solid block");
        chunk.setBlock(1, -20, 0, stone);
        require(ScatteredPiece::averageGround(chunk, 0, 0, 2, 1) == 35, "Ground under 0 counts as 0 in the mean");

        // the box starts in the chunk to the west: only its east part is written
        ScatteredPiece piece(level, chunk, 8, 64, 0);
        piece.placeBlock(stone, 0, 0, 1);
        require(piece.getBlock(0, 0, 1) == nullptr, "A block outside the chunk is read");
        piece.placeBlock(stone, 8, 0, 1);
        require(chunk.getBlock(0, 64, 1).mName == "minecraft:stone", "A block inside the chunk is not written");

        piece.generateBox(8, 0, 2, 10, 2, 4, stone, air, false);
        require(chunk.getBlock(0, 64, 2).mName == "minecraft:stone", "The box edge differs");
        require(chunk.getBlock(1, 65, 3).mName == "minecraft:air", "The box inside differs");
        piece.generateBox(8, 0, 2, 10, 2, 4, BlockState("minecraft:dirt"), BlockState("minecraft:dirt"), true);
        require(chunk.getBlock(1, 65, 3).mName == "minecraft:air", "A box skipping air fills air");
        require(chunk.getBlock(0, 64, 2).mName == "minecraft:dirt", "A box skipping air leaves blocks");

        const BlockState stairs("minecraft:sandstone_stairs");
        require(ScatteredPiece::stairs(stairs, 3).mStates.getInt("weirdo_direction", -1) == 2
                        && ScatteredPiece::stairs(stairs, 2).mStates.getInt("weirdo_direction", -1) == 3
                        && ScatteredPiece::stairs(stairs, 1).mStates.getInt("weirdo_direction", -1) == 1,
                "Unturned stairs swap 2 and 3");

        std::mt19937 random(7);
        require(piece.createChest(random, 12, 0, 6, 5, "loot_tables/chests/desert_pyramid.json"), "No chest placed");
        require(chunk.getBlock(4, 64, 6).mStates.getString("minecraft:cardinal_direction", "") == "east",
                "Chest data 5 faces east");
        require(level.getBlockActors().find<ChestBlockActor>(Vector3i(20, 64, 6)) != nullptr, "The chest has no actor");
        require(!piece.createChest(random, 12, 0, 6, 5, "loot_tables/chests/desert_pyramid.json"),
                "A chest is placed twice");
        require(!piece.createChest(random, 0, 0, 6, 5, "loot_tables/chests/desert_pyramid.json"),
                "A chest is placed outside the chunk");

        require(piece.createDispenser(random, 13, 0, 6, 3, "loot_tables/chests/dispenser_trap.json")
                        && chunk.getBlock(5, 64, 6).mStates.getInt("facing_direction", -1) == 3,
                "The dispenser keeps its data facing");
        require(level.getBlockActors().find<DispenserBlockActor>(Vector3i(21, 64, 6)) != nullptr,
                "The dispenser has no actor");

        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        TerrainCache cache(*generator);
        int32_t value = 0;
        require(!cache.findStartValue("desert_pyramid", 3, 4, value), "A start value exists before any chunk");
        require(cache.storeStartValue("desert_pyramid", 3, 4, 70) == 70, "The first start value is not kept");
        require(cache.storeStartValue("desert_pyramid", 3, 4, 80) == 70, "A later chunk replaces the start value");
        require(cache.findStartValue("desert_pyramid", 3, 4, value) && value == 70, "The start value is lost");
        require(!cache.findStartValue("desert_pyramid", 4, 4, value), "Start values leak between starts");
    }
}

namespace {
    void appendInt(std::string &out, int32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            out.push_back((char) (((uint32_t) value >> shift) & 0xff));
    }

    void checkStructureBounds() {
        using Box = ChunkStructureBounds::Box;
        const Box chunk = ChunkStructureBounds::chunkBox(0, 0, -64, 319);
        require(chunk.minX == 0 && chunk.maxX == 15 && chunk.minY == -64 && chunk.maxY == 319 && chunk.maxZ == 15,
                "The chunk box differs");

        ChunkStructureBounds bounds;
        bounds.addStart("minecraft:igloo", 0, {40, 64, 0, 46, 68, 7}, {{40, 60, 0, 46, 64, 7}}, chunk);
        require(bounds.empty(), "A start missing the chunk was registered");

        // a start reaching past the chunk is clipped to it, its piece too
        bounds.addStart("minecraft:desert_pyramid", 0, {8, 64, 8, 28, 78, 28}, {{8, 70, 8, 28, 84, 28}}, chunk);
        bounds.addStart("minecraft:desert_pyramid", 0, {0, 64, 0, 20, 78, 20}, {{0, 60, 0, 20, 74, 20}}, chunk);
        require(bounds.getNames().size() == 1 && bounds.getEntries().size() == 2,
                "A second start of the same name was registered");
        const ChunkStructureBounds::Entry &whole = bounds.getEntries()[0];
        require(whole.mWhole && whole.mEntity == 1 && whole.mName == 0 && whole.mBox.maxX == 15
                        && whole.mBox.maxY == 78,
                "The whole start is not the first clipped entry");

        require(bounds.namesAt(10, 72, 10) == std::vector<std::string>{"minecraft:desert_pyramid"},
                "The piece box does not hold its block");
        require(bounds.namesAt(10, 66, 10).empty(), "The whole start box answered a query");
        require(bounds.namesAt(4, 72, 4).empty(), "A block outside the piece answered a query");

        std::string expected;
        appendInt(expected, 1);
        appendInt(expected, 1);
        appendInt(expected, 0);
        expected += std::string("\x18\x00", 2) + "minecraft:desert_pyramid";
        appendInt(expected, 2);
        for (const int32_t value: {2, 8, 70, 8, 15, 84, 15, 1, 8, 64, 8, 15, 78, 15})
            appendInt(expected, value);
        appendInt(expected, 0);
        appendInt(expected, 2);
        for (const int32_t value: {2, 0, 0, 0, 1, 0, 0, 1})
            appendInt(expected, value);
        require(bounds.save() == expected, "The structure record differs from the game's layout");

        ChunkStructureBounds loaded;
        require(loaded.load(expected) && loaded.save() == expected, "The structure record does not load back");
        require(loaded.namesAt(10, 72, 10) == bounds.namesAt(10, 72, 10), "A loaded record answers differently");
        require(!loaded.load(expected.substr(0, expected.size() - 2)) && loaded.empty(), "A cut record was accepted");
        require(ChunkStructureBounds().save().empty(), "An empty registry writes a record");

        const std::shared_ptr<const StructureTemplate> tower = StructureTemplate::load(
                "pillageroutpost/watchtower.nbt");
        require(tower != nullptr, "No watchtower template");
        PillagerOutpostStructure::Piece piece;
        piece.x = 100;
        piece.z = 200;
        piece.mQuarterTurns = 1;
        const Box turned = PillagerOutpostStructure::startBox(piece, *tower, 320);
        require(turned.minX == 100 - tower->getSizeZ() && turned.maxX == 100 && turned.minZ == 200
                        && turned.maxZ == 200 + tower->getSizeX() && turned.minY == 63 && turned.maxY == 320,
                "The turned outpost piece box differs");

        // a 15 by 9 template turned once spans x 86..100: its box reaches one block past it
        const Box odd{85, 63, 200, 100, 320, 209};
        const std::array<std::pair<int32_t, int32_t>, 5> points = PillagerOutpostStructure::groundPoints(odd);
        require(points[0] == std::make_pair(93, 205) && points[1] == std::make_pair(101, 205)
                        && points[2] == std::make_pair(93, 210) && points[3] == std::make_pair(85, 205)
                        && points[4] == std::make_pair(93, 200),
                "The outpost ground points differ from the game's box samples");
    }
}

namespace {
    std::shared_ptr<const JigsawPlacer::Template> slab(const JigsawPlacer::Jigsaw &jigsaw) {
        auto out = std::make_shared<JigsawPlacer::Template>();
        out->mSizeX = 3;
        out->mSizeY = 2;
        out->mSizeZ = 3;
        out->mJigsaws.push_back(jigsaw);
        return out;
    }

    JigsawPlacer::Jigsaw jigsawAt(int32_t x, int32_t z, JigsawPlacer::Facing front, const std::string &name,
                                  const std::string &target, const std::string &pool) {
        JigsawPlacer::Jigsaw jigsaw;
        jigsaw.x = x;
        jigsaw.z = z;
        jigsaw.mFront = front;
        jigsaw.mName = name;
        jigsaw.mTarget = target;
        jigsaw.mPool = pool;
        return jigsaw;
    }

    struct PlacerWorld {
        std::map<std::string, JigsawPlacer::Pool> mPools;
        std::map<std::string, std::shared_ptr<const JigsawPlacer::Template>> mTemplates;

        JigsawPlacer placer() const {
            return JigsawPlacer(
                    [](int32_t) { return 0; },
                    [this](const std::string &name) -> const JigsawPlacer::Pool * {
                        const auto found = mPools.find(name);
                        return found == mPools.end() ? nullptr : &found->second;
                    },
                    [this](const JigsawPlacer::Element &element) -> std::shared_ptr<const JigsawPlacer::Template> {
                        const auto found = mTemplates.find(element.mLocation);
                        return found == mTemplates.end() ? nullptr : found->second;
                    },
                    [](int32_t, int32_t, JigsawPlacer::Projection) { return 70; });
        }
    };

    void checkJigsawPlacer() {
        const JigsawPlacer::Element start{"start", "", false};
        const JigsawPlacer::Element child{"child", "", false};
        const JigsawPlacer::Element blocked{"blocked", "", false};
        const JigsawPlacer::Element cap{"cap", "", false};
        PlacerWorld world;
        world.mPools["minecraft:empty"] = JigsawPlacer::Pool{{}, "minecraft:empty"};
        world.mPools["test:start"] = JigsawPlacer::Pool{{&start}, "minecraft:empty"};
        world.mPools["test:child"] = JigsawPlacer::Pool{{&child}, "minecraft:empty"};
        world.mPools["test:blocked"] = JigsawPlacer::Pool{{&blocked}, "minecraft:empty"};
        world.mPools["test:main"] = JigsawPlacer::Pool{{&child}, "test:cap"};
        world.mPools["test:cap"] = JigsawPlacer::Pool{{&cap}, "minecraft:empty"};
        world.mTemplates["start"] = slab(jigsawAt(1, 0, 2, "s", "c", "test:child"));
        world.mTemplates["child"] = slab(jigsawAt(1, 2, 3, "c", "c", "test:missing"));
        world.mTemplates["blocked"] = slab(jigsawAt(1, 0, 3, "c", "c", "test:missing"));
        world.mTemplates["cap"] = slab(jigsawAt(1, 2, 3, "c", "c", "test:missing"));

        JigsawPlacer::Settings settings;
        settings.mStartPool = "test:start";
        settings.mMaxDepth = 2;
        settings.mStartX = 100;
        settings.mStartY = 64;
        settings.mStartZ = 200;
        settings.mMaxDistanceHorizontal = 80;
        settings.mMaxDistanceVertical = 80;
        settings.mDimensionMinY = -64;
        settings.mDimensionHeight = 384;

        JigsawPlacer::Result result;
        require(world.placer().place(settings, result), "The jigsaw placer refused a valid start");
        // the start drops by its ground level delta when it is not projected
        require(result.x == 101 && result.y == 64 && result.z == 201, "The jigsaw start position differs");
        require(result.mPieces.size() == 2, "The north jigsaw did not attach its child");
        const JigsawPlacer::Piece &first = result.mPieces[0];
        const JigsawPlacer::Piece &second = result.mPieces[1];
        require(first.y == 63 && first.mBox.mMinY == 63 && first.mBox.mMaxY == 64, "The start piece was not lowered");
        require(second.mElement == &child && second.mQuarterTurns == 0 && second.x == 100 && second.y == 63
                        && second.z == 197 && second.mBox.mMaxZ == 199 && second.mDepth == 1,
                "The child piece does not face the start's jigsaw");
        require(first.mJunctions.size() == 1 && first.mJunctions[0].x == 101 && first.mJunctions[0].y == 64
                        && first.mJunctions[0].z == 199 && first.mJunctions[0].mDeltaY == 0,
                "The start junction differs");
        require(second.mJunctions.size() == 1 && second.mJunctions[0].z == 200, "The child junction differs");

        settings.mMaxDepth = 0;
        require(world.placer().place(settings, result) && result.mPieces.empty(),
                "A max depth of 0 placed pieces");

        // a child overlapping the start's box is refused on every turn
        settings.mMaxDepth = 2;
        world.mTemplates["start"] = slab(jigsawAt(1, 0, 2, "s", "c", "test:blocked"));
        require(world.placer().place(settings, result) && result.mPieces.size() == 1,
                "A child inside the start's box was placed");

        // at the max depth only the fallback pool is drawn from
        settings.mMaxDepth = 1;
        world.mTemplates["start"] = slab(jigsawAt(1, 0, 2, "s", "c", "test:child"));
        auto branching = std::make_shared<JigsawPlacer::Template>(*slab(jigsawAt(1, 2, 3, "c", "c", "test:missing")));
        branching->mJigsaws.push_back(jigsawAt(1, 0, 2, "n", "c", "test:main"));
        world.mTemplates["child"] = branching;
        require(world.placer().place(settings, result) && result.mPieces.size() == 3
                        && result.mPieces[2].mElement == &cap && result.mPieces[2].mDepth == 2,
                "The max depth did not fall back to the fallback pool");

        // a branch never leaves the maximum distance from the start
        settings.mMaxDistanceHorizontal = 2;
        require(world.placer().place(settings, result) && result.mPieces.size() == 1,
                "A piece left the maximum distance");

        JigsawPlacer::Jigsaw up = jigsawAt(0, 0, 1, "a", "b", "");
        up.mAligned = true;
        up.mTop = 2;
        JigsawPlacer::Jigsaw down = jigsawAt(0, 0, 0, "b", "", "");
        down.mTop = 2;
        require(JigsawPlacer::canAttach(up, 0, down, 0), "An aligned vertical jigsaw refused its match");
        require(!JigsawPlacer::canAttach(up, 0, down, 1), "An aligned vertical jigsaw accepted a turned top");
        up.mAligned = false;
        require(JigsawPlacer::canAttach(up, 0, down, 1), "A rollable vertical jigsaw refused a turned top");

        const std::shared_ptr<const StructureTemplate> tower = StructureTemplate::load("trail_ruins/tower/tower_1.nbt");
        require(tower != nullptr, "No trail ruins tower template");
        const std::shared_ptr<const JigsawPlacer::Template> read = JigsawPlacer::templateOf(*tower);
        require(read->mJigsaws.size() == 5, "The tower's jigsaws were not all read");
        const JigsawPlacer::Jigsaw &west = read->mJigsaws[0];
        require(west.x == 0 && west.y == 1 && west.z == 2 && west.mFront == 4 && west.mTop == 1
                        && west.mName == "minecraft:road_anchor" && west.mTarget == "minecraft:road_hall_anchor"
                        && west.mPool == "minecraft:trail_ruins/tower/additions" && !west.mAligned
                        && west.mFinalState == "minecraft:cobblestone",
                "The tower's first jigsaw was misread");
        const JigsawPlacer::Jigsaw &top = read->mJigsaws[4];
        require(top.mFront == 1 && top.mTop == 2 && top.mAligned
                        && top.mPool == "minecraft:trail_ruins/tower/tower_top",
                "The tower's aligned top jigsaw was misread");
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkProcessors();
        checkStyles();
        checkVillage();
        checkBlockActors();
        checkConversions();
        checkMirror();
        checkMaterials();
        checkOceanRuins();
        checkPillagerOutposts();
        checkRuinedPortals();
        checkScatteredPieces();
        checkStructureBounds();
        checkJigsawPlacer();
    } catch (const std::exception &error) {
        std::clog << "Structure regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
