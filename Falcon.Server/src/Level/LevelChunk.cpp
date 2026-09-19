#include "Level/LevelChunk.h"

#include <algorithm>

namespace {
    const unsigned char BIOME_COPY_PREVIOUS = 0xff;
}

LevelChunk::LevelChunk(int32_t x, int32_t z) : mX(x), mZ(z), mBiomeId(1), mDirty(false) {
    mSubChunks.reserve(SUB_CHUNK_COUNT);
    for (int i = 0; i < SUB_CHUNK_COUNT; i++)
        mSubChunks.push_back(SubChunk((int8_t) (LOWEST_SUB_CHUNK_Y + i)));

    mSubChunkNetworkCache.resize(SUB_CHUNK_COUNT);
    mSubChunkNetworkValid.assign(SUB_CHUNK_COUNT, 0);
}

void LevelChunk::invalidateNetworkCaches() {
    for (std::string &cached: mSubChunkNetworkCache)
        std::string().swap(cached);

    mSubChunkNetworkValid.assign(SUB_CHUNK_COUNT, 0);
    std::vector<int32_t>().swap(mTopHeightsCache);
    std::string().swap(mNetworkAnchorCache);
    mTopHeightsValid = false;
    mNetworkAnchorValid = false;
}

void LevelChunk::buildNetworkCaches() const {
    for (int i = 0; i < SUB_CHUNK_COUNT; i++) {
        if (mSubChunks[(size_t) i].isEmpty())
            continue;

        encodeSubChunkNetwork(i);
    }

    getTopBlockHeights();
    encodeNetworkAnchor();
}

int LevelChunk::_lightIndex(int x, int32_t y, int z) {
    return (((y - MIN_Y) << 8) | (z << 4) | x);
}

int32_t LevelChunk::getHeight(int x, int z) const {
    if (mHeightmap.empty())
        return MIN_Y;

    const int16_t height = mHeightmap[(size_t) ((z << 4) | x)];
    return height == (int16_t) UNKNOWN_HEIGHT ? MIN_Y : height;
}

bool LevelChunk::hasHeight(int x, int z) const {
    if (mHeightmap.empty())
        return false;

    return mHeightmap[(size_t) ((z << 4) | x)] != (int16_t) UNKNOWN_HEIGHT;
}

void LevelChunk::setHeight(int x, int z, int32_t height) {
    if (mHeightmap.empty())
        mHeightmap.assign(256, (int16_t) UNKNOWN_HEIGHT);

    mHeightmap[(size_t) ((z << 4) | x)] = (int16_t) height;
}

int LevelChunk::_getNibble(const std::vector<uint8_t> &storage, int x, int32_t y, int z) {
    if (storage.empty() || y < MIN_Y || y > MAX_Y)
        return 0;

    const int index = _lightIndex(x, y, z);
    const uint8_t packed = storage[(size_t) (index >> 1)];
    return (index & 1) != 0 ? packed >> 4 : packed & 0x0F;
}

void LevelChunk::_setNibble(std::vector<uint8_t> &storage, int x, int32_t y, int z, int value) {
    if (y < MIN_Y || y > MAX_Y)
        return;

    if (storage.empty())
        storage.assign(LIGHT_STORAGE_SIZE, 0);

    const int index = _lightIndex(x, y, z);
    uint8_t &packed = storage[(size_t) (index >> 1)];
    if ((index & 1) != 0)
        packed = (uint8_t) ((packed & 0x0F) | ((value & 0x0F) << 4));
    else
        packed = (uint8_t) ((packed & 0xF0) | (value & 0x0F));
}

int LevelChunk::getSkyLight(int x, int32_t y, int z) const {
    return _getNibble(mSkyLight, x, y, z);
}

void LevelChunk::setSkyLight(int x, int32_t y, int z, int value) {
    _setNibble(mSkyLight, x, y, z, value);
}

int LevelChunk::getBlockLight(int x, int32_t y, int z) const {
    return _getNibble(mBlockLight, x, y, z);
}

void LevelChunk::setBlockLight(int x, int32_t y, int z, int value) {
    _setNibble(mBlockLight, x, y, z, value);
}

void LevelChunk::clearSkyLight() {
    mSkyLight.clear();
    mHeightmap.clear();
}

const BlockState &LevelChunk::getBlock(int x, int32_t y, int z) const {
    static const BlockState air;

    if (y < MIN_Y || y > MAX_Y)
        return air;

    return mSubChunks[subChunkIndexFor(y)].getBlock(x, y & 15, z);
}

const BlockState &LevelChunk::getBlock(int x, int32_t y, int z, int layer) const {
    static const BlockState air;

    if (y < MIN_Y || y > MAX_Y)
        return air;

    return mSubChunks[subChunkIndexFor(y)].getBlock(x, y & 15, z, layer);
}

void LevelChunk::setBlock(int x, int32_t y, int z, const BlockState &state) {
    if (y < MIN_Y || y > MAX_Y)
        return;

    const int index = subChunkIndexFor(y);
    mSubChunks[index].setBlock(x, y & 15, z, state);
    mSubChunkNetworkValid[(size_t) index] = 0;
    mTopHeightsValid = false;
    mNetworkAnchorValid = false;
    mDirty = true;
}

void LevelChunk::setBlock(int x, int32_t y, int z, int layer, const BlockState &state) {
    if (y < MIN_Y || y > MAX_Y)
        return;

    if (layer <= 0) {
        setBlock(x, y, z, state);
        return;
    }

    const int index = subChunkIndexFor(y);
    mSubChunks[index].setBlock(x, y & 15, z, layer, state);
    mSubChunkNetworkValid[(size_t) index] = 0;
    mDirty = true;
}

void LevelChunk::forEachBlock(const std::function<void(int32_t, int32_t, int32_t, const BlockState &)> &callback) const {
    for (int32_t y = MIN_Y; y <= MAX_Y; ++y) {
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t z = 0; z < 16; ++z)
                callback(mX * 16 + x, y, mZ * 16 + z, getBlock(x, y, z));
        }
    }
}

void LevelChunk::setDimension(DimensionType dimension) {
    mDimension = dimension;
    mFirstNetworkSubChunk = Dimension::getLowestSubChunkIndex(dimension);
    mNetworkSubChunkLimit = Dimension::getSubChunkCount(dimension);
    mNetworkAnchorValid = false;
}

int LevelChunk::getNetworkSubChunkCount() const {
    const int last = mFirstNetworkSubChunk + mNetworkSubChunkLimit - 1;

    for (int i = last; i >= mFirstNetworkSubChunk; i--) {
        if (!mSubChunks[i].isEmpty())
            return i - mFirstNetworkSubChunk + 1;
    }

    return 0;
}

void LevelChunk::setBiome(uint32_t biomeId) {
    mBiomeId = biomeId;
    mNetworkAnchorValid = false;

    for (int32_t y = MIN_Y; y <= MAX_Y; y += 16) {
        SubChunk &subChunk = mSubChunks[subChunkIndexFor(y)];
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                for (int localY = 0; localY < 16; localY++)
                    subChunk.setBiome(x, localY, z, biomeId);
            }
        }
    }
}

void LevelChunk::setBiomeAt(int x, int32_t y, int z, uint32_t biomeId) {
    if (y < MIN_Y || y > MAX_Y)
        return;

    mSubChunks[subChunkIndexFor(y)].setBiome(x, y & 15, z, biomeId);
    mNetworkAnchorValid = false;
}

uint32_t LevelChunk::getBiomeAt(int x, int32_t y, int z) const {
    if (y < MIN_Y || y > MAX_Y)
        return mBiomeId;

    return mSubChunks[subChunkIndexFor(y)].getBiome(x, y & 15, z);
}

void LevelChunk::setColumnBiome(int x, int z, uint32_t biomeId) {
    for (int32_t y = MIN_Y; y <= MAX_Y; y++)
        setBiomeAt(x, y, z, biomeId);
}

uint32_t LevelChunk::getColumnBiome(int x, int z) const {
    return getBiomeAt(x, 63, z);
}

std::string LevelChunk::encodeBiomes(int sectionCount) const {
    BinaryStream stream;

    for (int i = 0; i < sectionCount; i++)
        mSubChunks[mFirstNetworkSubChunk + i].writeBiomes(stream, false);

    return stream.getBuffer();
}

bool LevelChunk::readBiomesPersistent(ReadOnlyBinaryStream &stream, int sectionCount) {
    for (int i = 0; i < sectionCount && i < SUB_CHUNK_COUNT; i++) {
        if (!mSubChunks[i].readBiomes(stream))
            return false;
    }

    mBiomeId = getBiomeAt(0, 63, 0);
    mNetworkAnchorValid = false;
    return true;
}

std::string LevelChunk::encodeBiomesPersistent(int sectionCount) const {
    BinaryStream stream;

    for (int i = 0; i < sectionCount; i++)
        mSubChunks[i].writeBiomes(stream, true);

    return stream.getBuffer();
}

std::string LevelChunk::encodeNetwork() const {
    BinaryStream stream;

    const int sectionCount = getNetworkSubChunkCount();

    for (int i = 0; i < sectionCount; i++)
        stream.put(encodeSubChunkNetwork(mFirstNetworkSubChunk + i));

    stream.put(encodeBiomes(sectionCount));
    stream.putByte(0);

    return stream.getBuffer();
}

const std::string &LevelChunk::encodeNetworkAnchor() const {
    if (mNetworkAnchorValid)
        return mNetworkAnchorCache;

    BinaryStream stream;

    const int sectionCount = std::max(1, getNetworkSubChunkCount());

    for (int i = 0; i < sectionCount; i++)
        mSubChunks[mFirstNetworkSubChunk + i].writeBiomes(stream, false);

    for (int i = sectionCount; i < mNetworkSubChunkLimit; i++)
        stream.putByte(BIOME_COPY_PREVIOUS);

    stream.putByte(0);

    mNetworkAnchorCache = stream.getBuffer();
    mNetworkAnchorValid = true;

    return mNetworkAnchorCache;
}

const std::string &LevelChunk::encodeSubChunkNetwork(int index) const {
    static const std::string empty;

    if (index < 0 || index >= SUB_CHUNK_COUNT)
        return empty;

    std::string &cached = mSubChunkNetworkCache[(size_t) index];
    if (mSubChunkNetworkValid[(size_t) index] != 0)
        return cached;

    BinaryStream stream;
    mSubChunks[(size_t) index].writeNetwork(stream);

    cached = stream.getBuffer();
    mSubChunkNetworkValid[(size_t) index] = 1;

    return cached;
}

const std::vector<int32_t> &LevelChunk::getTopBlockHeights() const {
    if (mTopHeightsValid)
        return mTopHeightsCache;

    std::vector<int32_t> heights((size_t) 256, MIN_Y - 1);
    size_t remaining = heights.size();

    for (int index = SUB_CHUNK_COUNT - 1; index >= 0 && remaining > 0; index--) {
        const SubChunk &subChunk = mSubChunks[(size_t) index];
        if (subChunk.isEmpty())
            continue;

        const int32_t baseY = MIN_Y + index * 16;

        for (int z = 0; z < 16; z++) {
            for (int x = 0; x < 16; x++) {
                const size_t slot = (size_t) ((z << 4) | x);
                if (heights[slot] != MIN_Y - 1)
                    continue;

                for (int localY = 15; localY >= 0; localY--) {
                    if (subChunk.getBlock(x, localY, z).mName == "minecraft:air")
                        continue;

                    heights[slot] = baseY + localY;
                    remaining--;
                    break;
                }
            }
        }
    }

    mTopHeightsCache = std::move(heights);
    mTopHeightsValid = true;

    return mTopHeightsCache;
}
