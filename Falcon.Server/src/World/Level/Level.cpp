#include "World/Level/Level.h"

#include "World/Level/Biome/BiomeRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/Particle/Particle.h"
#include "World/Level/LightSystem.h"

#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "Core/Debug/BedrockLog.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <random>
#include <stdexcept>
#include <utility>

#if defined(__linux__) && defined(__GLIBC__)
#include <malloc.h>
#endif

Level::Level(const std::string &name, int viewDistance, int64_t seed, DimensionType dimension)
        : mName(name), mViewDistance(viewDistance), mSeed(seed), mDimension(dimension),
          mGenerator(DimensionFactory::createGenerator(dimension, seed)), mLiquidPhysics(*this) {}

void Level::decorate(LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) {
    mGenerator->decorate(*this, chunk, overflow);
}

OverworldBiomeResult Level::pickBiomeResult(int32_t x, int32_t y, int32_t z) const {
    const OverworldGenerator *overworld = dynamic_cast<const OverworldGenerator *>(mGenerator.get());

    if (overworld == nullptr) {
        return OverworldBiomeResult(mGenerator->pickBiome(x, y, z), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    return overworld->pickBiomeResult(x, y, z);
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
    mStorage = std::move(other.mStorage);
    mChunks = std::move(other.mChunks);
    mPendingChunks = std::move(other.mPendingChunks);
    mActiveColumns = std::move(other.mActiveColumns);
    mCompletedChunks = std::move(other.mCompletedChunks);
    mRepopulatedChunks = std::move(other.mRepopulatedChunks);
    mIncomingChanges = std::move(other.mIncomingChanges);
    mPendingBlockChanges = std::move(other.mPendingBlockChanges);
    mPendingBlockChangeCount = other.mPendingBlockChangeCount;
    other.mPendingBlockChangeCount = 0;
    mBlockUpdateScheduler.moveStateFrom(std::move(other.mBlockUpdateScheduler));
    mBlockUpdates.moveStateFrom(std::move(other.mBlockUpdates));
    mLiquidPhysics.moveStateFrom(std::move(other.mLiquidPhysics));
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

    if (mDimension == DimensionType::Overworld) {
        Tag levelDat;
        if (mStorage.readLevelDat(levelDat)) {
            if (levelDat.contains("Time"))
                setTime(levelDat.getLong("Time"));

            if (levelDat.contains("RandomSeed") && levelDat.getLong("RandomSeed") != mSeed) {
                mSeed = levelDat.getLong("RandomSeed");
                mGenerator = DimensionFactory::createGenerator(mDimension, mSeed);
            }

            mWorldStartCount = (uint32_t) levelDat.getLong("worldStartCount", DEFAULT_WORLD_START_COUNT);
            mBonusChestEnabled = levelDat.getByte("bonusChestEnabled", 0) != 0;
            mBonusChestSpawned = levelDat.getByte("bonusChestSpawned", 0) != 0;

            if (levelDat.contains("SpawnY") && levelDat.getInt("SpawnY") != UNSET_SPAWN_Y)
                setSpawnPosition(Vector3i(levelDat.getInt("SpawnX"), levelDat.getInt("SpawnY"),
                                          levelDat.getInt("SpawnZ")));
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
    const Vector3i spawn = mHasSpawnPosition ? mSpawnPosition
                           : mHasDefaultSpawnPosition ? mDefaultSpawnPosition : Vector3i(0, UNSET_SPAWN_Y, 0);
    mStorage.writeLevelDat(mName, spawn.x, spawn.y, spawn.z, 0, 1, mSeed, mTime, mBonusChestEnabled,
                           mBonusChestSpawned, (int64_t) (uint32_t) (mWorldStartCount - 1));
}

bool Level::attachStorage(Level &overworld) {
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
    }

    if (wait || !async) {
        while (!mIncomingChanges.empty())
            processGeneratedChanges();
    }

    if (wait || !async) {
        _finishPendingChangeWrite(true);
        _flushPendingBlockChanges(true);
    } else {
        _flushPendingBlockChanges(false);
    }

    size_t saved = 0;
    for (auto &entry: mChunks) {
        if (!entry.second.isDirty() && (mChunkWorker == nullptr ||
                !mChunkWorker->hasFailedSave(entry.second.getX(), entry.second.getZ())))
            continue;

        if (async) {
            // The worker gets its own copy: resident chunks belong to the main thread and
            // keep changing while the save runs.
            std::unique_ptr<LevelChunk> copy(new LevelChunk(entry.second));
            copy->invalidateNetworkCaches();
            mChunkWorker->requestSave(std::move(copy));
            entry.second.clearDirty();
            saved++;
            continue;
        }

        if (mStorage.saveChunk(entry.second)) {
            entry.second.clearDirty();
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

std::vector<Tag> Level::loadEntities(int32_t chunkX, int32_t chunkZ) {
    if (!mStorage.isOpen())
        return std::vector<Tag>();

    return mStorage.loadEntities(chunkX, chunkZ);
}

void Level::eraseEntity(int64_t uniqueId) {
    if (!mStorage.isOpen())
        return;

    mStorage.eraseEntity(uniqueId);
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
    return (int32_t) (((mTime / 24000) % 8 + 8) % 8);
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
        return name == "minecraft:water" || name == "minecraft:flowing_water" || name == "minecraft:lava"
               || name == "minecraft:flowing_lava";
    };

    const std::string below = getBlockState(x, y - 1, z).mName;
    const bool supported = isSolidAt(x, y - 1, z) || below == "minecraft:water" || below == "minecraft:flowing_water";

    return supported && !isSolidAt(x, y, z) && !isLiquid(getBlockState(x, y, z).mName)
           && !isSolidAt(x, y + 1, z) && !isLiquid(getBlockState(x, y + 1, z).mName);
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
        if (getBlockState(x, y, z).mName != "minecraft:air")
            return y;
    }

    return LevelChunk::MIN_Y;
}

Vector3i Level::_findLandSpawn() {
    static const int32_t WATER_BIOMES[] = {
            BiomeIds::OCEAN, BiomeIds::DEEP_OCEAN, BiomeIds::WARM_OCEAN, BiomeIds::LUKEWARM_OCEAN,
            BiomeIds::DEEP_LUKEWARM_OCEAN, BiomeIds::COLD_OCEAN, BiomeIds::DEEP_COLD_OCEAN, BiomeIds::FROZEN_OCEAN,
            BiomeIds::DEEP_FROZEN_OCEAN, BiomeIds::LEGACY_FROZEN_OCEAN, BiomeIds::RIVER, BiomeIds::FROZEN_RIVER
    };
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

    // A synchronous read must not overtake an older pending-change write. Normal
    // streaming retries instead of waiting on the tick thread.
    if (mPendingChangeWrite.valid() && mPersistingChangeKey == key)
        _finishPendingChangeWrite(true);

    LevelChunk chunk(chunkX, chunkZ);
    chunk.setDimension(mDimension);

    if (!mStorage.isOpen() || !mStorage.loadChunk(chunk))
        _generate(chunk);

    if (mStorage.isOpen() && mPendingChunks.find(key) == mPendingChunks.end()) {
        const auto stored = mStorage.loadPendingBlockChanges(chunkX, chunkZ);
        for (const auto &change: stored) {
            if (change.mY >= LevelChunk::MIN_Y && change.mY <= LevelChunk::MAX_Y)
                chunk.setBlock(change.mX & 15, change.mY, change.mZ & 15, change.mState);
        }
        if (!stored.empty()) {
            LightSystem::computeSkyLight(chunk);
            LightSystem::computeBlockLight(chunk);
            mStorage.erasePendingBlockChanges(chunkX, chunkZ);
        }
    }

    mPendingChunks.erase(key);

    auto result = mChunks.emplace(key, std::move(chunk));
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
    if (result.second)
        _refreshChunkShapes(result.first->second.getX(), result.first->second.getZ());
    return result.first->second;
}

void Level::_refreshChunkShapes(int32_t chunkX, int32_t chunkZ) {
    // Only inspect sections whose palette contains a derived shape. Revisit neighbours so a
    // connection held at an unloaded chunk border is resolved when that chunk arrives.
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
    return chunk;
}

void Level::dropChunk(int32_t chunkX, int32_t chunkZ) {
    mChunks.erase(_packChunk(chunkX, chunkZ));
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

void Level::_applyGeneratedChanges(const std::vector<GeneratedBlockChange> &changes) {
    for (const GeneratedBlockChange &change: changes) {
        if (change.mY < LevelChunk::MIN_Y || change.mY > LevelChunk::MAX_Y)
            continue;

        const int32_t chunkX = change.mX >> 4;
        const int32_t chunkZ = change.mZ >> 4;
        const int64_t key = _packChunk(chunkX, chunkZ);

        LevelChunk *chunk = peekChunkPtr(chunkX, chunkZ);
        // Feature blocks that spill into a chunk which is absent or still being loaded are
        // kept aside, replayed once it arrives, or merged into storage on save.
        if (chunk == nullptr || mPendingChunks.find(key) != mPendingChunks.end()) {
            mPendingBlockChanges[key].push_back(change);
            ++mPendingBlockChangeCount;
            continue;
        }

        const int32_t localX = change.mX & 15;
        const int32_t localZ = change.mZ & 15;

        if (chunk->getBlock(localX, change.mY, localZ) == change.mState)
            continue;

        chunk->setBlock(localX, change.mY, localZ, change.mState);
        LightSystem::onBlockChanged(*this, change.mX, change.mY, change.mZ);
        mRepopulatedChunks.insert(key);
    }
}

size_t Level::processGeneratedChanges() {
    _flushPendingBlockChanges(false);
    if (mIncomingChanges.empty())
        return 0;

    // Batches are applied until the time budget is spent, always at least one, so a large backlog
    // uses the tick's spare time without stretching a busy tick by more than one batch.
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
    if (failed.empty())
        return;

    // Failed writes retain their payload. Older changes must precede changes received
    // while the write was running, so a retry still keeps the newest state per position.
    mPendingBlockChangeCount += failed.size();
    auto &newer = mPendingBlockChanges[mPersistingChangeKey];
    failed.insert(failed.end(), std::make_move_iterator(newer.begin()), std::make_move_iterator(newer.end()));
    newer = std::move(failed);
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
            mPersistingChangeKey = it->first;
            mPersistingChangeCount = it->second.size();
            mPendingBlockChangeCount -= mPersistingChangeCount;
            mPendingChangeWrite = mChunkWorker->requestPersistChanges(
                    (int32_t) (it->first >> 32), (int32_t) (it->first & 0xffffffff), std::move(it->second));
            mPendingBlockChanges.erase(it);
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

/**
 * Chunks can become resident without ever getting a loader: a synchronous getChunk() from a
 * game system, or an async load that completes after the player who asked for it moved on.
 * Nothing would queue those for unload, so they are collected here once they have stayed
 * without a loader, outside the ticking columns, for two sweeps in a row.
 */
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

size_t Level::processChunkUnloads() {
    if (++mUnloadSweepTicks >= UNLOAD_SWEEP_INTERVAL_TICKS) {
        mUnloadSweepTicks = 0;
        _sweepOrphanChunks();
    }

    if (mUnloadQueue.empty())
        return 0;

    size_t unloaded = 0;

    for (auto it = mUnloadQueue.begin(); it != mUnloadQueue.end() && unloaded < MAX_CHUNK_UNLOADS_PER_TICK;) {
        const int64_t key = *it;

        if (mChunkLoaders.find(key) != mChunkLoaders.end() || mOccupiedColumns.find(key) != mOccupiedColumns.end()) {
            it = mUnloadQueue.erase(it);
            continue;
        }

        // A load or populate still in flight would insert the chunk again after it is freed;
        // it stays queued until the result has been drained.
        if (mPendingChunks.find(key) != mPendingChunks.end()) {
            ++it;
            continue;
        }

        auto chunk = mChunks.find(key);
        if (chunk == mChunks.end()) {
            it = mUnloadQueue.erase(it);
            continue;
        }

        // The save is synchronous: an async save still queued on a worker could be overtaken by a
        // synchronous getChunk() reading the old version back from storage. For the same reason a
        // chunk waits while an earlier save of it is queued, which would otherwise land last. A
        // chunk that cannot be saved stays resident instead of losing its changes.
        if (mChunkWorker != nullptr && mChunkWorker->hasPendingSave(chunk->second.getX(), chunk->second.getZ())) {
            ++it;
            continue;
        }

        if (chunk->second.isDirty() && (!mStorage.isOpen() || !mStorage.saveChunk(chunk->second))) {
            it = mUnloadQueue.erase(it);
            continue;
        }

        const int32_t chunkX = chunk->second.getX();
        const int32_t chunkZ = chunk->second.getZ();
        mChunks.erase(chunk);
        mRepopulatedChunks.erase(key);
        mOrphanCandidates.erase(key);
        it = mUnloadQueue.erase(it);
        unloaded++;
        _dispatchChunkEvent(FALCON_EVENT_CHUNK_UNLOAD, chunkX, chunkZ, false);
    }

    mUnloadedSinceTrim += unloaded;
    if (mUnloadQueue.empty() && mUnloadedSinceTrim >= HEAP_TRIM_UNLOAD_THRESHOLD) {
        mUnloadedSinceTrim = 0;
#if defined(__linux__) && defined(__GLIBC__)
        // glibc keeps freed chunk memory in its arenas instead of returning it to the system, so
        // the resident size would stay at its peak after players leave an area.
        malloc_trim(0);
#endif
    }

    return unloaded;
}

bool Level::isChunkPopulated(int32_t chunkX, int32_t chunkZ) const {
    const auto it = mChunks.find(_packChunk(chunkX, chunkZ));
    return it != mChunks.end() && it->second.isPopulated();
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

void Level::updateSkyLightSubtracted() {
    mSkyLightSubtracted = LightSystem::calculateSkyLightSubtracted(*this);
}

bool Level::isColumnActive(int32_t chunkX, int32_t chunkZ) const {
    return mActiveColumns.find(_packChunk(chunkX, chunkZ)) != mActiveColumns.end();
}

void Level::setActiveColumns(std::vector<int64_t> columns) {
    std::unordered_set<int64_t> next(columns.begin(), columns.end());

    // Block updates that came due in a column while it was inactive were parked; they
    // resume as soon as the column becomes active again.
    for (int64_t column: columns) {
        if (mActiveColumns.find(column) == mActiveColumns.end())
            mBlockUpdateScheduler.activateColumn((int32_t) (column >> 32), (int32_t) (column & 0xffffffff));
    }

    mActiveColumns.swap(next);
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
        || (mPendingChangeWrite.valid() && mPersistingChangeKey == key))
        return false;

    if (!mPendingChunks.insert(key).second)
        return false;

    if (resident != mChunks.end()) {
        if (!mChunkWorker->requestPopulate(std::unique_ptr<LevelChunk>(new LevelChunk(resident->second))))
            mPendingChunks.erase(key);
        return false;
    }

    if (!mChunkWorker->requestLoad(chunkX, chunkZ))
        mPendingChunks.erase(key);
    return false;
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

    // Draining every tick returns worker credits as soon as results are taken, so generation
    // keeps running while earlier results wait for their insert slot.
    std::vector<ChunkLoadResult> results = mChunkWorker->drainCompleted();
    for (ChunkLoadResult &result: results)
        mCompletedChunks.push_back(std::move(result));

    size_t added = 0;

    // Inserts are capped per tick to bound main-thread work; leftovers stay queued for the
    // next tick. A populated result only replaces a resident chunk that is still
    // unpopulated, so it never overwrites edits made since the copy was taken.
    while (!mCompletedChunks.empty() && added < MAX_CHUNK_INSERTS_PER_TICK) {
        ChunkLoadResult &result = mCompletedChunks.front();
        const int64_t key = _packChunk(result.mX, result.mZ);
        mPendingChunks.erase(key);

        auto resident = mChunks.find(key);
        const bool canInsert = resident == mChunks.end();
        const bool canReplace = !canInsert && result.mReplacesResident && !resident->second.isPopulated();

        if (result.mChunk != nullptr && (canInsert || canReplace)) {
            if (canInsert) {
                mChunks.emplace(key, std::move(*result.mChunk));
                _dispatchChunkEvent(FALCON_EVENT_CHUNK_LOAD, result.mX, result.mZ, result.mGenerated);
            } else {
                resident->second = std::move(*result.mChunk);
                mRepopulatedChunks.insert(key);
            }

            added++;

            _queueGeneratedChanges(std::move(result.mOverflowChanges));
            _refreshChunkShapes(result.mX, result.mZ);
        }

        _replayPendingChanges(key);
        mCompletedChunks.pop_front();
    }

    return added;
}

std::string Level::getChunkData(int32_t chunkX, int32_t chunkZ, const BlockNetworkIdMap *blockIds) {
    return getChunk(chunkX, chunkZ).encodeNetwork(blockIds);
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
    if (state.mName == "minecraft:air")
        return false;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return true;

    return data->mSolid;
}

std::vector<AxisAlignedBB> Level::getCollisionBoxes(const AxisAlignedBB &area) {
    std::vector<AxisAlignedBB> boxes;
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
                if (state == nullptr || !BlockShape::hasCollision(*state))
                    continue;

                const AxisAlignedBB shape = BlockShape::getShapeAt(*state, x, y, z);
                if (shape.intersectsWith(area))
                    boxes.push_back(shape);
            }
        }
    }

    return boxes;
}

void Level::setBlockState(int32_t x, int32_t y, int32_t z, const BlockState &state) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    LevelChunk &chunk = getChunk(x >> 4, z >> 4);
    if (chunk.getBlock(x & 15, y, z & 15) == state)
        return;

    chunk.setBlock(x & 15, y, z & 15, state);
    LightSystem::onBlockChanged(*this, x, y, z);

    if (chunk.getBlock(x & 15, y, z & 15, 1).mName != "minecraft:air")
        mLiquidPhysics.normalizeWaterlogged(Vector3i(x, y, z));

    mLiquidPhysics.onBlockChanged(x, y, z);
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

    chunk.setBlock(x & 15, y, z & 15, layer, state);
    mLiquidPhysics.normalizeWaterlogged(Vector3i(x, y, z));
    mLiquidPhysics.onBlockChanged(x, y, z);
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

    mBlockUpdateScheduler.tick(
            [this](const Vector3i &position) {
                mLiquidPhysics.onScheduledUpdate(position);
            },
            [this](int32_t chunkX, int32_t chunkZ) {
                return isColumnActive(chunkX, chunkZ);
            });
}

void Level::scheduleBlockUpdate(const Vector3i &position, int64_t delay) {
    mBlockUpdateScheduler.schedule(position, delay);
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

    std::sort(chunks.begin(), chunks.end(), [centerChunkX, centerChunkZ](const ChunkPosition &left,
                                                                        const ChunkPosition &right) {
        const int64_t leftX = left.mX - centerChunkX;
        const int64_t leftZ = left.mZ - centerChunkZ;
        const int64_t rightX = right.mX - centerChunkX;
        const int64_t rightZ = right.mZ - centerChunkZ;

        return leftX * leftX + leftZ * leftZ < rightX * rightX + rightZ * rightZ;
    });

    return chunks;
}
