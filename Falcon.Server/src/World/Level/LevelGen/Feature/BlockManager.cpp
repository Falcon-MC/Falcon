#include "World/Level/LevelGen/Feature/BlockManager.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/SubChunk.h"

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
    return _getBlockAt(x, y, z, 0);
}

const BlockState &BlockManager::getExtraBlockAt(int32_t x, int32_t y, int32_t z) {
    return _getBlockAt(x, y, z, 1);
}

const BlockState &BlockManager::_getBlockAt(int32_t x, int32_t y, int32_t z, int32_t layer) {
    const int64_t key = hashXYZ(x, y, z) | layer;

    const std::unordered_map<int64_t, const BlockState *>::const_iterator cached = mCaches.find(key);
    if (cached != mCaches.end())
        return *cached->second;

    if (mPending != nullptr) {
        const BlockState *pending = mPending->getCachedBlock(x, y, z, layer);
        if (pending != nullptr)
            return *pending;
    }

    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return airState();

    LevelChunk *chunk = mLevel->peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return airState();

    return chunk->getBlock(x & 15, y, z & 15, layer);
}

const BlockState *BlockManager::getCachedBlock(int32_t x, int32_t y, int32_t z, int32_t layer) const {
    const std::unordered_map<int64_t, const BlockState *>::const_iterator cached = mCaches.find(hashXYZ(x, y, z) | layer);
    if (cached == mCaches.end())
        return nullptr;

    return cached->second;
}

void BlockManager::setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    const int64_t key = hashXYZ(x, y, z);
    const BlockState *interned = SubChunk::intern(state);
    mPlaces[key] = interned;
    mCaches[key] = interned;
}

void BlockManager::setExtraBlockAt(int32_t x, int32_t y, int32_t z, const BlockState &state) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    const int64_t key = hashXYZ(x, y, z) | 1;
    const BlockState *interned = SubChunk::intern(state);
    mPlaces[key] = interned;
    mCaches[key] = interned;
}

bool BlockManager::isCached(int32_t x, int32_t y, int32_t z) const {
    return mCaches.find(hashXYZ(x, y, z)) != mCaches.end();
}

void BlockManager::merge(const BlockManager &other) {
    for (const std::pair<const int64_t, const BlockState *> &entry: other.mPlaces) {
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
    for (const std::pair<const int64_t, const BlockState *> &entry: mPlaces) {
        const int64_t key = entry.first;
        const int32_t x = (int32_t) ((key >> 37) & 0x3FFFFFFLL) - 30000000;
        const int32_t z = (int32_t) ((key >> 11) & 0x3FFFFFFLL) - 30000000;
        const int32_t y = (int32_t) ((key >> 1) & 0x3FFLL) - 400;
        const int32_t layer = (int32_t) (key & 1);

        if (mOverflowSink != nullptr && ((x >> 4) != mCenterChunkX || (z >> 4) != mCenterChunkZ)) {
            GeneratedBlockChange change;
            change.mX = x;
            change.mY = y;
            change.mZ = z;
            change.mLayer = layer;
            change.mState = entry.second;
            mOverflowSink->push_back(change);
            continue;
        }

        LevelChunk *chunk = mLevel->peekChunkPtr(x >> 4, z >> 4);
        if (chunk == nullptr)
            continue;

        chunk->setBlock(x & 15, y, z & 15, layer, *entry.second);
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
