#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>

class ItemStack;
class Level;
class MapCreateLockedCopyPacket;
class MapInfoRequestPacket;
class MapItemSavedData;
class ServerNetworkHandler;
class ServerPlayer;
class World;

// the server side of maps: carried maps, maps in frames and the map packets
class MapHandler {
public:
    // the inventory tick for maps, then the ClientboundMapItemDataPacket each carried map owes the player
    static void tickPlayer(ServerNetworkHandler &owner, ServerPlayer &player);

    // the tick of frames holding a map
    static void tickFrames(ServerNetworkHandler &owner, World &world);

    // ItemFrameBlockActor::setItem: a framed map follows its frame with a green marker
    static void onFrameItemChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const ItemStack &previous, const ItemStack &current);

    static void handleInfoRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                                  const MapInfoRequestPacket &packet);

    static void handleCreateLockedCopy(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const MapCreateLockedCopyPacket &packet);

    static void sendMap(ServerNetworkHandler &owner, ServerPlayer &player, MapItemSavedData &data);

    static void broadcastCreation(ServerNetworkHandler &owner, World &world, MapItemSavedData &data);
};
