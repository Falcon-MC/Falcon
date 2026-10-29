#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

class ServerNetworkHandler;
class ServerPlayer;

struct ChunkStreamState {
    std::unordered_set<int64_t> mInRadius;
    std::vector<int64_t> mSendQueue;
    std::unordered_set<int64_t> mLoading;
    std::vector<int64_t> mReadyToSend;
    int64_t mLastLoaderChunk = INT64_MAX;
    int32_t mComparatorChunkX = 0;
    int32_t mComparatorChunkZ = 0;
    double mComparatorDirX = 0.0;
    double mComparatorDirZ = 0.0;
};

/**
 * Streams chunks to one player: tracks which chunks are in view distance, requests missing
 * ones from the level and sends them in view-priority order as they become ready.
 */
class ChunkStreamHandler {
public:
    static constexpr int32_t FIELD_OF_VIEW_DEGREES = 70;
    static constexpr int32_t TELEPORT_LOAD_COUNT = 8;

    static void tick(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleTeleport(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleViewDistanceChange(ServerNetworkHandler &owner, ServerPlayer &player);

    static void invalidateChunk(ServerPlayer &player, int64_t hash);

    static int64_t packChunk(int32_t chunkX, int32_t chunkZ);

    static int32_t unpackChunkX(int64_t hash);

    static int32_t unpackChunkZ(int64_t hash);
};
