#include "World/Level/LevelGen/ChunkGenerator.h"

#include "World/Level/Chunk/LevelChunk.h"

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
