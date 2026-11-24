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

    int32_t kelpColumns(const LevelChunk &chunk) {
        int32_t columns = 0;
        for (int32_t x = 0; x < 16; x++) {
            for (int32_t z = 0; z < 16; z++) {
                for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y++) {
                    if (chunk.getBlock(x, y, z).mName == "minecraft:kelp") {
                        columns++;
                        break;
                    }
                }
            }
        }
        return columns;
    }

    // columns counted in a BDS world of seed 777 around the monument at chunk (47, -45): the kelp rule reads
    // variable.origin* where the seagrass scatter before it left them, not at the chunk origin
    void kelpFollowsTheSeagrassOrigin() {
        const int64_t seed = 777;
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                           seed);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(seed, DimensionType::Overworld, cache);
        for (int32_t decorationClass = 0; decorationClass < 4; decorationClass++) {
            for (int32_t chunkZ = -46; chunkZ <= -42; chunkZ++) {
                for (int32_t chunkX = 45; chunkX <= 49; chunkX++) {
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

        const int32_t full = kelpColumns(cache.neighbour(46, -44));
        require(full == 30, "chunk (46, -44) has " + std::to_string(full) + " kelp columns instead of 30");
        const int32_t few = kelpColumns(cache.neighbour(48, -44));
        require(few == 7, "chunk (48, -44) has " + std::to_string(few) + " kelp columns instead of 7");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();
    try {
        kelpFollowsTheSeagrassOrigin();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
