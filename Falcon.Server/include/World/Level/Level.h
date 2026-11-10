#pragma once

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Level/BlockUpdateScheduler.h"
#include "World/Level/BlockUpdateType.h"
#include "World/Level/ClientSideGeneration.h"
#include "World/Level/Map/MapDataManager.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Chunk/ChunkWorker.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/OverworldGenerator.h"
#include "World/Level/GameRules.h"
#include "World/Level/Storage/LevelStorage.h"
#include "World/Village/VillageManager.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Actor;
class Packet;
class Particle;
class ServerNetworkHandler;
class TerrainCache;

// one dimension; main thread only: loading and generation go to a ChunkWorker, back via drainCompletedChunks()
class Level {
    friend class LevelPersistenceProbe;

public:
    struct ChunkPosition {
        int32_t mX;
        int32_t mZ;
    };

    Level(const std::string &name, int viewDistance, int64_t seed = 0,
          DimensionType dimension = DimensionType::Overworld);

    Level &operator=(Level &&other) noexcept;

    const std::string &getName() const {
        return mName;
    }

    int getViewDistance() const {
        return mViewDistance;
    }

    DimensionType getDimensionType() const {
        return mDimension;
    }

    uint32_t getWorldId() const {
        return mWorldId;
    }

    void setWorldId(uint32_t worldId) {
        mWorldId = worldId;
    }

    int getDimensionId() const {
        return Dimension::toId(mDimension);
    }

    int32_t getMinY() const {
        return Dimension::getMinY(mDimension);
    }

    int32_t getMaxY() const {
        return Dimension::getMaxY(mDimension);
    }

    bool hasSkyLight() const {
        return Dimension::hasSkyLight(mDimension);
    }

    void decorate(LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow);

    int64_t getTime() const {
        return mTime;
    }

    int64_t getDayTime() const {
        return mTime % 24000;
    }

    void setTime(int64_t time) {
        mTime = time < 0 ? 0 : time;
    }

    void addTime(int64_t time) {
        setTime(mTime + time);
    }

    bool isNight() const {
        return getDayTime() > 13184 && getDayTime() < 22800;
    }

    void tickTime() {
        if (mGameRules.getBool("dodaylightcycle"))
            ++mTime;
    }

    bool isRaining() const {
        return mRaining;
    }

    bool isThundering() const {
        return mRaining && mThundering;
    }

    int32_t getRainTime() const {
        return mRainTime;
    }

    int32_t getThunderTime() const {
        return mThunderTime;
    }

    void setRainTime(int32_t rainTime) {
        mRainTime = rainTime;
    }

    void setThunderTime(int32_t thunderTime) {
        mThunderTime = thunderTime;
    }

    void setRainingState(bool raining) {
        mRaining = raining;
    }

    void setThunderingState(bool thundering) {
        mThundering = thundering;
    }

    void initializeWeather();

    void saveWeather();

    using PacketBroadcaster = std::function<void(Level &, const Vector3f &, const Packet &)>;

    void setPacketBroadcaster(PacketBroadcaster broadcaster) {
        mPacketBroadcaster = std::move(broadcaster);
    }

    void addParticle(const Particle &particle);

    BlockActorStore &getBlockActors() {
        return mBlockActors;
    }

    // mobs spawned by a generating structure; the chunk worker stores them with the chunk
    void addGeneratedEntity(Tag entity) {
        mGeneratedEntities.push_back(std::move(entity));
    }

    std::vector<Tag> takeGeneratedEntities() {
        return std::exchange(mGeneratedEntities, std::vector<Tag>());
    }

    const BlockActorStore &getBlockActors() const {
        return mBlockActors;
    }

    GameRules &getGameRules() {
        return mRulesOwner == nullptr ? mGameRules : mRulesOwner->mGameRules;
    }

    const GameRules &getGameRules() const {
        return mRulesOwner == nullptr ? mGameRules : mRulesOwner->mGameRules;
    }

    // game rules belong to the world: Nether and End use the overworld ones
    void shareGameRulesWith(Level &overworld) {
        mRulesOwner = &overworld;
    }

    void initializeGameRules();

    void saveGameRules();

    void saveLevelDat();

    bool canRainAt(int32_t x, int32_t z);

    bool openStorage(const std::string &worldsDirectory);

    bool attachStorage(Level &overworld);

    void saveAll(bool wait = false);

    uint64_t getSaveFailureCount() const {
        return mStorage.getWriteFailures();
    }

    void closeStorage();

    void compactStorage() {
        mStorage.compact();
    }

    bool isStorageOpen() const {
        return mStorage.isOpen();
    }

    void saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities);

    std::vector<Tag> loadEntities(int32_t chunkX, int32_t chunkZ);

    void eraseEntity(int64_t uniqueId);

    // saved in an unloaded chunk
    bool hasStoredEntity(int64_t uniqueId) const;

    void saveTickingArea(const std::string &id, const Tag &area);

    void eraseTickingArea(const std::string &id);

    std::vector<std::pair<std::string, Tag>> loadTickingAreas();

    // wrapped: under a "data" compound, as SavedData stores it; world clocks store the tag itself
    bool saveSavedData(const std::string &key, const Tag &data, bool wrapped = true);

    bool loadSavedData(const std::string &key, Tag &data, bool wrapped = true);

    void saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities);

    std::vector<Tag> loadBlockEntities(int32_t chunkX, int32_t chunkZ);

    Vector3i getSpawnPosition() const;

    void setSpawnPosition(const Vector3i &position);

    Vector3f getSpawnPositionForPlayer();

    static constexpr int32_t UNSET_SPAWN_Y = 32767;

    static constexpr uint32_t DEFAULT_WORLD_START_COUNT = 0xFFFFFFFFu;

    uint32_t getWorldStartCount() const {
        return mWorldStartCount;
    }

    int32_t getMoonPhase() const;

    float getMoonBrightness() const;

    float getRegionalDifficulty(int32_t difficulty) const;

    bool isBonusChestPending() const {
        return mBonusChestEnabled && !mBonusChestSpawned;
    }

    void markBonusChestSpawned() {
        mBonusChestSpawned = true;
    }

    bool isStandable(int32_t x, int32_t y, int32_t z);

    Vector3i findSafeSpawn(const Vector3i &around);

    std::vector<ChunkPosition> getChunksAround(int32_t centerChunkX, int32_t centerChunkZ) const;

    LevelChunk &getChunk(int32_t chunkX, int32_t chunkZ);

    bool isChunkResident(int32_t chunkX, int32_t chunkZ) const;

    bool isChunkPopulated(int32_t chunkX, int32_t chunkZ) const;

    /**
     * Whether storage holds the chunk (DBChunkStorage::isChunkSaved, sub_1406CAF60). The answers are cached as the
     * game does, until the cache fills or storage is reopened; a chunk queued for saving counts as saved.
     */
    bool isChunkSaved(int32_t chunkX, int32_t chunkZ);

    /**
     * Whether a start of one of the structures may reach the chunk. Client side generation keeps such chunks on the
     * server when Falcon does not generate the structure exactly yet.
     */
    bool isNearStructure(const std::vector<const StructureFeature *> &features, int32_t chunkX, int32_t chunkZ);

    // sub_14228D8A0; false for an unknown map
    bool applyMapPixels(int64_t mapId, const std::vector<MapPixel> &pixels);

    // the world's maps, kept on the overworld whose database stores them; written by the next saveAll
    MapItemSavedData *getMapData(int64_t mapId) {
        return mMaps.getMap(&mStorage, mapId);
    }

    MapDataManager &getMaps() {
        return mMaps;
    }

    LevelStorage *getMapStorage() {
        return &mStorage;
    }

    void registerChunkLoader(uint64_t loaderId, int32_t chunkX, int32_t chunkZ);

    bool unregisterChunkLoader(uint64_t loaderId, int32_t chunkX, int32_t chunkZ);

    void unregisterAllChunkLoaders(uint64_t loaderId);

    size_t processChunkUnloads();

    // a bounded number per call
    void processDeferredSaves();

    // a few per tick: populated chunks unread for a whole pass, with no simulating player or actor;
    // unpacked on the next read
    void packIdleChunks();

    // process-wide heap: one call covers every level
    static void releaseFreedHeap();

    // columns under actors stay resident: actors tick anywhere and read blocks with synchronous loads,
    // it would unload and load straight back
    void setOccupiedColumns(std::unordered_set<int64_t> columns);

    void releaseChunkIfUnused(int32_t chunkX, int32_t chunkZ);

    size_t getUnloadQueueSize() const {
        return mUnloadQueue.size();
    }

    LevelChunk *peekChunkPtr(int32_t chunkX, int32_t chunkZ);

    LevelChunk &generateTerrainChunk(int32_t chunkX, int32_t chunkZ);

    LevelChunk &insertChunk(LevelChunk chunk);

    LevelChunk extractChunk(int32_t chunkX, int32_t chunkZ);

    void dropChunk(int32_t chunkX, int32_t chunkZ);

    int getSkyLightAt(int32_t x, int32_t y, int32_t z);

    int getBlockLightAt(int32_t x, int32_t y, int32_t z);

    void addBlockLightUpdate(int32_t x, int32_t y, int32_t z);

    void updateBlockLight();

    int32_t getHeightAt(int32_t x, int32_t z);

    // BlockSource::getAboveTopSolidBlock without water or leaves (0x1432D1700, LevelChunk part sub_1438650A0);
    // Nether: skips the solid blocks at y first, a column under the ceiling finds the ground
    int32_t getAboveTopSolidBlock(int32_t x, int32_t y, int32_t z);

    int32_t getSkyLightSubtracted() const {
        return mSkyLightSubtracted;
    }

    void updateSkyLightSubtracted();

    bool requestChunkAsync(int32_t chunkX, int32_t chunkZ);

    size_t drainCompletedChunks();

    void startWorkers(size_t threadCount);

    const ChunkWorker *getChunkWorker() const {
        return mChunkWorker.get();
    }

    // cached per translation; main thread only: caches filled here, cleared on block changes without a lock
    std::string getChunkData(int32_t chunkX, int32_t chunkZ, const BlockNetworkIdMap *blockIds = nullptr);

    int getChunkSubChunkCount(int32_t chunkX, int32_t chunkZ);

    uint32_t getSubChunkCount() const {
        return LevelChunk::SUB_CHUNK_COUNT;
    }

    size_t getLoadedChunkCount() const {
        return mChunks.size();
    }

    int32_t getBlock(int32_t x, int32_t y, int32_t z);

    BlockState getBlockState(int32_t x, int32_t y, int32_t z);

    bool peekBlockState(int32_t x, int32_t y, int32_t z, BlockState &out);

    const BlockState *peekBlockPtr(int32_t x, int32_t y, int32_t z, int layer = 0);

    bool isSolidAt(int32_t x, int32_t y, int32_t z);

    std::vector<AxisAlignedBB> getCollisionBoxes(const AxisAlignedBB &area, const Actor *actor = nullptr);

    void setBlock(int32_t x, int32_t y, int32_t z, int32_t blockHash);

    void setBlockState(int32_t x, int32_t y, int32_t z, const BlockState &state);

    BlockState getBlockStateAtLayer(int32_t x, int32_t y, int32_t z, int layer);

    void setBlockStateAtLayer(int32_t x, int32_t y, int32_t z, int layer, const BlockState &state);

    LiquidInfo getLiquidInfo(int32_t x, int32_t y, int32_t z);

    Vector3f getLiquidFlowVector(const Vector3i &position);

    void scheduleFluidTick(const Vector3i &position, int64_t delay = 1);

    void tick();

    VillageManager &getVillageManager() {
        return mVillageManager;
    }

    void scheduleBlockUpdate(const Vector3i &position, int64_t delay = 1);

    void setOwner(ServerNetworkHandler *owner) {
        mOwner = owner;
    }

    ServerNetworkHandler *getOwner() const {
        return mOwner;
    }

    void setBlock(const Vector3i &position, const BlockState &state, bool update);

    void updateAround(const Vector3i &position);

    // `changed`: the neighbour whose change caused a normal update, null for the block itself
    void updateAt(const Vector3i &position, BlockUpdateType type, const Vector3i *changed = nullptr);

    void scheduleUpdate(const Vector3i &position, int64_t delay);

    bool isUpdateScheduled(const Vector3i &position) const;

    void cancelUpdate(const Vector3i &position);

    void tickBlockUpdates();

    size_t getScheduledUpdateCount() const {
        return mBlockUpdates.getScheduledCount();
    }

    void onBlockPlaced(const Vector3i &position, const BlockState &state);

    void onBlockBroken(const Vector3i &position, const BlockState &previous);

    void setActiveColumns(std::vector<int64_t> columns);

    bool isColumnActive(int32_t chunkX, int32_t chunkZ) const;

    const std::unordered_set<int64_t> &getActiveColumns() const {
        return mActiveColumns;
    }

    size_t getScheduledFluidCount() const {
        return mLiquidPhysics.getScheduledCount();
    }

    size_t getLastFluidProcessedCount() const {
        return mLiquidPhysics.getLastProcessedCount();
    }

    LiquidPhysicsSystem &getLiquidPhysics() {
        return mLiquidPhysics;
    }

    using FluidChange = LiquidChange;

    std::vector<FluidChange> consumeFluidChanges();

    int32_t getAirHash() const {
        return mGenerator->getAirHash();
    }

    int64_t getSeed() const {
        return mSeed;
    }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const {
        return mGenerator->pickBiome(x, y, z);
    }

    const ChunkGenerator &getGenerator() const {
        return *mGenerator;
    }

    // for work outliving the level (biome search on its own thread)
    std::shared_ptr<const ChunkGenerator> shareGenerator() const {
        return mGenerator;
    }

    // the raw terrain shared by the generation workers, null outside a worker's scratch level
    TerrainCache *getTerrainCache() const {
        return mTerrainCache;
    }

    void setTerrainCache(TerrainCache *cache) {
        mTerrainCache = cache;
    }

    size_t processGeneratedChanges();

    std::vector<int64_t> consumeRepopulatedChunks();

    size_t getPendingChangeChunkCount() const {
        return mPendingBlockChanges.size();
    }

private:
    void _refreshChunkShapes(int32_t chunkX, int32_t chunkZ);

    void _syncPendingTicks(LevelChunk &chunk);

    // sky columns relit once at the end of a liquid tick instead of once a write
    void _flushLightBatch();

    static constexpr int MAX_UPDATE_DEPTH = 512;

    static int64_t _packChunk(int32_t x, int32_t z);

    void _generate(LevelChunk &chunk);

    Vector3i _findLandSpawn();

    int32_t _topSolidOrLiquidY(int32_t x, int32_t z);

    void _applyGeneratedChanges(const std::vector<GeneratedBlockChange> &changes);

    void _queueGeneratedChanges(std::vector<GeneratedBlockChange> changes);

    void _replayPendingChanges(int64_t key);

    void _restoreSpilledChanges(int64_t key, std::vector<GeneratedBlockChange> changes);

    void _dispatchChunkEvent(uint32_t type, int32_t chunkX, int32_t chunkZ, bool generated);

    void _flushPendingBlockChanges(bool includeInFlight);

    void _flushParkedChanges();

    bool _persistSpillAround(int32_t chunkX, int32_t chunkZ);

    void _queueCompletedSaves(int32_t chunkX, int32_t chunkZ);

    void _finishPendingChangeWrite(bool wait);

    size_t _generationBacklog() const {
        // parked changes only hold generation back once far beyond what storage is asked to take
        const size_t parkedOverflow = mPendingBlockChangeCount > MAX_PARKED_CHANGES
                                              ? mPendingBlockChangeCount - MAX_PARKED_CHANGES
                                              : 0;
        return mIncomingChanges.size() + parkedOverflow;
    }

    ServerNetworkHandler *mOwner = nullptr;
    Vector3i mSpawnPosition;
    Vector3i mDefaultSpawnPosition;
    bool mHasDefaultSpawnPosition = false;
    bool mBonusChestEnabled = false;
    bool mBonusChestSpawned = false;
    uint32_t mWorldStartCount = DEFAULT_WORLD_START_COUNT;
    bool mHasSpawnPosition = false;
    BlockUpdateScheduler mBlockUpdates;
    int mUpdateDepth = 0;
    std::string mName;
    int mViewDistance;
    int64_t mSeed;
    DimensionType mDimension;
    uint32_t mWorldId = 0;
    std::shared_ptr<ChunkGenerator> mGenerator;
    TerrainCache *mTerrainCache = nullptr;
    LevelStorage mStorage;
    LiquidPhysicsSystem mLiquidPhysics;
    int64_t mLiquidTick = 0;
    bool mBatchLight = false;
    std::unordered_set<int64_t> mBatchedLightColumns;
    VillageManager mVillageManager;
    std::unordered_map<int64_t, LevelChunk> mChunks;
    static const size_t MAX_CHUNK_INSERTS_PER_TICK = 32;
    // drained results awaiting insertion; new requests wait at this many
    static const size_t MAX_COMPLETED_BACKLOG = 64;
    static const size_t GENERATED_CHANGES_BATCH = 1024;
    // generation admission waits on this queue: drained for the whole budget
    static constexpr std::chrono::milliseconds GENERATED_CHANGES_BUDGET{8};
    // high-water mark; results already accepted may overshoot it
    static constexpr size_t MAX_GENERATION_BACKLOG = 65536;
    // parked changes stay in memory for a load to pick up; merging rewrites each chunk's whole list: done as
    // they arrive it keeps the database compacting; a level save writes them instead
    static constexpr size_t PARKED_CHANGES_IN_MEMORY = 1 << 21;
    static constexpr size_t MAX_PARKED_CHANGES = PARKED_CHANGES_IN_MEMORY * 2;

    void _sweepOrphanChunks();

    void _markChunkSaved(int64_t key);

    void _saveLoadedMaps();

    std::unordered_map<int64_t, bool> mSavedChunkCache;
    MapDataManager mMaps;
    ClientSideGeneration::StructureGuard mStructureGuard;

    std::unordered_set<int64_t> mPendingChunks;
    std::unordered_set<int64_t> mActiveColumns;
    // sorted by z, then x: block ticks run chunk by chunk in this order
    std::vector<int64_t> mActiveColumnOrder;
    std::deque<ChunkLoadResult> mCompletedChunks;
    // Copying a chunk duplicates its whole palette, so queued saves are copied within a small time budget.
    static constexpr std::chrono::milliseconds DEFERRED_SAVE_BUDGET{3};
    // Copies wait in the worker queue until LevelDB writes them, so they are only made while few are queued.
    static constexpr size_t MAX_QUEUED_BACKGROUND_SAVES = 64;
    std::deque<int64_t> mDeferredSaves;
    // generated content final; saved while no generation runs
    std::deque<int64_t> mIdleSaves;
    // on a worker: a LevelDB write can block for seconds while compacting; actors being deleted are hidden
    // from loads, they cannot come back
    std::vector<int64_t> mQueuedEntityErasures;
    std::vector<int64_t> mErasingEntityBatch;
    std::unordered_set<int64_t> mErasingEntities;
    std::future<void> mEntityEraseWrite;

    void _flushEntityErasures(bool wait);
    std::unordered_set<int64_t> mRepopulatedChunks;
    std::unordered_map<int64_t, std::unordered_set<uint64_t>> mChunkLoaders;
    std::unordered_set<int64_t> mUnloadQueue;
    static constexpr size_t MAX_CHUNK_UNLOADS_PER_TICK = 8;
    static constexpr int32_t UNLOAD_SWEEP_INTERVAL_TICKS = 200;
    static constexpr size_t HEAP_TRIM_UNLOAD_THRESHOLD = 256;
    static constexpr size_t MAX_PACK_VISITS_PER_TICK = 4096;
    static constexpr std::chrono::microseconds PACK_BUDGET{2000};
    // snapshot of the loaded chunks for the pass in progress
    std::vector<int64_t> mPackKeys;
    size_t mPackCursor = 0;
    std::unordered_set<int64_t> mOrphanCandidates;
    std::unordered_set<int64_t> mOccupiedColumns;
    int32_t mUnloadSweepTicks = 0;
    size_t mUnloadedSinceTrim = 0;
    std::deque<GeneratedBlockChange> mIncomingChanges;
    std::unordered_map<int64_t, std::vector<GeneratedBlockChange>> mPendingBlockChanges;
    size_t mPendingBlockChangeCount = 0;
    size_t mPersistingChangeCount = 0;
    // how many parked changes a background save still has to hand to storage
    size_t mParkedChangesToFlush = 0;
    // in the write in flight; a batch of chunks per write: persisting keeps up with generation
    std::unordered_set<int64_t> mPersistingChangeKeys;
    static constexpr size_t MAX_PERSIST_BATCH_CHUNKS = 64;
    std::future<std::vector<GeneratedBlockChange>> mPendingChangeWrite;
    std::unique_ptr<ChunkWorker> mChunkWorker;
    int64_t mTime = 0;
    bool mRaining = false;
    int32_t mRainTime = 0;
    bool mThundering = false;
    int32_t mThunderTime = 0;
    int32_t mSkyLightSubtracted = 0;
    GameRules mGameRules;
    Level *mRulesOwner = nullptr;
    BlockActorStore mBlockActors{*this};
    std::vector<Tag> mGeneratedEntities;
    PacketBroadcaster mPacketBroadcaster;
    std::unordered_set<int64_t> mBlockLightQueue;
};
