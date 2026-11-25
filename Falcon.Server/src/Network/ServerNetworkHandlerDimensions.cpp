#include <chrono>
#include "Network/ServerNetworkHandler.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/MapHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>
#include <vector>

Level &ServerNetworkHandler::getDimension(DimensionType dimension) {
    return mWorlds.getDefault().getLevel(dimension);
}

World &ServerNetworkHandler::getWorldFor(const Actor &actor) {
    World *world = mWorlds.find(actor.getWorldId());
    return world == nullptr ? mWorlds.getDefault() : *world;
}

World &ServerNetworkHandler::getWorldOf(const Level &level) {
    World *world = mWorlds.find(level.getWorldId());
    return world == nullptr ? mWorlds.getDefault() : *world;
}

Level &ServerNetworkHandler::getLevelFor(const Actor &actor) {
    return getWorldFor(actor).getLevel(actor.getDimension());
}

std::vector<Level *> ServerNetworkHandler::getLevels() {
    std::vector<Level *> levels;
    for (World *world: mWorlds.getWorlds()) {
        for (Level *level: world->getLevels())
            levels.push_back(level);
    }
    return levels;
}

// unlike other dimensions: every column within `tick-distance` of a player;
// the set is rebuilt only when player chunk centres change
void ServerNetworkHandler::_tickOverworld(World &world) {
    Level &level = world.getOverworld();

    mProfiler.beginSection(ProfilerSection::ChunkDrain);
    level.drainCompletedChunks();
    mProfiler.endSection(ProfilerSection::ChunkDrain);

    mProfiler.beginSection(ProfilerSection::ChunkPopulation);
    level.processGeneratedChanges();

    const std::vector<int64_t> repopulated = level.consumeRepopulatedChunks();
    if (!repopulated.empty()) {
        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || !player.isIn(level))
                continue;

            for (const int64_t hash: repopulated)
                ChunkStreamHandler::invalidateChunk(player, hash);
        }
    }
    mProfiler.endSection(ProfilerSection::ChunkPopulation);

    const int tickDistance = mProperties.getTickDistance();
    std::vector<int64_t> centers;

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent || !player.isIn(level))
            continue;

        const int32_t centerX = (int32_t) std::floor(player.getPosition().x) >> 4;
        const int32_t centerZ = (int32_t) std::floor(player.getPosition().z) >> 4;
        centers.push_back(((int64_t) centerX << 32) | (uint32_t) centerZ);
    }

    std::sort(centers.begin(), centers.end());
    centers.erase(std::unique(centers.begin(), centers.end()), centers.end());

    const bool viewMoved = world.mActiveColumnsStale || tickDistance != world.mActiveTickDistance
                           || centers != world.mActiveCenters;
    if (viewMoved) {
        world.mActiveColumnsStale = false;
        world.mActiveCenters = centers;
        world.mActiveTickDistance = tickDistance;

        const size_t span = (size_t) (2 * tickDistance + 1);
        std::vector<int64_t> activeColumns;
        activeColumns.reserve(centers.size() * span * span);

        for (const int64_t center: centers) {
            const int32_t centerX = (int32_t) (center >> 32);
            const int32_t centerZ = (int32_t) (center & 0xffffffff);

            for (int32_t dx = -tickDistance; dx <= tickDistance; ++dx) {
                for (int32_t dz = -tickDistance; dz <= tickDistance; ++dz)
                    activeColumns.push_back(((int64_t) (centerX + dx) << 32) | (uint32_t) (centerZ + dz));
            }
        }

        world.getTickingAreas().appendColumns(DimensionType::Overworld, activeColumns);
        level.setActiveColumns(activeColumns);
    }

    // chunks still on their way keep the actors pending: the columns themselves have not moved
    if (viewMoved || world.mActorPersistencePending) {
        mProfiler.beginSection(ProfilerSection::ActorPersistence);
        world.mActorPersistencePending = syncActorPersistence(level, level.getActiveColumnOrder());
        mProfiler.endSection(ProfilerSection::ActorPersistence);
    }

    mProfiler.beginSection(ProfilerSection::Fluids);
    level.tick();
    mProfiler.endSection(ProfilerSection::Fluids);

    _tickVillages(level);
    MapHandler::tickFrames(*this, world);

    mProfiler.beginSection(ProfilerSection::FluidBroadcast);
    _broadcastFluidChanges(level);
    mProfiler.endSection(ProfilerSection::FluidBroadcast);
}

void ServerNetworkHandler::_broadcastFluidChanges(Level &level) {
    for (const Level::FluidChange &change: level.consumeFluidChanges()) {
        UpdateBlockPacket update;
        update.mBlockPosition = change.position;
        update.mRuntimeId = change.runtimeId;
        update.mFlags = UpdateBlockPacket::Flag::All;
        update.mDataLayer = (uint32_t) change.layer;
        BlockActionHandler::broadcastToViewers(*this, level,
                                               Vector3f((float) change.position.x + 0.5f,
                                                        (float) change.position.y + 0.5f,
                                                        (float) change.position.z + 0.5f),
                                               update);
    }
}

void ServerNetworkHandler::_tickVillages(Level &level) {
    VillageManager &villages = level.getVillageManager();
    villages.tick(*this, level, mCurrentTick);

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isIn(level))
            continue;
        villages.tickVillagesNear(*this, level, mCurrentTick, player.getPosition(), mProperties.getTickDistance());
    }
}

void ServerNetworkHandler::_tickDimension(World &world, Level &level) {
    level.drainCompletedChunks();
    level.processGeneratedChanges();

    const std::vector<int64_t> repopulated = level.consumeRepopulatedChunks();
    if (!repopulated.empty()) {
        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || !player.isIn(level))
                continue;

            for (const int64_t hash: repopulated)
                ChunkStreamHandler::invalidateChunk(player, hash);
        }
    }

    std::vector<int64_t> activeColumns;
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isIn(level))
            continue;

        const int32_t centerX = (int32_t) std::floor(player.getPosition().x) >> 4;
        const int32_t centerZ = (int32_t) std::floor(player.getPosition().z) >> 4;

        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dz = -1; dz <= 1; ++dz)
                activeColumns.push_back(((int64_t) (centerX + dx) << 32) | (uint32_t) (centerZ + dz));
        }
    }

    world.getTickingAreas().appendColumns(level.getDimensionType(), activeColumns);
    level.setActiveColumns(activeColumns);
    syncActorPersistence(level, activeColumns);
    level.tick();
    if (level.getDimensionType() == DimensionType::TheEnd)
        world.getDragonFight().tick(*this, level);
    _tickVillages(level);
    _broadcastFluidChanges(level);
    _processChunkUnloads(level);
}

namespace {
    const std::chrono::milliseconds LATE_TICK{40};
    const int32_t PACK_HOLD_TICKS = 100;
}

// actors leave only with their column: one walking out alone keeps ticking there, its chunk resident
void ServerNetworkHandler::_processChunkUnloads(Level &level) {
    std::unordered_set<int64_t> occupied;
    const auto occupy = [&](const Actor &actor) {
        if (!actor.isIn(level))
            return;

        const Vector3f position = actor.getPosition();
        const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
        const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
        occupied.insert(((int64_t) chunkX << 32) | (uint32_t) chunkZ);
    };

    for (auto &entry: mActors)
        occupy(*entry.second);
    for (const std::unique_ptr<ItemActor> &item: mItemEntities)
        occupy(*item);

    level.setOccupiedColumns(std::move(occupied));
    level.processDeferredSaves();
    level.processChunkUnloads();
    // housekeeping waits for a tick that is not already late
    if (mLastTickLate)
        mPackHoldTicks = PACK_HOLD_TICKS;
    else if (mPackHoldTicks > 0)
        --mPackHoldTicks;
    // a busy stretch reads the same chunks again and again: packed, they would be unpacked the next tick
    if (mPackHoldTicks == 0 && std::chrono::steady_clock::now() - mTickStarted < LATE_TICK)
        level.packIdleChunks();
}

void ServerNetworkHandler::changePlayerDimension(ServerPlayer &player, DimensionType dimension,
                                                 const Vector3f &position) {
    changePlayerLevel(player, getWorldFor(player).getLevel(dimension), position);
}

void ServerNetworkHandler::changePlayerLevel(ServerPlayer &player, Level &destinationLevel, const Vector3f &position) {
    if (player.isIn(destinationLevel))
        return;

    const DimensionType dimension = destinationLevel.getDimensionType();
    const bool sameDimension = player.getDimension() == dimension;
    const bool otherWorld = player.getWorldId() != destinationLevel.getWorldId();

    Vector3f destination = position;
    EventSignal<PlayerLevelChangeBeforeEvent> &changeDimension = mEventBus.before().mPlayerChangeDimension;
    if (!sameDimension && changeDimension.hasSubscribers()) {
        PlayerLevelChangeBeforeEvent event(player, destinationLevel, position);
        changeDimension.emit(event);
        if (event.isCancelled())
            return;

        destination = event.mPosition;
    }

    EventSignal<PlayerLevelChangeBeforeEvent> &changeWorld = mEventBus.before().mPlayerChangeWorld;
    if (otherWorld && changeWorld.hasSubscribers()) {
        PlayerLevelChangeBeforeEvent event(player, destinationLevel, destination);
        changeWorld.emit(event);
        if (event.isCancelled())
            return;

        destination = event.mPosition;
    }

    Level &previous = getLevelFor(player);
    World &previousWorld = getWorldFor(player);
    const Vector3f fromLocation = player.getPosition();
    previous.unregisterAllChunkLoaders(player.getRuntimeId());

    player.moveToLevel(destinationLevel);
    // an awake player leaving the overworld can bring the remaining sleepers up to the threshold
    _updateSleepingPlayerList(previousWorld);
    player.getVisibleActors().clear();
    player.getVisiblePlayers().clear();
    player.setAwaitingDimensionAck(true);
    player.resetChunkStreaming();
    ChunkStreamState &stream = player.getChunkStreamState();
    const uint64_t sendTick = stream.mSendTick;
    const unsigned sendsThisTick = stream.mSendsThisTick;
    stream = ChunkStreamState();
    stream.mSendTick = sendTick;
    stream.mSendsThisTick = sendsThisTick;

    player.setPosition(destination);
    player.clearPendingMove();

    if (otherWorld)
        _sendWorldState(player);

    // the client drops terrain only on a dimension id change: same-dimension world moves hop through
    // another dimension and complete on the first ack
    if (sameDimension) {
        player.setPendingDimensionHop(true);
        _sendDimensionChange(player,
                             dimension == DimensionType::Overworld ? DimensionType::Nether : DimensionType::Overworld,
                             destination);
        return;
    }

    _sendDimensionChange(player, dimension, destination);
    ChunkStreamHandler::handleTeleport(*this, player);
    mScriptEngine.onPlayerDimensionChange(player, previous, fromLocation, destinationLevel, destination);
}

void ServerNetworkHandler::_sendDimensionChange(ServerPlayer &player, DimensionType dimension,
                                                const Vector3f &position) {
    ChangeDimensionPacket change;
    change.mDimension = Dimension::toId(dimension);
    change.mPosition = position;
    change.mRespawn = false;
    change.mHasLoadingScreenId = false;
    change.mLoadingScreenId = 0;
    sendPacketTo(player.getNetworkIdentifier(), change);
}

void ServerNetworkHandler::onPlayerDimensionChangeAck(ServerPlayer &player) {
    if (!player.isAwaitingDimensionAck())
        return;

    if (player.hasPendingDimensionHop()) {
        player.setPendingDimensionHop(false);
        _sendDimensionChange(player, player.getDimension(), player.getPosition());
        ChunkStreamHandler::handleTeleport(*this, player);
        return;
    }

    player.setAwaitingDimensionAck(false);
    player.teleport(*this, player.getPosition(), MovePlayerTeleportationCause::Behavior);
    ItemActorHandler::sendItemActorsTo(*this, player);
}
