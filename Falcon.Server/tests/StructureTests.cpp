#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"
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
        for (const StructureProcessors::Rule &rule : StructureProcessors::get(name))
            out.push_back(&rule);
        return out;
    }

    // a seed whose first draw, as Random::nextFloat reads it, is below or above the probability
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

        // a block of another name never reaches the random
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

        // every rule runs on what the earlier ones left: a pane that stays glass is then browned
        const auto decay = rules("minecraft:abandoned_plains_decay");
        std::mt19937 web(seedDrawing(0.5f, true));
        require(StructureProcessors::apply(decay, BlockState("minecraft:glass_pane"), air, web).mName
                        == "minecraft:web",
                "The pane did not turn into a web");
        std::mt19937 brown(seedDrawing(0.5f, false));
        require(StructureProcessors::apply(decay, BlockState("minecraft:glass_pane"), air, brown).mName
                        == "minecraft:brown_stained_glass_pane",
                "The pane kept its colour");

        // the location test reads the world: a street over water becomes a bridge, which the later water rule no
        // longer matches
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

    void checkVillage() {
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 42);
        const std::mt19937 stream = StructureFeature::createPostProcessRandom(42, -92, -27);
        require(stream == std::mt19937(StructureFeature::getPostProcessSeed(42, -92, -27)),
                "The post process seed does not seed the post process random");

        // seed 42 has a plains village at chunk -92, -27, whose centre BDS 1.26.61 places as meeting point 2
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
        for (const auto &entry : a.mBlocks) {
            require(std::get<0>(entry.first) >> 4 == -92 && std::get<2>(entry.first) >> 4 == -27,
                    "A block left its chunk");
        }
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkProcessors();
        checkVillage();
    } catch (const std::exception &error) {
        std::clog << "Structure regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
