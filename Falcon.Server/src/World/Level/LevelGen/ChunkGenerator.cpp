#include "World/Level/LevelGen/ChunkGenerator.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/BlockStateSet.h"

int32_t ChunkGenerator::getPreliminaryHeight(int32_t x, int32_t z) const {
    const DimensionType dimension = getDimensionType();
    LevelChunk chunk(x >> 4, z >> 4);
    chunk.setDimension(dimension);
    generate(chunk);

    const int32_t localX = x & 15;
    const int32_t localZ = z & 15;
    const int32_t minY = Dimension::getMinY(dimension);
    int32_t y = Dimension::getMaxY(dimension);

    // only the nether skips its roof before looking for the surface (dimension vfunc +0xF0 in
    // sub_14A2298E0, read by sub_1435B0910)
    if (dimension == DimensionType::Nether) {
        while (y > minY && chunk.getBlock(localX, y, localZ).getHash() != getAirHash())
            y--;
    }

    while (y >= minY && chunk.getBlock(localX, y, localZ).getHash() == getAirHash())
        y--;

    return y + 1;
}

int32_t ChunkGenerator::getPreliminarySurfaceLevel(int32_t quartX, int32_t quartZ) const {
    return getPreliminaryHeight(quartX * 4, quartZ * 4);
}

std::array<int32_t, 256> ChunkGenerator::getPreliminaryHeights(int32_t chunkX, int32_t chunkZ) const {
    std::array<int32_t, 256> heights{};
    for (int32_t z = 0; z < 16; ++z) {
        for (int32_t x = 0; x < 16; ++x)
            heights[(size_t) ((z << 4) | x)] = getPreliminaryHeight(chunkX * 16 + x, chunkZ * 16 + z);
    }
    return heights;
}

int32_t ChunkGenerator::getPreliminaryLowestHeight(int32_t chunkX, int32_t chunkZ) const {
    return getPreliminaryHeight(chunkX * 16, chunkZ * 16);
}

std::unique_ptr<LevelChunk> ChunkGenerator::generateTerrain(int32_t chunkX, int32_t chunkZ) const {
    auto chunk = std::make_unique<LevelChunk>(chunkX, chunkZ);
    chunk->setDimension(getDimensionType());
    generate(*chunk);
    return chunk;
}

int32_t ChunkGenerator::solidHeight(const LevelChunk &chunk, int32_t localX, int32_t localZ) {
    static const BlockStateSet SKIPPED{"minecraft:air", "minecraft:water", "minecraft:flowing_water",
                                       "minecraft:lava", "minecraft:flowing_lava", "minecraft:snow_layer"};

    int32_t y = LevelChunk::MAX_Y;
    while (y >= LevelChunk::MIN_Y && SKIPPED.contains(chunk.getBlock(localX, y, localZ)))
        y--;

    return y + 1;
}

int32_t ChunkGenerator::getGeneratedSolidHeight(int32_t x, int32_t z) const {
    return solidHeight(*generateTerrain(x >> 4, z >> 4), x & 15, z & 15);
}
