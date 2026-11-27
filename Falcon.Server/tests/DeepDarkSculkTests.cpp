#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the BDS 1.26.61 fixture has at least three catalysts and two sensors in the deep dark chunk
    void deepDarkPatchesGrowCatalystsAndSensors() {
        const int64_t seed = 1506020296095056367LL;
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                            seed);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(seed, DimensionType::Overworld, cache);
        std::vector<Tag> blockActors;
        std::vector<Tag> entities;
        const LevelChunk chunk = source.generate(0, 63, blockActors, entities);
        int catalystCount = 0;
        for (int32_t y = LevelChunk::MIN_Y; y < 0; ++y) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t x = 0; x < 16; ++x) {
                    if (chunk.getBlock(x, y, z).mName == "minecraft:sculk_catalyst")
                        ++catalystCount;
                }
            }
        }
        require(catalystCount >= 3, "expected at least three sculk catalysts below y=0");

        const auto requireBlock = [&](int32_t x, int32_t y, int32_t z, const std::string &name) {
            const std::string found = chunk.getBlock(x & 15, y, z & 15).mName;
            require(found == name, std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + " is "
                                           + found + " instead of " + name);
        };
        requireBlock(6, -38, 1021, "minecraft:sculk_sensor");
        requireBlock(6, -40, 1022, "minecraft:sculk_sensor");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();
    try {
        deepDarkPatchesGrowCatalystsAndSensors();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
