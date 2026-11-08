#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Surface/NoiseGradientSurfaceBuilder.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string nameAt(const NoiseGradientSurfaceBuilder &gradient, int32_t x, int32_t y, int32_t z) {
        const BlockState *block = gradient.blockAt(x, y, z);
        return block == nullptr ? std::string("none") : block->mName;
    }

    // BDS 1.26.61, seed 42, column 210,146
    void gradientMatchesTheGame() {
        const NoiseGradientSurfaceBuilder gradient = NoiseGradientSurfaceBuilder::sulfurCaves(42);
        for (int32_t y = -14; y <= -6; y++)
            require(nameAt(gradient, 210, y, 146) == "minecraft:sulfur", "sulfur at 210 " + std::to_string(y) + " 146");
        for (int32_t y = -20; y <= -17; y++)
            require(nameAt(gradient, 210, y, 146) == "minecraft:cinnabar",
                    "cinnabar at 210 " + std::to_string(y) + " 146");
        for (int32_t y = -5; y <= -1; y++)
            require(nameAt(gradient, 210, y, 146) == "none", "deepslate kept at 210 " + std::to_string(y) + " 146");
        require(nameAt(gradient, 210, -15, 146) == "none" && nameAt(gradient, 210, -16, 146) == "none",
                "deepslate kept between the sulfur and the cinnabar");
    }

    void buildKeepsWhatTheGameKeeps() {
        const NoiseGradientSurfaceBuilder gradient = NoiseGradientSurfaceBuilder::sulfurCaves(42);
        LevelChunk chunk(13, 9);
        const int32_t x = 210 & 15;
        const int32_t z = 146 & 15;
        const BlockState deepslate = VanillaBlocks::DEEPSLATE().toBlockState();
        for (int32_t y = -20; y <= -6; y++)
            chunk.setBlock(x, y, z, deepslate);
        chunk.setBlock(x, -10, z, VanillaBlocks::WATER().toBlockState());
        chunk.setBlock(x, -11, z, VanillaBlocks::AIR().toBlockState());
        chunk.setBlock(x, -12, z, VanillaBlocks::BEDROCK().toBlockState());
        chunk.setHeight(x, z, -8);

        gradient.build(chunk, x, z, -19, -6);
        require(chunk.getBlock(x, -20, z).mName == "minecraft:deepslate", "below the range is left");
        require(chunk.getBlock(x, -19, z).mName == "minecraft:cinnabar", "the range's bottom is built");
        require(chunk.getBlock(x, -13, z).mName == "minecraft:sulfur", "stone of the range is built");
        require(chunk.getBlock(x, -12, z).mName == "minecraft:bedrock", "bedrock is not replaceable");
        require(chunk.getBlock(x, -11, z).mName == "minecraft:air", "air is left");
        require(chunk.getBlock(x, -10, z).mName == "minecraft:water", "liquids are left");
        require(chunk.getBlock(x, -8, z).mName == "minecraft:sulfur", "the column's height is built");
        require(chunk.getBlock(x, -7, z).mName == "minecraft:deepslate", "above the column's height is left");

        chunk.setHeight(x, z, 0);
        gradient.build(chunk, x, z, -7, -6);
        require(chunk.getBlock(x, -7, z).mName == "minecraft:sulfur", "the range's bottom is its only block");
        require(chunk.getBlock(x, -6, z).mName == "minecraft:deepslate", "the range's top block is left");
    }
}

int main() {
    try {
        gradientMatchesTheGame();
        buildKeepsWhatTheGameKeeps();
        std::cout << "sulfur caves generation checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
