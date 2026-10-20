#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

int64_t NetherFeature::chunkHash(int32_t chunkX, int32_t chunkZ) {
    return (int64_t) (((uint64_t) (uint32_t) chunkX << 32) | (uint64_t) (uint32_t) chunkZ);
}

bool NetherFeature::isAir(const BlockState &state) {
    return state.mName == "minecraft:air";
}

bool NetherFeature::isName(const BlockState &state, const char *identifier) {
    return state.mName == identifier;
}

bool NetherFeature::isSolid(const BlockState &state) {
    return VanillaTreeObject::isSolid(state);
}

bool NetherFeature::canBeReplaced(const BlockState &state) {
    return VanillaTreeObject::canBeReplaced(state);
}

int32_t NetherFeature::getBiomeIdAt(Level &level, int32_t x, int32_t z) {
    LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return -1;

    return (int32_t) chunk->getBiomeAt(x & 15, 0, z & 15);
}

void NetherFeature::seedForChunk(Level &level, int32_t chunkX, int32_t chunkZ) {
    mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ));
}
