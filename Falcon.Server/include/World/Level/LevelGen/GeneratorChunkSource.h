#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Level.h"

#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ChunkGenerator;
class LevelChunk;

// shared by every worker: chunks as the decorations left them, handed out as copies; the game decorates its live
// chunks: reads what neighbours placed, writes into them at once (sub_1432CD550, sub_1432CE930)
class TerrainCache {
public:
    static constexpr size_t MAX_CACHED_CHUNKS = 1024;

    // holds the 3x3 around a decorated chunk; released on destruction
    class DecorationArea {
    public:
        DecorationArea(TerrainCache &cache, int32_t chunkX, int32_t chunkZ, uint64_t sequence);

        DecorationArea(const DecorationArea &) = delete;

        DecorationArea &operator=(const DecorationArea &) = delete;

        ~DecorationArea();

        uint64_t getSequence() const {
            return mSequence;
        }

    private:
        TerrainCache &mCache;
        int32_t mChunkX;
        int32_t mChunkZ;
        uint64_t mSequence;
    };

    explicit TerrainCache(const ChunkGenerator &generator);

    TerrainCache(const TerrainCache &) = delete;

    TerrainCache &operator=(const TerrainCache &) = delete;

    // in the order (z even, x even), (z even, x odd), (z odd, x odd), (z odd, x even): two chunks decorated
    // together by the game always resolve their overlap this way, whatever the load order
    static int32_t decorationClass(int32_t chunkX, int32_t chunkZ);

    // waits when another worker is generating the same chunk
    LevelChunk terrain(int32_t chunkX, int32_t chunkZ);

    // as terrain(), without the copy
    std::shared_ptr<const LevelChunk> sharedTerrain(int32_t chunkX, int32_t chunkZ);

    // the chunk as the decorations left it, else its raw terrain
    LevelChunk neighbour(int32_t chunkX, int32_t chunkZ);

    // as neighbour(); `created`: the sequence when the entry appeared: decorations from it on are in it
    LevelChunk base(int32_t chunkX, int32_t chunkZ, uint64_t &created);

    void put(LevelChunk chunk);

    // a decorated chunk from storage, false when absent or never decorated
    using StoredLoader = std::function<bool(LevelChunk &chunk)>;

    void setStoredLoader(StoredLoader loader);

    // sub_145CEA040 locks the 3x3 too: two neighbours never decorate at once; waits instead of retrying later
    std::unique_ptr<DecorationArea> lockArea(int32_t chunkX, int32_t chunkZ);

    // a decoration of this chunk is queued: neighbours of a later class wait for it
    void expectDecoration(int32_t chunkX, int32_t chunkZ);

    void finishDecoration(int32_t chunkX, int32_t chunkZ);

    bool awaitsEarlierNeighbour(int32_t chunkX, int32_t chunkZ);

    void waitForDecoration(std::chrono::milliseconds timeout);

    // a value the first decorated chunk of a structure start fixes for the whole start, as the game keeps it in the
    // piece every chunk shares; in memory only, as in the game: temple starts and pieces are only ever built by
    // createStart (sub_1473C7400, 26.60), never loaded, so the next session draws the value again
    bool findStartValue(const std::string &structure, int32_t startX, int32_t startZ, int32_t &value);

    // keeps the value stored first for the start and returns it
    int32_t storeStartValue(const std::string &structure, int32_t startX, int32_t startZ, int32_t value);

private:
    struct Entry {
        // shared and immutable once stored: a worker copies one out after releasing the lock
        std::shared_ptr<const LevelChunk> mTerrain;
        std::shared_ptr<const LevelChunk> mDecorated;
        std::list<int64_t>::iterator mOrder;
        uint64_t mCreated = 0;
        bool mStoredChecked = false;
    };

    // releases the claim even when generation throws: waiting workers carry on
    struct GenerationClaim {
        TerrainCache &mCache;
        int64_t mKey;

        ~GenerationClaim();
    };

    static int64_t _packChunk(int32_t chunkX, int32_t chunkZ);

    std::shared_ptr<const LevelChunk> _get(int32_t chunkX, int32_t chunkZ, bool decorated, uint64_t *created);

    std::shared_ptr<const LevelChunk> _stored(int32_t chunkX, int32_t chunkZ, uint64_t *created);

    Entry &_entry(int64_t key);

    bool _isAreaFree(int32_t chunkX, int32_t chunkZ) const;

    const ChunkGenerator &mGenerator;
    std::mutex mMutex;
    std::condition_variable mPublished;
    std::unordered_set<int64_t> mGenerating;
    std::unordered_set<int64_t> mLoading;
    StoredLoader mStoredLoader;
    std::unordered_map<int64_t, Entry> mChunks;
    std::list<int64_t> mOrder;

    std::unordered_set<int64_t> mLocked;
    std::condition_variable mReleased;
    std::unordered_map<int64_t, int32_t> mExpected;
    uint64_t mSequence = 0;

    std::mutex mStartMutex;
    std::map<std::tuple<std::string, int32_t, int32_t>, int32_t> mStartValues;
};

// per worker: a private scratch level holding a chunk and its neighbours, from the shared TerrainCache, while decorated
class GeneratorChunkSource {
public:
    GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache);

    GeneratorChunkSource(const GeneratorChunkSource &) = delete;

    GeneratorChunkSource &operator=(const GeneratorChunkSource &) = delete;

    // `chunk`: the base to decorate; neighbours written in the cache at once, and in `overflow` for the
    // resident ones; returns the decoration's sequence
    uint64_t populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow, std::vector<Tag> &blockActors,
                      std::vector<Tag> &entities);

    // a chunk generated now: decorated from the cache's copy, holding what its neighbours spilled; `older`:
    // spills recorded elsewhere, applied first unless the copy already holds them; `base`: see LevelChunk
    uint64_t populateGenerated(LevelChunk &chunk, const std::vector<GeneratedBlockChange> &older,
                               std::vector<GeneratedBlockChange> &overflow, std::vector<Tag> &blockActors,
                               std::vector<Tag> &entities, uint64_t &base);

private:
    uint64_t _decorate(LevelChunk &chunk, uint64_t sequence, std::vector<GeneratedBlockChange> &overflow,
                       std::vector<Tag> &blockActors, std::vector<Tag> &entities);

    Level mScratch;
    TerrainCache &mCache;
};
