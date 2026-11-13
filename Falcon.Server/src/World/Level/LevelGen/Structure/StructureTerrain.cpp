#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"

StructureTerrain::StructureTerrain(const ChunkGenerator &generator, TerrainCache *cache)
        : mGenerator(generator), mCache(cache) {}

StructureTerrain::~StructureTerrain() = default;

int32_t StructureTerrain::getSolidHeight(int32_t x, int32_t z) {
    return ChunkGenerator::solidHeight(*_chunkAt(x, z).mChunk, x & 15, z & 15);
}

int32_t StructureTerrain::getTopHeight(int32_t x, int32_t z) {
    Chunk &chunk = _chunkAt(x, z);
    int16_t &top = chunk.mTops[(size_t) (((z & 15) << 4) | (x & 15))];
    if (top != INT16_MIN)
        return top;

    // the chunk heightmap: a snow layer or a plant on top lets the street under it
    int32_t y = LevelChunk::MAX_Y;
    while (y >= LevelChunk::MIN_Y) {
        const int32_t packed = BlockLightProperties::packed(chunk.mChunk->getBlock(x & 15, y, z & 15));
        if (BlockLightProperties::lightFilter(packed) > 1 || BlockLightProperties::diffusesSkyLight(packed))
            break;
        y--;
    }

    top = (int16_t) (y + 1);
    return top;
}

const BlockState &StructureTerrain::getBlock(int32_t x, int32_t y, int32_t z) {
    return _chunkAt(x, z).mChunk->getBlock(x & 15, y, z & 15);
}

StructureTerrain::Chunk &StructureTerrain::_chunkAt(int32_t x, int32_t z) {
    Chunk &chunk = mChunks[{x >> 4, z >> 4}];
    if (chunk.mChunk == nullptr) {
        chunk.mChunk = mCache != nullptr
                               ? mCache->sharedTerrain(x >> 4, z >> 4)
                               : std::shared_ptr<const LevelChunk>(mGenerator.generateTerrain(x >> 4, z >> 4));
        chunk.mTops.fill(INT16_MIN);
    }

    return chunk;
}
