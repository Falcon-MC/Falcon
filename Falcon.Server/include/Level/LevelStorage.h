#pragma once

#include "Level/GeneratedBlockChange.h"
#include "Level/LevelChunk.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <memory>
#include <string>
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
    BlockEntities = '1',
    Entities = '2',
    FinalizedState = '6',
    LegacyVersion = 'v'
};

class LevelStorage {
public:
    LevelStorage();

    ~LevelStorage();

    LevelStorage(const LevelStorage &) = delete;

    LevelStorage &operator=(const LevelStorage &) = delete;

    LevelStorage(LevelStorage &&other) noexcept;

    LevelStorage &operator=(LevelStorage &&other) noexcept;

    bool open(const std::string &worldsDirectory, const std::string &levelName, int dimensionId = 0);

    bool attach(const LevelStorage &source, int dimensionId);

    void close();

    void compact();

    bool isOpen() const { return mDb != nullptr; }

    bool saveChunk(const LevelChunk &chunk);

    bool loadChunk(LevelChunk &chunk);

    bool mergePendingBlockChanges(int32_t chunkX, int32_t chunkZ, const std::vector<GeneratedBlockChange> &changes);

    std::vector<GeneratedBlockChange> loadPendingBlockChanges(int32_t chunkX, int32_t chunkZ);

    bool erasePendingBlockChanges(int32_t chunkX, int32_t chunkZ);

    bool saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities);

    std::vector<Tag> loadEntities(int32_t chunkX, int32_t chunkZ);

    bool saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities);

    std::vector<Tag> loadBlockEntities(int32_t chunkX, int32_t chunkZ);

    bool saveWeather(bool raining, int32_t rainTime, bool thundering, int32_t thunderTime);

    bool loadWeather(bool &raining, int32_t &rainTime, bool &thundering, int32_t &thunderTime);

    bool saveGameRules(const Tag &rules);

    bool loadGameRules(Tag &rules);

    void writeLevelDat(const std::string &levelName, int32_t spawnX, int32_t spawnY, int32_t spawnZ,
                       int32_t gameType, int32_t difficulty, int64_t seed, int64_t time, bool bonusChestEnabled,
                       bool bonusChestSpawned) const;

    bool readLevelDat(Tag &out) const;

private:
    std::string _makeKey(int32_t chunkX, int32_t chunkZ, LevelDbTag tag) const;

    std::string _makeDataVersionKey(int32_t chunkX, int32_t chunkZ) const;

    std::string _makeSubChunkKey(int32_t chunkX, int32_t chunkZ, int8_t subY) const;

    std::string _makePendingChangesKey(int32_t chunkX, int32_t chunkZ) const;

    static void _appendLInt(std::string &out, int32_t value);

    leveldb::ReadOptions _readOptions() const;

    std::shared_ptr<const leveldb::FilterPolicy> mFilterPolicy;
    std::shared_ptr<leveldb::Cache> mBlockCache;
    std::shared_ptr<leveldb::DB> mDb;
    std::shared_ptr<leveldb::DecompressAllocator> mDecompressAllocator;
    std::string mPath;
    int mDimensionId;
};
