#include "Item/Items/ThrowableItem.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

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
