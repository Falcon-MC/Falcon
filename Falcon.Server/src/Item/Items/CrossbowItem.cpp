#include "Item/Items/CrossbowItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(CrossbowItem, 61,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:crossbow";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<CrossbowItem>(item);
                            });

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Item/Items/RangedWeaponHelpers.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <cmath>

using namespace RangedWeaponHelpers;

namespace {
    const char *CHARGED_ITEM_TAG = "chargedItem";
    const char *CHARGED_NAME_TAG = "Name";
    const char *CHARGED_LOAD_TICK_TAG = "LoadTick";
    const int64_t CROSSBOW_FIRE_DELAY_TICKS = 5;

    const float CROSSBOW_FORCE = 3.5f;
    const int32_t CROSSBOW_LOAD_TICKS = 25;
    const int32_t CROSSBOW_QUICK_CHARGE_TICKS = 5;

    bool isCrossbowLoaded(const ItemStack &item) {
        return item.mTag.getType() == Tag::Type::Compound && item.mTag.get(CHARGED_ITEM_TAG) != nullptr;
    }

    int64_t crossbowLoadTick(const ItemStack &item) {
        if (item.mTag.getType() != Tag::Type::Compound)
            return 0;

        const Tag *charged = item.mTag.get(CHARGED_ITEM_TAG);
        if (charged == nullptr || charged->getType() != Tag::Type::Compound)
            return 0;

        return (int64_t) charged->getInt(CHARGED_LOAD_TICK_TAG);
    }
}

CrossbowItem::CrossbowItem(const Item &base) : Item(base) {
}

bool CrossbowItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (!isCrossbowLoaded(item))
        return false;

    if (owner.getCurrentTick() - crossbowLoadTick(item) <= CROSSBOW_FIRE_DELAY_TICKS)
        return true;

    const int32_t multishot = ItemEnchantments::getLevel(item, EnchantmentIds::MULTISHOT);
    const int32_t piercing = ItemEnchantments::getLevel(item, EnchantmentIds::PIERCING);
    const int32_t shots = multishot > 0 ? 3 : 1;

    for (int32_t index = 0; index < shots; ++index) {
        ServerActor *arrow = owner.spawnProjectile(player, ARROW_ACTOR, CROSSBOW_FORCE);
        if (arrow == nullptr)
            continue;

        ProjectileData &data = arrow->getProjectileData();
        data.mBaseDamage = ARROW_BASE_DAMAGE;
        data.mCritical = true;
        data.mPiercingLevel = piercing;

        if (index > 0) {
            const float spread = index == 1 ? -0.17f : 0.17f;
            Vector3f motion = arrow->getMotion();
            const float x = motion.x * std::cos(spread) - motion.z * std::sin(spread);
            const float z = motion.x * std::sin(spread) + motion.z * std::cos(spread);
            motion.x = x;
            motion.z = z;
            arrow->setMotion(motion);
            owner.sendActorMotion(*arrow);
            data.mPickupCreativeOnly = true;
        }
    }

    ItemStack updated = item;
    if (updated.mTag.getType() == Tag::Type::Compound)
        updated.mTag.remove(CHARGED_ITEM_TAG);
    player.getInventory().setItemInHand(std::move(updated));
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                          player.getInventory().getSelectedSlot());

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::CROSSBOW_SHOOT, player.getPosition(),
                         "minecraft:player");
    return true;
}

bool CrossbowItem::onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (isCrossbowLoaded(item))
        return false;

    if (hasFiniteResources(player) && findArrowSlot(player) < 0)
        return false;

    const bool quickCharge = ItemEnchantments::getLevel(item, EnchantmentIds::QUICK_CHARGE) > 0;
    owner.playLevelSound(owner.getLevelFor(player),
                         quickCharge ? LevelSoundEvent::CROSSBOW_QUICK_CHARGE_START
                                     : LevelSoundEvent::CROSSBOW_LOADING_START,
                         player.getPosition(), "minecraft:player");
    return true;
}

void CrossbowItem::onUsingTick(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                               int32_t elapsedTicks) const {
    if (isCrossbowLoaded(item))
        return;

    const int32_t quickCharge = ItemEnchantments::getLevel(item, EnchantmentIds::QUICK_CHARGE);
    const int32_t requiredTicks = CROSSBOW_LOAD_TICKS - quickCharge * CROSSBOW_QUICK_CHARGE_TICKS;
    if (elapsedTicks < requiredTicks)
        return;

    const bool finiteResources = hasFiniteResources(player);
    const int arrowSlot = findArrowSlot(player);
    if (finiteResources && arrowSlot < 0)
        return;

    ItemStack updated = item;
    if (updated.mTag.getType() != Tag::Type::Compound)
        updated.mTag = Tag::ofCompound();

    Tag chargedItem = Tag::ofCompound();
    chargedItem.put(CHARGED_NAME_TAG, Tag::ofString(ARROW_IDENTIFIER));
    chargedItem.putInt(CHARGED_LOAD_TICK_TAG, (int32_t) owner.getCurrentTick());
    updated.mTag.put(CHARGED_ITEM_TAG, std::move(chargedItem));
    player.getInventory().setItemInHand(std::move(updated));
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                          player.getInventory().getSelectedSlot());

    if (finiteResources) {
        consumeArrow(player, arrowSlot);
        owner.damagePlayerHeldItem(player, 2);
    }

    owner.playLevelSound(owner.getLevelFor(player),
                         quickCharge > 0 ? LevelSoundEvent::CROSSBOW_QUICK_CHARGE_END
                                         : LevelSoundEvent::CROSSBOW_LOADING_END,
                         player.getPosition(), "minecraft:player");
}

bool CrossbowItem::onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                               int32_t elapsedTicks) const {
    (void) owner;
    (void) player;
    (void) item;
    (void) elapsedTicks;
    return true;
}
