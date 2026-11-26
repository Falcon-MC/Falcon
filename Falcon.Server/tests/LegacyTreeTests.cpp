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

    // chunks with every decoration reaching them, as the generation workers make them
    struct DecoratedArea {
        explicit DecoratedArea(int64_t seed)
                : mGenerator(DimensionFactory::createGenerator(DimensionType::Overworld, seed)), mCache(*mGenerator),
                  mSource(seed, DimensionType::Overworld, mCache) {}

        // the logs stacked from the block up
        int32_t trunkAt(int32_t x, int32_t y, int32_t z, const std::string &log) {
            std::vector<Tag> blockActors;
            std::vector<Tag> entities;
            const LevelChunk chunk = mSource.generate(x >> 4, z >> 4, blockActors, entities);
            if (chunk.getBlock(x & 15, y - 1, z & 15).mName == log)
                return 0;
            int32_t height = 0;
            while (chunk.getBlock(x & 15, y + height, z & 15).mName == log)
                height++;
            return height;
        }

        std::unique_ptr<ChunkGenerator> mGenerator;
        TerrainCache mCache;
        GeneratorChunkSource mSource;
    };

    void requireTrunk(DecoratedArea &area, int32_t x, int32_t y, int32_t z, const std::string &log, int32_t height) {
        const int32_t found = area.trunkAt(x, y, z, log);
        require(found == height, log + " at " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z)
                                         + " is " + std::to_string(found) + " high instead of "
                                         + std::to_string(height));
    }

    // trunks of a BDS 1.26.61 world of seed -265159075239982727 in chunk (-12, -15), a mutated birch forest:
    // super birch trees grow up to 15 logs, the plain birch at most 7
    void mutatedBirchForestGrowsSuperBirches() {
        DecoratedArea area(-265159075239982727LL);
        requireTrunk(area, -187, 69, -228, "minecraft:birch_log", 12);
        requireTrunk(area, -190, 69, -226, "minecraft:birch_log", 6);
        requireTrunk(area, -183, 68, -233, "minecraft:birch_log", 8);
    }

    // trunks of a BDS 1.26.61 world of seed 8242991190943039654 in chunk (82, 113), a flower forest
    void flowerForestGrowsItsTrees() {
        DecoratedArea area(8242991190943039654LL);
        requireTrunk(area, 1314, 66, 1809, "minecraft:oak_log", 11);
        requireTrunk(area, 1313, 66, 1815, "minecraft:birch_log", 5);
        requireTrunk(area, 1319, 67, 1816, "minecraft:oak_log", 6);
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();
    try {
        mutatedBirchForestGrowsSuperBirches();
        flowerForestGrowsItsTrees();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
