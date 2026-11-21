#include "World/Item/ThrownPotionItem.h"

#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Block/DispenseSource.h"

#include <utility>

FALCON_REGISTER_ITEM_CUSTOM(
        ThrownPotionItem, 70,
        [](const std::string &identifier) {
            return identifier == "minecraft:splash_potion" || identifier == "minecraft:lingering_potion";
        },
        [](const Item &item) -> std::unique_ptr<Item> {
            return std::make_unique<ThrownPotionItem>(item, item.getIdentifier());
        });

ThrownPotionItem::ThrownPotionItem(const Item &base, std::string entityIdentifier)
        : ThrowableItem(base, std::move(entityIdentifier), 1.5f, 0, DispenseMode::Shoot) {}

bool ThrownPotionItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    ServerActor *projectile = owner.spawnProjectile(player, mEntityIdentifier, mThrowForce,
                                                    ServerNetworkHandler::THROWN_PROJECTILE_DROP);
    if (projectile == nullptr)
        return false;

    if (PotionActor *potion = dynamic_cast<PotionActor *>(projectile))
        potion->setPotionId(item.mDamage);
    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(), "minecraft:player");

    player.consumeOneHeldItem();
    return true;
}

// splash and lingering: the stack's aux is the potion
bool ThrownPotionItem::dispense(const DispenseSource &source) const {
    const int32_t potionId = source.getItem().mDamage;
    ServerActor *projectile = _shootFrom(source);
    if (projectile == nullptr)
        return false;

    if (PotionActor *potion = dynamic_cast<PotionActor *>(projectile))
        potion->setPotionId(potionId);
    source.removeOne();
    source.broadcastShootEvent(DispenseSource::LEVEL_EVENT_SHOOT);
    return true;
}
