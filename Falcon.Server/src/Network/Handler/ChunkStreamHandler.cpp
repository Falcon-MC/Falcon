#include "Network/Handler/ChunkStreamHandler.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "Core/Math/MathConstants.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Utility/BinaryStream.h"
#include "Protocol/Packets/LevelChunkPacket.h"
#include "Protocol/Packets/NetworkChunkPublisherUpdatePacket.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/TickingAreaManager.h"
#include "World/Level/World.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace {
    const double MIN_FOV_CHECK_DISTANCE = 4.0;
    const double MIN_FOV_CHECK_DISTANCE_SQUARED = MIN_FOV_CHECK_DISTANCE * MIN_FOV_CHECK_DISTANCE;

    int32_t streamRadius(const ChunkStreamState &state, const Level &level) {
        return state.mRadius > 0 ? std::min(state.mRadius, level.getViewDistance()) : level.getViewDistance();
    }

    void refreshComparatorContext(ChunkStreamState &state, const ServerPlayer &player) {
        const Vector3f position = player.getPosition();
        state.mComparatorChunkX = (int32_t) std::floor(position.x) >> 4;
        state.mComparatorChunkZ = (int32_t) std::floor(position.z) >> 4;

        const double yaw = (double) player.getRotation().y * MathConstants::DEGREES_TO_RADIANS;
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
                                              * MathConstants::DEGREES_TO_RADIANS);
        return dot >= cosFov;
    }

    // chunks in front first, then nearest; the few closest always count as visible:
    // the ground underfoot never waits on looking away
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

    // binary heap over a vector, comparator reversed: the best chunk at the front
    void makeQueue(const ChunkStreamState &state, std::vector<int64_t> &queue) {
        std::make_heap(queue.begin(), queue.end(),
                       [&state](int64_t left, int64_t right) { return comesBefore(state, right, left); });
    }

    void pushQueue(const ChunkStreamState &state, std::vector<int64_t> &queue, int64_t hash) {
        queue.push_back(hash);
        std::push_heap(queue.begin(), queue.end(),
                       [&state](int64_t left, int64_t right) { return comesBefore(state, right, left); });
    }

    int64_t popQueue(const ChunkStreamState &state, std::vector<int64_t> &queue) {
        std::pop_heap(queue.begin(), queue.end(),
                      [&state](int64_t left, int64_t right) { return comesBefore(state, right, left); });

        const int64_t hash = queue.back();
        queue.pop_back();
        return hash;
    }

    // Client side generation from server.properties; every player of a level follows it.
    bool isClientSideGeneration(const ServerNetworkHandler &owner) {
        return owner.getProperties().getClientSideChunkGenerationEnabled();
    }

    // A chunk the server holds with all eight neighbours decorated, without loading anything. It is what the
    // publisher view takes as it is even when the player view leaves it to
    // the client, because another player or a ticking area made the server build it.
    bool isBuiltWithNeighbours(const Level &level, int32_t chunkX, int32_t chunkZ) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dz = -1; dz <= 1; ++dz) {
                if (!level.isChunkPopulated(chunkX + dx, chunkZ + dz))
                    return false;
            }
        }
        return true;
    }

    /**
     * Inputs of ClientSideGeneration::isServerBuilt for one player and tick, built on first use: most ticks
     * decide no chunk at all.
     */
    class BuildDecision {
    public:
        BuildDecision(ServerNetworkHandler &owner, ServerPlayer &player, Level &level, int32_t chunkRadius)
                : mOwner(owner), mPlayer(player), mLevel(level), mChunkRadius(chunkRadius) {}

        bool isServerBuilt(int32_t chunkX, int32_t chunkZ) {
            if (!mReady)
                _prepare();
            return ClientSideGeneration::isServerBuilt(chunkX, chunkZ, mContext);
        }

    private:
        void _prepare() {
            mReady = true;
            const PropertiesSettings &properties = mOwner.getProperties();
            const int32_t tickRange = properties.getTickDistance();
            const int32_t viewRadius = ClientSideGeneration::computeViewRadius(mChunkRadius, tickRange);
            mContext.mBuildRadius = ClientSideGeneration::computeServerBuildRadius(mPlayer.getServerBuildRatio(),
                                                                                   viewRadius, tickRange);

            for (auto &entry: mOwner.getPlayers()) {
                const ServerPlayer &other = entry.second;
                if (other.getLoginState() < ServerPlayer::LoginState::StartGameSent || !other.isIn(mLevel))
                    continue;

                const Vector3f position = other.getPosition();
                mContext.mPlayerChunks.emplace_back((int32_t) std::floor(position.x) >> 4,
                                                    (int32_t) std::floor(position.z) >> 4);
            }
            mContext.mPlayerCount = std::max<size_t>(mContext.mPlayerChunks.size(), 1);

            for (const TickingArea &area: mOwner.getWorldOf(mLevel).getTickingAreas().getAreas()) {
                if (area.mDimension != mLevel.getDimensionType())
                    continue;

                ClientSideGeneration::TickingAreaBounds bounds;
                bounds.mMinChunkX = area.mMinX >> 4;
                bounds.mMinChunkZ = area.mMinZ >> 4;
                bounds.mMaxChunkX = area.mMaxX >> 4;
                bounds.mMaxChunkZ = area.mMaxZ >> 4;
                mContext.mTickingAreas.push_back(bounds);
            }

            Level *level = &mLevel;
            mContext.mIsSaved = [level](int32_t chunkX, int32_t chunkZ) { return level->isChunkSaved(chunkX, chunkZ); };

            if (mLevel.getDimensionType() != DimensionType::Overworld)
                return;

            std::shared_ptr<std::vector<const StructureFeature *>> features =
                    std::make_shared<std::vector<const StructureFeature *>>();
            const std::string names = properties.getClientSideChunkGenerationServerStructures();
            size_t start = 0;
            while (start <= names.size()) {
                size_t end = names.find(',', start);
                if (end == std::string::npos)
                    end = names.size();

                std::string name = names.substr(start, end - start);
                name.erase(0, name.find_first_not_of(' '));
                name.erase(name.find_last_not_of(' ') + 1);
                if (const StructureFeature *feature = StructureFeature::get(name))
                    features->push_back(feature);
                start = end + 1;
            }
            if (features->empty())
                return;

            mContext.mIsForced = [level, features](int32_t chunkX, int32_t chunkZ) {
                return level->isNearStructure(*features, chunkX, chunkZ);
            };
        }

        ServerNetworkHandler &mOwner;
        ServerPlayer &mPlayer;
        Level &mLevel;
        int32_t mChunkRadius;
        bool mReady = false;
        ClientSideGeneration::BuildContext mContext;
    };

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

    void pruneQueueOutOfRadius(ChunkStreamState &state, std::vector<int64_t> &queue, Level &level, uint64_t loaderId) {
        std::vector<int64_t> kept;
        kept.reserve(queue.size());

        for (const int64_t hash: queue) {
            if (state.mInRadius.find(hash) != state.mInRadius.end())
                kept.push_back(hash);
            else
                level.unregisterChunkLoader(loaderId, ChunkStreamHandler::unpackChunkX(hash),
                                            ChunkStreamHandler::unpackChunkZ(hash));
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

    // decoration spills trees and grass into neighbours: complete only once all eight are populated,
    // else the vegetation pops in front of the player
    bool neighboursPopulated(Level &level, int32_t chunkX, int32_t chunkZ) {
        bool populated = true;
        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dz = -1; dz <= 1; ++dz) {
                if ((dx == 0 && dz == 0) || level.isChunkPopulated(chunkX + dx, chunkZ + dz))
                    continue;

                level.requestChunkAsync(chunkX + dx, chunkZ + dz);
                populated = false;
            }
        }
        return populated;
    }

    void loadQueuedChunks(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player,
                          int32_t attempts) {
        if (state.mSendQueue.empty())
            return;

        Level &level = owner.getLevelFor(player);
        const bool clientSideGeneration = isClientSideGeneration(owner);
        BuildDecision decision(owner, player, level, streamRadius(state, level));
        std::vector<int64_t> requeue;
        int32_t tried = 0;

        while (!state.mSendQueue.empty() && tried < attempts) {
            ++tried;

            const int64_t hash = popQueue(state, state.mSendQueue);

            const int32_t chunkX = ChunkStreamHandler::unpackChunkX(hash);
            const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(hash);

            // The view leaves the chunk to the client: nothing is loaded for it.
            if (clientSideGeneration && !decision.isServerBuilt(chunkX, chunkZ)) {
                state.mLoading.erase(hash);
                if (isBuiltWithNeighbours(level, chunkX, chunkZ))
                    pushQueue(state, state.mReadyToSend, hash);
                else
                    state.mClientBuilt.push_back(hash);
                continue;
            }

            if (level.isChunkPopulated(chunkX, chunkZ)) {
                state.mLoading.erase(hash);
                if (neighboursPopulated(level, chunkX, chunkZ))
                    pushQueue(state, state.mReadyToSend, hash);
                else
                    requeue.push_back(hash);
                continue;
            }

            if (level.isChunkResident(chunkX, chunkZ)) {
                state.mLoading.erase(hash);
                level.requestChunkAsync(chunkX, chunkZ);
                requeue.push_back(hash);
                continue;
            }

            // generation backpressure can defer admission; Level dedupes accepted requests: retry anyway
            level.requestChunkAsync(chunkX, chunkZ);
            state.mLoading.insert(hash);

            requeue.push_back(hash);
        }

        for (const int64_t hash: requeue)
            pushQueue(state, state.mSendQueue, hash);
    }

    LevelChunkPacket makeChunkPacket(Level &level, int32_t chunkX, int32_t chunkZ, const PacketCodecContext &context,
                                     const std::string &blockActors) {
        LevelChunkPacket chunk;
        chunk.mChunkX = chunkX;
        chunk.mChunkZ = chunkZ;
        chunk.mDimension = level.getDimensionId();
        chunk.mCachingEnabled = false;

        chunk.mSubChunksLength = (uint32_t) level.getChunkSubChunkCount(chunkX, chunkZ);
        chunk.mRequestSubChunks = false;
        chunk.mData = level.getChunkData(chunkX, chunkZ, context.getBlockNetworkIds()) + blockActors;
        return chunk;
    }

    // players sent the same chunk revision share one encoding and one compression
    void sendChunk(ServerNetworkHandler &owner, ServerPlayer &player, Level &level, int32_t chunkX, int32_t chunkZ,
                   int64_t hash) {
        const NetworkIdentifier &id = player.getNetworkIdentifier();
        const PacketCodecContext &context = owner.getCodecContext(id);

        BlockActorStore &blockActors = level.getBlockActors();
        ChunkPacketCache::Stamp stamp;
        if (blockActors.isChunkLoaded(chunkX, chunkZ))
            stamp.mBlockActors = blockActors.encodeChunkNetwork(chunkX, chunkZ);
        else
            stamp.mBlockActors = BlockActorStore::encodeNetwork(level.loadBlockEntities(chunkX, chunkZ));

        const LevelChunk &levelChunk = level.getChunk(chunkX, chunkZ);
        stamp.mInstance = levelChunk.getNetworkInstance();
        stamp.mRevision = levelChunk.getNetworkRevision();

        const ChunkPacketCache::Key key{&level, hash, &context};
        owner.getNetworkHandler().sendShared(
                id, owner.getChunkPacketCache().get(key, stamp, owner.getChunkStreamTick(), [&]() {
                    BinaryStream stream;
                    makeChunkPacket(level, chunkX, chunkZ, context, stamp.mBlockActors)
                            .writeWithHeader(stream, context);
                    return stream.getBuffer();
                }));
    }

    void sendReadyChunks(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player) {
        if (state.mSendTick != owner.getChunkStreamTick()) {
            state.mSendTick = owner.getChunkStreamTick();
            state.mSendsThisTick = 0;
        }
        // the spawn status waits on the nearest chunks: one tick sends them, their compression is shared;
        // meanwhile players already in hold back the link and the clients' decoding
        const bool spawning = !player.hasSpawnChunksReady();
        unsigned budget = ChunkStreamHandler::MAX_SENDS_PER_TICK;
        if (spawning)
            budget = (unsigned) ChunkStreamHandler::SPAWN_CHUNK_COUNT;
        else if (owner.isJoinBurst())
            budget = ChunkStreamHandler::JOIN_BURST_SENDS_PER_TICK;
        if (state.mReadyToSend.empty() || state.mSendsThisTick >= budget)
            return;

        // chunks encoded faster than the network thread compresses only pile up and delay everything else
        NetworkHandler &network = owner.getNetworkHandler();
        const NetworkIdentifier &id = player.getNetworkIdentifier();
        const auto hasRoom = [&]() {
            return spawning || network.getPendingOutboundBytes(id) < ChunkStreamHandler::MAX_QUEUED_CHUNK_BYTES;
        };
        if (!hasRoom())
            return;

        Level &level = owner.getLevelFor(player);
        const bool clientSideGeneration = isClientSideGeneration(owner);

        // The client discards chunks outside the last publisher radius it received, so the
        // publisher update has to go out before the chunks themselves. Under client side generation the
        // update follows its own schedule, and the view only moves when it is sent.
        if (!clientSideGeneration) {
            const Vector3f position = player.getPosition();
            NetworkChunkPublisherUpdatePacket publisher;
            publisher.mPosition = Vector3i((int32_t) position.x, (int32_t) position.y, (int32_t) position.z);
            publisher.mRadius = (uint32_t) (streamRadius(state, level) * 16);
            owner.getNetworkHandler().send(id, publisher, owner.getCodecContext());
        }

        std::unordered_set<int64_t> &sent = player.getSentChunks();
        unsigned added = 0;

        // Encoding a chunk and loading its block entities runs on the main thread, so a burst
        // of ready chunks after a teleport is spread over several ticks in priority order.
        while (!state.mReadyToSend.empty() && state.mSendsThisTick < budget && hasRoom()) {
            const int64_t hash = popQueue(state, state.mReadyToSend);

            if (state.mInRadius.find(hash) == state.mInRadius.end()) {
                sent.erase(hash);
                level.unregisterChunkLoader(player.getRuntimeId(), ChunkStreamHandler::unpackChunkX(hash),
                                            ChunkStreamHandler::unpackChunkZ(hash));
                continue;
            }

            const int32_t chunkX = ChunkStreamHandler::unpackChunkX(hash);
            const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(hash);

            sendChunk(owner, player, level, chunkX, chunkZ, hash);
            level.registerChunkLoader(player.getRuntimeId(), chunkX, chunkZ);
            sent.insert(hash);
            ++added;
            ++state.mSendsThisTick;

            // every chunk sent under client side generation is one the server built, and the
            // client learns it from the next publisher update.
            if (clientSideGeneration) {
                state.mSavedChunks.add(chunkX, chunkZ);
                state.mChunkSentSinceUpdate = true;
                state.mLastChunkSent = std::chrono::steady_clock::now();
            }
        }

        player.addSentChunkCount(added);
    }
}

namespace {
    Vector3i publisherPosition(const ServerPlayer &player) {
        const Vector3f position = player.getPosition();
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    // the update carries the chunks sent since the previous one, then the view follows it.
    void publishClientSideGeneration(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player) {
        Level &level = owner.getLevelFor(player);
        const int32_t radius = streamRadius(state, level);
        const Vector3i position = publisherPosition(player);
        const uint32_t blockRadius = (uint32_t) (radius * 16);
        if (!ChunkStreamHandler::isPublisherUpdateDue(state, position, blockRadius, std::chrono::steady_clock::now()))
            return;

        NetworkChunkPublisherUpdatePacket publisher;
        publisher.mPosition = position;
        publisher.mRadius = blockRadius;
        publisher.mSavedChunks = state.mSavedChunks.drain();
        owner.getNetworkHandler().send(player.getNetworkIdentifier(), publisher, owner.getCodecContext());

        const bool radiusChanged = !state.mPublished || state.mPublisherRadius != blockRadius;
        state.mPublished = true;
        state.mForcePublisherMove = false;
        state.mChunkSentSinceUpdate = false;
        state.mPublisherPosition = position;
        state.mPublisherRadius = blockRadius;

        const int64_t centre = ChunkStreamHandler::packChunk(position.x >> 4, position.z >> 4);
        if (centre == state.mLastLoaderChunk && !radiusChanged)
            return;

        state.mLastLoaderChunk = centre;
        updateInRadiusChunks(state, radius, position.x >> 4, position.z >> 4);
        removeOutOfRadiusChunks(state, owner, player);
        pruneLoadingQueueOutOfRadius(state);
        pruneQueueOutOfRadius(state, state.mReadyToSend, level, player.getRuntimeId());
        // The chunks left to the client go back through the decision with the rest of the queue.
        state.mClientBuilt.clear();
        state.mClientBuiltCursor = 0;
        updateChunkSendingQueue(state, player);
    }

    // Chunks left to the client are sent once the server holds them for another reason, like the publisher
    // view picking up chunks another view loaded. A few are looked at each tick.
    void recheckClientBuilt(ChunkStreamState &state, ServerNetworkHandler &owner, ServerPlayer &player) {
        static const size_t CHECKS_PER_TICK = 64;
        if (state.mClientBuilt.empty())
            return;

        const Level &level = owner.getLevelFor(player);
        const std::unordered_set<int64_t> &sent = player.getSentChunks();
        for (size_t checked = 0; checked < CHECKS_PER_TICK && !state.mClientBuilt.empty(); ++checked) {
            if (state.mClientBuiltCursor >= state.mClientBuilt.size())
                state.mClientBuiltCursor = 0;

            const int64_t hash = state.mClientBuilt[state.mClientBuiltCursor];
            const bool inRadius = state.mInRadius.find(hash) != state.mInRadius.end();
            const bool built = inRadius && sent.find(hash) == sent.end()
                               && isBuiltWithNeighbours(level, ChunkStreamHandler::unpackChunkX(hash),
                                                        ChunkStreamHandler::unpackChunkZ(hash));
            if (inRadius && !built) {
                ++state.mClientBuiltCursor;
                continue;
            }

            state.mClientBuilt[state.mClientBuiltCursor] = state.mClientBuilt.back();
            state.mClientBuilt.pop_back();
            if (built)
                pushQueue(state, state.mReadyToSend, hash);
        }
    }
}

bool ChunkStreamHandler::isPublisherUpdateDue(const ChunkStreamState &state, const Vector3i &position, uint32_t radius,
                                              std::chrono::steady_clock::time_point now) {
    if (state.mChunkSentSinceUpdate) {
        const int64_t elapsed =
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - state.mLastChunkSent).count();
        if (elapsed < ClientSideGeneration::PUBLISHER_DELAY_NANOSECONDS)
            return false;
    }

    if (!state.mPublished || state.mPublisherRadius != radius || !state.mSavedChunks.empty())
        return true;

    const int64_t threshold = state.mForcePublisherMove ? 0 : ClientSideGeneration::PUBLISHER_MOVE_THRESHOLD;
    const int64_t dx = (int64_t) position.x - state.mPublisherPosition.x;
    const int64_t dy = (int64_t) position.y - state.mPublisherPosition.y;
    const int64_t dz = (int64_t) position.z - state.mPublisherPosition.z;
    return dx * dx + dy * dy + dz * dz >= threshold * threshold;
}

void ChunkStreamHandler::dropPendingChunks(ChunkStreamState &state) {
    state.mSendQueue.clear();
    state.mLoading.clear();
    state.mReadyToSend.clear();
    state.mClientBuilt.clear();
    state.mClientBuiltCursor = 0;
}

bool ChunkStreamHandler::isInActorRange(const Vector3f &viewer, const Vector3f &position) {
    // the game table holds every offset with dx * dx + dz * dz <= 17
    static const int64_t ACTOR_RANGE_SQUARED = 17;

    const int64_t dx = (int64_t) ((int32_t) std::floor(position.x) >> 4) - ((int32_t) std::floor(viewer.x) >> 4);
    const int64_t dz = (int64_t) ((int32_t) std::floor(position.z) >> 4) - ((int32_t) std::floor(viewer.z) >> 4);
    return dx * dx + dz * dz <= ACTOR_RANGE_SQUARED;
}

int64_t ChunkStreamHandler::packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

bool ChunkStreamHandler::isPositionRelevant(ServerPlayer &player, const Level &level, const Vector3i &position) {
    const ChunkStreamState &state = player.getChunkStreamState();
    if (state.mLastLoaderChunk == INT64_MAX || !player.isIn(level))
        return false;
    // the view centre is the corner of its chunk, its radius in blocks
    const int64_t dx = (int64_t) position.x - 16 * (int64_t) unpackChunkX(state.mLastLoaderChunk);
    const int64_t dz = (int64_t) position.z - 16 * (int64_t) unpackChunkZ(state.mLastLoaderChunk);
    const int64_t reach = 16 * (int64_t) streamRadius(state, level) + 46;
    return dx * dx + dz * dz < reach * reach;
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

    const bool clientSideGeneration = isClientSideGeneration(owner);
    if (clientSideGeneration) {
        // ServerPlayer::tick moves the publisher before sending its queue.
        publishClientSideGeneration(state, owner, player);
    } else {
        const int64_t loaderChunk = packChunk(state.mComparatorChunkX, state.mComparatorChunkZ);
        if (loaderChunk != state.mLastLoaderChunk) {
            state.mLastLoaderChunk = loaderChunk;
            updateInRadiusChunks(state, streamRadius(state, owner.getLevelFor(player)), state.mComparatorChunkX,
                                 state.mComparatorChunkZ);
            removeOutOfRadiusChunks(state, owner, player);
            updateChunkSendingQueue(state, player);
        }
    }

    const int32_t attempts = player.hasSpawnChunksReady()
                                     ? (int32_t) owner.getChunksPerTick()
                                     : std::max((int32_t) owner.getChunksPerTick(), (int32_t) SPAWN_CHUNK_COUNT);
    loadQueuedChunks(state, owner, player, attempts);
    sendReadyChunks(state, owner, player);
    if (clientSideGeneration)
        recheckClientBuilt(state, owner, player);

    player.setLastChunkPosition(state.mComparatorChunkX, state.mComparatorChunkZ);
}

void ChunkStreamHandler::handleTeleport(ServerNetworkHandler &owner, ServerPlayer &player) {
    ChunkStreamState &state = player.getChunkStreamState();

    refreshComparatorContext(state, player);

    // The view moves with the next publisher update, which a teleport no longer makes wait for distance.
    if (isClientSideGeneration(owner)) {
        // the old view would keep sending and rearm the publisher delay; its chunks are never registered as loaded
        dropPendingChunks(state);
        state.mForcePublisherMove = true;
        publishClientSideGeneration(state, owner, player);
        loadQueuedChunks(state, owner, player, TELEPORT_LOAD_COUNT);
        sendReadyChunks(state, owner, player);
        return;
    }
    state.mLastLoaderChunk = packChunk(state.mComparatorChunkX, state.mComparatorChunkZ);

    // the full view around the destination: a short hop keeps what the client holds, tick() only redoes
    // this when the chunk changes; the destination's first chunks load now, not on the next tick
    updateInRadiusChunks(state, streamRadius(state, owner.getLevelFor(player)), state.mComparatorChunkX,
                         state.mComparatorChunkZ);
    removeOutOfRadiusChunks(state, owner, player);

    pruneLoadingQueueOutOfRadius(state);
    pruneQueueOutOfRadius(state, state.mReadyToSend, owner.getLevelFor(player), player.getRuntimeId());
    updateChunkSendingQueue(state, player);

    loadQueuedChunks(state, owner, player, TELEPORT_LOAD_COUNT);
    sendReadyChunks(state, owner, player);
}

void ChunkStreamHandler::invalidateChunk(ServerPlayer &player, int64_t hash) {
    ChunkStreamState &state = player.getChunkStreamState();

    // a sent chunk outside the radius stays in the sent set: leaving the radius still releases its loader
    if (state.mInRadius.find(hash) == state.mInRadius.end())
        return;

    if (player.getSentChunks().erase(hash) == 0)
        return;

    pushQueue(state, state.mReadyToSend, hash);
}

void ChunkStreamHandler::handleViewDistanceChange(ServerNetworkHandler &owner, ServerPlayer &player) {
    ChunkStreamState &state = player.getChunkStreamState();

    refreshComparatorContext(state, player);

    // The new radius reaches the view with the publisher update it makes due.
    if (isClientSideGeneration(owner))
        return;
    updateInRadiusChunks(state, streamRadius(state, owner.getLevelFor(player)), state.mComparatorChunkX,
                         state.mComparatorChunkZ);
    removeOutOfRadiusChunks(state, owner, player);
    pruneQueueOutOfRadius(state, state.mSendQueue, owner.getLevelFor(player), player.getRuntimeId());
    pruneQueueOutOfRadius(state, state.mReadyToSend, owner.getLevelFor(player), player.getRuntimeId());
    pruneLoadingQueueOutOfRadius(state);
    updateChunkSendingQueue(state, player);
}
