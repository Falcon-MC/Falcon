#include "World/Level/Chunk/LevelChunk.h"

static_assert(LightStorage::SECTION_COUNT == LevelChunk::SUB_CHUNK_COUNT && LightStorage::MIN_Y == LevelChunk::MIN_Y,
              "LightStorage sections must match the chunk sub-chunks");

#include <zlib.h>

#include <algorithm>
#include <stdexcept>

LevelChunk::LevelChunk(int32_t x, int32_t z) : mX(x), mZ(z), mBiomeId(1), mDirty(false) {
    mSubChunks.reserve(SUB_CHUNK_COUNT);
    for (int i = 0; i < SUB_CHUNK_COUNT; i++)
        mSubChunks.push_back(SubChunk((int8_t) (LOWEST_SUB_CHUNK_Y + i)));
}

void LevelChunk::invalidateNetworkCaches() {
    // Swapping with empty objects releases the memory, which clear() would keep; this runs
    // on copies sent off for saving, where the encoded buffers are dead weight.
    std::vector<std::string>().swap(mSubChunkNetworkCache);
    std::vector<uint8_t>().swap(mSubChunkNetworkValid);
    std::unordered_map<const BlockNetworkIdMap *, std::vector<std::string>>().swap(mTranslatedSubChunkCache);
    std::vector<int32_t>().swap(mTopHeightsCache);
    std::string().swap(mNetworkAnchorCache);
    mTopHeightsValid = false;
    mNetworkAnchorValid = false;
}

void LevelChunk::pack() {
    if (mPacked)
        return;

    std::string raw;
    for (const SubChunk &subChunk: mSubChunks)
        subChunk.packRaw(raw);
    mSkyLight.packRaw(raw);
    mBlockLight.packRaw(raw);

    // Index words are long runs of the same few values, so even the fastest zlib level shrinks them several fold.
    uLongf size = compressBound((uLong) raw.size());
    std::string packed(sizeof(uint32_t) + size, '\0');
    const uint32_t rawSize = (uint32_t) raw.size();
    std::memcpy(&packed[0], &rawSize, sizeof(rawSize));
    if (compress2((Bytef *) &packed[sizeof(uint32_t)], &size, (const Bytef *) raw.data(), (uLong) raw.size(),
                  Z_BEST_SPEED) != Z_OK)
        return;

    packed.resize(sizeof(uint32_t) + size);
    packed.shrink_to_fit();
    mPackedData = std::move(packed);
    std::vector<SubChunk>().swap(mSubChunks);
    mSkyLight.clear();
    mBlockLight.clear();
    invalidateNetworkCaches();
    mPacked = true;
}

void LevelChunk::_unpack() const {
    uint32_t rawSize = 0;
    std::memcpy(&rawSize, mPackedData.data(), sizeof(rawSize));
    std::string raw(rawSize, '\0');
    uLongf size = rawSize;
    if (uncompress((Bytef *) &raw[0], &size, (const Bytef *) mPackedData.data() + sizeof(uint32_t),
                   (uLong) (mPackedData.size() - sizeof(uint32_t))) != Z_OK || size != rawSize)
        throw std::logic_error("Packed chunk could not be unpacked");

    RawPack::Reader reader(raw.data(), raw.size());
    std::vector<SubChunk> subChunks;
    subChunks.reserve(SUB_CHUNK_COUNT);
    for (int i = 0; i < SUB_CHUNK_COUNT; i++) {
        subChunks.push_back(SubChunk((int8_t) (LOWEST_SUB_CHUNK_Y + i)));
        if (!subChunks.back().unpackRaw(reader))
            throw std::logic_error("Packed chunk holds a malformed section");
    }
    if (!mSkyLight.unpackRaw(reader) || !mBlockLight.unpackRaw(reader) || !reader.atEnd())
        throw std::logic_error("Packed chunk holds malformed light");

    mSubChunks = std::move(subChunks);
    std::string().swap(mPackedData);
    mPacked = false;
}

bool LevelChunk::consumeAccessed() {
    const bool accessed = mAccessed;
    mAccessed = false;
    return accessed;
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

int LevelChunk::getSkyLight(int x, int32_t y, int z) const {
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return 0;

    return mSkyLight.get(x, y, z);
}

void LevelChunk::setSkyLight(int x, int32_t y, int z, int value) {
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return;

    mSkyLight.set(x, y, z, value);
}

int LevelChunk::getBlockLight(int x, int32_t y, int z) const {
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return 0;

    return mBlockLight.get(x, y, z);
}

void LevelChunk::setBlockLight(int x, int32_t y, int z, int value) {
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return;

    mBlockLight.set(x, y, z, value);
}

void LevelChunk::clearSkyLight() {
    _ensureUnpacked();
    mSkyLight.clear();
    mHeightmap.clear();
}

void LevelChunk::compactLight() {
    _ensureUnpacked();
    mSkyLight.compact();
    mBlockLight.compact();
}

void LevelChunk::compactBiomes() {
    _ensureUnpacked();
    for (SubChunk &subChunk: mSubChunks)
        subChunk.compactBiomes();
}

const BlockState &LevelChunk::getBlock(int x, int32_t y, int z) const {
    _ensureUnpacked();
    static const BlockState air;

    if (y < MIN_Y || y > MAX_Y)
        return air;

    return mSubChunks[subChunkIndexFor(y)].getBlock(x, y & 15, z);
}

const BlockState &LevelChunk::getBlock(int x, int32_t y, int z, int layer) const {
    _ensureUnpacked();
    static const BlockState air;

    if (y < MIN_Y || y > MAX_Y)
        return air;

    return mSubChunks[subChunkIndexFor(y)].getBlock(x, y & 15, z, layer);
}

void LevelChunk::setBlock(int x, int32_t y, int z, const BlockState &state) {
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return;

    const int index = subChunkIndexFor(y);
    mSubChunks[index].setBlock(x, y & 15, z, state);
    if (!mSubChunkNetworkValid.empty())
        mSubChunkNetworkValid[(size_t) index] = 0;
    for (auto &translated: mTranslatedSubChunkCache)
        translated.second[(size_t) index].clear();
    mTopHeightsValid = false;
    mNetworkAnchorValid = false;
    mDirty = true;
}

void LevelChunk::setBlock(int x, int32_t y, int z, int layer, const BlockState &state) {
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return;

    if (layer <= 0) {
        setBlock(x, y, z, state);
        return;
    }

    const int index = subChunkIndexFor(y);
    mSubChunks[index].setBlock(x, y & 15, z, layer, state);
    if (!mSubChunkNetworkValid.empty())
        mSubChunkNetworkValid[(size_t) index] = 0;
    for (auto &translated: mTranslatedSubChunkCache)
        translated.second[(size_t) index].clear();
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
    _ensureUnpacked();
    const int last = mFirstNetworkSubChunk + mNetworkSubChunkLimit - 1;

    for (int i = last; i >= mFirstNetworkSubChunk; i--) {
        if (!mSubChunks[i].isEmpty())
            return i - mFirstNetworkSubChunk + 1;
    }

    return 0;
}

void LevelChunk::setBiome(uint32_t biomeId) {
    _ensureUnpacked();
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
    _ensureUnpacked();
    if (y < MIN_Y || y > MAX_Y)
        return;

    mSubChunks[subChunkIndexFor(y)].setBiome(x, y & 15, z, biomeId);
    mNetworkAnchorValid = false;
}

void LevelChunk::fillSubChunkBiome(int index, uint32_t biomeId) {
    _ensureUnpacked();
    mSubChunks[index].fillBiome(biomeId);
    mNetworkAnchorValid = false;
}

uint32_t LevelChunk::getBiomeAt(int x, int32_t y, int z) const {
    _ensureUnpacked();
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
    _ensureUnpacked();
    BinaryStream stream;

    for (int i = 0; i < sectionCount; i++)
        mSubChunks[mFirstNetworkSubChunk + i].writeBiomes(stream, false);

    return stream.getBuffer();
}

bool LevelChunk::readBiomesPersistent(ReadOnlyBinaryStream &stream, int firstSection, int sectionCount) {
    _ensureUnpacked();
    const int end = std::min(firstSection + sectionCount, SUB_CHUNK_COUNT);
    for (int i = firstSection; i < end; i++) {
        if (!mSubChunks[i].readBiomes(stream, i > firstSection ? &mSubChunks[i - 1] : nullptr))
            return false;
    }

    mBiomeId = getBiomeAt(0, 63, 0);
    mNetworkAnchorValid = false;
    return true;
}

std::string LevelChunk::encodeBiomesPersistent(int firstSection, int sectionCount) const {
    _ensureUnpacked();
    BinaryStream stream;

    // like the game, sections above the highest non-empty one repeat it instead of keeping their own biomes
    int storedCount = sectionCount;
    while (storedCount > 1 && mSubChunks[firstSection + storedCount - 1].isEmpty())
        storedCount--;

    for (int i = 0; i < sectionCount; i++) {
        if (i < storedCount)
            mSubChunks[firstSection + i].writeBiomes(stream, true);
        else
            stream.putByte(SubChunk::BIOME_COPY_PREVIOUS);
    }

    return stream.getBuffer();
}

std::string LevelChunk::encodeNetwork(const BlockNetworkIdMap *blockIds) const {
    BinaryStream stream;

    const int sectionCount = getNetworkSubChunkCount();

    // Full chunk payload: the non-empty sub-chunks up to the highest one, one biome palette
    // per sent section, then a zero byte for the border block count. Block actors are
    // appended by the caller.
    for (int i = 0; i < sectionCount; i++) {
        if (blockIds == nullptr)
            stream.put(encodeSubChunkNetwork(mFirstNetworkSubChunk + i));
        else
            stream.put(encodeSubChunkNetwork(mFirstNetworkSubChunk + i, *blockIds));
    }

    stream.put(encodeBiomes(sectionCount));
    stream.putByte(0);

    return stream.getBuffer();
}

const std::string &LevelChunk::encodeNetworkAnchor() const {
    _ensureUnpacked();
    if (mNetworkAnchorValid)
        return mNetworkAnchorCache;

    BinaryStream stream;

    const int sectionCount = std::max(1, getNetworkSubChunkCount());

    for (int i = 0; i < sectionCount; i++)
        mSubChunks[mFirstNetworkSubChunk + i].writeBiomes(stream, false);

    // Sections above the last non-empty one reuse the previous biome palette; 0xff is the
    // protocol's one-byte marker for that.
    for (int i = sectionCount; i < mNetworkSubChunkLimit; i++)
        stream.putByte(SubChunk::BIOME_COPY_PREVIOUS);

    stream.putByte(0);

    mNetworkAnchorCache = stream.getBuffer();
    mNetworkAnchorValid = true;

    return mNetworkAnchorCache;
}

/**
 * Encoded sub-chunks are cached per section and only re-encoded after a block in that
 * section changes, since the same data is sent to every player who loads the chunk.
 */
const std::string &LevelChunk::encodeSubChunkNetwork(int index) const {
    _ensureUnpacked();
    static const std::string empty;

    if (index < 0 || index >= SUB_CHUNK_COUNT)
        return empty;

    // Most chunks are encoded once, when they are sent, so the per-section cache only exists once it is used.
    if (mSubChunkNetworkCache.empty()) {
        mSubChunkNetworkCache.resize(SUB_CHUNK_COUNT);
        mSubChunkNetworkValid.assign(SUB_CHUNK_COUNT, 0);
    }

    std::string &cached = mSubChunkNetworkCache[(size_t) index];
    if (mSubChunkNetworkValid[(size_t) index] != 0)
        return cached;

    BinaryStream stream;
    mSubChunks[(size_t) index].writeNetwork(stream);

    cached = stream.getBuffer();
    mSubChunkNetworkValid[(size_t) index] = 1;

    return cached;
}

const std::string &LevelChunk::encodeSubChunkNetwork(int index, const BlockNetworkIdMap &blockIds) const {
    _ensureUnpacked();
    static const std::string empty;

    if (index < 0 || index >= SUB_CHUNK_COUNT)
        return empty;

    std::vector<std::string> &sections = mTranslatedSubChunkCache[&blockIds];
    if (sections.empty())
        sections.resize(SUB_CHUNK_COUNT);

    std::string &cached = sections[(size_t) index];
    if (!cached.empty())
        return cached;

    BinaryStream stream;
    mSubChunks[(size_t) index].writeNetwork(stream, &blockIds);
    cached = stream.getBuffer();
    return cached;
}

const std::vector<int32_t> &LevelChunk::getTopBlockHeights() const {
    _ensureUnpacked();
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
