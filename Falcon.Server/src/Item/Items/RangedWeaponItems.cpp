#include "Item/Items/RangedWeaponItems.h"

#include "Actor/ServerActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Core/Math/MathConstants.h"
#include "Level/Level.h"
#include "Actor/ServerPlayer.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *ARROW_IDENTIFIER = "minecraft:arrow";
    const char *ARROW_ACTOR = "minecraft:arrow";
    const char *TRIDENT_ACTOR = "minecraft:thrown_trident";
    const char *CHARGED_ITEM_TAG = "chargedItem";
    const char *CHARGED_NAME_TAG = "Name";
    const char *CHARGED_LOAD_TICK_TAG = "LoadTick";
    const int64_t CROSSBOW_FIRE_DELAY_TICKS = 5;

    const float BOW_MAX_FORCE = 3.5f;
    const float TRIDENT_MAX_FORCE = 2.5f;
    const float CROSSBOW_FORCE = 3.5f;
    const float ARROW_BASE_DAMAGE = 2.0f;
    const float TRIDENT_BASE_DAMAGE = 8.0f;
    const int32_t BOW_MINIMUM_TICKS = 3;
    const int32_t TRIDENT_MINIMUM_TICKS = 5;
    const int32_t CROSSBOW_LOAD_TICKS = 25;
    const int32_t CROSSBOW_QUICK_CHARGE_TICKS = 5;
    const int32_t FLAME_ARROW_FIRE_TICKS = 45 * 60;
    const float RIPTIDE_GROUND_LIFT = 1.2f;
    const int32_t SPIN_ATTACK_TICKS = 20;

    bool hasFiniteResources(const ServerPlayer &player) {
        const int32_t gameType = player.getGameType();
        return gameType == (int32_t) GameType::Survival || gameType == (int32_t) GameType::Adventure;
    }

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

    bool isArrowStack(const ItemStack &stack) {
        return !stack.isAir() && stack.mDefinition != nullptr &&
               stack.mDefinition->getIdentifier() == ARROW_IDENTIFIER;
    }

    int findArrowSlot(const ServerPlayer &player) {
        const PlayerInventory &inventory = player.getInventory();
        for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
            if (isArrowStack(inventory.getItem(slot)))
                return slot;
        }
        return -1;
    }

    void consumeArrow(ServerPlayer &player, int slot) {
        PlayerInventory &inventory = player.getInventory();
        ItemStack arrow = inventory.getItem(slot);
        arrow.mCount -= 1;
        if (arrow.mCount <= 0)
            inventory.setItem(slot, ItemStack::air());
        else
            inventory.setItem(slot, std::move(arrow));

        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    }

    float chargeForce(int32_t elapsedTicks, float maxForce) {
        const float p = (float) elapsedTicks / 20.0f;
        return std::min((p * p + p * 2.0f) / 3.0f, 1.0f) * maxForce;
    }

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

bool isArrowProjectile(const std::string &identifier) {
    return identifier == ARROW_ACTOR || identifier == TRIDENT_ACTOR;
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

    owner.playLevelSound(LevelSoundEvent::BOW, player.getPosition(), "minecraft:player");

    if (finiteResources) {
        if (!data.mPickupCreativeOnly)
            consumeArrow(player, arrowSlot);
        owner.damagePlayerHeldItem(player, 1);
    }

    return true;
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

    owner.playLevelSound(LevelSoundEvent::CROSSBOW_SHOOT, player.getPosition(), "minecraft:player");
    return true;
}

bool CrossbowItem::onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    if (isCrossbowLoaded(item))
        return false;

    if (hasFiniteResources(player) && findArrowSlot(player) < 0)
        return false;

    const bool quickCharge = ItemEnchantments::getLevel(item, EnchantmentIds::QUICK_CHARGE) > 0;
    owner.playLevelSound(quickCharge ? LevelSoundEvent::CROSSBOW_QUICK_CHARGE_START
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

    owner.playLevelSound(quickCharge > 0 ? LevelSoundEvent::CROSSBOW_QUICK_CHARGE_END
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

TridentItem::TridentItem(const Item &base) : Item(base) {
}

bool TridentItem::applyRiptide(ServerNetworkHandler &owner, ServerPlayer &player, int32_t level) const {
    if (!LiquidBlocksFetch::at(owner.getLevel(), player.getPosition()).water && !owner.getLevel().isRaining())
        return false;

    const Vector3f rotation = player.getRotation();
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;

    float x = -std::sin(yaw) * std::cos(pitch);
    float y = -std::sin(pitch);
    float z = std::cos(yaw) * std::cos(pitch);
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.0f)
        return false;

    const float strength = 3.0f * ((1.0f + (float) level) / 4.0f);
    x *= strength / length;
    y *= strength / length;
    z *= strength / length;

    Vector3f motion = player.getMotion();
    motion.x += x;
    motion.y += y;
    motion.z += z;

    if (player.isOnGround())
        motion.y += RIPTIDE_GROUND_LIFT;

    player.setMotion(motion);
    owner.sendActorMotion(player);

    player.startSpinAttack(owner, SPIN_ATTACK_TICKS);

    const char *sound = level >= 3 ? LevelSoundEvent::TRIDENT_RIPTIDE_3
                                   : (level == 2 ? LevelSoundEvent::TRIDENT_RIPTIDE_2
                                                 : LevelSoundEvent::TRIDENT_RIPTIDE_1);
    owner.playLevelSound(sound, player.getPosition(), "minecraft:player");

    if (hasFiniteResources(player))
        owner.damagePlayerHeldItem(player, 1);

    return true;
}

bool TridentItem::onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) owner;
    (void) player;
    (void) item;
    return true;
}

bool TridentItem::onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                              int32_t elapsedTicks) const {
    const int32_t riptide = ItemEnchantments::getLevel(item, EnchantmentIds::RIPTIDE);
    if (riptide > 0) {
        if (elapsedTicks < TRIDENT_MINIMUM_TICKS)
            return false;
        return applyRiptide(owner, player, riptide);
    }

    const float force = chargeForce(elapsedTicks, TRIDENT_MAX_FORCE);
    if (force < 0.1f || elapsedTicks < TRIDENT_MINIMUM_TICKS)
        return false;

    ServerActor *trident = owner.spawnProjectile(player, TRIDENT_ACTOR, force);
    if (trident == nullptr)
        return false;

    ProjectileData &data = trident->getProjectileData();
    data.mBaseDamage = TRIDENT_BASE_DAMAGE;
    data.mLoyaltyLevel = ItemEnchantments::getLevel(item, EnchantmentIds::LOYALTY);
    data.mImpalingLevel = ItemEnchantments::getLevel(item, EnchantmentIds::IMPALING);
    data.mChanneling = ItemEnchantments::getLevel(item, EnchantmentIds::CHANNELING) > 0;
    data.mPickupItem = item;
    data.mPickupItem.mCount = 1;
    data.mFavoredSlot = player.getInventory().getSelectedSlot();

    owner.playLevelSound(LevelSoundEvent::TRIDENT_THROW, player.getPosition(), "minecraft:player");

    if (hasFiniteResources(player)) {
        PlayerInventory &inventory = player.getInventory();
        ItemStack held = inventory.getItemInHand();
        held.mCount -= 1;
        if (held.mCount <= 0)
            inventory.setItemInHand(ItemStack::air());
        else
            inventory.setItemInHand(std::move(held));

        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    return true;
}
