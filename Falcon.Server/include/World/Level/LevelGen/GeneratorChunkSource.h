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

/**
 * Terrain shared by every worker of a level. Populating a chunk needs the terrain of its eight
 * neighbours, and adjacent chunks usually land on different workers, so a per-worker cache would
 * regenerate most of them. Entries are handed out as copies, so callers need no locking.
 *
 * A chunk can hold two versions: its raw terrain, as the generator produced it, and the neighbour
 * a decoration left behind, with the blocks it spilled. A chunk that is loaded must start from the
 * raw terrain, or its content would depend on which neighbours happened to be decorated first;
 * the decorated version only serves as context when the next neighbour is decorated, as the
 * per-worker cache did before it was shared.
 */
class TerrainCache {
public:
    static constexpr size_t MAX_CACHED_CHUNKS = 1024;

    explicit TerrainCache(const ChunkGenerator &generator);

    TerrainCache(const TerrainCache &) = delete;

    TerrainCache &operator=(const TerrainCache &) = delete;

    /**
     * The raw terrain of a chunk, generated and cached when it is not cached yet. A caller asking
     * for terrain another worker is generating waits for it instead of generating it again.
     */
    LevelChunk terrain(int32_t chunkX, int32_t chunkZ);

    // The chunk as the last decoration of a neighbour left it, or its raw terrain when none did.
    LevelChunk neighbour(int32_t chunkX, int32_t chunkZ);

    // Keeps a neighbour as a decoration left it, for the decoration of the next one.
    void put(LevelChunk chunk);

private:
    struct Entry {
        // both shared and never changed once stored, so a worker copies one out after letting go of the lock
        std::shared_ptr<const LevelChunk> mTerrain;
        std::shared_ptr<const LevelChunk> mDecorated;
        std::list<int64_t>::iterator mOrder;
    };

    // Releases the claim on a chunk being generated, even when generation throws, so waiting workers carry on.
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

/**
 * Per-worker population context: a private scratch level that holds a chunk and its
 * neighbours while it is decorated. The neighbours come from the level's shared terrain cache.
 */
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
