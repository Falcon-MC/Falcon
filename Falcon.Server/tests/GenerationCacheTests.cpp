#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
    const int64_t SEED = -1051743253566878335LL;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    uint64_t mix(uint64_t hash, uint64_t value) {
        return hash ^ (value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
    }

    // every block of the decorated area, in the workers' decoration order
    uint64_t generateArea(int32_t originX, int32_t originZ, int32_t size) {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         SEED);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(SEED, DimensionType::Overworld, cache);
        for (int32_t chunkZ = originZ; chunkZ < originZ + size; chunkZ++) {
            for (const int32_t pass: {0, 1}) {
                for (int32_t chunkX = originX; chunkX < originX + size; chunkX++) {
                    if ((TerrainCache::decorationClass(chunkX, chunkZ) & 1) != pass)
                        continue;
                    LevelChunk chunk(chunkX, chunkZ);
                    chunk.setDimension(DimensionType::Overworld);
                    std::vector<GeneratedBlockChange> overflow;
                    std::vector<Tag> blockActors;
                    std::vector<Tag> entities;
                    uint64_t base = 0;
                    source.populateGenerated(chunk, {}, overflow, blockActors, entities, base);
                    cache.put(std::move(chunk));
                }
            }
        }

        uint64_t hash = 0;
        for (int32_t chunkZ = originZ; chunkZ < originZ + size; chunkZ++) {
            for (int32_t chunkX = originX; chunkX < originX + size; chunkX++) {
                const LevelChunk chunk = cache.neighbour(chunkX, chunkZ);
                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y++) {
                            hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 0).getHash());
                            hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 1).getHash());
                        }
                    }
                }
            }
        }
        return hash;
    }

    // the per-thread caches of cave sources, structure candidates and mineshaft rolls: an area generated with
    // them cold, on a new thread, and warm from an overlapping area comes out block for block the same
    void cachesKeepTheWorld() {
        uint64_t cold = 0;
        std::thread([&cold]() { cold = generateArea(40, -12, 4); }).join();

        uint64_t warm = 0;
        std::thread([&warm]() {
            generateArea(36, -14, 6);
            warm = generateArea(40, -12, 4);
        }).join();
        require(cold != 0, "the area hashed to nothing");
        require(cold == warm, "the generation caches changed the world");
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        cachesKeepTheWorld();
        std::cout << "generation cache checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
    return 0;
}
