#pragma once

#include "Block/BlockActorStore.h"
#include "Block/Systems/LiquidPhysicsSystem.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Level/BlockUpdateScheduler.h"
#include "Level/BlockUpdateType.h"
#include "Level/GeneratedBlockChange.h"
#include "Level/LevelChunk.h"
#include "Level/ChunkWorker.h"
#include "Level/Dimension.h"
#include "Level/Generator/ChunkGenerator.h"
#include "Level/Generator/Overworld/OverworldGenerator.h"
#include "Level/GameRules.h"
#include "Level/LevelStorage.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Packet;
class Particle;
class ServerNetworkHandler;

class Level {
public:
    struct ChunkPosition {
        int32_t mX;
        int32_t mZ;
    };

    Level(const std::string &name, int viewDistance, int64_t seed = 0,
          DimensionType dimension = DimensionType::Overworld);

    Level &operator=(Level &&other) noexcept;

    const std::string &getName() const { return mName; }

    int getViewDistance() const { return mViewDistance; }

    DimensionType getDimensionType() const { return mDimension; }

    int getDimensionId() const { return Dimension::toId(mDimension); }

    int32_t getMinY() const { return Dimension::getMinY(mDimension); }

    int32_t getMaxY() const { return Dimension::getMaxY(mDimension); }

    bool hasSkyLight() const { return Dimension::hasSkyLight(mDimension); }

    void decorate(LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow);

    int64_t getTime() const { return mTime; }

    int64_t getDayTime() const { return mTime % 24000; }

    void setTime(int64_t time) { mTime = time < 0 ? 0 : time; }

    void addTime(int64_t time) { setTime(mTime + time); }

    bool isNight() const {
        return getDayTime() > 13184 && getDayTime() < 22800;
    }

    void tickTime() {
        if (mGameRules.getBool("dodaylightcycle"))
            ++mTime;
    }

    bool isRaining() const { return mRaining; }

    bool isThundering() const { return mRaining && mThundering; }

    int32_t getRainTime() const { return mRainTime; }

    int32_t getThunderTime() const { return mThunderTime; }

    void setRainTime(int32_t rainTime) { mRainTime = rainTime; }

    void setThunderTime(int32_t thunderTime) { mThunderTime = thunderTime; }

    void setRainingState(bool raining) { mRaining = raining; }

    void setThunderingState(bool thundering) { mThundering = thundering; }

    void initializeWeather();

    void saveWeather();

    using PacketBroadcaster = std::function<void(Level &, const Vector3f &, const Packet &)>;

    void setPacketBroadcaster(PacketBroadcaster broadcaster) {
        mPacketBroadcaster = std::move(broadcaster);
    }

    void addParticle(const Particle &particle);

    BlockActorStore &getBlockActors() { return mBlockActors; }

    const BlockActorStore &getBlockActors() const { return mBlockActors; }

    GameRules &getGameRules() { return mGameRules; }

    const GameRules &getGameRules() const { return mGameRules; }

    void initializeGameRules();

    void saveGameRules();

    void saveLevelDat();

    bool canRainAt(int32_t x, int32_t z);

    bool openStorage(const std::string &worldsDirectory);

    bool attachStorage(Level &overworld);

    void saveAll();

    void closeStorage();

    void compactStorage() { mStorage.compact(); }

    bool isStorageOpen() const { return mStorage.isOpen(); }

    void saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities);

    std::vector<Tag> loadEntities(int32_t chunkX, int32_t chunkZ);

    void eraseEntity(int64_t uniqueId);

    void saveTickingArea(const std::string &id, const Tag &area);

    void eraseTickingArea(const std::string &id);

    std::vector<std::pair<std::string, Tag>> loadTickingAreas();

    void saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities);

    std::vector<Tag> loadBlockEntities(int32_t chunkX, int32_t chunkZ);

    Vector3i getSpawnPosition() const;

    void setSpawnPosition(const Vector3i &position);

    Vector3f getSpawnPositionForPlayer();

    static constexpr int32_t UNSET_SPAWN_Y = 32767;

    static constexpr uint32_t DEFAULT_WORLD_START_COUNT = 0xFFFFFFFFu;

    uint32_t getWorldStartCount() const { return mWorldStartCount; }

    int32_t getMoonPhase() const;

    float getMoonBrightness() const;

    float getRegionalDifficulty(int32_t difficulty) const;

    bool isBonusChestPending() const { return mBonusChestEnabled && !mBonusChestSpawned; }

    void markBonusChestSpawned() { mBonusChestSpawned = true; }

    bool isStandable(int32_t x, int32_t y, int32_t z);

    Vector3i findSafeSpawn(const Vector3i &around);

    std::vector<ChunkPosition> getChunksAround(int32_t centerChunkX, int32_t centerChunkZ) const;

    LevelChunk &getChunk(int32_t chunkX, int32_t chunkZ);

    bool isChunkResident(int32_t chunkX, int32_t chunkZ) const;

    bool isChunkPopulated(int32_t chunkX, int32_t chunkZ) const;

    void registerChunkLoader(uint64_t loaderId, int32_t chunkX, int32_t chunkZ);

    bool unregisterChunkLoader(uint64_t loaderId, int32_t chunkX, int32_t chunkZ);

    void unregisterAllChunkLoaders(uint64_t loaderId);

    size_t processChunkUnloads();

    void releaseChunkIfUnused(int32_t chunkX, int32_t chunkZ);

    size_t getUnloadQueueSize() const { return mUnloadQueue.size(); }

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

    int32_t getSkyLightSubtracted() const { return mSkyLightSubtracted; }

    void updateSkyLightSubtracted();

    bool requestChunkAsync(int32_t chunkX, int32_t chunkZ);

    size_t drainCompletedChunks();

    void startWorkers(size_t threadCount);

    const ChunkWorker *getChunkWorker() const { return mChunkWorker.get(); }

    std::string getChunkData(int32_t chunkX, int32_t chunkZ);

    int getChunkSubChunkCount(int32_t chunkX, int32_t chunkZ);

    uint32_t getSubChunkCount() const { return LevelChunk::SUB_CHUNK_COUNT; }

    size_t getLoadedChunkCount() const { return mChunks.size(); }

    int32_t getBlock(int32_t x, int32_t y, int32_t z);

    BlockState getBlockState(int32_t x, int32_t y, int32_t z);

    bool peekBlockState(int32_t x, int32_t y, int32_t z, BlockState &out);

    const BlockState *peekBlockPtr(int32_t x, int32_t y, int32_t z, int layer = 0);

    bool isSolidAt(int32_t x, int32_t y, int32_t z);

    std::vector<AxisAlignedBB> getCollisionBoxes(const AxisAlignedBB &area);

    void setBlock(int32_t x, int32_t y, int32_t z, int32_t blockHash);

    void setBlockState(int32_t x, int32_t y, int32_t z, const BlockState &state);

    BlockState getBlockStateAtLayer(int32_t x, int32_t y, int32_t z, int layer);

    void setBlockStateAtLayer(int32_t x, int32_t y, int32_t z, int layer, const BlockState &state);

    LiquidInfo getLiquidInfo(int32_t x, int32_t y, int32_t z);

    Vector3f getLiquidFlowVector(const Vector3i &position);

    void scheduleFluidTick(const Vector3i &position, int64_t delay = 1);

    void tick();

    void scheduleBlockUpdate(const Vector3i &position, int64_t delay = 1);

    void setOwner(ServerNetworkHandler *owner) { mOwner = owner; }

    ServerNetworkHandler *getOwner() const { return mOwner; }

    void setBlock(const Vector3i &position, const BlockState &state, bool update);

    void updateAround(const Vector3i &position);

    void updateAt(const Vector3i &position, BlockUpdateType type);

    void scheduleUpdate(const Vector3i &position, int64_t delay);

    bool isUpdateScheduled(const Vector3i &position) const;

    void cancelUpdate(const Vector3i &position);

    void tickBlockUpdates();

    size_t getScheduledUpdateCount() const { return mBlockUpdates.getScheduledCount(); }

    void onBlockPlaced(const Vector3i &position, const BlockState &state);

    void onBlockBroken(const Vector3i &position, const BlockState &previous);

    BlockUpdateScheduler &getBlockUpdateScheduler() { return mBlockUpdateScheduler; }

    void setActiveColumns(std::vector<int64_t> columns);

    bool isColumnActive(int32_t chunkX, int32_t chunkZ) const;

    const std::unordered_set<int64_t> &getActiveColumns() const { return mActiveColumns; }

    size_t getScheduledFluidCount() const { return mBlockUpdateScheduler.getScheduledCount(); }

    size_t getLastFluidProcessedCount() const { return mBlockUpdateScheduler.getLastProcessedCount(); }

    using FluidChange = LiquidChange;

    std::vector<FluidChange> consumeFluidChanges();

    int32_t getAirHash() const { return mGenerator->getAirHash(); }

    int64_t getSeed() const { return mSeed; }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const { return mGenerator->pickBiome(x, y, z); }

    OverworldBiomeResult pickBiomeResult(int32_t x, int32_t y, int32_t z) const;

    size_t processGeneratedChanges();

    std::vector<int64_t> consumeRepopulatedChunks();

    size_t getPendingChangeChunkCount() const { return mPendingBlockChanges.size(); }

private:
    static constexpr int MAX_UPDATE_DEPTH = 512;

    static int64_t _packChunk(int32_t x, int32_t z);

    void _generate(LevelChunk &chunk);

    Vector3i _findLandSpawn();

    int32_t _topSolidOrLiquidY(int32_t x, int32_t z);

    void _applyGeneratedChanges(const std::vector<GeneratedBlockChange> &changes);

    void _queueGeneratedChanges(std::vector<GeneratedBlockChange> changes);

    void _replayPendingChanges(int64_t key);

    void _dispatchChunkEvent(uint32_t type, int32_t chunkX, int32_t chunkZ, bool generated);

    void _flushPendingBlockChanges(bool includeInFlight);

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
    std::unique_ptr<ChunkGenerator> mGenerator;
    LevelStorage mStorage;
    BlockUpdateScheduler mBlockUpdateScheduler;
    LiquidPhysicsSystem mLiquidPhysics;
    std::unordered_map<int64_t, LevelChunk> mChunks;
    static const size_t MAX_CHUNK_INSERTS_PER_TICK = 8;

    std::unordered_map<int64_t, std::string> mChunkNetworkCache;
    std::unordered_set<int64_t> mPendingChunks;
    std::unordered_set<int64_t> mActiveColumns;
    std::deque<ChunkLoadResult> mCompletedChunks;
    std::unordered_set<int64_t> mRepopulatedChunks;
    std::unordered_map<int64_t, std::unordered_set<uint64_t>> mChunkLoaders;
    std::unordered_set<int64_t> mUnloadQueue;
    static constexpr size_t MAX_CHUNK_UNLOADS_PER_TICK = 8;
    std::vector<GeneratedBlockChange> mIncomingChanges;
    std::unordered_map<int64_t, std::vector<GeneratedBlockChange>> mPendingBlockChanges;
    std::unique_ptr<ChunkWorker> mChunkWorker;
    int64_t mTime = 0;
    bool mRaining = false;
    int32_t mRainTime = 0;
    bool mThundering = false;
    int32_t mThunderTime = 0;
    int32_t mSkyLightSubtracted = 0;
    GameRules mGameRules;
    BlockActorStore mBlockActors{*this};
    PacketBroadcaster mPacketBroadcaster;
    std::unordered_set<int64_t> mBlockLightQueue;
};
