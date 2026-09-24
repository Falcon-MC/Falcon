#include "Item/Items/ThrownPotionItem.h"

#include "Actor/Projectile/ProjectileActor.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include "Item/ItemClassRegistry.h"

#include <utility>

FALCON_REGISTER_ITEM_CUSTOM(ThrownPotionItem, 70,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:splash_potion"
                                       || identifier == "minecraft:lingering_potion";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<ThrownPotionItem>(item, item.getIdentifier());
                            });

ThrownPotionItem::ThrownPotionItem(const Item &base, std::string entityIdentifier)
        : ThrowableItem(base, std::move(entityIdentifier), 1.5f, 0) {}

bool ThrownPotionItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    ServerActor *projectile = owner.spawnProjectile(player, mEntityIdentifier, mThrowForce,
                                                    ServerNetworkHandler::THROWN_PROJECTILE_DROP);
    if (projectile == nullptr)
        return false;

    if (PotionActor *potion = dynamic_cast<PotionActor *>(projectile))
        potion->setPotionId(item.mDamage);
    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(),
                         "minecraft:player");

    player.consumeOneHeldItem();
    return true;
}
