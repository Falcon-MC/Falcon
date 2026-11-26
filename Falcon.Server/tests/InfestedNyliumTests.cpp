#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // every chunk of a size x size area with all its decorations, handed to check one by one
    void generateArea(DimensionType dimension, int64_t seed, int32_t originX, int32_t originZ, int32_t size,
                      const std::function<void(const LevelChunk &)> &check) {
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(dimension, seed);
        TerrainCache cache(*generator);
        GeneratorChunkSource source(seed, dimension, cache);
        for (int32_t chunkZ = originZ; chunkZ < originZ + size; chunkZ++) {
            for (int32_t chunkX = originX; chunkX < originX + size; chunkX++) {
                std::vector<Tag> blockActors;
                std::vector<Tag> entities;
                check(source.generate(chunkX, chunkZ, blockActors, entities));
            }
        }
    }

    // the silverfish veins after the surface: the game's jagged peaks at -218,190 of this seed hold infested stone
    void infestedStoneInMountains() {
        int32_t stone = 0;
        int32_t deepslate = 0;
        generateArea(DimensionType::Overworld, -265159075239982727LL, -219, 189, 3, [&](const LevelChunk &chunk) {
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    for (int32_t y = LevelChunk::MIN_Y; y <= 64; y++) {
                        const std::string &name = chunk.getBlock(x, y, z).mName;
                        stone += name == "minecraft:infested_stone" ? 1 : 0;
                        deepslate += name == "minecraft:infested_deepslate" ? 1 : 0;
                    }
                }
            }
        });
        require(stone > 0, "no infested stone in the jagged peaks");
        require(deepslate > 0, "no infested deepslate in the jagged peaks");
    }

    // the block right under the bedrock roof of a crimson or warped forest is nylium
    void nyliumUnderTheRoof() {
        int32_t columns = 0;
        int32_t nylium = 0;
        generateArea(DimensionType::Nether, -9223204536751074085LL, 2, -1, 3, [&](const LevelChunk &chunk) {
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    int32_t y = 127;
                    while (y > 0 && chunk.getBlock(x, y, z).mName == "minecraft:bedrock")
                        y--;
                    const std::string &name = chunk.getBlock(x, y, z).mName;
                    if (name == "minecraft:air")
                        continue;
                    columns++;
                    nylium += name == "minecraft:crimson_nylium" || name == "minecraft:warped_nylium" ? 1 : 0;
                }
            }
        });
        require(columns > 0 && nylium * 10 >= columns * 9,
                "only " + std::to_string(nylium) + " of " + std::to_string(columns)
                        + " columns have nylium under the roof");
    }
}

int main() {
    try {
        BlockPaletteRegistry::getInstance().initialize();
        BiomeChunkGenDataRegistry::initialize();
        infestedStoneInMountains();
        nyliumUnderTheRoof();
        std::cout << "infested stone and roof nylium checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
    return 0;
}
