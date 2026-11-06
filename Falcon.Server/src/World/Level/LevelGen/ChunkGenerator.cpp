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

int32_t ChunkGenerator::getPreliminaryLowestHeight(int32_t chunkX, int32_t chunkZ) const {
    return getPreliminaryHeight(chunkX * 16, chunkZ * 16);
}

int32_t ChunkGenerator::getGeneratedSolidHeight(int32_t x, int32_t z) const {
    static const BlockStateSet SKIPPED{"minecraft:air", "minecraft:water", "minecraft:flowing_water",
                                       "minecraft:lava", "minecraft:flowing_lava", "minecraft:snow_layer"};

    LevelChunk chunk(x >> 4, z >> 4);
    chunk.setDimension(getDimensionType());
    generate(chunk);

    int32_t y = LevelChunk::MAX_Y;
    while (y >= LevelChunk::MIN_Y && SKIPPED.contains(chunk.getBlock(x & 15, y, z & 15)))
        y--;

    return y + 1;
}
