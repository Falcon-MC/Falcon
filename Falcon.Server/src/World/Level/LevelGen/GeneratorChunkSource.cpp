#include "World/Level/LevelGen/GeneratorChunkSource.h"

#include "World/Level/Chunk/LevelChunk.h"

#include <utility>

int64_t TerrainCache::_packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

bool TerrainCache::acquire(int32_t chunkX, int32_t chunkZ, LevelChunk &out) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    std::shared_ptr<const LevelChunk> cached;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mPublished.wait(lock, [this, key] { return mGenerating.count(key) == 0; });

        const auto found = mChunks.find(key);
        if (found == mChunks.end()) {
            mGenerating.insert(key);
            return false;
        }

        mOrder.splice(mOrder.end(), mOrder, found->second.second);
        cached = found->second.first;
    }

    out = *cached;
    return true;
}

void TerrainCache::publish(LevelChunk chunk) {
    const int64_t key = _packChunk(chunk.getX(), chunk.getZ());
    std::shared_ptr<const LevelChunk> shared = std::make_shared<const LevelChunk>(std::move(chunk));
    {
        std::lock_guard<std::mutex> lock(mMutex);
        _store(key, std::move(shared));
        mGenerating.erase(key);
    }
    mPublished.notify_all();
}

void TerrainCache::put(LevelChunk chunk) {
    const int64_t key = _packChunk(chunk.getX(), chunk.getZ());
    std::shared_ptr<const LevelChunk> shared = std::make_shared<const LevelChunk>(std::move(chunk));

    std::lock_guard<std::mutex> lock(mMutex);
    _store(key, std::move(shared));
}

void TerrainCache::_store(int64_t key, std::shared_ptr<const LevelChunk> chunk) {
    const auto found = mChunks.find(key);
    if (found != mChunks.end()) {
        found->second.first = std::move(chunk);
        mOrder.splice(mOrder.end(), mOrder, found->second.second);
        return;
    }

    mOrder.push_back(key);
    mChunks.emplace(key, std::make_pair(std::move(chunk), std::prev(mOrder.end())));

    while (mOrder.size() > MAX_CACHED_CHUNKS) {
        mChunks.erase(mOrder.front());
        mOrder.pop_front();
    }
}

GeneratorChunkSource::GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache)
        : mScratch("generator", 1, seed, dimension), mCache(cache) {
}

void GeneratorChunkSource::_stageNeighbour(int32_t chunkX, int32_t chunkZ) {
    LevelChunk terrain(chunkX, chunkZ);
    if (mCache.acquire(chunkX, chunkZ, terrain)) {
        mScratch.insertChunk(std::move(terrain));
        return;
    }

    mCache.publish(mScratch.generateTerrainChunk(chunkX, chunkZ));
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
