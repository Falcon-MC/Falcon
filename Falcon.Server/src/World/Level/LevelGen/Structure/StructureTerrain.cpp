#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

StructureTerrain::StructureTerrain(const ChunkGenerator &generator) : mGenerator(generator) {}

StructureTerrain::~StructureTerrain() = default;

int32_t StructureTerrain::getSolidHeight(int32_t x, int32_t z) {
    return ChunkGenerator::solidHeight(*_chunkAt(x, z), x & 15, z & 15);
}

int32_t StructureTerrain::getTopHeight(int32_t x, int32_t z) {
    // the chunk heightmap: a snow layer or a plant on top lets the street under it (sub_14775E430)
    const LevelChunk &chunk = *_chunkAt(x, z);
    int32_t y = LevelChunk::MAX_Y;
    while (y >= LevelChunk::MIN_Y) {
        const int32_t packed = BlockLightProperties::packed(chunk.getBlock(x & 15, y, z & 15));
        if (BlockLightProperties::lightFilter(packed) > 1 || BlockLightProperties::diffusesSkyLight(packed))
            break;
        y--;
    }

    return y + 1;
}

const BlockState &StructureTerrain::getBlock(int32_t x, int32_t y, int32_t z) {
    return _chunkAt(x, z)->getBlock(x & 15, y, z & 15);
}

const LevelChunk *StructureTerrain::_chunkAt(int32_t x, int32_t z) {
    std::unique_ptr<LevelChunk> &chunk = mChunks[{x >> 4, z >> 4}];
    if (chunk == nullptr)
        chunk = mGenerator.generateTerrain(x >> 4, z >> 4);

    return chunk.get();
}
