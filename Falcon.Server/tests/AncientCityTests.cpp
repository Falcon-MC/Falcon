#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    // parity seed s5: BDS 1.26.61 places an ancient city around block 872, 904, its centre piece in chunk 54, 56
    const int64_t SEED = 1506020296095056367LL;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a city piece empties its box over its ground, fills under it, and reaches 5 blocks past its radius above
    // the ground
    void shaverAroundPiece() {
        TerrainBeard beard;
        beard.addPiece(0, 0, {0, -51, 0, 9, -30, 9, 0, TerrainBeard::Adaptation::BeardAndShaver});
        require(beard.compute(5, -40, 5) < 0.0, "the inside of the piece is not emptied");
        require(beard.compute(5, -56, 5) > 0.0, "the ground under the piece is not filled");
        require(beard.compute(13, -40, 5) < 0.0, "the shaver stops at the radius above the ground");
        require(beard.compute(30, -40, 5) == 0.0, "the shaver reaches past the kernel");
    }

    void cityAtTheBdsStart() {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(DimensionType::Overworld,
                                                                                         SEED);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(SEED, DimensionType::Overworld, cache);
        int32_t reinforced = 0;
        int32_t bricks = 0;
        for (int32_t chunkZ = 55; chunkZ <= 57; chunkZ++) {
            for (int32_t chunkX = 53; chunkX <= 54; chunkX++) {
                std::vector<Tag> blockActors;
                std::vector<Tag> entities;
                const LevelChunk chunk = source.generate(chunkX, chunkZ, blockActors, entities);
                for (int32_t x = 0; x < 16; x++) {
                    for (int32_t z = 0; z < 16; z++) {
                        for (int32_t y = -55; y <= -15; y++) {
                            const std::string &name = chunk.getBlock(x, y, z).mName;
                            reinforced += name == "minecraft:reinforced_deepslate";
                            bricks += name == "minecraft:deepslate_bricks";
                        }
                    }
                }
            }
        }
        require(reinforced > 0, "the city centre has no reinforced deepslate");
        require(bricks > 100, "the city centre has no deepslate bricks");
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        shaverAroundPiece();
        cityAtTheBdsStart();
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: ancient city\n";
    return 0;
}
