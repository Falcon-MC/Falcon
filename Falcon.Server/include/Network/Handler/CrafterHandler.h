#pragma once

class PlayerToggleCrafterSlotRequestPacket;
class ServerNetworkHandler;
class ServerPlayer;

// the server side of the crafter window
class CrafterHandler {
public:
    // only for the crafter whose window the player has open
    static void handleToggleSlot(ServerNetworkHandler &owner, ServerPlayer &player,
                                 const PlayerToggleCrafterSlotRequestPacket &packet);
};
