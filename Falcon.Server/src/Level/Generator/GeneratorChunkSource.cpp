#include "Level/Generator/GeneratorChunkSource.h"

#include "Level/LevelChunk.h"

#include <algorithm>
#include <utility>

GeneratorChunkSource::GeneratorChunkSource(int64_t seed, DimensionType dimension)
        : mScratch("generator", 1, seed, dimension) {
}

int64_t GeneratorChunkSource::_packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

void GeneratorChunkSource::_stageNeighbour(int32_t chunkX, int32_t chunkZ) {
    const std::unordered_map<int64_t, LevelChunk>::iterator cached =
            mTerrainCache.find(_packChunk(chunkX, chunkZ));

    if (cached == mTerrainCache.end()) {
        mCacheMisses++;
        mScratch.generateTerrainChunk(chunkX, chunkZ);
        return;
    }

    mCacheHits++;
    mScratch.insertChunk(std::move(cached->second));
    mTerrainCache.erase(cached);
}

void GeneratorChunkSource::_retireNeighbour(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    mTerrainCache.insert_or_assign(key, mScratch.extractChunk(chunkX, chunkZ));

    const std::deque<int64_t>::iterator found = std::find(mCacheOrder.begin(), mCacheOrder.end(), key);
    if (found != mCacheOrder.end())
        mCacheOrder.erase(found);

    mCacheOrder.push_back(key);
}

void GeneratorChunkSource::_trim() {
    while (mCacheOrder.size() > MAX_CACHED_CHUNKS) {
        const int64_t key = mCacheOrder.front();
        mCacheOrder.pop_front();
        mTerrainCache.erase(key);
    }
}

void GeneratorChunkSource::populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow,
                                    std::vector<Tag> &blockActors) {
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

    for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
            generatedActors.unloadChunk(chunkX + offsetX, chunkZ + offsetZ);
            if (offsetX == 0 && offsetZ == 0)
                continue;

            _retireNeighbour(chunkX + offsetX, chunkZ + offsetZ);
        }
    }

    _trim();
}
