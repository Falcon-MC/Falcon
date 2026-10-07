#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ServerPlayer.h"
#include "Level/Dimension.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Plugin/PluginManager.h"
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

/**
 * Ticks the overworld of one world. Unlike the other dimensions it ticks every column within
 * `tick-distance` of a player, and only rebuilds that set when the player chunk centres change.
 */
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

    if (world.mActorPersistencePending || tickDistance != world.mActiveTickDistance
        || centers != world.mActiveCenters) {
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

        mProfiler.beginSection(ProfilerSection::ActorPersistence);
        world.mActorPersistencePending = syncActorPersistence(level, activeColumns);
        mProfiler.endSection(ProfilerSection::ActorPersistence);
    }

    mProfiler.beginSection(ProfilerSection::Fluids);
    level.tick();
    mProfiler.endSection(ProfilerSection::Fluids);

    mProfiler.beginSection(ProfilerSection::FluidBroadcast);
    _broadcastFluidChanges(level);
    mProfiler.endSection(ProfilerSection::FluidBroadcast);
}

void ServerNetworkHandler::_broadcastFluidChanges(Level &level) {
    for (const Level::FluidChange &change: level.consumeFluidChanges()) {
        UpdateBlockPacket update;
        update.mBlockPosition = change.position;
        update.mRuntimeId = (uint32_t) BlockStateHasher::hash(change.state.mName, change.state.mStates);
        update.mFlags = UpdateBlockPacket::Flag::All;
        update.mDataLayer = (uint32_t) change.layer;
        BlockActionHandler::broadcastToViewers(*this, level,
                                               Vector3f((float) change.position.x + 0.5f,
                                                        (float) change.position.y + 0.5f,
                                                        (float) change.position.z + 0.5f),
                                               update);
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
    _broadcastFluidChanges(level);
    _processChunkUnloads(level);
}

/**
 * Actors outside the active columns are only saved away when their column leaves the active set,
 * so one that walked out on its own keeps ticking there. Its chunk stays resident while it does.
 */
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
    level.processChunkUnloads();
}

void ServerNetworkHandler::changePlayerDimension(ServerPlayer &player, DimensionType dimension,
                                                 const Vector3f &position) {
    changePlayerLevel(player, getWorldFor(player).getLevel(dimension), position);
}

void ServerNetworkHandler::changePlayerLevel(ServerPlayer &player, Level &destinationLevel,
                                             const Vector3f &position) {
    if (player.isIn(destinationLevel))
        return;

    const DimensionType dimension = destinationLevel.getDimensionType();
    const bool sameDimension = player.getDimension() == dimension;
    const bool otherWorld = player.getWorldId() != destinationLevel.getWorldId();

    Vector3f destination = position;
    if (!sameDimension) {
        PluginEvent dimensionEvent;
        dimensionEvent.mType = FALCON_EVENT_PLAYER_CHANGE_DIMENSION;
        dimensionEvent.mCancellable = true;
        dimensionEvent.mPlayer = &player;
        dimensionEvent.mLevel = &destinationLevel;
        dimensionEvent.mDimension = (uint32_t) Dimension::toId(dimension);
        dimensionEvent.mPreviousDimension = (uint32_t) Dimension::toId(player.getDimension());
        dimensionEvent.mFrom = player.getPosition();
        dimensionEvent.mTo = position;
        mPluginManager->dispatch(dimensionEvent);
        if (dimensionEvent.mCancelled)
            return;

        if (dimensionEvent.mToChanged)
            destination = dimensionEvent.mTo;
    }

    if (otherWorld) {
        PluginEvent worldEvent;
        worldEvent.mType = FALCON_EVENT_PLAYER_CHANGE_WORLD;
        worldEvent.mCancellable = true;
        worldEvent.mPlayer = &player;
        worldEvent.mLevel = &destinationLevel;
        worldEvent.mWorldName = getWorldOf(destinationLevel).getName();
        worldEvent.mPreviousWorldName = getWorldFor(player).getName();
        worldEvent.mFrom = player.getPosition();
        worldEvent.mTo = destination;
        mPluginManager->dispatch(worldEvent);
        if (worldEvent.mCancelled)
            return;

        if (worldEvent.mToChanged)
            destination = worldEvent.mTo;
    }

    Level &previous = getLevelFor(player);
    previous.unregisterAllChunkLoaders(player.getRuntimeId());

    player.moveToLevel(destinationLevel);
    player.getVisibleActors().clear();
    player.getVisiblePlayers().clear();
    player.setAwaitingDimensionAck(true);
    player.resetChunkStreaming();
    player.getChunkStreamState() = ChunkStreamState();

    player.setPosition(destination);
    player.clearPendingMove();

    if (otherWorld)
        _sendWorldState(player);

    // The client only drops its terrain when the dimension id changes, so a move between two worlds
    // in the same dimension first goes through another dimension and completes on the first ack.
    if (sameDimension) {
        player.setPendingDimensionHop(true);
        _sendDimensionChange(player, dimension == DimensionType::Overworld ? DimensionType::Nether
                                                                          : DimensionType::Overworld, destination);
        return;
    }

    _sendDimensionChange(player, dimension, destination);
    ChunkStreamHandler::handleTeleport(*this, player);
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
