#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class Level;

class BlockManager {
public:
    explicit BlockManager(Level &level);

    virtual ~BlockManager() = default;

    const BlockState &getBlockAt(int32_t x, int32_t y, int32_t z);

    // the second block of the position, the water of a waterlogged block
    const BlockState &getExtraBlockAt(int32_t x, int32_t y, int32_t z);

    const BlockState *getCachedBlock(int32_t x, int32_t y, int32_t z, int32_t layer = 0) const;

    virtual void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state);

    void setExtraBlockAt(int32_t x, int32_t y, int32_t z, const BlockState &state);

    bool isCached(int32_t x, int32_t y, int32_t z) const;

    void merge(const BlockManager &other);

    void applySubChunkUpdate();

    void setOverflowSink(int32_t centerChunkX, int32_t centerChunkZ, std::vector<GeneratedBlockChange> *sink);

    void readPendingFrom(const BlockManager *pending) {
        mPending = pending;
    }

    int32_t getHeightAt(int32_t x, int32_t z);

    Level &getLevel() const { return *mLevel; }

    static int64_t hashXYZ(int32_t x, int32_t y, int32_t z);

protected:
    const BlockState &_getBlockAt(int32_t x, int32_t y, int32_t z, int32_t layer);

    Level *mLevel;
    // keyed by hashXYZ with the layer in the lowest bit; the states are interned (SubChunk::intern), so
    // recording a block copies no state and placing it later finds the palette entry by address
    std::unordered_map<int64_t, const BlockState *> mCaches;
    std::unordered_map<int64_t, const BlockState *> mPlaces;
    std::vector<GeneratedBlockChange> *mOverflowSink = nullptr;
    const BlockManager *mPending = nullptr;
    int32_t mCenterChunkX = 0;
    int32_t mCenterChunkZ = 0;
};
