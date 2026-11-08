#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/ClientSideGeneration.h"

#include <chrono>
#include <cstddef>
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
    uint64_t mSendTick = UINT64_MAX;
    unsigned mSendsThisTick = 0;
    int32_t mComparatorChunkX = 0;
    int32_t mComparatorChunkZ = 0;
    double mComparatorDirX = 0.0;
    double mComparatorDirZ = 0.0;
    // Radius granted to the client in ChunkRadiusUpdated, 0 until it asks. The client drops chunks beyond it,
    // so streaming further marks chunks as sent that the client never keeps.
    int32_t mRadius = 0;

    // Client side generation only (NetworkChunkPublisher, sub_14255F580 and sub_1425606A0).
    ClientSideGeneration::SavedChunkList mSavedChunks;
    // Chunks in the radius the server leaves to the client. They are sent anyway once something else on the
    // server builds them, and decided again whenever the view moves.
    std::vector<int64_t> mClientBuilt;
    size_t mClientBuiltCursor = 0;
    bool mPublished = false;
    // A teleport moves the view without waiting for the player to travel PUBLISHER_MOVE_THRESHOLD blocks.
    bool mForcePublisherMove = false;
    bool mChunkSentSinceUpdate = false;
    std::chrono::steady_clock::time_point mLastChunkSent;
    Vector3i mPublisherPosition;
    uint32_t mPublisherRadius = 0;
};

/**
 * Streams chunks to one player: tracks which chunks are in view distance, requests missing
 * ones from the level and sends them in view-priority order as they become ready.
 */
class ChunkStreamHandler {
public:
    static constexpr int32_t FIELD_OF_VIEW_DEGREES = 70;
    static constexpr int32_t TELEPORT_LOAD_COUNT = 8;
    static constexpr unsigned MAX_SENDS_PER_TICK = 16;
    // pending outbound bytes before more chunks are encoded for this player
    static constexpr size_t MAX_QUEUED_CHUNK_BYTES = 256 * 1024;

    static void tick(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleTeleport(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleViewDistanceChange(ServerNetworkHandler &owner, ServerPlayer &player);

    static void invalidateChunk(ServerPlayer &player, int64_t hash);

    /**
     * Whether a NetworkChunkPublisherUpdatePacket is due under client side generation (sub_14255F580): never within
     * PUBLISHER_DELAY_NANOSECONDS of the last chunk sent since the previous update, and then when the player moved
     * far enough, the radius changed or chunks wait to be listed.
     */
    static bool isPublisherUpdateDue(const ChunkStreamState &state, const Vector3i &position, uint32_t radius,
                                     std::chrono::steady_clock::time_point now);

    /**
     * Forgets the chunks still waiting to be sent for the current view. Under client side generation a teleport
     * drops them, since each one sent would push the publisher update and the view move back.
     */
    static void dropPendingChunks(ChunkStreamState &state);

    static int64_t packChunk(int32_t chunkX, int32_t chunkZ);

    static int32_t unpackChunkX(int64_t hash);

    static int32_t unpackChunkZ(int64_t hash);
};
