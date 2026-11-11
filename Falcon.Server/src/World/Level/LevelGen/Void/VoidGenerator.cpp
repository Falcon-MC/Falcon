#include "World/Level/LevelGen/Void/VoidGenerator.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "Protocol/BlockStateHasher.h"

namespace {
    // sub_14C703FD0: a script dimension's default biome is minecraft:ocean
    const int32_t VOID_BIOME = BiomeIds::OCEAN;
}

VoidGenerator::VoidGenerator(DimensionType dimension, int64_t seed)
        : mDimension(dimension), mSeed(seed), mAirHash(BlockStateHasher::hash("minecraft:air")) {}

void VoidGenerator::generate(LevelChunk &chunk) const {
    for (int32_t x = 0; x < 16; ++x) {
        for (int32_t z = 0; z < 16; ++z)
            chunk.setHeight(x, z, LevelChunk::MIN_Y);
    }
    chunk.setBiome((uint32_t) VOID_BIOME);
    chunk.clearDirty();
}

void VoidGenerator::decorate(Level &, LevelChunk &, std::vector<GeneratedBlockChange> *) const {}

int VoidGenerator::getSpawnY() const {
    return 64;
}

int32_t VoidGenerator::pickBiome(int32_t, int32_t, int32_t) const {
    return VOID_BIOME;
}
