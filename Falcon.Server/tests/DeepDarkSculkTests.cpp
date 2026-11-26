#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
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

    // blocks of a BDS 1.26.61 world of seed 1506020296095056367 in chunk (0, 63), in the deep dark: the patches
    // end on a catalyst one time in two and grow sensors as their charge runs out
    void deepDarkPatchesGrowCatalystsAndSensors() {
        const int64_t seed = 1506020296095056367LL;
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                            seed);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(seed, DimensionType::Overworld, cache);
        for (int32_t decorationClass = 0; decorationClass < 4; decorationClass++) {
            for (int32_t chunkZ = 62; chunkZ <= 64; chunkZ++) {
                for (int32_t chunkX = -1; chunkX <= 1; chunkX++) {
                    if (TerrainCache::decorationClass(chunkX, chunkZ) != decorationClass)
                        continue;
                    LevelChunk chunk(chunkX, chunkZ);
                    chunk.setDimension(DimensionType::Overworld);
                    std::vector<GeneratedBlockChange> overflow;
                    std::vector<Tag> blockActors;
                    std::vector<Tag> entities;
                    uint64_t base = 0;
                    source.populateGenerated(chunk, {}, overflow, blockActors, entities, base);
                }
            }
        }

        const LevelChunk chunk = cache.neighbour(0, 63);
        const auto requireBlock = [&](int32_t x, int32_t y, int32_t z, const std::string &name) {
            const std::string found = chunk.getBlock(x & 15, y, z & 15).mName;
            require(found == name, std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + " is "
                                           + found + " instead of " + name);
        };
        requireBlock(5, -41, 1022, "minecraft:sculk_catalyst");
        requireBlock(6, -14, 1022, "minecraft:sculk_catalyst");
        requireBlock(12, -32, 1021, "minecraft:sculk_catalyst");
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
