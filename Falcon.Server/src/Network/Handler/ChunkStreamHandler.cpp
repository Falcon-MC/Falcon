#include "Network/Handler/ChunkStreamHandler.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockActorStore.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelChunkPacket.h"
#include "Protocol/Packets/NetworkChunkPublisherUpdatePacket.h"

#include <algorithm>
#include <cmath>

namespace {
    const double MIN_FOV_CHECK_DISTANCE = 4.0;
    const double MIN_FOV_CHECK_DISTANCE_SQUARED = MIN_FOV_CHECK_DISTANCE * MIN_FOV_CHECK_DISTANCE;
    const double DEGREES_TO_RADIANS = 3.14159265358979323846 / 180.0;

    void refreshComparatorContext(ChunkStreamState &state, const ServerPlayer &player) {
        const Vector3f position = player.getPosition();
        state.mComparatorChunkX = (int32_t) std::floor(position.x) >> 4;
        state.mComparatorChunkZ = (int32_t) std::floor(position.z) >> 4;

        const double yaw = (double) player.getRotation().y * DEGREES_TO_RADIANS;
        state.mComparatorDirX = -std::sin(yaw);
        state.mComparatorDirZ = std::cos(yaw);
    }

    bool isInFieldOfView(const ChunkStreamState &state, int32_t dx, int32_t dz, double squaredDistance) {
        if (squaredDistance < MIN_FOV_CHECK_DISTANCE_SQUARED)
            return true;

        const double length = std::sqrt(squaredDistance);
        const double dot = state.mComparatorDirX * ((double) dx / length)
                           + state.mComparatorDirZ * ((double) dz / length);

        static const double cosFov = std::cos((double) ChunkStreamHandler::FIELD_OF_VIEW_DEGREES
                                              * DEGREES_TO_RADIANS);
        return dot >= cosFov;
    }

    bool comesBefore(const ChunkStreamState &state, int64_t left, int64_t right) {
        const int32_t leftDx = ChunkStreamHandler::unpackChunkX(left) - state.mComparatorChunkX;
        const int32_t leftDz = ChunkStreamHandler::unpackChunkZ(left) - state.mComparatorChunkZ;
        const int32_t rightDx = ChunkStreamHandler::unpackChunkX(right) - state.mComparatorChunkX;
        const int32_t rightDz = ChunkStreamHandler::unpackChunkZ(right) - state.mComparatorChunkZ;

        const double leftDistance = (double) leftDx * leftDx + (double) leftDz * leftDz;
        const double rightDistance = (double) rightDx * rightDx + (double) rightDz * rightDz;

        const bool leftInFov = isInFieldOfView(state, leftDx, leftDz, leftDistance);
        const bool rightInFov = isInFieldOfView(state, rightDx, rightDz, rightDistance);

        if (leftInFov != rightInFov)
            return leftInFov;

        return leftDistance < rightDistance;
    }

    void makeQueue(const ChunkStreamState &state, std::vector<int64_t> &queue) {
        std::make_heap(queue.begin(), queue.end(), [&state](int64_t left, int64_t right) {
            return comesBefore(state, right, left);
        });
    }

    void pushQueue(const ChunkStreamState &state, std::vector<int64_t> &queue, int64_t hash) {
        queue.push_back(hash);
        std::push_heap(queue.begin(), queue.end(), [&state](int64_t left, int64_t right) {
            return comesBefore(state, right, left);
        });
    }

    int64_t popQueue(const ChunkStreamState &state, std::vector<int64_t> &queue) {
        std::pop_heap(queue.begin(), queue.end(), [&state](int64_t left, int64_t right) {
            return comesBefore(state, right, left);
        });

        const int64_t hash = queue.back();
        queue.pop_back();
        return hash;
    }

    void updateInRadiusChunks(ChunkStreamState &state, int32_t viewDistance, int32_t centerX, int32_t centerZ) {
        state.mInRadius.clear();

        for (int32_t dx = -viewDistance; dx <= viewDistance; ++dx) {
            for (int32_t dz = -viewDistance; dz <= viewDistance; ++dz) {
                if (dx * dx + dz * dz > viewDistance * viewDistance)
                    continue;

                state.mInRadius.insert(ChunkStreamHandler::packChunk(centerX + dx, centerZ + dz));
            }
        }
    }

    void removeOutOfRadiusChunks(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player) {
        std::unordered_set<int64_t> &sent = player.getSentChunks();
        Level &level = owner.getLevelFor(player);

        for (auto it = sent.begin(); it != sent.end();) {
            if (state.mInRadius.find(*it) != state.mInRadius.end()) {
                ++it;
                continue;
            }

            level.unregisterChunkLoader(player.getRuntimeId(), ChunkStreamHandler::unpackChunkX(*it),
                                        ChunkStreamHandler::unpackChunkZ(*it));
            it = sent.erase(it);
        }
    }

    void updateChunkSendingQueue(ChunkStreamState &state, ServerPlayer &player) {
        const std::unordered_set<int64_t> &sent = player.getSentChunks();

        state.mSendQueue.clear();
        for (const int64_t hash: state.mInRadius) {
            if (sent.find(hash) == sent.end())
                state.mSendQueue.push_back(hash);
        }

        makeQueue(state, state.mSendQueue);
    }

    void pruneQueueOutOfRadius(ChunkStreamState &state, std::vector<int64_t> &queue) {
        std::vector<int64_t> kept;
        kept.reserve(queue.size());

        for (const int64_t hash: queue) {
            if (state.mInRadius.find(hash) != state.mInRadius.end())
                kept.push_back(hash);
        }

        queue.swap(kept);
        makeQueue(state, queue);
    }

    void pruneLoadingQueueOutOfRadius(ChunkStreamState &state) {
        for (auto it = state.mLoading.begin(); it != state.mLoading.end();) {
            if (state.mInRadius.find(*it) == state.mInRadius.end())
                it = state.mLoading.erase(it);
            else
                ++it;
        }
    }

    void loadQueuedChunks(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player,
                          int32_t attempts) {
        if (state.mSendQueue.empty())
            return;

        Level &level = owner.getLevelFor(player);
        std::vector<int64_t> requeue;
        int32_t tried = 0;

        while (!state.mSendQueue.empty() && tried < attempts) {
            ++tried;

            const int64_t hash = popQueue(state, state.mSendQueue);

            const int32_t chunkX = ChunkStreamHandler::unpackChunkX(hash);
            const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(hash);

            if (level.isChunkPopulated(chunkX, chunkZ)) {
                state.mLoading.erase(hash);
                pushQueue(state, state.mReadyToSend, hash);
                continue;
            }

            if (level.isChunkResident(chunkX, chunkZ)) {
                state.mLoading.erase(hash);
                level.requestChunkAsync(chunkX, chunkZ);
                requeue.push_back(hash);
                continue;
            }

            if (state.mLoading.find(hash) == state.mLoading.end()) {
                level.requestChunkAsync(chunkX, chunkZ);
                state.mLoading.insert(hash);
            }

            requeue.push_back(hash);
        }

        for (const int64_t hash: requeue)
            pushQueue(state, state.mSendQueue, hash);
    }

    void sendReadyChunks(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player) {
        if (state.mReadyToSend.empty())
            return;

        Level &level = owner.getLevelFor(player);
        const NetworkIdentifier &id = player.getNetworkIdentifier();
        const Vector3f position = player.getPosition();

        NetworkChunkPublisherUpdatePacket publisher;
        publisher.mPosition = Vector3i((int32_t) position.x, (int32_t) position.y, (int32_t) position.z);
        publisher.mRadius = (uint32_t) (level.getViewDistance() * 16);
        owner.getNetworkHandler().send(id, publisher, owner.getCodecContext());

        std::unordered_set<int64_t> &sent = player.getSentChunks();
        unsigned added = 0;

        while (!state.mReadyToSend.empty()) {
            const int64_t hash = popQueue(state, state.mReadyToSend);

            if (state.mInRadius.find(hash) == state.mInRadius.end()) {
                sent.erase(hash);
                continue;
            }

            const int32_t chunkX = ChunkStreamHandler::unpackChunkX(hash);
            const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(hash);

            LevelChunkPacket chunk;
            chunk.mChunkX = chunkX;
            chunk.mChunkZ = chunkZ;
            chunk.mDimension = level.getDimensionId();
            chunk.mCachingEnabled = false;

            chunk.mSubChunksLength = (uint32_t) level.getChunkSubChunkCount(chunkX, chunkZ);
            chunk.mRequestSubChunks = false;
            chunk.mData = level.getChunkData(chunkX, chunkZ);

            BlockActorStore &blockActors = BlockActorStore::getInstance();
            if (blockActors.isChunkLoaded(chunkX, chunkZ))
                chunk.mData += blockActors.encodeChunkNetwork(chunkX, chunkZ);
            else
                chunk.mData += BlockActorStore::encodeNetwork(level.loadBlockEntities(chunkX, chunkZ));

            owner.getNetworkHandler().send(id, chunk, owner.getCodecContext());
            level.registerChunkLoader(player.getRuntimeId(), chunkX, chunkZ);
            sent.insert(hash);
            ++added;
        }

        player.addSentChunkCount(added);
    }
}

int64_t ChunkStreamHandler::packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

int32_t ChunkStreamHandler::unpackChunkX(int64_t hash) {
    return (int32_t) (hash >> 32);
}

int32_t ChunkStreamHandler::unpackChunkZ(int64_t hash) {
    return (int32_t) (hash & 0xffffffff);
}

void ChunkStreamHandler::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    ChunkStreamState &state = player.getChunkStreamState();

    refreshComparatorContext(state, player);

    const int64_t loaderChunk = packChunk(state.mComparatorChunkX, state.mComparatorChunkZ);
    if (loaderChunk != state.mLastLoaderChunk) {
        state.mLastLoaderChunk = loaderChunk;
        updateInRadiusChunks(state, owner.getLevelFor(player).getViewDistance(), state.mComparatorChunkX,
                             state.mComparatorChunkZ);
        removeOutOfRadiusChunks(state, owner, player);
        updateChunkSendingQueue(state, player);
    }

    loadQueuedChunks(state, owner, player, owner.getChunksPerTick());
    sendReadyChunks(state, owner, player);

    player.setLastChunkPosition(state.mComparatorChunkX, state.mComparatorChunkZ);
}

void ChunkStreamHandler::handleTeleport(ServerNetworkHandler &owner, ServerPlayer &player) {
    ChunkStreamState &state = player.getChunkStreamState();

    refreshComparatorContext(state, player);
    state.mLastLoaderChunk = packChunk(state.mComparatorChunkX, state.mComparatorChunkZ);

    updateInRadiusChunks(state, 1, state.mComparatorChunkX, state.mComparatorChunkZ);
    removeOutOfRadiusChunks(state, owner, player);
    updateInRadiusChunks(state, TELEPORT_LOAD_COUNT, state.mComparatorChunkX, state.mComparatorChunkZ);

    pruneLoadingQueueOutOfRadius(state);
    pruneQueueOutOfRadius(state, state.mReadyToSend);
    updateChunkSendingQueue(state, player);

    loadQueuedChunks(state, owner, player, TELEPORT_LOAD_COUNT);
    sendReadyChunks(state, owner, player);
}

void ChunkStreamHandler::invalidateChunk(ServerPlayer &player, int64_t hash) {
    ChunkStreamState &state = player.getChunkStreamState();

    if (player.getSentChunks().erase(hash) == 0)
        return;

    if (state.mInRadius.find(hash) == state.mInRadius.end())
        return;

    pushQueue(state, state.mReadyToSend, hash);
}

void ChunkStreamHandler::handleViewDistanceChange(ServerNetworkHandler &owner, ServerPlayer &player) {
    ChunkStreamState &state = player.getChunkStreamState();

    refreshComparatorContext(state, player);
    updateInRadiusChunks(state, owner.getLevelFor(player).getViewDistance(), state.mComparatorChunkX,
                         state.mComparatorChunkZ);
    removeOutOfRadiusChunks(state, owner, player);
    pruneQueueOutOfRadius(state, state.mSendQueue);
    pruneQueueOutOfRadius(state, state.mReadyToSend);
    pruneLoadingQueueOutOfRadius(state);
    updateChunkSendingQueue(state, player);
}
