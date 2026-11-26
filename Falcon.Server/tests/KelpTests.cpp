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

    // chunks with every decoration reaching them, as the generation workers make them
    struct DecoratedArea {
        explicit DecoratedArea(int64_t seed)
                : mGenerator(DimensionFactory::createGenerator(DimensionType::Overworld, seed)), mCache(*mGenerator),
                  mSource(seed, DimensionType::Overworld, mCache) {}

        int32_t kelpAt(int32_t chunkX, int32_t chunkZ) {
            std::vector<Tag> blockActors;
            std::vector<Tag> entities;
            return kelpColumns(mSource.generate(chunkX, chunkZ, blockActors, entities));
        }

        std::unique_ptr<ChunkGenerator> mGenerator;
        TerrainCache mCache;
        GeneratorChunkSource mSource;
    };

    // columns counted in a BDS world of seed 777 around the monument at chunk (47, -45): the kelp rule right after
    // the seagrass scatter reads variable.origin* where the scatter left them
    void kelpFollowsTheSeagrassOrigin() {
        DecoratedArea area(777);
        const int32_t full = area.kelpAt(46, -44);
        require(full == 30, "chunk (46, -44) has " + std::to_string(full) + " kelp columns instead of 30");
        const int32_t few = area.kelpAt(48, -44);
        require(few == 7, "chunk (48, -44) has " + std::to_string(few) + " kelp columns instead of 7");
    }

    // a BDS 1.26.61 world of seed 5002596503772886084: with the pumpkin and brown mushroom rules between the
    // seagrass and the kelp, the deep oceans read the kelp noise at the chunk origin
    void kelpReadsTheChunkOrigin() {
        DecoratedArea area(5002596503772886084LL);
        const int32_t none = area.kelpAt(61, 64);
        require(none == 0, "chunk (61, 64) has " + std::to_string(none) + " kelp columns instead of none");
        const int32_t many = area.kelpAt(63, 65);
        // 68 in the BDS world, 62 of them grow here
        require(many >= 60, "chunk (63, 65) has " + std::to_string(many) + " kelp columns instead of about 68");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();
    BiomeChunkGenDataRegistry::initialize();
    try {
        kelpFollowsTheSeagrassOrigin();
        kelpReadsTheChunkOrigin();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
