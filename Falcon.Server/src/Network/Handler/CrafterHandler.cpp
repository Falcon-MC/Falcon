#include "Network/Handler/CrafterHandler.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerToggleCrafterSlotRequestPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

void CrafterHandler::handleToggleSlot(ServerNetworkHandler &owner, ServerPlayer &player,
                                      const PlayerToggleCrafterSlotRequestPacket &packet) {
    const InventoryManager &manager = player.getInventoryManager();
    if (!manager.isContainerOpen() || manager.getCurrentWindowType() != ContainerType::Crafter
        || !(manager.getContainerPosition() == packet.mBlockPosition))
        return;

    Level &level = owner.getLevelFor(player);
    CrafterBlockActor *crafter = level.getBlockActors().find<CrafterBlockActor>(packet.mBlockPosition);
    if (crafter == nullptr || !crafter->setSlotDisabled(packet.mSlot, packet.mDisabled))
        return;

    BlockActionHandler::broadcastBlockActorData(owner, level, *crafter);
    RedstoneSystem::queueRedstoneNotification(level, packet.mBlockPosition);
}
