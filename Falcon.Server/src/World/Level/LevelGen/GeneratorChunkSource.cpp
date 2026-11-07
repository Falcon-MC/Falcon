#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <iterator>
#include <utility>

TerrainCache::TerrainCache(const ChunkGenerator &generator) : mGenerator(generator) {
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
    return _get(chunkX, chunkZ, false);
}

LevelChunk TerrainCache::neighbour(int32_t chunkX, int32_t chunkZ) {
    return _get(chunkX, chunkZ, true);
}

LevelChunk TerrainCache::_get(int32_t chunkX, int32_t chunkZ, bool decorated) {
    const int64_t key = _packChunk(chunkX, chunkZ);

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
        return *cached;

    const GenerationClaim claim{*this, key};
    LevelChunk chunk(chunkX, chunkZ);
    chunk.setDimension(mGenerator.getDimensionType());
    mGenerator.generate(chunk);

    std::shared_ptr<const LevelChunk> shared = std::make_shared<const LevelChunk>(chunk);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        _entry(key).mTerrain = std::move(shared);
    }
    return chunk;
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

    while (mOrder.size() > MAX_CACHED_CHUNKS) {
        mChunks.erase(mOrder.front());
        mOrder.pop_front();
    }
    return entry;
}

GeneratorChunkSource::GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache)
        : mScratch("generator", 1, seed, dimension), mCache(cache) {
}

void GeneratorChunkSource::_stageNeighbour(int32_t chunkX, int32_t chunkZ) {
    mScratch.insertChunk(mCache.neighbour(chunkX, chunkZ));
}

/**
 * Decorates one chunk inside a private scratch level that holds its 3x3 neighbourhood as
 * terrain, so trees and other features can be placed across chunk borders. Blocks that land in
 * a neighbour are reported through `overflow` so the real neighbour receives them. The
 * neighbours go back to the shared cache afterwards, with what this decoration placed in them,
 * because adjacent chunks are usually populated soon after and would otherwise regenerate the
 * same terrain.
 */
void GeneratorChunkSource::populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow,
                                    std::vector<Tag> &blockActors, std::vector<Tag> &entities) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            if (offsetX == 0 && offsetZ == 0)
                continue;

            _stageNeighbour(chunkX + offsetX, chunkZ + offsetZ);
        }
    }

    LevelChunk &center = mScratch.insertChunk(std::move(chunk));
    mScratch.decorate(center, &overflow);

    chunk = mScratch.extractChunk(chunkX, chunkZ);

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
}
