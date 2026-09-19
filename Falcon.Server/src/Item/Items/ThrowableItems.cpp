#include "Item/Items/ThrowableItems.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <utility>

namespace {
    const std::string SPAWN_EGG_SUFFIX = "_spawn_egg";

    std::string resolveSpawnedActor(const std::string &identifier) {
        if (identifier == "minecraft:villager")
            return "minecraft:villager_v2";
        if (identifier == "minecraft:zombie_villager")
            return "minecraft:zombie_villager_v2";
        return identifier;
    }
}

ThrowableItem::ThrowableItem(const Item &base, std::string entityIdentifier, float throwForce,
                             int32_t cooldownTicks)
        : Item(base), mEntityIdentifier(std::move(entityIdentifier)), mThrowForce(throwForce),
          mCooldownTicks(cooldownTicks) {}

bool ThrowableItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (player.hasItemCooldown(item, owner.getCurrentTick()))
        return true;

    ServerActor *projectile = owner.spawnProjectile(player, mEntityIdentifier, mThrowForce,
                                                    ServerNetworkHandler::THROWN_PROJECTILE_DROP);
    if (projectile == nullptr)
        return false;

    owner.playLevelSound(LevelSoundEvent::THROW, player.getPosition(), "minecraft:player");

    if (mCooldownTicks > 0)
        player.startItemCooldown(item, owner.getCurrentTick(), mCooldownTicks);

    player.consumeOneHeldItem();
    return true;
}

ThrownPotionItem::ThrownPotionItem(const Item &base, std::string entityIdentifier)
        : ThrowableItem(base, std::move(entityIdentifier), 1.5f, 0) {}

bool ThrownPotionItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    ServerActor *projectile = owner.spawnProjectile(player, mEntityIdentifier, mThrowForce,
                                                    ServerNetworkHandler::THROWN_PROJECTILE_DROP);
    if (projectile == nullptr)
        return false;

    owner.setProjectilePotionData(projectile->getUniqueId(), item.mDamage);
    owner.playLevelSound(LevelSoundEvent::THROW, player.getPosition(), "minecraft:player");

    player.consumeOneHeldItem();
    return true;
}

SpawnEggItem::SpawnEggItem(const Item &base) : Item(base) {}

bool SpawnEggItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                const Vector3i &blockPosition, int32_t face,
                                const Vector3f &clickPosition) const {
    (void) clickPosition;

    if (item.mDefinition == nullptr)
        return false;

    const std::string &identifier = item.mDefinition->getIdentifier();
    if (identifier.size() <= SPAWN_EGG_SUFFIX.size() ||
        identifier.compare(identifier.size() - SPAWN_EGG_SUFFIX.size(), SPAWN_EGG_SUFFIX.size(),
                           SPAWN_EGG_SUFFIX) != 0)
        return false;

    const std::string entityIdentifier =
            resolveSpawnedActor(identifier.substr(0, identifier.size() - SPAWN_EGG_SUFFIX.size()));

    Vector3f spawnPosition((float) blockPosition.x + 0.5f, (float) blockPosition.y,
                           (float) blockPosition.z + 0.5f);
    switch (face) {
        case 0:
            spawnPosition.y -= 1.0f;
            break;
        case 1:
            spawnPosition.y += 1.0f;
            break;
        case 2:
            spawnPosition.z -= 1.0f;
            break;
        case 3:
            spawnPosition.z += 1.0f;
            break;
        case 4:
            spawnPosition.x -= 1.0f;
            break;
        case 5:
            spawnPosition.x += 1.0f;
            break;
        default:
            spawnPosition.y += 1.0f;
            break;
    }

    if (owner.spawnActor(entityIdentifier, spawnPosition) == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}

bool isThrownProjectile(const std::string &identifier) {
    return identifier == "minecraft:snowball" || identifier == "minecraft:egg" ||
           identifier == "minecraft:ender_pearl" || identifier == "minecraft:xp_bottle" ||
           identifier == "minecraft:splash_potion" || identifier == "minecraft:lingering_potion" ||
           identifier == "minecraft:wind_charge_projectile" || identifier == "minecraft:eye_of_ender_signal";
}
