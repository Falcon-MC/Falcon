#include "World/Level/Level.h"

#include "World/Level/Biome/BiomeRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/Particle/Particle.h"
#include "World/Level/LightSystem.h"

#include "World/Level/Block/Actor/DataDrivenBlockActor.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "Core/Debug/BedrockLog.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <random>
#include <stdexcept>
#include <utility>

#if defined(__linux__) && defined(__GLIBC__)
#include <malloc.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#endif

namespace {
    // level.dat "experiments": a byte per toggle name beside two bookkeeping flags
    std::vector<std::string> readExperiments(const Tag &levelDat) {
        std::vector<std::string> names;
        const Tag *experiments = levelDat.get("experiments");
        if (experiments == nullptr || !experiments->isCompound())
            return names;
        const std::vector<std::string> &keys = experiments->getKeys();
        const std::vector<Tag> &values = experiments->getValues();
        for (size_t index = 0; index < keys.size() && index < values.size(); ++index) {
            if (keys[index] == "experiments_ever_used" || keys[index] == "saved_with_toggled_experiments")
                continue;
            if (values[index].getType() == Tag::Type::Byte && values[index].asByte() != 0)
                names.push_back(keys[index]);
        }
        return names;
    }
}

bool Level::hasExperiment(const std::string &name) const {
    return std::find(mExperiments.begin(), mExperiments.end(), name) != mExperiments.end();
}

Level::Level(const std::string &name, int viewDistance, int64_t seed, DimensionType dimension)
        : mName(name), mViewDistance(viewDistance), mSeed(seed), mDimension(dimension),
          mGenerator(DimensionFactory::createGenerator(dimension, seed)), mLiquidPhysics(*this) {}

void Level::decorate(LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) {
    mGenerator->decorate(*this, chunk, overflow);
}

Level &Level::operator=(Level &&other) noexcept {
    if (this == &other)
        return *this;

    _finishPendingChangeWrite(true);
    other._finishPendingChangeWrite(true);
    if (mChunkWorker != nullptr)
        mChunkWorker->stop();
    mChunkWorker.reset();

    if (other.mChunkWorker != nullptr)
        other.mChunkWorker->stop();
    other.mChunkWorker.reset();

    mName = std::move(other.mName);
    mViewDistance = other.mViewDistance;
    mTime = other.mTime;
    mSeed = other.mSeed;
    mDimension = other.mDimension;
    mWorldId = other.mWorldId;
    mSpawnPosition = other.mSpawnPosition;
    mHasSpawnPosition = other.mHasSpawnPosition;
    mGenerator = std::move(other.mGenerator);
    mTerrainCache = other.mTerrainCache;
    mStorage = std::move(other.mStorage);
    mChunks = std::move(other.mChunks);
    mPendingChunks = std::move(other.mPendingChunks);
    mActiveColumns = std::move(other.mActiveColumns);
    mActiveColumnOrder = std::move(other.mActiveColumnOrder);
    mCompletedChunks = std::move(other.mCompletedChunks);
    mDeferredSaves = std::move(other.mDeferredSaves);
    mIdleSaves = std::move(other.mIdleSaves);
    mQueuedEntityErasures = std::move(other.mQueuedEntityErasures);
    mErasingEntityBatch = std::move(other.mErasingEntityBatch);
    mErasingEntities = std::move(other.mErasingEntities);
    mEntityEraseWrite = std::move(other.mEntityEraseWrite);
    mRepopulatedChunks = std::move(other.mRepopulatedChunks);
    mSavedChunkCache = std::move(other.mSavedChunkCache);
    mMaps = std::move(other.mMaps);
    mStructureGuard = std::move(other.mStructureGuard);
    mIncomingChanges = std::move(other.mIncomingChanges);
    mPendingBlockChanges = std::move(other.mPendingBlockChanges);
    mPendingBlockChangeCount = other.mPendingBlockChangeCount;
    other.mPendingBlockChangeCount = 0;
    mLiquidTick = other.mLiquidTick;
    mBlockUpdates.moveStateFrom(std::move(other.mBlockUpdates));
    mLiquidPhysics.moveStateFrom(std::move(other.mLiquidPhysics));
    mNamedTicks = std::move(other.mNamedTicks);
    mNeighborChanges = std::move(other.mNeighborChanges);
    mExperiments = std::move(other.mExperiments);
    mVillageManager = std::move(other.mVillageManager);
    mPoiManager = std::move(other.mPoiManager);
    mGameRules = std::move(other.mGameRules);
    mRulesOwner = other.mRulesOwner;
    mPacketBroadcaster = std::move(other.mPacketBroadcaster);
    mBlockLightQueue = std::move(other.mBlockLightQueue);
    mBlockActors.moveStateFrom(std::move(other.mBlockActors));
    return *this;
}

void Level::addParticle(const Particle &particle) {
    if (!mPacketBroadcaster)
        return;

    const std::unique_ptr<Packet> packet = particle.encode();
    if (packet != nullptr)
        mPacketBroadcaster(*this, particle.getPosition(), *packet);
}

bool Level::openStorage(const std::string &worldsDirectory) {
    if (!mStorage.open(worldsDirectory, mName, getDimensionId()))
        return false;

    mSavedChunkCache.clear();
    mStructureGuard.clear();

    if (mDimension == DimensionType::Overworld) {
        Tag levelDat;
        if (mStorage.readLevelDat(levelDat)) {
            if (levelDat.contains("Time"))
                setTime(levelDat.getLong("Time"));

            const bool reseeded = levelDat.contains("RandomSeed") && levelDat.getLong("RandomSeed") != mSeed;
            if (reseeded)
                mSeed = levelDat.getLong("RandomSeed");

            const auto generator = (DimensionFactory::GeneratorType) levelDat.getInt(
                    "Generator", (int32_t) DimensionFactory::GeneratorType::Overworld);
            if (reseeded || generator == DimensionFactory::GeneratorType::Flat)
                mGenerator = DimensionFactory::createGenerator(mDimension, mSeed, generator,
                                                               levelDat.getString("FlatWorldLayers"));

            mWorldStartCount = (uint32_t) levelDat.getLong("worldStartCount", DEFAULT_WORLD_START_COUNT);
            mExperiments = readExperiments(levelDat);
            mBonusChestEnabled = levelDat.getByte("bonusChestEnabled", 0) != 0;
            mBonusChestSpawned = levelDat.getByte("bonusChestSpawned", 0) != 0;

            if (levelDat.contains("SpawnY") && levelDat.getInt("SpawnY") != UNSET_SPAWN_Y)
                setSpawnPosition(
                        Vector3i(levelDat.getInt("SpawnX"), levelDat.getInt("SpawnY"), levelDat.getInt("SpawnZ")));
        }

        if (!mHasSpawnPosition) {
            mDefaultSpawnPosition = _findLandSpawn();
            mHasDefaultSpawnPosition = true;
        }

        saveLevelDat();
    }

    return true;
}

void Level::saveLevelDat() {
    const Vector3i spawn = mHasSpawnPosition          ? mSpawnPosition
                           : mHasDefaultSpawnPosition ? mDefaultSpawnPosition
                                                      : Vector3i(0, UNSET_SPAWN_Y, 0);
    mStorage.writeLevelDat(mName, spawn.x, spawn.y, spawn.z, 0, 1, mSeed, mTime, mBonusChestEnabled, mBonusChestSpawned,
                           (int64_t) (uint32_t) (mWorldStartCount - 1));
}

bool Level::attachStorage(Level &overworld) {
    mSavedChunkCache.clear();
    return mStorage.attach(overworld.mStorage, getDimensionId());
}

void Level::saveAll(bool wait) {
    if (!mStorage.isOpen())
        return;

    const uint64_t failures = mStorage.getWriteFailures();
    const bool async = mChunkWorker != nullptr && mChunkWorker->isRunning();
    if (wait && async) {
        mChunkWorker->waitForSaves();
        while (drainCompletedChunks() != 0) {
        }
    }

    if (mDimension == DimensionType::Overworld) {
        saveWeather();
        saveGameRules();
        saveLevelDat();
        _saveLoadedMaps();
    }

    if (wait || !async) {
        while (!mIncomingChanges.empty())
            processGeneratedChanges();
    }

    if (wait || !async) {
        _finishPendingChangeWrite(true);
        _flushPendingBlockChanges(true);
        mParkedChangesToFlush = 0;
    } else {
        // parked changes written over the next ticks, a batch at a time: the save covers what its chunks
        // spilled into neighbours
        mParkedChangesToFlush = mPendingBlockChangeCount;
        _flushParkedChanges();
    }

    size_t saved = 0;
    // a full save covers what a background save still has queued
    if (wait || !async) {
        mDeferredSaves.clear();
        mIdleSaves.clear();
        _flushEntityErasures(true);
    }

    for (auto &entry: mChunks) {
        _syncPendingTicks(entry.second);
        if (!entry.second.isDirty()
            && (mChunkWorker == nullptr || !mChunkWorker->hasFailedSave(entry.second.getX(), entry.second.getZ())))
            continue;

        // copying every dirty chunk at once stalls the tick for seconds after a generation burst:
        // processDeferredSaves() copies a few per tick
        if (async && !wait) {
            mDeferredSaves.push_back(entry.first);
            continue;
        }

        if (async) {
            // copy: resident chunks belong to the main thread and keep changing during the save
            std::unique_ptr<LevelChunk> copy(new LevelChunk(entry.second));
            copy->invalidateNetworkCaches();
            mChunkWorker->requestSave(std::move(copy));
            entry.second.clearDirty();
            _markChunkSaved(entry.first);
            saved++;
            continue;
        }

        if (mStorage.saveChunk(entry.second)) {
            entry.second.clearDirty();
            _markChunkSaved(entry.first);
            saved++;
        }
    }

    if (saved != 0)
        LOG_INFO(LogAreaID::Server, "Saved %zu chunk(s) for level %s", saved, mName.c_str());
    if (wait) {
        const bool workerSuccess = !async || mChunkWorker->waitForSaves();
        if (!workerSuccess || mStorage.getWriteFailures() != failures)
            throw std::runtime_error("Could not save level: " + mName);
    }
}

void Level::saveEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities) {
    if (!mStorage.isOpen())
        return;

    mStorage.saveEntities(chunkX, chunkZ, entities);
}

void Level::appendEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &entities) {
    if (!mStorage.isOpen())
        return;

    mStorage.appendEntities(chunkX, chunkZ, entities);
}

std::vector<Tag> Level::loadEntities(int32_t chunkX, int32_t chunkZ) {
    if (!mStorage.isOpen())
        return std::vector<Tag>();

    std::vector<Tag> entities = mStorage.loadEntities(chunkX, chunkZ);
    if (mErasingEntities.empty())
        return entities;

    entities.erase(std::remove_if(entities.begin(), entities.end(),
                                  [this](const Tag &entity) {
                                      return mErasingEntities.count(entity.getLong("UniqueID", 0)) != 0;
                                  }),
                   entities.end());
    return entities;
}

bool Level::hasStoredEntity(int64_t uniqueId) const {
    return mStorage.isOpen() && mStorage.hasEntity(uniqueId);
}

void Level::eraseEntity(int64_t uniqueId) {
    if (!mStorage.isOpen())
        return;

    if (mChunkWorker == nullptr || !mChunkWorker->isRunning()) {
        mStorage.eraseEntity(uniqueId);
        return;
    }

    mQueuedEntityErasures.push_back(uniqueId);
    mErasingEntities.insert(uniqueId);
}

void Level::_flushEntityErasures(bool wait) {
    if (mEntityEraseWrite.valid()) {
        if (!wait && mEntityEraseWrite.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;

        mEntityEraseWrite.get();
        for (const int64_t uniqueId: mErasingEntityBatch)
            mErasingEntities.erase(uniqueId);
        mErasingEntityBatch.clear();
    }

    if (mQueuedEntityErasures.empty())
        return;

    if (mChunkWorker == nullptr || !mChunkWorker->isRunning()) {
        for (const int64_t uniqueId: mQueuedEntityErasures) {
            mStorage.eraseEntity(uniqueId);
            mErasingEntities.erase(uniqueId);
        }
        mQueuedEntityErasures.clear();
        return;
    }

    mErasingEntityBatch.swap(mQueuedEntityErasures);
    mEntityEraseWrite = mChunkWorker->requestEraseEntities(mErasingEntityBatch);
    if (wait)
        _flushEntityErasures(true);
}

void Level::saveTickingArea(const std::string &id, const Tag &area) {
    if (!mStorage.isOpen())
        return;

    mStorage.saveTickingArea(id, area);
}

void Level::eraseTickingArea(const std::string &id) {
    if (!mStorage.isOpen())
        return;

    mStorage.eraseTickingArea(id);
}

std::vector<std::pair<std::string, Tag>> Level::loadTickingAreas() {
    if (!mStorage.isOpen())
        return std::vector<std::pair<std::string, Tag>>();

    return mStorage.loadTickingAreas();
}

bool Level::saveSavedData(const std::string &key, const Tag &data, bool wrapped) {
    return mStorage.saveSavedData(key, data, wrapped);
}

bool Level::loadSavedData(const std::string &key, Tag &data, bool wrapped) {
    return mStorage.loadSavedData(key, data, wrapped);
}

void Level::saveBlockEntities(int32_t chunkX, int32_t chunkZ, const std::vector<Tag> &blockEntities) {
    if (!mStorage.isOpen())
        return;

    mStorage.saveBlockEntities(chunkX, chunkZ, blockEntities);
}

std::vector<Tag> Level::loadBlockEntities(int32_t chunkX, int32_t chunkZ) {
    if (!mStorage.isOpen())
        return std::vector<Tag>();

    return mStorage.loadBlockEntities(chunkX, chunkZ);
}

void Level::closeStorage() {
    if (mChunkWorker != nullptr) {
        mChunkWorker->discardPendingGeneration();
        mChunkWorker->stop();
        _finishPendingChangeWrite(true);
        while (drainCompletedChunks() != 0) {
        }
        for (auto &entry: mChunks) {
            if (mChunkWorker->hasFailedSave(entry.second.getX(), entry.second.getZ()))
                entry.second.markDirty();
        }
        mChunkWorker.reset();
    }
    mPendingChunks.clear();
    try {
        saveAll(true);
    } catch (...) {
        mStorage.close();
        throw;
    }

    mStorage.close();
    mSavedChunkCache.clear();
}

Vector3i Level::getSpawnPosition() const {
    if (mHasSpawnPosition)
        return mSpawnPosition;

    if (mHasDefaultSpawnPosition)
        return mDefaultSpawnPosition;

    return Vector3i(0, mGenerator->getSpawnY(), 0);
}

void Level::setSpawnPosition(const Vector3i &position) {
    mSpawnPosition = position;
    mHasSpawnPosition = true;
}

Vector3f Level::getSpawnPositionForPlayer() {
    const Vector3i spawn = findSafeSpawn(getSpawnPosition());
    return Vector3f((float) spawn.x + 0.5f, (float) spawn.y, (float) spawn.z + 0.5f);
}

int32_t Level::getMoonPhase() const {
    return (int32_t) (((mTime / TICKS_PER_DAY) % 8 + 8) % 8);
}

float Level::getMoonBrightness() const {
    static const float MOON_BRIGHTNESS[] = {1.0f, 0.75f, 0.5f, 0.25f, 0.0f, 0.25f, 0.5f, 0.75f};
    return MOON_BRIGHTNESS[getMoonPhase()];
}

float Level::getRegionalDifficulty(int32_t difficulty) const {
    if (difficulty <= 0)
        return 0.0f;

    const float timeFactor = std::min(std::max((float) mTime - 0.5f, 0.0f) * 0.25f, 0.25f);
    const float moonFactor = std::min(getMoonBrightness() * 0.25f, timeFactor);

    float bonus = moonFactor + (difficulty == 3 ? 0.5f : 0.375f);
    if (difficulty == 1)
        bonus *= 0.5f;

    const float total = (timeFactor + 0.75f + bonus) * (float) difficulty;
    if (total < 2.0f)
        return 0.0f;

    return total <= 4.0f ? (total - 2.0f) * 0.5f : 1.0f;
}

bool Level::isStandable(int32_t x, int32_t y, int32_t z) {
    const auto isLiquid = [](const std::string &name) {
        return name == BlockNames::WATER || name == "minecraft:flowing_water" || name == BlockNames::LAVA
               || name == "minecraft:flowing_lava";
    };

    const std::string below = getBlockState(x, y - 1, z).mName;
    const bool supported = isSolidAt(x, y - 1, z) || below == BlockNames::WATER || below == "minecraft:flowing_water";

    return supported && !isSolidAt(x, y, z) && !isLiquid(getBlockState(x, y, z).mName) && !isSolidAt(x, y + 1, z)
           && !isLiquid(getBlockState(x, y + 1, z).mName);
}

Vector3i Level::findSafeSpawn(const Vector3i &around) {
    if (around.y > LevelChunk::MIN_Y && around.y < LevelChunk::MAX_Y && isStandable(around.x, around.y, around.z))
        return around;

    const int32_t top = _topSolidOrLiquidY(around.x, around.z);
    for (int32_t y = std::max(top + 1, LevelChunk::MIN_Y + 1); y < LevelChunk::MAX_Y; ++y) {
        if (isStandable(around.x, y, around.z))
            return Vector3i(around.x, y, around.z);
    }

    return Vector3i(around.x, top + 1, around.z);
}

int32_t Level::_topSolidOrLiquidY(int32_t x, int32_t z) {
    for (int32_t y = LevelChunk::MAX_Y; y > LevelChunk::MIN_Y; --y) {
        if (getBlockState(x, y, z).mName != BlockNames::AIR)
            return y;
    }

    return LevelChunk::MIN_Y;
}

Vector3i Level::_findLandSpawn() {
    static const int32_t WATER_BIOMES[] = {BiomeIds::OCEAN,
                                           BiomeIds::DEEP_OCEAN,
                                           BiomeIds::WARM_OCEAN,
                                           BiomeIds::LUKEWARM_OCEAN,
                                           BiomeIds::DEEP_LUKEWARM_OCEAN,
                                           BiomeIds::COLD_OCEAN,
                                           BiomeIds::DEEP_COLD_OCEAN,
                                           BiomeIds::FROZEN_OCEAN,
                                           BiomeIds::DEEP_FROZEN_OCEAN,
                                           BiomeIds::LEGACY_FROZEN_OCEAN,
                                           BiomeIds::RIVER,
                                           BiomeIds::FROZEN_RIVER};
    const int32_t SEARCH_STEP = 16;
    const int32_t SEARCH_RADIUS = 1024;
    const int32_t SAMPLE_Y = 64;

    const auto isLand = [&](int32_t x, int32_t z) {
        const int32_t biome = pickBiome(x, SAMPLE_Y, z);
        return std::find(std::begin(WATER_BIOMES), std::end(WATER_BIOMES), biome) == std::end(WATER_BIOMES);
    };

    for (int32_t radius = 0; radius <= SEARCH_RADIUS; radius += SEARCH_STEP) {
        for (int32_t x = -radius; x <= radius; x += SEARCH_STEP) {
            for (int32_t z = -radius; z <= radius; z += SEARCH_STEP) {
                if (std::abs(x) != radius && std::abs(z) != radius)
                    continue;

                if (isLand(x, z))
                    return Vector3i(x, _topSolidOrLiquidY(x, z) + 1, z);
            }
        }
    }

    return Vector3i(0, mGenerator->getSpawnY(), 0);
}

int64_t Level::_packChunk(int32_t x, int32_t z) {
    return ((int64_t) x << 32) | (uint32_t) z;
}

void Level::_generate(LevelChunk &chunk) {
    mGenerator->generate(chunk);
}

LevelChunk &Level::getChunk(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    auto it = mChunks.find(key);
    if (it != mChunks.end())
        return it->second;

    // a synchronous read never overtakes an older pending-change write; streaming retries instead of
    // waiting on the tick thread
    if (mPendingChangeWrite.valid() && mPersistingChangeKeys.count(key) != 0)
        _finishPendingChangeWrite(true);

    LevelChunk chunk(chunkX, chunkZ);
    chunk.setDimension(mDimension);

    if (!mStorage.isOpen() || !mStorage.loadChunk(chunk))
        _generate(chunk);

    if (mStorage.isOpen() && mPendingChunks.find(key) == mPendingChunks.end()) {
        const auto stored = mStorage.loadPendingBlockChanges(chunkX, chunkZ);
        for (const auto &change: stored) {
            if (change.mY >= LevelChunk::MIN_Y && change.mY <= LevelChunk::MAX_Y)
                chunk.setBlock(change.mX & 15, change.mY, change.mZ & 15, *change.mState);
        }
        if (!stored.empty()) {
            LightSystem::computeSkyLight(chunk);
            LightSystem::computeBlockLight(chunk);
            mStorage.erasePendingBlockChanges(chunkX, chunkZ);
        }
    }

    mPendingChunks.erase(key);

    auto result = mChunks.emplace(key, std::move(chunk));
    _loadPendingTicks(result.first->second);
    _replayPendingChanges(key);
    _refreshChunkShapes(chunkX, chunkZ);
    return result.first->second;
}

LevelChunk &Level::generateTerrainChunk(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    auto it = mChunks.find(key);
    if (it != mChunks.end())
        return it->second;

    LevelChunk chunk(chunkX, chunkZ);
    chunk.setDimension(mDimension);
    _generate(chunk);

    return mChunks.emplace(key, std::move(chunk)).first->second;
}

LevelChunk &Level::insertChunk(LevelChunk chunk) {
    const int64_t key = _packChunk(chunk.getX(), chunk.getZ());
    chunk.setDimension(mDimension);
    auto result = mChunks.emplace(key, std::move(chunk));
    if (result.second) {
        LevelChunk &inserted = result.first->second;
        _loadPendingTicks(inserted);
        _refreshChunkShapes(inserted.getX(), inserted.getZ());
    }
    return result.first->second;
}

void Level::_refreshChunkShapes(int32_t chunkX, int32_t chunkZ) {
    // neighbours too: a connection held at an unloaded chunk border resolves on its arrival
    for (int32_t dx = -1; dx <= 1; ++dx) {
        for (int32_t dz = -1; dz <= 1; ++dz) {
            LevelChunk *chunk = peekChunkPtr(chunkX + dx, chunkZ + dz);
            if (chunk == nullptr)
                continue;
            for (int index = 0; index < LevelChunk::SUB_CHUNK_COUNT; ++index) {
                if (!chunk->getSubChunk(index).hasDynamicShapes())
                    continue;
                for (int x = 0; x < 16; ++x) {
                    for (int z = 0; z < 16; ++z) {
                        for (int y = 0; y < 16; ++y) {
                            const int worldY = (index + LevelChunk::LOWEST_SUB_CHUNK_Y) * 16 + y;
                            const BlockState state = chunk->getBlock(x, worldY, z);
                            const Block *block = VanillaBlocks::fromIdentifier(state.mName);
                            if (block == nullptr || !block->hasDynamicShape())
                                continue;
                            const Vector3i position(chunk->getX() * 16 + x, worldY, chunk->getZ() * 16 + z);
                            const BlockState shaped = block->updateShape(*this, position, state);
                            if (!(shaped == state))
                                setBlock(position, shaped, false);
                        }
                    }
                }
            }
        }
    }
}

// ticks change without dirtying the blocks: a chunk is saved again when its record differs
void Level::_syncPendingTicks(LevelChunk &chunk) {
    bool changed = mLiquidPhysics.savePendingTicks(chunk);
    std::string named = mNamedTicks.save(chunk.getX(), chunk.getZ(), getMinY());
    if (named != chunk.getNamedTicks()) {
        chunk.setNamedTicks(std::move(named));
        changed = true;
    }
    if (changed)
        chunk.markDirty();
}

void Level::_loadPendingTicks(LevelChunk &chunk) {
    mLiquidPhysics.loadPendingTicks(chunk);
    mNamedTicks.load(chunk.getX(), chunk.getZ(), getMinY(), chunk.getNamedTicks(),
                     mOwner != nullptr ? mOwner->getCurrentTick() : 0);
}

void Level::_dropPendingTicks(int32_t chunkX, int32_t chunkZ) {
    mLiquidPhysics.dropPendingTicks(chunkX, chunkZ);
    mNamedTicks.drop(chunkX, chunkZ);
}

LevelChunk Level::extractChunk(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    auto it = mChunks.find(key);
    if (it == mChunks.end()) {
        LevelChunk empty(chunkX, chunkZ);
        empty.setDimension(mDimension);
        return empty;
    }

    LevelChunk chunk = std::move(it->second);
    mChunks.erase(it);
    _dropPendingTicks(chunkX, chunkZ);
    return chunk;
}

void Level::dropChunk(int32_t chunkX, int32_t chunkZ) {
    mChunks.erase(_packChunk(chunkX, chunkZ));
    _dropPendingTicks(chunkX, chunkZ);
}

void Level::_queueGeneratedChanges(std::vector<GeneratedBlockChange> changes) {
    if (changes.empty())
        return;

    mIncomingChanges.insert(mIncomingChanges.end(), std::make_move_iterator(changes.begin()),
                            std::make_move_iterator(changes.end()));
}

void Level::_replayPendingChanges(int64_t key) {
    auto pending = mPendingBlockChanges.find(key);
    if (pending == mPendingBlockChanges.end())
        return;

    std::vector<GeneratedBlockChange> changes = std::move(pending->second);
    mPendingBlockChangeCount -= changes.size();
    mPendingBlockChanges.erase(pending);
    _queueGeneratedChanges(std::move(changes));
}

// older than anything parked or applied since the task left: a resident chunk gets them at once, else
// they go before the blocks parked meanwhile
void Level::_restoreSpilledChanges(int64_t key, std::vector<GeneratedBlockChange> changes) {
    if (changes.empty())
        return;

    if (mChunks.find(key) != mChunks.end() && mPendingChunks.find(key) == mPendingChunks.end()) {
        _applyGeneratedChanges(changes);
        return;
    }

    std::vector<GeneratedBlockChange> &parked = mPendingBlockChanges[key];
    mPendingBlockChangeCount += changes.size();
    changes.insert(changes.end(), std::make_move_iterator(parked.begin()), std::make_move_iterator(parked.end()));
    parked = std::move(changes);
}

void Level::_applyGeneratedChanges(const std::vector<GeneratedBlockChange> &changes) {
    // features spill into the same few columns: relit once per column after the batch, same light
    std::unordered_map<int64_t, std::pair<LevelChunk *, std::bitset<256>>> touchedColumns;
    int64_t lastKey = 0;
    LevelChunk *lastChunk = nullptr;
    bool lastParked = false;
    bool haveLast = false;

    for (const GeneratedBlockChange &change: changes) {
        if (change.mY < LevelChunk::MIN_Y || change.mY > LevelChunk::MAX_Y)
            continue;

        const int32_t chunkX = change.mX >> 4;
        const int32_t chunkZ = change.mZ >> 4;
        const int64_t key = _packChunk(chunkX, chunkZ);

        if (!haveLast || key != lastKey) {
            lastKey = key;
            lastChunk = peekChunkPtr(chunkX, chunkZ);
            lastParked = lastChunk == nullptr || mPendingChunks.find(key) != mPendingChunks.end();
            haveLast = true;
        }

        LevelChunk *chunk = lastChunk;
        // absent or loading chunk: kept aside, replayed on arrival or merged into storage on save
        if (lastParked) {
            mPendingBlockChanges[key].push_back(change);
            ++mPendingBlockChangeCount;
            continue;
        }
        // decorated later, from a copy already holding it: its own blocks won there
        if (decorationHolds(chunk->getBaseSequence(), chunk->getDecorationSequence(), change.mSequence))
            continue;

        const int32_t localX = change.mX & 15;
        const int32_t localZ = change.mZ & 15;

        // both interned: equal exactly when the same object
        const BlockState &previous = chunk->getBlock(localX, change.mY, localZ, change.mLayer);
        if (&previous == change.mState)
            continue;

        const int32_t previousLight = BlockLightProperties::packed(previous);
        const int32_t nextLight = BlockLightProperties::packed(*change.mState);
        chunk->setBlock(localX, change.mY, localZ, change.mLayer, *change.mState);
        std::pair<LevelChunk *, std::bitset<256>> &touched = touchedColumns[key];
        touched.first = chunk;

        // mostly stone variants swapped: same light properties keep height and sky light, and an opaque
        // non-emitter holds no block light either way
        if (previousLight == nextLight) {
            if (!BlockLightProperties::isTransparent(nextLight) && BlockLightProperties::lightLevel(nextLight) == 0)
                continue;
        } else {
            touched.second.set((size_t) (localX * 16 + localZ));
        }
        addBlockLightUpdate(change.mX, change.mY, change.mZ);
    }

    for (const auto &entry: touchedColumns) {
        mRepopulatedChunks.insert(entry.first);
        if (entry.second.second.none())
            continue;

        for (size_t column = 0; column < 256; ++column) {
            if (entry.second.second.test(column))
                LightSystem::relightColumn(*entry.second.first, (int) (column / 16), (int) (column % 16));
        }
    }
}

size_t Level::processGeneratedChanges() {
    _flushParkedChanges();
    if (mIncomingChanges.empty())
        return 0;

    // until the budget is spent, at least one batch: a backlog uses spare time, a busy tick grows by one
    // batch at most
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + GENERATED_CHANGES_BUDGET;
    std::vector<GeneratedBlockChange> batch;
    size_t applied = 0;
    do {
        const size_t count = std::min(mIncomingChanges.size(), GENERATED_CHANGES_BATCH);
        batch.clear();
        batch.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            batch.push_back(std::move(mIncomingChanges.front()));
            mIncomingChanges.pop_front();
        }

        _applyGeneratedChanges(batch);
        applied += count;
    } while (!mIncomingChanges.empty() && std::chrono::steady_clock::now() < deadline);

    return applied;
}

void Level::_finishPendingChangeWrite(bool wait) {
    if (!mPendingChangeWrite.valid())
        return;
    if (!wait && mPendingChangeWrite.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    auto failed = mPendingChangeWrite.get();
    mPersistingChangeCount = 0;
    mPersistingChangeKeys.clear();
    if (failed.empty())
        return;

    // failed writes keep their payload, ahead of changes received meanwhile: a retry keeps the newest
    // state per position
    mPendingBlockChangeCount += failed.size();
    std::unordered_map<int64_t, std::vector<GeneratedBlockChange>> older;
    for (GeneratedBlockChange &change: failed)
        older[_packChunk(change.mX >> 4, change.mZ >> 4)].push_back(std::move(change));

    for (auto &entry: older) {
        auto &newer = mPendingBlockChanges[entry.first];
        entry.second.insert(entry.second.end(), std::make_move_iterator(newer.begin()),
                            std::make_move_iterator(newer.end()));
        newer = std::move(entry.second);
    }
}

void Level::_flushParkedChanges() {
    if (mParkedChangesToFlush == 0 && mPendingBlockChangeCount <= PARKED_CHANGES_IN_MEMORY) {
        _finishPendingChangeWrite(false);
        return;
    }

    const size_t before = mPendingBlockChangeCount;
    _flushPendingBlockChanges(false);
    const size_t handed = before > mPendingBlockChangeCount ? before - mPendingBlockChangeCount : 0;
    mParkedChangesToFlush = handed >= mParkedChangesToFlush || mPendingBlockChangeCount == 0
                                    ? 0
                                    : mParkedChangesToFlush - handed;
}

// true: nothing parked or in flight for the eight neighbours; else starts writing theirs if no write runs;
// only for early saves: storage never merges every parked change as it arrives
bool Level::_persistSpillAround(int32_t chunkX, int32_t chunkZ) {
    _finishPendingChangeWrite(false);

    bool settled = true;
    std::vector<int64_t> keys;
    std::vector<GeneratedBlockChange> batch;
    for (int32_t dx = -1; dx <= 1; ++dx) {
        for (int32_t dz = -1; dz <= 1; ++dz) {
            if (dx == 0 && dz == 0)
                continue;

            const int64_t key = _packChunk(chunkX + dx, chunkZ + dz);
            if (mPersistingChangeKeys.count(key) != 0) {
                settled = false;
                continue;
            }

            const auto parked = mPendingBlockChanges.find(key);
            if (parked == mPendingBlockChanges.end())
                continue;

            settled = false;
            if (mPendingChangeWrite.valid() || mPendingChunks.find(key) != mPendingChunks.end())
                continue;

            keys.push_back(key);
            mPendingBlockChangeCount -= parked->second.size();
            batch.insert(batch.end(), std::make_move_iterator(parked->second.begin()),
                         std::make_move_iterator(parked->second.end()));
            mPendingBlockChanges.erase(parked);
        }
    }

    if (keys.empty())
        return settled;

    mPersistingChangeKeys.insert(keys.begin(), keys.end());
    mPersistingChangeCount = batch.size();
    mPendingChangeWrite = mChunkWorker->requestPersistChanges((int32_t) (keys.front() >> 32),
                                                              (int32_t) (keys.front() & 0xffffffff), std::move(batch));
    return false;
}

void Level::_flushPendingBlockChanges(bool includeInFlight) {
    _finishPendingChangeWrite(includeInFlight);
    if (!mStorage.isOpen())
        return;
    if (mPendingChangeWrite.valid())
        return;

    for (auto it = mPendingBlockChanges.begin(); it != mPendingBlockChanges.end();) {
        if (!includeInFlight && mPendingChunks.find(it->first) != mPendingChunks.end()) {
            ++it;
            continue;
        }

        if (!includeInFlight && mChunkWorker != nullptr && mChunkWorker->isRunning()) {
            const int64_t first = it->first;
            std::vector<GeneratedBlockChange> batch;
            while (it != mPendingBlockChanges.end() && mPersistingChangeKeys.size() < MAX_PERSIST_BATCH_CHUNKS) {
                if (mPendingChunks.find(it->first) != mPendingChunks.end()) {
                    ++it;
                    continue;
                }

                mPersistingChangeKeys.insert(it->first);
                batch.insert(batch.end(), std::make_move_iterator(it->second.begin()),
                             std::make_move_iterator(it->second.end()));
                it = mPendingBlockChanges.erase(it);
            }

            mPersistingChangeCount = batch.size();
            mPendingBlockChangeCount -= mPersistingChangeCount;
            mPendingChangeWrite = mChunkWorker->requestPersistChanges((int32_t) (first >> 32),
                                                                      (int32_t) (first & 0xffffffff), std::move(batch));
            return;
        }

        const size_t count = it->second.size();
        if (mStorage.mergePendingBlockChanges((int32_t) (it->first >> 32), (int32_t) (it->first & 0xffffffff),
                                              it->second)) {
            mPendingBlockChangeCount -= count;
            it = mPendingBlockChanges.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<int64_t> Level::consumeRepopulatedChunks() {
    std::vector<int64_t> chunks(mRepopulatedChunks.begin(), mRepopulatedChunks.end());
    mRepopulatedChunks.clear();
    return chunks;
}

bool Level::isChunkResident(int32_t chunkX, int32_t chunkZ) const {
    return mChunks.find(_packChunk(chunkX, chunkZ)) != mChunks.end();
}

void Level::registerChunkLoader(uint64_t loaderId, int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    mChunkLoaders[key].insert(loaderId);
    mUnloadQueue.erase(key);
}

bool Level::unregisterChunkLoader(uint64_t loaderId, int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    auto entry = mChunkLoaders.find(key);
    if (entry == mChunkLoaders.end())
        return false;

    if (entry->second.erase(loaderId) == 0)
        return false;

    if (!entry->second.empty())
        return false;

    mChunkLoaders.erase(entry);
    mUnloadQueue.insert(key);
    return true;
}

void Level::unregisterAllChunkLoaders(uint64_t loaderId) {
    for (auto entry = mChunkLoaders.begin(); entry != mChunkLoaders.end();) {
        if (entry->second.erase(loaderId) == 0 || !entry->second.empty()) {
            ++entry;
            continue;
        }

        mUnloadQueue.insert(entry->first);
        entry = mChunkLoaders.erase(entry);
    }
}

void Level::releaseChunkIfUnused(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    if (mChunkLoaders.find(key) == mChunkLoaders.end() && mChunks.find(key) != mChunks.end())
        mUnloadQueue.insert(key);
}

// resident without a loader (synchronous getChunk(), async load finished after the player moved on):
// nothing queues them for unload; collected after two sweeps loaderless and outside ticking columns
void Level::_sweepOrphanChunks() {
    std::unordered_set<int64_t> orphans;

    for (const auto &entry: mChunks) {
        const int64_t key = entry.first;
        if (mChunkLoaders.find(key) != mChunkLoaders.end() || mPendingChunks.find(key) != mPendingChunks.end()
            || mActiveColumns.find(key) != mActiveColumns.end() || mOccupiedColumns.find(key) != mOccupiedColumns.end())
            continue;

        if (mOrphanCandidates.find(key) != mOrphanCandidates.end())
            mUnloadQueue.insert(key);
        else
            orphans.insert(key);
    }

    mOrphanCandidates.swap(orphans);
}

void Level::setOccupiedColumns(std::unordered_set<int64_t> columns) {
    mOccupiedColumns = std::move(columns);
}

// eight populated neighbours: no more spilled blocks, content final; saved once generation goes quiet,
// fresh terrain hits disk before the auto-save without competing with players waiting for terrain
void Level::_queueCompletedSaves(int32_t chunkX, int32_t chunkZ) {
    if (!mStorage.isOpen() || mChunkWorker == nullptr || !mChunkWorker->isRunning())
        return;

    for (int32_t dx = -1; dx <= 1; ++dx) {
        for (int32_t dz = -1; dz <= 1; ++dz) {
            const int32_t x = chunkX + dx;
            const int32_t z = chunkZ + dz;
            bool complete = isChunkPopulated(x, z);
            for (int32_t nx = -1; nx <= 1 && complete; ++nx) {
                for (int32_t nz = -1; nz <= 1 && complete; ++nz)
                    complete = isChunkPopulated(x + nx, z + nz);
            }

            if (complete)
                mIdleSaves.push_back(_packChunk(x, z));
        }
    }
}

void Level::processDeferredSaves() {
    _flushEntityErasures(false);

    // unapplied spilled blocks could still be parked next to a chunk saved now
    const bool generationIdle = mChunkWorker != nullptr && mChunkWorker->getOutstandingGenerationCount() == 0
                                && mPendingChunks.empty() && mCompletedChunks.empty() && mIncomingChanges.empty();
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + DEFERRED_SAVE_BUDGET;
    while ((!mDeferredSaves.empty() || (generationIdle && !mIdleSaves.empty()))
           && std::chrono::steady_clock::now() < deadline && mChunkWorker != nullptr
           && mChunkWorker->getPendingSaveCount() < MAX_QUEUED_BACKGROUND_SAVES) {
        std::deque<int64_t> &queue = mDeferredSaves.empty() ? mIdleSaves : mDeferredSaves;
        const int64_t key = queue.front();
        queue.pop_front();

        auto chunk = mChunks.find(key);
        if (chunk == mChunks.end() || mChunkWorker == nullptr || !mChunkWorker->isRunning())
            continue;
        _syncPendingTicks(chunk->second);
        if (!chunk->second.isDirty() && !mChunkWorker->hasFailedSave(chunk->second.getX(), chunk->second.getZ()))
            continue;
        // idle save waits for its spill into absent neighbours to reach storage: no crash with half its trees;
        // spill into a resident neighbour is written with it, else the generation frontier is written twice
        if (&queue == &mIdleSaves && !_persistSpillAround(chunk->second.getX(), chunk->second.getZ())) {
            mIdleSaves.push_front(key);
            break;
        }

        std::unique_ptr<LevelChunk> copy(new LevelChunk(chunk->second));
        copy->invalidateNetworkCaches();
        mChunkWorker->requestSave(std::move(copy));
        chunk->second.clearDirty();
        _markChunkSaved(key);
    }
}

void Level::packIdleChunks() {
    if (mPackCursor >= mPackKeys.size()) {
        mPackKeys.clear();
        mPackKeys.reserve(mChunks.size());
        for (const auto &entry: mChunks)
            mPackKeys.push_back(entry.first);
        mPackCursor = 0;
    }

    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + PACK_BUDGET;
    for (size_t visited = 0; visited < MAX_PACK_VISITS_PER_TICK && mPackCursor < mPackKeys.size(); ++visited) {
        const int64_t key = mPackKeys[mPackCursor++];
        auto it = mChunks.find(key);
        if (it == mChunks.end() || it->second.isPacked())
            continue;

        // read since the last pass: another pass before packing
        LevelChunk &chunk = it->second;
        if (chunk.consumeAccessed() || !chunk.isPopulated() || mActiveColumns.count(key) != 0
            || mOccupiedColumns.count(key) != 0 || mPendingChunks.count(key) != 0)
            continue;

        chunk.pack();
        if (std::chrono::steady_clock::now() >= deadline)
            break;
    }
}

size_t Level::processChunkUnloads() {
    if (++mUnloadSweepTicks >= UNLOAD_SWEEP_INTERVAL_TICKS) {
        mUnloadSweepTicks = 0;
        _sweepOrphanChunks();
    }

    if (mUnloadQueue.empty())
        return 0;

    size_t unloaded = 0;
    size_t queuedSaves = 0;

    for (auto it = mUnloadQueue.begin();
         it != mUnloadQueue.end() && unloaded + queuedSaves < MAX_CHUNK_UNLOADS_PER_TICK;) {
        const int64_t key = *it;

        if (mChunkLoaders.find(key) != mChunkLoaders.end() || mOccupiedColumns.find(key) != mOccupiedColumns.end()) {
            it = mUnloadQueue.erase(it);
            continue;
        }

        // a load or populate in flight would insert it again after the free: wait for the drain
        if (mPendingChunks.find(key) != mPendingChunks.end()) {
            ++it;
            continue;
        }

        auto chunk = mChunks.find(key);
        if (chunk == mChunks.end()) {
            it = mUnloadQueue.erase(it);
            continue;
        }

        // no free while a save is queued: a synchronous getChunk() would read the old version back; a failed
        // save keeps it resident
        if (mChunkWorker != nullptr && mChunkWorker->hasPendingSave(chunk->second.getX(), chunk->second.getZ())) {
            ++it;
            continue;
        }

        _syncPendingTicks(chunk->second);
        // saved by a worker from a copy, the tick never waits on storage; resident until it lands
        if (chunk->second.isDirty() && mStorage.isOpen() && mChunkWorker != nullptr && mChunkWorker->isRunning()) {
            std::unique_ptr<LevelChunk> copy(new LevelChunk(chunk->second));
            copy->invalidateNetworkCaches();
            mChunkWorker->requestSave(std::move(copy));
            chunk->second.clearDirty();
            _markChunkSaved(key);
            ++it;
            queuedSaves++;
            continue;
        }

        if (chunk->second.isDirty() && (!mStorage.isOpen() || !mStorage.saveChunk(chunk->second))) {
            it = mUnloadQueue.erase(it);
            continue;
        }
        if (chunk->second.isDirty())
            _markChunkSaved(key);

        const int32_t chunkX = chunk->second.getX();
        const int32_t chunkZ = chunk->second.getZ();
        mChunks.erase(chunk);
        _dropPendingTicks(chunkX, chunkZ);
        mRepopulatedChunks.erase(key);
        mOrphanCandidates.erase(key);
        it = mUnloadQueue.erase(it);
        unloaded++;
        _dispatchChunkEvent(FALCON_EVENT_CHUNK_UNLOAD, chunkX, chunkZ, false);
    }

    mUnloadedSinceTrim += unloaded;
    if (mUnloadQueue.empty() && mUnloadedSinceTrim >= HEAP_TRIM_UNLOAD_THRESHOLD) {
        mUnloadedSinceTrim = 0;
        releaseFreedHeap();
    }

    return unloaded;
}

void Level::releaseFreedHeap() {
    // the allocator keeps freed memory: RSS would stay at its peak after players leave or generation
    // stops copying chunks
#if defined(__linux__) && defined(__GLIBC__)
    malloc_trim(0);
#elif defined(__APPLE__)
    malloc_zone_pressure_relief(nullptr, 0);
#endif
}

bool Level::isChunkPopulated(int32_t chunkX, int32_t chunkZ) const {
    const auto it = mChunks.find(_packChunk(chunkX, chunkZ));
    return it != mChunks.end() && it->second.isPopulated();
}

bool Level::isChunkSaved(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);
    const auto cached = mSavedChunkCache.find(key);
    if (cached != mSavedChunkCache.end())
        return cached->second;

    // the game empties the whole cache rather than evicting single entries
    if (mSavedChunkCache.size() >= ClientSideGeneration::SAVED_CHUNK_CACHE_LIMIT)
        mSavedChunkCache.clear();

    const bool saved = mStorage.hasChunk(chunkX, chunkZ);
    mSavedChunkCache.emplace(key, saved);
    return saved;
}

bool Level::applyMapPixels(int64_t mapId, const std::vector<MapPixel> &pixels) {
    MapItemSavedData *map = getMapData(mapId);
    if (map == nullptr)
        return false;

    map->applyClientPixels(pixels);
    return true;
}

void Level::_saveLoadedMaps() {
    mMaps.saveDirty(mStorage);
    mMaps.unloadUnused();
}

void Level::_markChunkSaved(int64_t key) {
    // the game flags a chunk queued for saving so storage is read again
    mSavedChunkCache[key] = true;
}

bool Level::isNearStructure(const std::vector<const StructureFeature *> &features, int32_t chunkX, int32_t chunkZ) {
    if (features.empty() || mGenerator == nullptr)
        return false;

    return mStructureGuard.isNearStart(*mGenerator, features, chunkX, chunkZ);
}

LevelChunk *Level::peekChunkPtr(int32_t chunkX, int32_t chunkZ) {
    auto it = mChunks.find(_packChunk(chunkX, chunkZ));
    return it == mChunks.end() ? nullptr : &it->second;
}

int Level::getSkyLightAt(int32_t x, int32_t y, int32_t z) {
    LevelChunk *chunk = peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return 0;

    if (!chunk->hasSkyLight())
        LightSystem::computeSkyLight(*chunk);

    return chunk->getSkyLight(x & 15, y, z & 15);
}

int Level::getBlockLightAt(int32_t x, int32_t y, int32_t z) {
    LevelChunk *chunk = peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return 0;

    return chunk->getBlockLight(x & 15, y, z & 15);
}

void Level::addBlockLightUpdate(int32_t x, int32_t y, int32_t z) {
    mBlockLightQueue.insert(LightSystem::packPosition(x, y, z));
}

void Level::updateBlockLight() {
    LightSystem::updateBlockLight(*this, mBlockLightQueue);
}

int32_t Level::getHeightAt(int32_t x, int32_t z) {
    LevelChunk *chunk = peekChunkPtr(x >> 4, z >> 4);
    if (chunk == nullptr)
        return LevelChunk::MIN_Y;

    const int localX = x & 15;
    const int localZ = z & 15;

    if (!chunk->hasHeight(localX, localZ))
        LightSystem::updateHeightAt(*chunk, localX, localZ);

    return chunk->getHeight(localX, localZ);
}

int32_t Level::getAboveTopSolidBlock(int32_t x, int32_t y, int32_t z) {
    LevelChunk *chunk = peekChunkPtr(x >> 4, z >> 4);
    // the block source answers its height limit for a column it has no chunk for
    if (chunk == nullptr)
        return getMaxY() + 1;

    const int localX = x & 15;
    const int localZ = z & 15;
    const auto isLeaves = [](const BlockState &state) {
        const Block *block = VanillaBlocks::fromState(state);
        return block != nullptr && block->isLeaves();
    };

    int32_t current = std::min(y, getMaxY());
    if (getDimensionType() == DimensionType::Nether) {
        while (current > getMinY()) {
            const BlockState &state = chunk->getBlock(localX, current, localZ);
            if (isLeaves(state) || !BlockSupport::isSolid(state))
                break;
            --current;
        }
    }

    for (; current >= getMinY(); --current) {
        const BlockState &state = chunk->getBlock(localX, current, localZ);
        if (isLeaves(state))
            continue;
        if (BlockSupport::isSolid(state))
            return current + 1;
    }
    return getMinY();
}

void Level::updateSkyLightSubtracted() {
    mSkyLightSubtracted = LightSystem::calculateSkyLightSubtracted(*this);
}

bool Level::isColumnActive(int32_t chunkX, int32_t chunkZ) const {
    return mActiveColumns.find(_packChunk(chunkX, chunkZ)) != mActiveColumns.end();
}

void Level::setActiveColumns(std::vector<int64_t> columns) {
    std::unordered_set<int64_t> previous;
    previous.swap(mActiveColumns);
    mActiveColumnOrder.clear();
    for (int64_t column: columns) {
        if (!mActiveColumns.insert(column).second)
            continue;
        mActiveColumnOrder.push_back(column);
        // updates due while inactive were parked: resume them
        if (previous.find(column) == previous.end())
            mBlockUpdates.activateColumn((int32_t) (column >> 32), (int32_t) (column & 0xffffffff));
    }
    // a view source walks its bounds in rows of x when it does not shuffle
    std::sort(mActiveColumnOrder.begin(), mActiveColumnOrder.end(), [](int64_t left, int64_t right) {
        const int32_t leftZ = (int32_t) (left & 0xffffffff);
        const int32_t rightZ = (int32_t) (right & 0xffffffff);
        if (leftZ != rightZ)
            return leftZ < rightZ;
        return (int32_t) (left >> 32) < (int32_t) (right >> 32);
    });
}

void Level::startWorkers(size_t threadCount) {
    if (mChunkWorker == nullptr)
        mChunkWorker.reset(new ChunkWorker(*mGenerator, mStorage));

    mChunkWorker->start(threadCount);
}

bool Level::requestChunkAsync(int32_t chunkX, int32_t chunkZ) {
    const int64_t key = _packChunk(chunkX, chunkZ);

    auto resident = mChunks.find(key);

    if (resident != mChunks.end() && resident->second.isPopulated())
        return true;

    if (mChunkWorker == nullptr || !mChunkWorker->isRunning())
        return false;

    _finishPendingChangeWrite(false);
    if (_generationBacklog() >= MAX_GENERATION_BACKLOG || mCompletedChunks.size() >= MAX_COMPLETED_BACKLOG
        || mChunkWorker->getOutstandingGenerationCount() >= ChunkWorker::MAX_OUTSTANDING_GENERATION
        || (mPendingChangeWrite.valid() && mPersistingChangeKeys.count(key) != 0))
        return false;

    if (!mPendingChunks.insert(key).second)
        return false;

    if (resident != mChunks.end()) {
        if (!mChunkWorker->requestPopulate(std::unique_ptr<LevelChunk>(new LevelChunk(resident->second))))
            mPendingChunks.erase(key);
        return false;
    }

    // spilled blocks travel with the load, applied on the worker: no replay on the main thread
    std::vector<GeneratedBlockChange> parked;
    const auto pending = mPendingBlockChanges.find(key);
    if (pending != mPendingBlockChanges.end())
        parked.swap(pending->second);
    const size_t parkedCount = parked.size();

    if (!mChunkWorker->requestLoad(chunkX, chunkZ, parked)) {
        mPendingChunks.erase(key);
        if (parkedCount != 0)
            pending->second.swap(parked);
        return false;
    }

    if (parkedCount != 0) {
        mPendingBlockChangeCount -= parkedCount;
        mPendingBlockChanges.erase(pending);
    }
    return false;
}

void Level::_storeGeneratedContent(ChunkLoadResult &result) {
    if (mOwner == nullptr)
        return;

    if (!result.mBlockActors.empty() && mBlockActors.isChunkLoaded(result.mX, result.mZ))
        mBlockActors.addLoaded(result.mBlockActors, mOwner->getCodecContext());

    if (result.mEntities.empty() || !mStorage.isOpen())
        return;

    for (Tag &entity: result.mEntities)
        entity.putLong("UniqueID", mOwner->allocateActorUniqueId());
    mStorage.appendEntities(result.mX, result.mZ, result.mEntities);

    const int64_t column = ((int64_t) result.mX << 32) | (uint32_t) result.mZ;
    if (mOwner->getWorldOf(*this).getActorLoadedChunks(getDimensionType()).count(column) != 0)
        mOwner->spawnStoredActors(*this, result.mEntities);
}

void Level::_dispatchChunkEvent(uint32_t type, int32_t chunkX, int32_t chunkZ, bool generated) {
    PluginManager *plugins = PluginManager::findWithSubscribers(type);
    if (plugins == nullptr)
        return;

    PluginEvent event;
    event.mType = type;
    event.mLevel = this;
    event.mChunkX = chunkX;
    event.mChunkZ = chunkZ;
    event.mState = generated;
    plugins->dispatch(event);
}

size_t Level::drainCompletedChunks() {
    if (mChunkWorker == nullptr)
        return 0;

    // drain every tick: credits return at once, generation runs while results wait for an insert slot
    std::vector<ChunkLoadResult> results = mChunkWorker->drainCompleted();
    for (ChunkLoadResult &result: results)
        mCompletedChunks.push_back(std::move(result));

    size_t added = 0;

    // capped per tick; a populated result only replaces a still-unpopulated resident chunk: never
    // overwrites edits made since the copy
    while (!mCompletedChunks.empty() && added < MAX_CHUNK_INSERTS_PER_TICK) {
        ChunkLoadResult &result = mCompletedChunks.front();
        const int64_t key = _packChunk(result.mX, result.mZ);
        mPendingChunks.erase(key);

        auto resident = mChunks.find(key);
        const bool canInsert = resident == mChunks.end();
        const bool canReplace = !canInsert && result.mReplacesResident && !resident->second.isPopulated();

        if (result.mChunk != nullptr && (canInsert || canReplace)) {
            if (canInsert) {
                LevelChunk &inserted = mChunks.emplace(key, std::move(*result.mChunk)).first->second;
                _loadPendingTicks(inserted);
                _dispatchChunkEvent(FALCON_EVENT_CHUNK_LOAD, result.mX, result.mZ, result.mGenerated);
            } else {
                resident->second = std::move(*result.mChunk);
                _loadPendingTicks(resident->second);
                mRepopulatedChunks.insert(key);
            }

            added++;

            _queueGeneratedChanges(std::move(result.mOverflowChanges));
            _refreshChunkShapes(result.mX, result.mZ);
            _queueCompletedSaves(result.mX, result.mZ);
            _storeGeneratedContent(result);
        } else {
            // overtaken by a synchronous getChunk(), or discarded at shutdown: keep the spilled blocks it carried
            _restoreSpilledChanges(key, std::move(result.mSpilledChanges));
        }

        _replayPendingChanges(key);
        mCompletedChunks.pop_front();
    }

    return added;
}

std::string Level::getChunkData(int32_t chunkX, int32_t chunkZ, const BlockNetworkIdMap *blockIds) {
    // encoding is cheap next to caching sections for every loaded chunk; most have a single viewer: nothing
    // kept once sent
    LevelChunk &chunk = getChunk(chunkX, chunkZ);
    std::string data = chunk.encodeNetwork(blockIds);
    chunk.invalidateNetworkCaches();
    return data;
}

int Level::getChunkSubChunkCount(int32_t chunkX, int32_t chunkZ) {
    return getChunk(chunkX, chunkZ).getNetworkSubChunkCount();
}

int32_t Level::getBlock(int32_t x, int32_t y, int32_t z) {
    return getChunk(x >> 4, z >> 4).getBlock(x & 15, y, z & 15).getHash();
}

BlockState Level::getBlockState(int32_t x, int32_t y, int32_t z) {
    return getChunk(x >> 4, z >> 4).getBlock(x & 15, y, z & 15);
}

bool Level::peekBlockState(int32_t x, int32_t y, int32_t z, BlockState &out) {
    const BlockState *state = peekBlockPtr(x, y, z);
    if (state == nullptr)
        return false;

    out = *state;
    return true;
}

const BlockState *Level::peekBlockPtr(int32_t x, int32_t y, int32_t z, int layer) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return nullptr;

    auto it = mChunks.find(_packChunk(x >> 4, z >> 4));
    if (it == mChunks.end())
        return nullptr;

    return &it->second.getBlock(x & 15, y, z & 15, layer);
}

bool Level::isSolidAt(int32_t x, int32_t y, int32_t z) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return false;

    const BlockState &state = getChunk(x >> 4, z >> 4).getBlock(x & 15, y, z & 15);
    if (state.mName == BlockNames::AIR)
        return false;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return true;

    return data->mSolid;
}

std::vector<AxisAlignedBB> Level::getCollisionBoxes(const AxisAlignedBB &area, const Actor *actor) {
    std::vector<AxisAlignedBB> boxes;
    const BlockShape::Neighbours neighbours = [this](int32_t x, int32_t y, int32_t z) { return peekBlockPtr(x, y, z); };
    const int32_t minX = (int32_t) std::floor(area.mMinX);
    const int32_t minY = std::max((int32_t) std::floor(area.mMinY) - 1, LevelChunk::MIN_Y);
    const int32_t minZ = (int32_t) std::floor(area.mMinZ);
    const int32_t maxX = (int32_t) std::floor(area.mMaxX);
    const int32_t maxY = std::min((int32_t) std::floor(area.mMaxY), LevelChunk::MAX_Y);
    const int32_t maxZ = (int32_t) std::floor(area.mMaxZ);

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t z = minZ; z <= maxZ; ++z) {
            for (int32_t y = minY; y <= maxY; ++y) {
                const BlockState *state = peekBlockPtr(x, y, z);
                if (state == nullptr)
                    continue;

                const size_t first = boxes.size();
                BlockShape::appendShapesAt(*state, x, y, z, actor, neighbours, boxes);
                // only the boxes the area touches
                boxes.erase(std::remove_if(boxes.begin() + (std::ptrdiff_t) first, boxes.end(),
                                           [&area](const AxisAlignedBB &box) { return !box.intersectsWith(area); }),
                            boxes.end());
            }
        }
    }

    appendCollidableActorBoxes(area, actor, boxes);
    return boxes;
}

void Level::appendCollidableActorBoxes(const AxisAlignedBB &area, const Actor *actor,
                                       std::vector<AxisAlignedBB> &out) {
    // a mob with minecraft:is_collidable carries actor flag 121: others stand on it like a block
    if (mOwner != nullptr) {
        const Vector3f center((area.mMinX + area.mMaxX) * 0.5f, (area.mMinY + area.mMaxY) * 0.5f,
                              (area.mMinZ + area.mMaxZ) * 0.5f);
        const float radius = std::max(area.mMaxX - area.mMinX, area.mMaxZ - area.mMinZ) * 0.5f;
        mOwner->forEachActorNear(center, radius, [&](ServerActor &other) {
            if (&other == actor || !other.isIn(*this) || !other.getFlags().get(ActorFlag::Collidable))
                return;
            if (actor != nullptr && actor->getVehicleId() == other.getUniqueId())
                return;
            const AxisAlignedBB shape = ActorPushSystem::boundingBoxOf(other);
            if (shape.intersectsWith(area))
                out.push_back(shape);
        });
    }
}

void Level::setBlockState(int32_t x, int32_t y, int32_t z, const BlockState &state) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    LevelChunk &chunk = getChunk(x >> 4, z >> 4);
    if (chunk.getBlock(x & 15, y, z & 15) == state)
        return;

    // interned: still the old block once the chunk holds the new one
    const BlockState &previous = chunk.getBlock(x & 15, y, z & 15);
    chunk.setBlock(x & 15, y, z & 15, state);
    if (previous.mName != state.mName) {
        mBlockActors.onBlockReplaced(Vector3i(x, y, z));
        removeForeignBlockActor(Vector3i(x, y, z), state);
    }
    if (mOwner != nullptr && CustomContentRegistry::getInstance().hasNeighborListeners())
        _recordNeighborChange(x, y, z, previous, state);
    if (mBatchLight) {
        mBatchedLightColumns.insert(((int64_t) x << 32) | (uint32_t) z);
        addBlockLightUpdate(x, y, z);
    } else {
        LightSystem::onBlockChanged(*this, x, y, z);
    }

    mLiquidPhysics.onBlockChanged(Vector3i(x, y, z), 0, previous);
}

void Level::removeForeignBlockActor(const Vector3i &position, const BlockState &state) {
    BlockActor *actor = mBlockActors.find(position);
    if (actor == nullptr)
        return;
    const Block *block = VanillaBlocks::fromState(state);
    const char *carried = block == nullptr ? nullptr : block->getBlockActorId();
    if (carried != nullptr && std::strcmp(carried, actor->getBlockActorId()) == 0)
        return;
    // pack blocks share the data-driven actor
    if (dynamic_cast<DataDrivenBlockActor *>(actor) != nullptr
        && CustomContentRegistry::getInstance().isCustomBlock(state.mName))
        return;
    mBlockActors.remove(position);
}

BlockState Level::getBlockStateAtLayer(int32_t x, int32_t y, int32_t z, int layer) {
    if (layer <= 0)
        return getBlockState(x, y, z);

    return getChunk(x >> 4, z >> 4).getBlock(x & 15, y, z & 15, layer);
}

void Level::setBlockStateAtLayer(int32_t x, int32_t y, int32_t z, int layer, const BlockState &state) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    if (layer <= 0) {
        setBlockState(x, y, z, state);
        return;
    }

    LevelChunk &chunk = getChunk(x >> 4, z >> 4);
    if (chunk.getBlock(x & 15, y, z & 15, layer) == state)
        return;

    const BlockState &previous = chunk.getBlock(x & 15, y, z & 15, layer);
    chunk.setBlock(x & 15, y, z & 15, layer, state);
    mLiquidPhysics.onBlockChanged(Vector3i(x, y, z), layer, previous);
}

void Level::setBlock(int32_t x, int32_t y, int32_t z, int32_t blockHash) {
    if (blockHash == mGenerator->getAirHash()) {
        setBlockState(x, y, z, VanillaBlocks::AIR().toBlockState());
        return;
    }

    setBlockState(x, y, z, VanillaBlocks::STONE().toBlockState());
}

LiquidInfo Level::getLiquidInfo(int32_t x, int32_t y, int32_t z) {
    return mLiquidPhysics.getLiquidInfo(x, y, z);
}

Vector3f Level::getLiquidFlowVector(const Vector3i &position) {
    return mLiquidPhysics.getFlowVector(position);
}

void Level::scheduleFluidTick(const Vector3i &position, int64_t delay) {
    mLiquidPhysics.schedule(position, delay);
}

void Level::tick() {
    updateBlockLight();

    mBatchLight = true;
    mLiquidPhysics.tick(++mLiquidTick, mActiveColumnOrder);
    mBatchLight = false;
    _flushLightBatch();
}

void Level::_flushLightBatch() {
    for (const int64_t column: mBatchedLightColumns) {
        const int32_t x = (int32_t) (column >> 32);
        const int32_t z = (int32_t) (column & 0xffffffff);
        if (LevelChunk *chunk = peekChunkPtr(x >> 4, z >> 4))
            LightSystem::relightColumn(*chunk, x & 15, z & 15);
    }
    mBatchedLightColumns.clear();
}

void Level::scheduleBlockUpdate(const Vector3i &position, int64_t delay) {
    scheduleUpdate(position, delay);
}

bool Level::canRainAt(int32_t x, int32_t z) {
    const int32_t biomeId = (int32_t) getChunk(x >> 4, z >> 4).getColumnBiome(x & 15, z & 15);
    const ClimateAttributes *climate = ClimateAttributes::getForBiome(biomeId);
    if (climate == nullptr)
        return true;

    return climate->mRain && climate->mDownfall > 0.0f;
}

void Level::initializeWeather() {
    static std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<int32_t> clearDuration(0, 167999);

    mStorage.loadWeather(mRaining, mRainTime, mThundering, mThunderTime);

    if (mRainTime <= 0)
        mRainTime = clearDuration(generator) + 12000;
    if (mThunderTime <= 0)
        mThunderTime = clearDuration(generator) + 12000;
    mRainLevel = mRaining ? 1.0f : 0.0f;
    mLightningLevel = mThundering ? 1.0f : 0.0f;
}

void Level::tickWeatherLevels() {
    const auto approach = [](float level, float target) {
        const float next = target >= level ? std::min(level + 0.01f, target) : std::max(level - 0.01f, target);
        return std::clamp(next, 0.0f, 1.0f);
    };
    mLightningLevel = approach(mLightningLevel, mThundering ? 1.0f : 0.0f);
    mRainLevel = approach(mRainLevel, mRaining ? 1.0f : 0.0f);
}

void Level::saveWeather() {
    mStorage.saveWeather(mRaining, mRainTime, mThundering, mThunderTime);
}

void Level::initializeGameRules() {
    Tag stored = Tag::ofCompound();
    if (mStorage.loadGameRules(stored))
        mGameRules.load(stored);
}

void Level::saveGameRules() {
    mStorage.saveGameRules(mGameRules.save());
}

std::vector<Level::FluidChange> Level::consumeFluidChanges() {
    return mLiquidPhysics.consumeChanges();
}

std::vector<Level::ChunkPosition> Level::getChunksAround(int32_t centerChunkX, int32_t centerChunkZ) const {
    std::vector<ChunkPosition> chunks;

    const int64_t radiusSquared = (int64_t) mViewDistance * mViewDistance;

    for (int32_t x = -mViewDistance; x <= mViewDistance; x++) {
        for (int32_t z = -mViewDistance; z <= mViewDistance; z++) {
            if ((int64_t) x * x + (int64_t) z * z > radiusSquared)
                continue;

            chunks.push_back(ChunkPosition{centerChunkX + x, centerChunkZ + z});
        }
    }

    std::sort(chunks.begin(), chunks.end(),
              [centerChunkX, centerChunkZ](const ChunkPosition &left, const ChunkPosition &right) {
                  const int64_t leftX = left.mX - centerChunkX;
                  const int64_t leftZ = left.mZ - centerChunkZ;
                  const int64_t rightX = right.mX - centerChunkX;
                  const int64_t rightZ = right.mZ - centerChunkZ;

                  return leftX * leftX + leftZ * leftZ < rightX * rightX + rightZ * rightZ;
              });

    return chunks;
}
