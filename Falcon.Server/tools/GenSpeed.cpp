// times chunk generation on one thread and prints a hash of every generated block: a change that speeds the
// generators up must leave the hash as it was
//
// usage: FalconGenSpeed [--seed <int64>] [--at x,z] [--size n] [--repeat n]
//   --at: lowest chunk of the area, --size: side in chunks; terrain then decoration, in the workers' order

#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using Clock = std::chrono::steady_clock;

    double millisecondsSince(Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    uint64_t mix(uint64_t hash, uint64_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        return hash;
    }

    uint64_t hashChunk(const LevelChunk &chunk) {
        uint64_t hash = 0;
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y++) {
                    hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 0).getHash());
                    hash = mix(hash, (uint32_t) chunk.getBlock(x, y, z, 1).getHash());
                }
            }
        }
        return hash;
    }

    struct Run {
        double mTerrain = 0.0;
        double mDecoration = 0.0;
        uint64_t mHash = 0;
    };

    Run run(int64_t seed, int32_t originX, int32_t originZ, int32_t size) {
        Run result;
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         seed);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(seed, DimensionType::Overworld, cache);

        // the 3x3 of every decorated chunk: one ring more than the decorated area
        const Clock::time_point terrainStart = Clock::now();
        for (int32_t chunkZ = originZ - 1; chunkZ <= originZ + size; chunkZ++) {
            for (int32_t chunkX = originX - 1; chunkX <= originX + size; chunkX++)
                cache.sharedTerrain(chunkX, chunkZ);
        }
        result.mTerrain = millisecondsSince(terrainStart);

        const Clock::time_point decorationStart = Clock::now();
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
        result.mDecoration = millisecondsSince(decorationStart);

        for (int32_t chunkZ = originZ; chunkZ < originZ + size; chunkZ++) {
            for (int32_t chunkX = originX; chunkX < originX + size; chunkX++)
                result.mHash = mix(result.mHash, hashChunk(cache.neighbour(chunkX, chunkZ)));
        }
        return result;
    }
}

int main(int argc, char **argv) {
    int64_t seed = -1051743253566878335LL;
    int32_t originX = 1250;
    int32_t originZ = 1250;
    int32_t size = 8;
    int32_t repeat = 3;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        const std::string value = argv[i + 1];
        if (flag == "--seed")
            seed = std::strtoll(value.c_str(), nullptr, 10);
        else if (flag == "--at")
            std::sscanf(value.c_str(), "%d,%d", &originX, &originZ);
        else if (flag == "--size")
            size = std::max(1, std::atoi(value.c_str()));
        else if (flag == "--repeat")
            repeat = std::max(1, std::atoi(value.c_str()));
    }

    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();

    const int32_t terrainChunks = (size + 2) * (size + 2);
    const int32_t decoratedChunks = size * size;
    double bestTerrain = 1e30;
    double bestDecoration = 1e30;
    uint64_t hash = 0;
    for (int32_t i = 0; i < repeat; i++) {
        const Run result = run(seed, originX, originZ, size);
        if (i > 0 && result.mHash != hash) {
            std::printf("hash changed between runs: %016llx then %016llx\n", (unsigned long long) hash,
                        (unsigned long long) result.mHash);
            return 1;
        }
        hash = result.mHash;
        bestTerrain = std::min(bestTerrain, result.mTerrain / terrainChunks);
        bestDecoration = std::min(bestDecoration, result.mDecoration / decoratedChunks);
        std::printf("run %d: terrain %.2f ms/chunk, decoration %.2f ms/chunk\n", i + 1,
                    result.mTerrain / terrainChunks, result.mDecoration / decoratedChunks);
    }
    std::printf("best: terrain %.2f ms/chunk, decoration %.2f ms/chunk, total %.2f ms per decorated chunk\n",
                bestTerrain, bestDecoration, bestTerrain * terrainChunks / decoratedChunks + bestDecoration);
    std::printf("hash %016llx\n", (unsigned long long) hash);
    return 0;
}
