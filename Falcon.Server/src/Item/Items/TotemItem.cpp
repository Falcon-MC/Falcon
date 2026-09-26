#include "Item/Items/TotemItem.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"

#include <utility>

namespace {
    const int32_t REGENERATION_DURATION = 40 * 20;
    const int32_t REGENERATION_AMPLIFIER = 1;
    const int32_t FIRE_RESISTANCE_DURATION = 40 * 20;
    const int32_t ABSORPTION_DURATION = 5 * 20;
    const int32_t ABSORPTION_AMPLIFIER = 1;
    const char *TOTEM_PARTICLE = "minecraft:totem_particle";

    MobEffectInstance makeEffect(MobEffectId id, int32_t duration, int32_t amplifier) {
        MobEffectInstance effect;
        effect.mId = id;
        effect.mDuration = duration;
        effect.mAmplifier = amplifier;
        return effect;
    }
}

bool TotemItem::matches(const ItemStack &item) {
    return !item.isAir() && item.mDefinition != nullptr && item.mDefinition->getIdentifier() == IDENTIFIER;
}

bool TotemItem::consume(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerInventory &inventory = player.getInventory();

    const bool inOffhand = matches(inventory.getOffhand());
    if (!inOffhand && !matches(inventory.getItemInHand()))
        return false;

    player.setFireTicks(0);
    player.setOnFire(false);
    player.getEffects().clear();
    player.setHealth(1.0f);

    player.addEffect(makeEffect(MobEffectId::Regeneration, REGENERATION_DURATION, REGENERATION_AMPLIFIER));
    player.addEffect(makeEffect(MobEffectId::FireResistance, FIRE_RESISTANCE_DURATION, 0));
    player.addEffect(makeEffect(MobEffectId::Absorption, ABSORPTION_DURATION, ABSORPTION_AMPLIFIER));
    player.syncEffects();

    if (inOffhand) {
        ItemStack totem = inventory.getOffhand();
        totem.mCount -= 1;
        inventory.setOffhand(totem.mCount <= 0 ? ItemStack::air() : std::move(totem));
        player.getInventoryManager().syncContents(InventoryManager::InventoryId::Offhand);
        InventoryHandler::sendOffhandContent(owner, player);
    } else {
        ItemStack totem = inventory.getItemInHand();
        totem.mCount -= 1;
        inventory.setItemInHand(totem.mCount <= 0 ? ItemStack::air() : std::move(totem));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    owner._broadcastEntityEvent(player, (uint8_t) EntityEventType::ConsumeTotem);
    Level &level = owner.getLevelFor(player);
    owner.spawnParticleEffect(level, TOTEM_PARTICLE, player.getPosition());
    owner.broadcastLevelEvent(level, LevelEventPacket::SoundTotemUsed, player.getPosition(), 0);
    owner._sendHealth(player);
    owner._sendEntityData(player);

    return true;
}
