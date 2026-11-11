#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LightSystem.h"

#include <cstdlib>
#include <iterator>
#include <utility>

TerrainCache::TerrainCache(const ChunkGenerator &generator) : mGenerator(generator) {}

TerrainCache::DecorationArea::DecorationArea(TerrainCache &cache, int32_t chunkX, int32_t chunkZ, uint64_t sequence)
        : mCache(cache), mChunkX(chunkX), mChunkZ(chunkZ), mSequence(sequence) {}

TerrainCache::DecorationArea::~DecorationArea() {
    {
        std::lock_guard<std::mutex> lock(mCache.mMutex);
        for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
            for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++)
                mCache.mLocked.erase(_packChunk(mChunkX + offsetX, mChunkZ + offsetZ));
        }
    }
    mCache.mReleased.notify_all();
}

int32_t TerrainCache::decorationClass(int32_t chunkX, int32_t chunkZ) {
    static const int32_t CLASSES[2][2] = {{0, 1}, {3, 2}};
    return CLASSES[chunkZ & 1][chunkX & 1];
}

TerrainCache::GenerationClaim::~GenerationClaim() {
    {
        std::lock_guard<std::mutex> lock(mCache.mMutex);
        mCache.mGenerating.erase(mKey);
    }
    mCache.mPublished.notify_all();
}

int64_t TerrainCache::_packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

LevelChunk TerrainCache::terrain(int32_t chunkX, int32_t chunkZ) {
    return *_get(chunkX, chunkZ, false, nullptr);
}

std::shared_ptr<const LevelChunk> TerrainCache::sharedTerrain(int32_t chunkX, int32_t chunkZ) {
    return _get(chunkX, chunkZ, false, nullptr);
}

LevelChunk TerrainCache::neighbour(int32_t chunkX, int32_t chunkZ) {
    return *_get(chunkX, chunkZ, true, nullptr);
}

LevelChunk TerrainCache::base(int32_t chunkX, int32_t chunkZ, uint64_t &created) {
    return *_get(chunkX, chunkZ, true, &created);
}

void TerrainCache::setStoredLoader(StoredLoader loader) {
    std::lock_guard<std::mutex> lock(mMutex);
    mStoredLoader = std::move(loader);
}

// a neighbour decorated in another session, or evicted: its raw terrain would let a feature write over its plants
std::shared_ptr<const LevelChunk> TerrainCache::_stored(int32_t chunkX, int32_t chunkZ, uint64_t *created) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    StoredLoader loader;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        while (mLoading.count(key) != 0)
            mPublished.wait(lock);
        if (!mStoredLoader)
            return nullptr;
        const auto found = mChunks.find(key);
        if (found != mChunks.end() && (found->second.mDecorated != nullptr || found->second.mStoredChecked))
            return nullptr;
        mLoading.insert(key);
        loader = mStoredLoader;
    }

    std::shared_ptr<const LevelChunk> shared;
    try {
        LevelChunk chunk(chunkX, chunkZ);
        chunk.setDimension(mGenerator.getDimensionType());
        if (loader(chunk))
            shared = std::make_shared<const LevelChunk>(std::move(chunk));
    } catch (...) {
        shared = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mLoading.erase(key);
        Entry &entry = _entry(key);
        entry.mStoredChecked = true;
        if (shared != nullptr && entry.mDecorated == nullptr)
            entry.mDecorated = shared;
        else
            shared = entry.mDecorated;
        if (shared != nullptr && created != nullptr)
            *created = entry.mCreated;
    }
    mPublished.notify_all();
    return shared;
}

std::shared_ptr<const LevelChunk> TerrainCache::_get(int32_t chunkX, int32_t chunkZ, bool decorated,
                                                     uint64_t *created) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    if (decorated) {
        std::shared_ptr<const LevelChunk> stored = _stored(chunkX, chunkZ, created);
        if (stored != nullptr)
            return stored;
    }

    std::shared_ptr<const LevelChunk> cached;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        while (true) {
            const auto found = mChunks.find(key);
            if (found != mChunks.end()) {
                const Entry &entry = found->second;
                cached = decorated && entry.mDecorated != nullptr ? entry.mDecorated : entry.mTerrain;
                if (cached != nullptr) {
                    mOrder.splice(mOrder.end(), mOrder, entry.mOrder);
                    if (created != nullptr)
                        *created = entry.mCreated;
                    break;
                }
            }
            if (mGenerating.count(key) == 0)
                break;

            mPublished.wait(lock);
        }

        if (cached == nullptr)
            mGenerating.insert(key);
    }

    if (cached != nullptr)
        return cached;

    const GenerationClaim claim{*this, key};
    LevelChunk chunk(chunkX, chunkZ);
    chunk.setDimension(mGenerator.getDimensionType());
    mGenerator.generate(chunk);

    std::shared_ptr<const LevelChunk> shared = std::make_shared<const LevelChunk>(std::move(chunk));
    {
        std::lock_guard<std::mutex> lock(mMutex);
        Entry &entry = _entry(key);
        entry.mTerrain = shared;
        if (created != nullptr)
            *created = entry.mCreated;
    }
    return shared;
}

void TerrainCache::put(LevelChunk chunk) {
    const int64_t key = _packChunk(chunk.getX(), chunk.getZ());
    std::shared_ptr<const LevelChunk> shared = std::make_shared<const LevelChunk>(std::move(chunk));

    std::lock_guard<std::mutex> lock(mMutex);
    _entry(key).mDecorated = std::move(shared);
}

TerrainCache::Entry &TerrainCache::_entry(int64_t key) {
    const auto found = mChunks.find(key);
    if (found != mChunks.end()) {
        mOrder.splice(mOrder.end(), mOrder, found->second.mOrder);
        return found->second;
    }

    mOrder.push_back(key);
    Entry &entry = mChunks[key];
    entry.mOrder = std::prev(mOrder.end());
    entry.mCreated = mSequence;

    while (mOrder.size() > MAX_CACHED_CHUNKS) {
        mChunks.erase(mOrder.front());
        mOrder.pop_front();
    }
    return entry;
}

bool TerrainCache::_isAreaFree(int32_t chunkX, int32_t chunkZ) const {
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            if (mLocked.count(_packChunk(chunkX + offsetX, chunkZ + offsetZ)) != 0)
                return false;
        }
    }
    return true;
}

std::unique_ptr<TerrainCache::DecorationArea> TerrainCache::lockArea(int32_t chunkX, int32_t chunkZ) {
    std::unique_lock<std::mutex> lock(mMutex);
    mReleased.wait(lock, [&]() { return _isAreaFree(chunkX, chunkZ); });

    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++)
            mLocked.insert(_packChunk(chunkX + offsetX, chunkZ + offsetZ));
    }
    // taken under the lock: a neighbour decorated earlier has a smaller sequence and its blocks are in the cache
    return std::unique_ptr<DecorationArea>(new DecorationArea(*this, chunkX, chunkZ, ++mSequence));
}

void TerrainCache::expectDecoration(int32_t chunkX, int32_t chunkZ) {
    std::lock_guard<std::mutex> lock(mMutex);
    mExpected[_packChunk(chunkX, chunkZ)]++;
}

void TerrainCache::finishDecoration(int32_t chunkX, int32_t chunkZ) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto found = mExpected.find(_packChunk(chunkX, chunkZ));
        if (found != mExpected.end() && --found->second <= 0)
            mExpected.erase(found);
    }
    mReleased.notify_all();
}

bool TerrainCache::awaitsEarlierNeighbour(int32_t chunkX, int32_t chunkZ) {
    const int32_t own = decorationClass(chunkX, chunkZ);
    std::lock_guard<std::mutex> lock(mMutex);
    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            const int32_t neighbourX = chunkX + offsetX;
            const int32_t neighbourZ = chunkZ + offsetZ;
            if (decorationClass(neighbourX, neighbourZ) < own
                && mExpected.count(_packChunk(neighbourX, neighbourZ)) != 0)
                return true;
        }
    }
    return false;
}

void TerrainCache::waitForDecoration(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mMutex);
    mReleased.wait_for(lock, timeout);
}

GeneratorChunkSource::GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache)
        : mScratch("generator", 1, seed, dimension), mCache(cache) {
    mScratch.setTerrainCache(&cache);
}

uint64_t GeneratorChunkSource::populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow,
                                        std::vector<Tag> &blockActors, std::vector<Tag> &entities) {
    const std::unique_ptr<TerrainCache::DecorationArea> area = mCache.lockArea(chunk.getX(), chunk.getZ());
    return _decorate(chunk, area->getSequence(), overflow, blockActors, entities);
}

uint64_t GeneratorChunkSource::populateGenerated(LevelChunk &chunk, const std::vector<GeneratedBlockChange> &older,
                                                 std::vector<GeneratedBlockChange> &overflow,
                                                 std::vector<Tag> &blockActors, std::vector<Tag> &entities,
                                                 uint64_t &base) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const std::unique_ptr<TerrainCache::DecorationArea> area = mCache.lockArea(chunkX, chunkZ);

    uint64_t created = 0;
    chunk = mCache.base(chunkX, chunkZ, created);
    // an entry appears under a lock holding its chunk: the decoration creating it writes into it too
    base = created == 0 ? 0 : created - 1;
    for (const GeneratedBlockChange &change: older) {
        if (change.mSequence > base || change.mY < LevelChunk::MIN_Y || change.mY > LevelChunk::MAX_Y)
            continue;

        chunk.setBlock(change.mX & 15, change.mY, change.mZ & 15, change.mLayer, *change.mState);
    }
    if (!chunk.hasHeightmap())
        LightSystem::computeHeightmap(chunk);

    return _decorate(chunk, area->getSequence(), overflow, blockActors, entities);
}

// the 3x3 neighbourhood lets features cross chunk borders: the neighbours come from the cache as earlier
// decorations left them, and go back with this one's blocks; the last decorated wins, as in the game
uint64_t GeneratorChunkSource::_decorate(LevelChunk &chunk, uint64_t sequence,
                                         std::vector<GeneratedBlockChange> &overflow, std::vector<Tag> &blockActors,
                                         std::vector<Tag> &entities) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            if (offsetX != 0 || offsetZ != 0)
                mScratch.insertChunk(mCache.neighbour(chunkX + offsetX, chunkZ + offsetZ));
        }
    }

    const size_t first = overflow.size();
    LevelChunk &center = mScratch.insertChunk(std::move(chunk));
    mScratch.decorate(center, &overflow);

    // beyond the 3x3: in no cached copy, left unordered
    for (size_t index = first; index < overflow.size(); index++) {
        GeneratedBlockChange &change = overflow[index];
        const bool inArea = std::abs((change.mX >> 4) - chunkX) <= 1 && std::abs((change.mZ >> 4) - chunkZ) <= 1;
        change.mSequence = inArea ? sequence : 0;
    }

    chunk = mScratch.extractChunk(chunkX, chunkZ);
    // later neighbours tell it from a raw one: structure pieces read its blocks (sub_147761B80)
    chunk.setPopulated(true);
    mCache.put(chunk);

    BlockActorStore &generatedActors = mScratch.getBlockActors();
    blockActors = generatedActors.saveChunk(chunkX, chunkZ);
    entities = mScratch.takeGeneratedEntities();

    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            generatedActors.unloadChunk(chunkX + offsetX, chunkZ + offsetZ);
            if (offsetX == 0 && offsetZ == 0)
                continue;

            mCache.put(mScratch.extractChunk(chunkX + offsetX, chunkZ + offsetZ));
        }
    }
    return sequence;
}
