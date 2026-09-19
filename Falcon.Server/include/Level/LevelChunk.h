#pragma once

#include "Level/Dimension.h"
#include "Level/SubChunk.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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

    const BlockState &getBlock(int x, int32_t y, int z) const;

    const BlockState &getBlock(int x, int32_t y, int z, int layer) const;

    void setBlock(int x, int32_t y, int z, const BlockState &state);

    void setBlock(int x, int32_t y, int z, int layer, const BlockState &state);

    void forEachBlock(const std::function<void(int32_t, int32_t, int32_t, const BlockState &)> &callback) const;

    void setBiome(uint32_t biomeId);

    uint32_t getBiome() const { return mBiomeId; }

    void setBiomeAt(int x, int32_t y, int z, uint32_t biomeId);

    uint32_t getBiomeAt(int x, int32_t y, int z) const;

    void setColumnBiome(int x, int z, uint32_t biomeId);

    uint32_t getColumnBiome(int x, int z) const;

    std::string encodeNetwork() const;

    const std::string &encodeNetworkAnchor() const;

    const std::string &encodeSubChunkNetwork(int index) const;

    const std::vector<int32_t> &getTopBlockHeights() const;

    void buildNetworkCaches() const;

    void invalidateNetworkCaches();

    std::string encodeBiomes(int sectionCount) const;

    std::string encodeBiomesPersistent(int sectionCount) const;

    bool readBiomesPersistent(ReadOnlyBinaryStream &stream, int sectionCount);

    int getNetworkSubChunkCount() const;

    SubChunk &getSubChunk(int index) { return mSubChunks[index]; }

    const SubChunk &getSubChunk(int index) const { return mSubChunks[index]; }

    static int subChunkIndexFor(int32_t y) { return (y - MIN_Y) >> 4; }

    static const int32_t UNKNOWN_HEIGHT = -32768;

    int32_t getHeight(int x, int z) const;

    void setHeight(int x, int z, int32_t height);

    bool hasHeight(int x, int z) const;

    int getSkyLight(int x, int32_t y, int z) const;

    void setSkyLight(int x, int32_t y, int z, int value);

    int getBlockLight(int x, int32_t y, int z) const;

    void setBlockLight(int x, int32_t y, int z, int value);

    bool hasSkyLight() const { return !mSkyLight.empty(); }

    bool hasBlockLight() const { return !mBlockLight.empty(); }

    bool hasHeightmap() const { return !mHeightmap.empty(); }

    void clearSkyLight();
    void setDimension(DimensionType dimension);

    DimensionType getDimension() const { return mDimension; }

    int getFirstNetworkSubChunk() const { return mFirstNetworkSubChunk; }

private:
    static const size_t LIGHT_STORAGE_SIZE = (size_t) SUB_CHUNK_COUNT * 16 * 256 / 2;

    static int _lightIndex(int x, int32_t y, int z);

    static int _getNibble(const std::vector<uint8_t> &storage, int x, int32_t y, int z);

    static void _setNibble(std::vector<uint8_t> &storage, int x, int32_t y, int z, int value);
    int32_t mX;
    int32_t mZ;
    uint32_t mBiomeId;
    DimensionType mDimension = DimensionType::Overworld;
    int mFirstNetworkSubChunk = 0;
    int mNetworkSubChunkLimit = SUB_CHUNK_COUNT;
    bool mDirty;
    bool mPopulated = false;
    std::vector<SubChunk> mSubChunks;
    std::vector<int16_t> mHeightmap;
    std::vector<uint8_t> mSkyLight;
    std::vector<uint8_t> mBlockLight;
    mutable std::vector<std::string> mSubChunkNetworkCache;
    mutable std::vector<uint8_t> mSubChunkNetworkValid;
    mutable std::vector<int32_t> mTopHeightsCache;
    mutable std::string mNetworkAnchorCache;
    mutable bool mTopHeightsValid = false;
    mutable bool mNetworkAnchorValid = false;
};
