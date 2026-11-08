#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Level.h"

#include <cstdint>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ChunkGenerator;
class LevelChunk;

// shared by every worker: a chunk's eight neighbours usually land on other workers; entries handed out as copies
// two versions per chunk, raw terrain and the neighbour a decoration left behind: a loaded chunk starts from the raw
// one, or its content would depend on which neighbours were decorated first
class TerrainCache {
public:
    static constexpr size_t MAX_CACHED_CHUNKS = 1024;

    explicit TerrainCache(const ChunkGenerator &generator);

    TerrainCache(const TerrainCache &) = delete;

    TerrainCache &operator=(const TerrainCache &) = delete;

    // waits when another worker is generating the same chunk
    LevelChunk terrain(int32_t chunkX, int32_t chunkZ);

    // the chunk as the last neighbour decoration left it, else its raw terrain
    LevelChunk neighbour(int32_t chunkX, int32_t chunkZ);

    // the neighbour as a decoration left it: context for the next one
    void put(LevelChunk chunk);

private:
    struct Entry {
        // shared and immutable once stored: a worker copies one out after releasing the lock
        std::shared_ptr<const LevelChunk> mTerrain;
        std::shared_ptr<const LevelChunk> mDecorated;
        std::list<int64_t>::iterator mOrder;
    };

    // releases the claim even when generation throws: waiting workers carry on
    struct GenerationClaim {
        TerrainCache &mCache;
        int64_t mKey;

        ~GenerationClaim();
    };

    static int64_t _packChunk(int32_t chunkX, int32_t chunkZ);

    LevelChunk _get(int32_t chunkX, int32_t chunkZ, bool decorated);

    Entry &_entry(int64_t key);

    const ChunkGenerator &mGenerator;
    std::mutex mMutex;
    std::condition_variable mPublished;
    std::unordered_set<int64_t> mGenerating;
    std::unordered_map<int64_t, Entry> mChunks;
    std::list<int64_t> mOrder;
};

// per worker: a private scratch level holding a chunk and its neighbours, from the shared TerrainCache, while decorated
class GeneratorChunkSource {
public:
    GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache);

    GeneratorChunkSource(const GeneratorChunkSource &) = delete;

    GeneratorChunkSource &operator=(const GeneratorChunkSource &) = delete;

    void populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow, std::vector<Tag> &blockActors,
                  std::vector<Tag> &entities);

private:
    void _stageNeighbour(int32_t chunkX, int32_t chunkZ);

    Level mScratch;
    TerrainCache &mCache;
};
