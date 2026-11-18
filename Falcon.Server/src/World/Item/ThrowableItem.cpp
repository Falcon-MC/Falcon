#include "World/Item/ThrowableItem.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Block/DispenseSource.h"

#include <utility>

namespace {
    struct ThrowableDefinition {
        const char *mIdentifier;
        const char *mProjectile;
        float mThrowForce;
        int32_t mCooldownTicks;
        ThrowableItem::DispenseMode mDispenseMode;
    };

    using Mode = ThrowableItem::DispenseMode;

    // snowball, egg and experience bottle; the ender pearl keeps
    // Item::dispense; ice ball and wind charge are ComponentItems (26.60)
    const ThrowableDefinition THROWABLES[] = {
            {"minecraft:ice_ball", "minecraft:ice_ball", 1.0f, 0, Mode::ShootWithClick},
            {"minecraft:snowball", "minecraft:snowball", 1.5f, 0, Mode::Shoot},
            {"minecraft:egg", "minecraft:egg", 1.5f, 0, Mode::Shoot},
            {"minecraft:ender_pearl", "minecraft:ender_pearl", 1.5f, 20, Mode::None},
            {"minecraft:experience_bottle", "minecraft:xp_bottle", 1.0f, 0, Mode::Shoot},
            {"minecraft:wind_charge", "minecraft:wind_charge_projectile", 1.5f, 10, Mode::ShootWithClick}};

    const ThrowableDefinition *findThrowable(const std::string &identifier) {
        for (const ThrowableDefinition &definition: THROWABLES) {
            if (identifier == definition.mIdentifier)
                return &definition;
        }

        return nullptr;
    }
}

FALCON_REGISTER_ITEM_CUSTOM(
        ThrowableItem, 10, [](const std::string &identifier) { return findThrowable(identifier) != nullptr; },
        [](const Item &item) -> std::unique_ptr<Item> {
            const ThrowableDefinition *definition = findThrowable(item.getIdentifier());
            return std::make_unique<ThrowableItem>(item, definition->mProjectile, definition->mThrowForce,
                                                   definition->mCooldownTicks, definition->mDispenseMode);
        });

ThrowableItem::ThrowableItem(const Item &base, std::string entityIdentifier, float throwForce, int32_t cooldownTicks,
                             DispenseMode dispenseMode)
        : Item(base), mEntityIdentifier(std::move(entityIdentifier)), mThrowForce(throwForce),
          mCooldownTicks(cooldownTicks), mDispenseMode(dispenseMode) {}

bool ThrowableItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (player.hasItemCooldown(item, owner.getCurrentTick()))
        return true;

    ServerActor *projectile = owner.spawnProjectile(player, mEntityIdentifier, mThrowForce,
                                                    ServerNetworkHandler::THROWN_PROJECTILE_DROP);
    if (projectile == nullptr)
        return false;

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(), "minecraft:player");

    if (mCooldownTicks > 0)
        player.startItemCooldown(item, owner.getCurrentTick(), mCooldownTicks);

    player.consumeOneHeldItem();
    return true;
}

bool ThrowableItem::dispense(const DispenseSource &source) const {
    if (mDispenseMode == DispenseMode::None)
        return false;

    if (_shootFrom(source) == nullptr)
        return false;

    source.removeOne();
    if (mDispenseMode == DispenseMode::ShootWithClick)
        source.playClick();
    source.broadcastShootEvent(DispenseSource::LEVEL_EVENT_SHOOT);
    return true;
}

// the facing side, 0.1 up
ServerActor *ThrowableItem::_shootFrom(const DispenseSource &source) const {
    Vector3f direction = source.getFacingVector();
    direction.y += 0.1f;
    return source.mOwner.spawnDispensedProjectile(source.mLevel, mEntityIdentifier, source.mPosition, direction);
}
