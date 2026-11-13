#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace leveldb {
    class Cache;
    class DB;
    class FilterPolicy;
    class DecompressAllocator;
    struct ReadOptions;
}

enum class LevelDbTag : unsigned char {
    Data3D = '+',
    Version = ',',
    Data2D = '-',
    SubChunkPrefix = '/',
    LegacyTerrain = '0',
    BlockEntities = '1',
    PendingTicks = '3',
    LegacyEntities = '2',
    FinalizedState = '6',
    RandomTicks = ':',
    StructureBounds = 'w',
    NamedTicks = 'z',
    LegacyVersion = 'v'
};

// one dimension; called from the main thread and the chunk workers, on LevelDB's own thread safety
class LevelStorage {
public:
    LevelStorage();

    ~LevelStorage();

    LevelStorage(const LevelStorage &) = delete;

    LevelStorage &operator=(const LevelStorage &) = delete;

    LevelStorage(LevelStorage &&other) noexcept;

    LevelStorage &operator=(LevelStorage &&other) noexcept;

    bool open(const std::string &worldsDirectory, const std::string &levelName, int dimensionId = 0);

    // opens a level's database on another thread; the next open() of that level adopts it
    static void preopen(const std::string &worldsDirectory, const std::string &levelName);

    // closes a database preopen() left unclaimed: it holds the level's lock
    static void discardPreopened();

    bool attach(const LevelStorage &source, int dimensionId);

    void close();

    void compact();

    bool isOpen() const {
        return mDb != nullptr;
    }

    uint64_t getWriteFailures() const {
        return mWriteFailures.load();
    }

    bool saveChunk(const LevelChunk &chunk);

    bool loadChunk(LevelChunk &chunk);

    /**
     * Whether a chunk of any format is stored, from the same version and legacy terrain records the game checks
     * (DBChunkStorage, sub_1406CA2F0). It reads storage, so callers cache the answer.
     */
    bool hasChunk(int32_t chunkX, int32_t chunkZ) const;

    bool mergePendingBlockChanges(int32_t chunkX, int32_t chunkZ, const std::vector<GeneratedBlockChange> &changes);

    std::vector<GeneratedBlockChange> loadPendingBlockChanges(int32_t chunkX, int32_t chunkZ);

    bool erasePendingBlockChanges(int32_t chunkX, int32_t chunkZ);

    bool saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities);

    // adds to the chunk's stored actors instead of replacing them: for a chunk whose actors are not loaded
    bool appendEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities);

    std::vector<Tag> loadEntities(int32_t chunkX, int32_t chunkZ);

    bool eraseEntity(int64_t uniqueId);

    bool hasEntity(int64_t uniqueId) const;

    bool saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities);

    std::vector<Tag> loadBlockEntities(int32_t chunkX, int32_t chunkZ);

    bool saveWeather(bool raining, int32_t rainTime, bool thundering, int32_t thunderTime);

    bool loadWeather(bool &raining, int32_t &rainTime, bool &thundering, int32_t &thunderTime);

    bool saveTickingArea(const std::string &id, const Tag &area);

    bool eraseTickingArea(const std::string &id);

    std::vector<std::pair<std::string, Tag>> loadTickingAreas();

    // The saved data of a map, stored level-wide under map_<id> like the game does.
    bool loadMapData(int64_t mapId, Tag &out) const;

    bool saveMapData(int64_t mapId, const Tag &data);

    bool eraseMapData(int64_t mapId);

    bool saveGameRules(const Tag &rules);

    /** Data the game saves under a plain key, such as "TheEnd", as {"data": compound}. */
    // wrapped: under a "data" compound, as SavedData stores it; world clocks store the tag itself
    bool saveSavedData(const std::string &key, const Tag &data, bool wrapped = true);

    bool loadSavedData(const std::string &key, Tag &data, bool wrapped = true);

    // every key starting with prefix, prefix removed, with its unwrapped tag
    std::vector<std::pair<std::string, Tag>> loadSavedDataWithPrefix(const std::string &prefix);

    bool eraseSavedData(const std::string &key);

    bool loadGameRules(Tag &rules);

    // display name, game mode, difficulty and the game's own options are kept from the file;
    // the arguments only fill them in when missing
    void writeLevelDat(const std::string &levelName, int32_t spawnX, int32_t spawnY, int32_t spawnZ, int32_t gameType,
                       int32_t difficulty, int64_t seed, int64_t time, bool bonusChestEnabled, bool bonusChestSpawned,
                       int64_t worldStartCount) const;

    bool readLevelDat(Tag &out) const;

private:
    bool _recordWrite(bool success) const;
    std::string _makeKey(int32_t chunkX, int32_t chunkZ, LevelDbTag tag) const;

    std::string _makeDataVersionKey(int32_t chunkX, int32_t chunkZ) const;

    int _firstBiomeSection() const;

    int _biomeSectionCount() const;

    std::string _makeSubChunkKey(int32_t chunkX, int32_t chunkZ, int8_t subY) const;

    std::string _makePendingChangesKey(int32_t chunkX, int32_t chunkZ) const;

    std::string _makeActorDigestKey(int32_t chunkX, int32_t chunkZ) const;

    static std::string _makeActorStorageId(int64_t uniqueId);

    static void _appendLInt(std::string &out, int32_t value);

    leveldb::ReadOptions _readOptions() const;

    std::shared_ptr<const leveldb::FilterPolicy> mFilterPolicy;
    std::shared_ptr<leveldb::Cache> mBlockCache;
    std::shared_ptr<leveldb::DB> mDb;
    std::shared_ptr<leveldb::DecompressAllocator> mDecompressAllocator;
    std::string mPath;
    int mDimensionId;
    mutable std::atomic<uint64_t> mWriteFailures{0};
};
