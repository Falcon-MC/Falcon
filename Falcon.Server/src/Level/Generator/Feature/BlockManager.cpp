#include "Level/Generator/Feature/BlockManager.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {
    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }
}

BlockManager::BlockManager(Level &level) : mLevel(&level) {
}

int64_t BlockManager::hashXYZ(int32_t x, int32_t y, int32_t z) {
    return (((int64_t) (x + 30000000) & 0x3FFFFFFLL) << 37)
           | (((int64_t) (z + 30000000) & 0x3FFFFFFLL) << 11)
           | (((int64_t) (y + 400) & 0x3FFLL) << 1);
}

const BlockState &BlockManager::getBlockAt(int32_t x, int32_t y, int32_t z) {
    const int64_t key = hashXYZ(x, y, z);

    const std::unordered_map<int64_t, BlockState>::const_iterator cached = mCaches.find(key);
    if (cached != mCaches.end())
        return cached->second;

    if (mPending != nullptr) {
        const BlockState *pending = mPending->getCachedBlock(x, y, z);
        if (pending != nullptr)
            return *pending;
    }

    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return airState();

    LevelChunk *chunk = mLevel->peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return airState();

    return chunk->getBlock(x & 15, y, z & 15);
}

const BlockState *BlockManager::getCachedBlock(int32_t x, int32_t y, int32_t z) const {
    const std::unordered_map<int64_t, BlockState>::const_iterator cached = mCaches.find(hashXYZ(x, y, z));
    if (cached == mCaches.end())
        return nullptr;

    return &cached->second;
}

void BlockManager::setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    const int64_t key = hashXYZ(x, y, z);
    mPlaces[key] = state;
    mCaches[key] = state;
}

bool BlockManager::isCached(int32_t x, int32_t y, int32_t z) const {
    return mCaches.find(hashXYZ(x, y, z)) != mCaches.end();
}

void BlockManager::merge(const BlockManager &other) {
    for (const std::pair<const int64_t, BlockState> &entry: other.mPlaces) {
        mPlaces[entry.first] = entry.second;
        mCaches[entry.first] = entry.second;
    }
}

void BlockManager::setOverflowSink(int32_t centerChunkX, int32_t centerChunkZ,
                                   std::vector<GeneratedBlockChange> *sink) {
    mCenterChunkX = centerChunkX;
    mCenterChunkZ = centerChunkZ;
    mOverflowSink = sink;
}

void BlockManager::applySubChunkUpdate() {
    for (const std::pair<const int64_t, BlockState> &entry: mPlaces) {
        const int64_t key = entry.first;
        const int32_t x = (int32_t) ((key >> 37) & 0x3FFFFFFLL) - 30000000;
        const int32_t z = (int32_t) ((key >> 11) & 0x3FFFFFFLL) - 30000000;
        const int32_t y = (int32_t) ((key >> 1) & 0x3FFLL) - 400;

        if (mOverflowSink != nullptr && ((x >> 4) != mCenterChunkX || (z >> 4) != mCenterChunkZ)) {
            GeneratedBlockChange change;
            change.mX = x;
            change.mY = y;
            change.mZ = z;
            change.mState = entry.second;
            mOverflowSink->push_back(change);
            continue;
        }

        LevelChunk *chunk = mLevel->peekChunkPtr(x >> 4, z >> 4);
        if (chunk == nullptr)
            continue;

        chunk->setBlock(x & 15, y, z & 15, entry.second);
    }

    mPlaces.clear();
    mCaches.clear();
}

int32_t BlockManager::getHeightAt(int32_t x, int32_t z) {
    LevelChunk *chunk = mLevel->peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return LevelChunk::MIN_Y;

    return chunk->getHeight(x & 15, z & 15);
}
