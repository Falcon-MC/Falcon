#pragma once

#include "Core/NBT/Tag.h"
#include "World/Level/Chunk/ChunkStructureBounds.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Level.h"

#include <array>
#include <atomic>
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

// what one chunk's decoration changed in its 3x3, net per block; the same for every load order
struct ChunkDecoration {
    // a ChunkWrite with its states numbered in mStates: thousands per decoration, thousands of decorations kept
    struct Write {
        uint32_t mIndex = 0;
        uint16_t mBefore = 0;
        uint16_t mAfter = 0;
    };

    // `write` at the end of the slot's writes, which stay in index order
    void add(size_t slot, const ChunkWrite &write);

    ChunkWrite get(size_t slot, size_t index) const {
        const Write &write = mWrites[slot][index];
        return {write.mIndex, mStates[write.mBefore], mStates[write.mAfter]};
    }

    // what the cache counts it as
    size_t bytes() const;

    // slot (dx + 1) * 3 + (dz + 1), sorted by ChunkWrite::mIndex
    std::array<std::vector<Write>, 9> mWrites;
    std::vector<const BlockState *> mStates;
    // anywhere in the 3x3, sorted by position
    std::vector<Tag> mBlockActors;
    // the decorated chunk's own: entities, structures, ticks
    std::vector<Tag> mEntities;
    ChunkStructureBounds mStructures;
    std::string mPendingTicks;
};

// shared by every worker: raw terrain and decorations, both functions of the seed and the position only.
// The game decorates two neighbours in decorationClass order (measured in BDS worlds); here a decoration
// reads the terrain of its 3x3 with the writes of the earlier-class decorations around it, and never
// those of its own class or later ones: a chunk is the same whatever order chunks are asked for in
class TerrainCache {
public:
    // a decoration reads the terrain of chunks up to 5 away from the chunks asked for; at view distance 32 a
    // spawn spiral generates each terrain about once with this many
    static constexpr size_t MAX_CACHED_CHUNKS = 8192;
    // about 45 KB each: what a decoration's writes take; evicted ones are decorated again, the same
    static constexpr size_t MAX_DECORATION_BYTES = (size_t) 64 << 20;

    using Decoration = std::function<void(Level &, LevelChunk &)>;

    // `decoration`: the generator's own when empty; one per cache, its decorations are kept
    explicit TerrainCache(const ChunkGenerator &generator, Decoration decoration = {});

    TerrainCache(const TerrainCache &) = delete;

    TerrainCache &operator=(const TerrainCache &) = delete;

    // in the order (z even, x even), (z even, x odd), (z odd, x odd), (z odd, x even): two chunks decorated
    // together by the game always resolve their overlap this way, whatever the load order
    static int32_t decorationClass(int32_t chunkX, int32_t chunkZ);

    // waits when another worker is generating the same chunk
    LevelChunk terrain(int32_t chunkX, int32_t chunkZ);

    // as terrain(), without the copy
    std::shared_ptr<const LevelChunk> sharedTerrain(int32_t chunkX, int32_t chunkZ);

    // builds the terrain unless it is cached or another worker is building it; never waits
    void prepareTerrain(int32_t chunkX, int32_t chunkZ);

    bool hasTerrain(int32_t chunkX, int32_t chunkZ);

    // drops every cached terrain and decoration; what is in use stays alive until released
    void clear();

    uint64_t getTerrainGenerations() const {
        return mTerrainGenerations.load();
    }

    uint64_t getDecorationCount() const {
        return mDecorationCount.load();
    }

    const ChunkGenerator &getGenerator() const {
        return mGenerator;
    }

    const Decoration &getDecoration() const {
        return mDecoration;
    }

    std::shared_ptr<const ChunkDecoration> findDecoration(int32_t chunkX, int32_t chunkZ);

    // true: the caller decorates the chunk and stores it; false: `found` holds it, another worker's, or is null
    // when another worker is still decorating it and `wait` is false
    bool claimDecoration(int32_t chunkX, int32_t chunkZ, std::shared_ptr<const ChunkDecoration> &found, bool wait);

    // a null decoration only releases the claim
    void storeDecoration(int32_t chunkX, int32_t chunkZ, std::shared_ptr<const ChunkDecoration> decoration);

    // until a decoration is stored, at most `timeout`
    void awaitDecoration(std::chrono::milliseconds timeout);

    // a value the first decorated chunk of a structure start fixes for the whole start, as the game keeps it in the
    // piece every chunk shares; that chunk is the start's earliest class, decorated before the others; in memory
    // only, as in the game: temple starts and pieces are only ever built by createStart, never loaded
    bool findStartValue(const std::string &structure, int32_t startX, int32_t startZ, int32_t &value);

    // keeps the value stored first for the start and returns it
    int32_t storeStartValue(const std::string &structure, int32_t startX, int32_t startZ, int32_t value);

private:
    struct Entry {
        // shared and immutable once stored: a worker copies one out after releasing the lock
        std::shared_ptr<const LevelChunk> mTerrain;
        std::list<int64_t>::iterator mOrder;
    };

    struct DecorationEntry {
        std::shared_ptr<const ChunkDecoration> mDecoration;
        std::list<int64_t>::iterator mOrder;
    };

    // releases the claim even when generation throws: waiting workers carry on
    struct GenerationClaim {
        TerrainCache &mCache;
        int64_t mKey;

        ~GenerationClaim();
    };

    static int64_t _packChunk(int32_t chunkX, int32_t chunkZ);

    std::shared_ptr<const LevelChunk> _get(int32_t chunkX, int32_t chunkZ);

    const ChunkGenerator &mGenerator;
    const Decoration mDecoration;

    std::mutex mMutex;
    std::condition_variable mPublished;
    std::unordered_set<int64_t> mGenerating;
    std::unordered_map<int64_t, Entry> mChunks;
    std::list<int64_t> mOrder;

    std::unordered_set<int64_t> mDecorating;
    std::condition_variable mDecorated;
    std::unordered_map<int64_t, DecorationEntry> mDecorations;
    std::list<int64_t> mDecorationOrder;
    size_t mDecorationBytes = 0;

    std::atomic<uint64_t> mTerrainGenerations{0};
    std::atomic<uint64_t> mDecorationCount{0};

    std::mutex mStartMutex;
    std::map<std::tuple<std::string, int32_t, int32_t>, int32_t> mStartValues;
};

// per worker: a private scratch level holding a chunk and its neighbours, from the shared TerrainCache, while decorated
class GeneratorChunkSource {
public:
    GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache);

    GeneratorChunkSource(const GeneratorChunkSource &) = delete;

    GeneratorChunkSource &operator=(const GeneratorChunkSource &) = delete;

    // the chunk with every decoration reaching it: its terrain, then the decorations of its 3x3 in
    // decorationClass order; `blockActors` and `entities`: what they placed in it; `skipped`: bit
    // (dx + 1) * 3 + (dz + 1) set leaves out that neighbour's decoration
    LevelChunk generate(int32_t chunkX, int32_t chunkZ, std::vector<Tag> &blockActors, std::vector<Tag> &entities,
                        uint16_t skipped = 0);

    // as generate(), onto `chunk` in place of the terrain: a resident chunk never decorated
    void populate(LevelChunk &chunk, std::vector<Tag> &blockActors, std::vector<Tag> &entities, uint16_t skipped = 0);

    // what generating the chunk needs and no cache holds yet, terrain first then decorations by class: work
    // other workers can take ahead
    struct Need {
        int32_t mX;
        int32_t mZ;
        bool mDecoration;
    };

    std::vector<Need> missing(int32_t chunkX, int32_t chunkZ);

    // one need of another worker's chunk; skipped when a worker already builds it; false: a decoration whose
    // earlier ones other workers are still building, to try again later
    bool prepare(const Need &need);

    std::shared_ptr<const ChunkDecoration> decoration(int32_t chunkX, int32_t chunkZ) {
        return _decoration(chunkX, chunkZ, true);
    }

    // the decoration's writes in `slot`, each placed only if the block is still the one the decoration saw:
    // the first of one class wins
    static void apply(LevelChunk &chunk, const ChunkDecoration &decoration, size_t slot);

private:
    struct Earlier {
        int32_t mX;
        int32_t mZ;
        std::shared_ptr<const ChunkDecoration> mDecoration;
    };

    // null when `wait` is false and another worker is decorating it
    std::shared_ptr<const ChunkDecoration> _decoration(int32_t chunkX, int32_t chunkZ, bool wait);

    // each one's decoration in application order; those other workers are decorating come last: this worker
    // decorates the free ones meanwhile
    void _resolve(std::vector<Earlier> &chunks);

    std::vector<Earlier> _earlier(int32_t chunkX, int32_t chunkZ);

    // the decorations reaching the chunk in their application order
    std::vector<Earlier> _around(int32_t chunkX, int32_t chunkZ);

    std::shared_ptr<const ChunkDecoration> _decorate(int32_t chunkX, int32_t chunkZ,
                                                     const std::vector<Earlier> &earlier);

    void _finish(LevelChunk &chunk, std::vector<Tag> &blockActors, std::vector<Tag> &entities, uint16_t skipped);

    Level mScratch;
    TerrainCache &mCache;
};
