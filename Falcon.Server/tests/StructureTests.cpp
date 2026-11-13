#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Structure/OceanRuinFeature.h"
#include "World/Level/LevelGen/Structure/OceanRuinStructure.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"
#include "World/Level/LevelGen/Structure/VillageStructure.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

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
        require(BlockMaterial::find(BlockState("minecraft:cobblestone_wall"), type)
                        && type == BlockMaterialType::Solid,
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
        require(!StructureMaterials::stopsTopSolidSearch(BlockState("minecraft:short_grass"), true, true),
                "grass never stops");
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

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkProcessors();
        checkStyles();
        checkVillage();
        checkBlockActors();
        checkConversions();
        checkMaterials();
        checkOceanRuins();
    } catch (const std::exception &error) {
        std::clog << "Structure regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
