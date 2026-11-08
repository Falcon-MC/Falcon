#pragma once

#include "World/Level/Chunk/LightStorage.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Chunk/SubChunk.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// fresh on construction and on every copy: a copy can diverge from its source
struct LevelChunkInstance {
    LevelChunkInstance() : mId(next()) {}

    LevelChunkInstance(const LevelChunkInstance &) : mId(next()) {}

    LevelChunkInstance &operator=(const LevelChunkInstance &) {
        mId = next();
        return *this;
    }

    static uint64_t next();

    uint64_t mId;
};

// main thread once resident; workers only see copies; network encodings cached per sub-chunk
class LevelChunk {
public:
    static constexpr int SUB_CHUNK_COUNT = 24;
    static constexpr int LOWEST_SUB_CHUNK_Y = -4;
    static constexpr int MIN_Y = LOWEST_SUB_CHUNK_Y * 16;
    static constexpr int MAX_Y = MIN_Y + SUB_CHUNK_COUNT * 16 - 1;
    static constexpr uint8_t STORAGE_VERSION = 40;

    LevelChunk(int32_t x, int32_t z);

    int32_t getX() const { return mX; }

    int32_t getZ() const { return mZ; }

    bool isDirty() const { return mDirty; }

    void clearDirty() { mDirty = false; }

    void markDirty() { mDirty = true; }

    bool isPopulated() const { return mPopulated; }

    void setPopulated(bool populated) { mPopulated = populated; }

    // in memory only, this session's TerrainCache order: see decorationHolds
    uint64_t getDecorationSequence() const {
        return mDecorationSequence;
    }

    uint64_t getBaseSequence() const {
        return mBaseSequence;
    }

    void setDecorationSequence(uint64_t base, uint64_t own) {
        mBaseSequence = base;
        mDecorationSequence = own;
    }

    const BlockState &getBlock(int x, int32_t y, int z) const;

    const BlockState &getBlock(int x, int32_t y, int z, int layer) const;

    void setBlock(int x, int32_t y, int z, const BlockState &state);

    void setBlock(int x, int32_t y, int z, int layer, const BlockState &state);

    void forEachBlock(const std::function<void(int32_t, int32_t, int32_t, const BlockState &)> &callback) const;

    void setBiome(uint32_t biomeId);

    uint32_t getBiome() const { return mBiomeId; }

    void setBiomeAt(int x, int32_t y, int z, uint32_t biomeId);

    void fillSubChunkBiome(int index, uint32_t biomeId);

    uint32_t getBiomeAt(int x, int32_t y, int z) const;

    void setColumnBiome(int x, int z, uint32_t biomeId);

    uint32_t getColumnBiome(int x, int z) const;

    std::string encodeNetwork(const BlockNetworkIdMap *blockIds = nullptr) const;

    const std::string &encodeNetworkAnchor() const;

    const std::string &encodeSubChunkNetwork(int index) const;

    // cached per translation; main thread only: filled here, cleared by setBlock without a lock
    const std::string &encodeSubChunkNetwork(int index, const BlockNetworkIdMap &blockIds) const;

    const std::vector<int32_t> &getTopBlockHeights() const;

    void invalidateNetworkCaches();

    // instance and revision together name one network encoding: a shared packet stays valid while both match
    uint64_t getNetworkInstance() const {
        return mInstance.mId;
    }

    uint32_t getNetworkRevision() const {
        return mNetworkRevision;
    }

    std::string encodeBiomes(int sectionCount) const;

    std::string encodeBiomesPersistent(int firstSection, int sectionCount) const;

    bool readBiomesPersistent(ReadOnlyBinaryStream &stream, int firstSection, int sectionCount);

    int getNetworkSubChunkCount() const;

    // writes through it skip the network revision: only for chunks not resident yet, like a storage load
    SubChunk &getSubChunk(int index) {
        _ensureUnpacked();
        return mSubChunks[index];
    }

    const SubChunk &getSubChunk(int index) const {
        _ensureUnpacked();
        return mSubChunks[index];
    }

    static int subChunkIndexFor(int32_t y) { return (y - MIN_Y) >> 4; }

    static const int32_t UNKNOWN_HEIGHT = -32768;

    int32_t getHeight(int x, int z) const;

    void setHeight(int x, int z, int32_t height);

    bool hasHeight(int x, int z) const;

    int getSkyLight(int x, int32_t y, int z) const;

    void setSkyLight(int x, int32_t y, int z, int value);

    int getBlockLight(int x, int32_t y, int z) const;

    void setBlockLight(int x, int32_t y, int z, int value);

    bool hasSkyLight() const {
        _ensureUnpacked();
        return mSkyLight.isAllocated();
    }

    bool hasBlockLight() const {
        _ensureUnpacked();
        return mBlockLight.isAllocated();
    }

    bool hasHeightmap() const { return !mHeightmap.empty(); }

    void clearSkyLight();

    void compactLight();

    void compactBiomes();
    void setDimension(DimensionType dimension);

    DimensionType getDimension() const { return mDimension; }

    int getFirstNetworkSubChunk() const { return mFirstNetworkSubChunk; }

    // sections and light into one compressed buffer: most loaded chunks are only kept for viewers; any read
    // or write unpacks it; main thread only, a copy stays packed
    void pack();

    bool isPacked() const {
        return mPacked;
    }

    // read or written since the last call: the packer leaves busy chunks alone
    bool consumeAccessed();

private:
    void _ensureUnpacked() const {
        mAccessed = true;
        if (mPacked)
            _unpack();
    }

    void _unpack() const;

    int32_t mX;
    int32_t mZ;
    uint32_t mBiomeId;
    DimensionType mDimension = DimensionType::Overworld;
    int mFirstNetworkSubChunk = 0;
    int mNetworkSubChunkLimit = SUB_CHUNK_COUNT;
    bool mDirty;
    bool mPopulated = false;
    uint64_t mBaseSequence = 0;
    uint64_t mDecorationSequence = 0;
    // mutable: const reads unpack a packed chunk first
    mutable std::vector<SubChunk> mSubChunks;
    std::vector<int16_t> mHeightmap;
    mutable LightStorage mSkyLight;
    mutable LightStorage mBlockLight;
    mutable std::string mPackedData;
    mutable bool mPacked = false;
    mutable bool mAccessed = false;
    mutable std::vector<std::string> mSubChunkNetworkCache;
    mutable std::vector<uint8_t> mSubChunkNetworkValid;
    mutable std::unordered_map<const BlockNetworkIdMap *, std::vector<std::string>> mTranslatedSubChunkCache;
    mutable std::vector<int32_t> mTopHeightsCache;
    mutable std::string mNetworkAnchorCache;
    mutable bool mTopHeightsValid = false;
    mutable bool mNetworkAnchorValid = false;
    LevelChunkInstance mInstance;
    uint32_t mNetworkRevision = 0;
};
