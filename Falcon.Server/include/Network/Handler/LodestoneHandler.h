#pragma once

#include "Core/Math/Vector3i.h"

class Level;
class PositionTrackingDBClientRequestPacket;
class ServerNetworkHandler;
class ServerPlayer;
class World;
struct TrackingRecord;

// lodestones and the compasses linked to them through the world's PositionTrackingDB
class LodestoneHandler {
public:
    // a compass becomes a lodestone compass pointing here; a lodestone compass is relinked
    static bool useCompassOn(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

    // the record of a removed lodestone is destroyed and the clients it concerns told
    static void release(ServerNetworkHandler &owner, Level &level, int32_t trackingId);

    // the record goes back to the asker if it carries the compass or sees the lodestone; NotFound otherwise
    static void handleRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                              const PositionTrackingDBClientRequestPacket &packet);

    // tracking ids are sequential: a player may know a record only when it carries the compass linked to it or
    // the lodestone sits in its loaded chunks, so a modified client cannot walk the ids to find others' lodestones
    static bool mayKnow(ServerNetworkHandler &owner, ServerPlayer &player, const TrackingRecord &record);

    // Update while valid, Destroy once destroyed, nothing while pending; only to the players the record concerns
    static void broadcast(ServerNetworkHandler &owner, World &world, const TrackingRecord &record);
};
