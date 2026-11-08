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
    // Player::inventoryTick for maps, then the ClientboundMapItemDataPacket each carried map owes the player
    static void tickPlayer(ServerNetworkHandler &owner, ServerPlayer &player);

    // ItemFrameBlockActor::tick (sub_144C33490) for frames holding a map
    static void tickFrames(ServerNetworkHandler &owner, World &world);

    // ItemFrameBlockActor::setItem (sub_144C347D0): a framed map follows its frame with a green marker
    static void onFrameItemChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const ItemStack &previous, const ItemStack &current);

    // sub_14228D8A0
    static void handleInfoRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                                  const MapInfoRequestPacket &packet);

    // sub_14228D450
    static void handleCreateLockedCopy(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const MapCreateLockedCopyPacket &packet);

    static void sendMap(ServerNetworkHandler &owner, ServerPlayer &player, MapItemSavedData &data);

    static void broadcastCreation(ServerNetworkHandler &owner, World &world, MapItemSavedData &data);
};
