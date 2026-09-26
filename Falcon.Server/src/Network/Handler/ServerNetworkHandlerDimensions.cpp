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

#include <cmath>
#include <vector>

Level &ServerNetworkHandler::getDimension(DimensionType dimension) {
    if (dimension == DimensionType::Nether && mNetherLevel != nullptr)
        return *mNetherLevel;

    if (dimension == DimensionType::TheEnd && mTheEndLevel != nullptr)
        return *mTheEndLevel;

    return mLevel;
}

Level &ServerNetworkHandler::getLevelFor(const Actor &actor) {
    return getDimension(actor.getDimension());
}

std::vector<Level *> ServerNetworkHandler::getLevels() {
    std::vector<Level *> levels{&mLevel};
    if (mNetherLevel != nullptr)
        levels.push_back(mNetherLevel.get());
    if (mTheEndLevel != nullptr)
        levels.push_back(mTheEndLevel.get());
    return levels;
}

void ServerNetworkHandler::_tickDimension(Level &level) {
    level.drainCompletedChunks();
    level.processGeneratedChanges();

    const std::vector<int64_t> repopulated = level.consumeRepopulatedChunks();
    if (!repopulated.empty()) {
        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || player.getDimension() != level.getDimensionType())
                continue;

            for (const int64_t hash: repopulated)
                ChunkStreamHandler::invalidateChunk(player, hash);
        }
    }

    std::vector<int64_t> activeColumns;
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.getDimension() != level.getDimensionType())
            continue;

        const int32_t centerX = (int32_t) std::floor(player.getPosition().x) >> 4;
        const int32_t centerZ = (int32_t) std::floor(player.getPosition().z) >> 4;

        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dz = -1; dz <= 1; ++dz)
                activeColumns.push_back(((int64_t) (centerX + dx) << 32) | (uint32_t) (centerZ + dz));
        }
    }

    mTickingAreas.appendColumns(level.getDimensionType(), activeColumns);
    level.setActiveColumns(activeColumns);
    syncActorPersistence(level, activeColumns);
    level.tick();

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

    level.processChunkUnloads();
}

void ServerNetworkHandler::changePlayerDimension(ServerPlayer &player, DimensionType dimension,
                                                 const Vector3f &position) {
    if (player.getDimension() == dimension)
        return;

    PluginEvent dimensionEvent;
    dimensionEvent.mType = FALCON_EVENT_PLAYER_CHANGE_DIMENSION;
    dimensionEvent.mCancellable = true;
    dimensionEvent.mPlayer = &player;
    dimensionEvent.mDimension = (uint32_t) Dimension::toId(dimension);
    dimensionEvent.mPreviousDimension = (uint32_t) Dimension::toId(player.getDimension());
    dimensionEvent.mFrom = player.getPosition();
    dimensionEvent.mTo = position;
    mPluginManager->dispatch(dimensionEvent);
    if (dimensionEvent.mCancelled)
        return;

    const Vector3f destination = dimensionEvent.mToChanged ? dimensionEvent.mTo : position;
    Level &previous = getLevelFor(player);
    previous.unregisterAllChunkLoaders(player.getRuntimeId());

    player.setDimension(dimension);
    player.getVisibleActors().clear();
    player.getVisiblePlayers().clear();
    player.setAwaitingDimensionAck(true);
    player.resetChunkStreaming();
    player.getChunkStreamState() = ChunkStreamState();

    player.setPosition(destination);
    player.clearPendingMove();

    ChangeDimensionPacket change;
    change.mDimension = Dimension::toId(dimension);
    change.mPosition = destination;
    change.mRespawn = false;
    change.mHasLoadingScreenId = false;
    change.mLoadingScreenId = 0;
    sendPacketTo(player.getNetworkIdentifier(), change);

    ChunkStreamHandler::handleTeleport(*this, player);
}

void ServerNetworkHandler::onPlayerDimensionChangeAck(ServerPlayer &player) {
    if (!player.isAwaitingDimensionAck())
        return;

    player.setAwaitingDimensionAck(false);
    player.teleport(*this, player.getPosition(), MovePlayerTeleportationCause::Behavior);
    ItemActorHandler::sendItemActorsTo(*this, player);
}
