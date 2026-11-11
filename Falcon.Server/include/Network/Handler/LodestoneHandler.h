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

    static void destroy(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    // sub_1437F93C0: the record goes to everyone, an unknown id back to the asker as NotFound
    static void handleRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                              const PositionTrackingDBClientRequestPacket &packet);

    // sub_1437F94F0: Update while valid, Destroy once destroyed, nothing while pending
    static void broadcast(ServerNetworkHandler &owner, World &world, const TrackingRecord &record);
};
