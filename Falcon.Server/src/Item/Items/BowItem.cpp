#include "Item/Items/BowItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(BowItem, 60,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:bow";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<BowItem>(item);
                            });

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Item/Items/RangedWeaponHelpers.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

using namespace RangedWeaponHelpers;

namespace {
    const float BOW_MAX_FORCE = 3.5f;
    const int32_t BOW_MINIMUM_TICKS = 3;
    const int32_t FLAME_ARROW_FIRE_TICKS = 45 * 60;

    void applyBowEnchantments(const ItemStack &bow, ProjectileData &data) {
        const int32_t power = ItemEnchantments::getLevel(bow, EnchantmentIds::POWER);
        if (power > 0)
            data.mBaseDamage += (float) power * 0.5f + 0.5f;

        data.mPunchLevel = ItemEnchantments::getLevel(bow, EnchantmentIds::PUNCH);

        if (ItemEnchantments::getLevel(bow, EnchantmentIds::FLAME) > 0)
            data.mFlameTicks = FLAME_ARROW_FIRE_TICKS;

        if (ItemEnchantments::getLevel(bow, EnchantmentIds::INFINITY_ENCHANTMENT) > 0)
            data.mPickupCreativeOnly = true;
    }
}

BowItem::BowItem(const Item &base) : Item(base) {
}

bool BowItem::onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) owner;
    (void) item;

    if (!hasFiniteResources(player))
        return true;

    return findArrowSlot(player) >= 0;
}

bool BowItem::onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                          int32_t elapsedTicks) const {
    const bool finiteResources = hasFiniteResources(player);
    const int arrowSlot = findArrowSlot(player);
    if (finiteResources && arrowSlot < 0)
        return false;

    const float force = chargeForce(elapsedTicks, BOW_MAX_FORCE);
    if (force < 0.1f || elapsedTicks < BOW_MINIMUM_TICKS)
        return false;

    ServerActor *arrow = owner.spawnProjectile(player, ARROW_ACTOR, force);
    if (arrow == nullptr)
        return false;

    ProjectileData &data = arrow->getProjectileData();
    data.mBaseDamage = ARROW_BASE_DAMAGE;
    data.mCritical = force >= BOW_MAX_FORCE;
    if (finiteResources && arrowSlot >= 0) {
        data.mPickupItem = player.getInventory().getItem(arrowSlot);
        data.mPickupItem.mCount = 1;
    }
    applyBowEnchantments(item, data);

    if (data.mPickupCreativeOnly)
        data.mPickupItem = ItemStack::air();

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::BOW, player.getPosition(), "minecraft:player");

    if (finiteResources) {
        if (!data.mPickupCreativeOnly)
            consumeArrow(player, arrowSlot);
        owner.damagePlayerHeldItem(player, 1);
    }

    return true;
}
