#include "Item/Items/ThrowableItems.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include "Item/ItemClassRegistry.h"

#include <utility>

namespace {
    struct ThrowableDefinition {
        const char *mIdentifier;
        const char *mProjectile;
        float mThrowForce;
        int32_t mCooldownTicks;
    };

    const ThrowableDefinition THROWABLES[] = {
            {"minecraft:snowball",           "minecraft:snowball",              1.5f, 0},
            {"minecraft:egg",                "minecraft:egg",                   1.5f, 0},
            {"minecraft:ender_pearl",        "minecraft:ender_pearl",           1.5f, 20},
            {"minecraft:experience_bottle",  "minecraft:xp_bottle",             1.0f, 0},
            {"minecraft:wind_charge",        "minecraft:wind_charge_projectile", 1.5f, 10},
            {"minecraft:ender_eye",          "minecraft:eye_of_ender_signal",   1.2f, 0}
    };

    const ThrowableDefinition *findThrowable(const std::string &identifier) {
        for (const ThrowableDefinition &definition: THROWABLES) {
            if (identifier == definition.mIdentifier)
                return &definition;
        }

        return nullptr;
    }

    const std::string SPAWN_EGG_SUFFIX = "_spawn_egg";

    std::string resolveSpawnedActor(const std::string &identifier) {
        if (identifier == "minecraft:villager")
            return "minecraft:villager_v2";
        if (identifier == "minecraft:zombie_villager")
            return "minecraft:zombie_villager_v2";
        return identifier;
    }
}

FALCON_REGISTER_ITEM_CUSTOM(ThrowableItem, 10,
                            [](const std::string &identifier) {
                                return findThrowable(identifier) != nullptr;
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                const ThrowableDefinition *definition = findThrowable(item.getIdentifier());
                                return std::make_unique<ThrowableItem>(item, definition->mProjectile,
                                                                       definition->mThrowForce,
                                                                       definition->mCooldownTicks);
                            });

FALCON_REGISTER_ITEM_CUSTOM(ThrownPotionItem, 70,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:splash_potion"
                                       || identifier == "minecraft:lingering_potion";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<ThrownPotionItem>(item, item.getIdentifier());
                            });

FALCON_REGISTER_ITEM_CUSTOM(SpawnEggItem, 90,
                            [](const std::string &identifier) {
                                return identifier.size() > SPAWN_EGG_SUFFIX.size()
                                       && identifier.compare(identifier.size() - SPAWN_EGG_SUFFIX.size(),
                                                             SPAWN_EGG_SUFFIX.size(), SPAWN_EGG_SUFFIX) == 0;
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<SpawnEggItem>(item);
                            });

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

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(),
                         "minecraft:player");

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
    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(),
                         "minecraft:player");

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

    if (owner.spawnActor(owner.getLevelFor(player), entityIdentifier, spawnPosition) == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}
