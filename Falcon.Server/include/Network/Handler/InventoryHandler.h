#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Types/HandSlot.h"

#include <cstdint>

class ServerNetworkHandler;
class ServerPlayer;
class NetworkIdentifier;
class MobEquipmentPacket;
class PlayerHotbarPacket;
class ItemStackRequestPacket;
class InventoryTransactionPacket;
class ContainerClosePacket;
class CraftingEventPacket;
class MobArmorEquipmentPacket;

// an item use from the offhand: the held item is the offhand until the scope ends, then the slot is synced
class ActiveHandScope {
public:
    ActiveHandScope(ServerNetworkHandler &owner, ServerPlayer &player, HandSlot hand, bool sync = true);

    ~ActiveHandScope();

    void syncOnExit() {
        mSync = true;
    }

    ActiveHandScope(const ActiveHandScope &) = delete;
    ActiveHandScope &operator=(const ActiveHandScope &) = delete;

private:
    ServerNetworkHandler &mOwner;
    ServerPlayer &mPlayer;
    bool mOffhand;
    bool mSync;
};

class InventoryHandler {
public:
    static void sendInventory(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendArmorContent(ServerNetworkHandler &owner, ServerPlayer &player);

    // the armor other players see: hidden pieces go out as air
    static void fillArmorEquipment(ServerNetworkHandler &owner, ServerPlayer &player, MobArmorEquipmentPacket &packet);

    static void setArmorHidden(ServerNetworkHandler &owner, ServerPlayer &player, bool hidden);

    static void revealHiddenArmor(ServerNetworkHandler &owner, ServerPlayer &player);

    static void tickHiddenArmor(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendOffhandContent(ServerNetworkHandler &owner, ServerPlayer &player);

    static void sendHeldItem(ServerNetworkHandler &owner, ServerPlayer &player);

    static void swapOffhand(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleMobEquipment(ServerNetworkHandler &owner, ServerPlayer &player, const MobEquipmentPacket &packet);

    static void handlePlayerHotbar(ServerNetworkHandler &owner, ServerPlayer &player, const PlayerHotbarPacket &packet);

    static void handleOpenInventory(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleItemStackRequest(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                       const ItemStackRequestPacket &packet);

    static void handleTransaction(ServerNetworkHandler &owner, ServerPlayer &player,
                                  const InventoryTransactionPacket &packet);

    // a click on an entity: false when it is gone, cancelled or ignores the click
    static bool interactWithEntity(ServerNetworkHandler &owner, ServerPlayer &player, uint64_t runtimeId,
                                   const Vector3f &clickPosition);

    static void handleContainerClose(ServerPlayer &player, const ContainerClosePacket &packet);

    static void resetInventory(ServerNetworkHandler &owner, ServerPlayer &player);

    static void handleCraftingEvent(ServerNetworkHandler &owner, ServerPlayer &player,
                                    const CraftingEventPacket &packet);

    static void _updateEnchantOptions(ServerNetworkHandler &owner, ServerPlayer &player);
};
