#include "World/Item/PackShooter.h"

#include "Core/Math/MathConstants.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/Projectile/ProjectileComponent.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ItemNetworkIdTable.h"
#include "World/Item/RangedWeaponHelpers.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace RangedWeaponHelpers;

namespace {
    const float MAX_DRAW_TICKS = 1200.0f;
    const int32_t QUICK_CHARGE_TICKS_PER_LEVEL = 5;
    const float MULTISHOT_SPREAD_DEGREES = 10.0f;
    const int32_t FLAME_FIRE_TICKS = 100;
    // below this power the game shoots nothing
    const float MINIMUM_POWER = 0.1f;
    // the arrow item carries its projectile in code: the arrow entity, critical at full power only
    const float ARROW_MINIMUM_CRITICAL_POWER = 1.0f;
    // the event the game sends once a charge_on_draw shooter holds its charge
    const uint8_t ACTOR_EVENT_CHARGED = 74;
    const char *const CHARGED_ITEM_TAG = "chargedItem";

    struct Ammunition {
        ItemStack mStack;
        int mSlot = -1;
        bool mOffhand = false;
    };

    struct ProjectileSource {
        std::string mEntity;
        float mMinimumCriticalPower = 0.0f;
    };

    std::string identifierOf(const ItemStack &stack) {
        return stack.isAir() || stack.mDefinition == nullptr ? std::string() : stack.mDefinition->getIdentifier();
    }

    std::string entityOf(const std::string &projectileEntity) {
        return projectileEntity.substr(0, projectileEntity.find('<'));
    }

    // the ammunition's own minecraft:projectile; an item without one shoots nothing
    bool projectileOf(const std::string &item, ProjectileSource &out) {
        const CustomItemDefinition *custom = CustomContentRegistry::getInstance().findItem(item);
        if (custom != nullptr) {
            if (!custom->mHasProjectile)
                return false;
            out.mEntity = entityOf(custom->mProjectileEntity);
            out.mMinimumCriticalPower = custom->mMinimumCriticalPower;
            return true;
        }

        if (item == ARROW_IDENTIFIER) {
            out.mEntity = ARROW_ACTOR;
            out.mMinimumCriticalPower = ARROW_MINIMUM_CRITICAL_POWER;
            return true;
        }

        const ItemNetworkIdEntry *entry = ItemNetworkIdTable::find(item);
        const Tag *components = entry == nullptr ? nullptr : entry->mComponents.get("components");
        const Tag *projectile = components == nullptr ? nullptr : components->get("minecraft:projectile");
        if (projectile == nullptr)
            return false;

        out.mEntity = entityOf(projectile->getString("projectile_entity"));
        const Tag *critical = projectile->get("minimum_critical_power");
        out.mMinimumCriticalPower = critical == nullptr ? 0.0f : critical->asFloat();
        return !out.mEntity.empty();
    }

    // every offhand entry first, then every inventory entry; creative falls back on a use_in_creative entry
    bool findAmmunition(ServerNetworkHandler &owner, const ServerPlayer &player, const CustomItemDefinition &item,
                        Ammunition &out) {
        const PlayerInventory &inventory = player.getInventory();
        for (const CustomAmmunition &ammo: item.mAmmunition) {
            if (!ammo.mUseOffhand || identifierOf(inventory.getOffhand()) != ammo.mItem)
                continue;
            out.mStack = inventory.getOffhand();
            out.mSlot = OFFHAND_SLOT;
            out.mOffhand = true;
            return true;
        }

        for (const CustomAmmunition &ammo: item.mAmmunition) {
            if (!ammo.mSearchInventory)
                continue;
            for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
                if (identifierOf(inventory.getItem(slot)) != ammo.mItem)
                    continue;
                out.mStack = inventory.getItem(slot);
                out.mSlot = slot;
                return true;
            }
        }

        if (!player.isCreative())
            return false;

        for (const CustomAmmunition &ammo: item.mAmmunition) {
            if (!ammo.mUseInCreative)
                continue;
            out.mStack = owner.createItemStack(ammo.mItem, 1);
            return !out.mStack.isAir();
        }
        return false;
    }

    // the offhand always pays; with infinity the inventory pays only for a tipped arrow
    void consumeAmmunition(ServerNetworkHandler &owner, ServerPlayer &player, const Ammunition &ammo,
                           bool infinity) {
        if (player.isCreative())
            return;

        if (ammo.mOffhand) {
            consumeArrow(owner, player, OFFHAND_SLOT);
            return;
        }

        if (ammo.mSlot < 0)
            return;

        const bool tippedArrow = identifierOf(ammo.mStack) == ARROW_IDENTIFIER && ammo.mStack.mDamage > 0;
        if (infinity && !tippedArrow)
            return;
        consumeArrow(owner, player, ammo.mSlot);
    }

    int32_t maxUseDuration(const CustomItemDefinition &item) {
        return item.mHasUseModifiers ? (int32_t) std::lround(item.mUseDuration * 20.0f) : 0;
    }

    // quick charge shortens the draw of a charge_on_draw shooter only
    int32_t shooterUseDuration(const CustomItemDefinition &item, const ItemStack &held) {
        const int32_t duration = maxUseDuration(item);
        if (!item.mChargeOnDraw)
            return duration;
        const int32_t quickCharge = ItemEnchantments::getLevel(held, EnchantmentIds::QUICK_CHARGE);
        return quickCharge > 0 ? std::max(duration - QUICK_CHARGE_TICKS_PER_LEVEL * quickCharge, 0) : duration;
    }

    float drawRatio(int32_t elapsedTicks, int32_t drawTicks) {
        const float ratio = (float) elapsedTicks / (float) drawTicks;
        return (ratio + ratio + ratio * ratio) / 3.0f;
    }

    int32_t shooterDrawTicks(const CustomItemDefinition &item) {
        return (int32_t) std::min(MAX_DRAW_TICKS, item.mShooterMaxDrawDuration * 20.0f);
    }

    // a zero draw divides by zero in the game: the shot goes at full power
    float shooterPower(const CustomItemDefinition &item, int32_t elapsedTicks) {
        if (!item.mShooterScalePowerByDraw && !item.mChargeOnDraw)
            return 1.0f;
        const int32_t drawTicks = shooterDrawTicks(item);
        if (drawTicks <= 0)
            return 1.0f;
        return std::min(drawRatio(elapsedTicks, drawTicks), 1.0f);
    }

    // speed is the entity's own projectile power times the launch power
    ServerActor *launch(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &entity, float power,
                        float minimumCriticalPower, float verticalOffset) {
        ProjectileComponent component;
        const bool found = ProjectileComponent::find(
                entity, CustomContentRegistry::getInstance().getActorDefinition(entity), component);
        ServerActor *projectile = owner.spawnProjectile(player, entity, found ? component.mPower * power : 0.0f,
                                                        verticalOffset);
        if (projectile != nullptr)
            projectile->getProjectileData().mCritical = minimumCriticalPower >= 0.0f && power >= minimumCriticalPower;
        return projectile;
    }

    void applyEnchantments(const ItemStack &weapon, ProjectileData &data) {
        const int32_t power = ItemEnchantments::getLevel(weapon, EnchantmentIds::POWER);
        if (power > 0)
            data.mBaseDamage += (float) power * 0.5f + 0.5f;
        data.mPunchLevel = ItemEnchantments::getLevel(weapon, EnchantmentIds::PUNCH);
        data.mPiercingLevel = ItemEnchantments::getLevel(weapon, EnchantmentIds::PIERCING);
        if (ItemEnchantments::getLevel(weapon, EnchantmentIds::FLAME) > 0)
            data.mFlameTicks = std::max(data.mFlameTicks, FLAME_FIRE_TICKS);
    }

    void turnMotion(ServerNetworkHandler &owner, ServerActor &projectile, float degrees) {
        const float radians = degrees * MathConstants::DEGREES_TO_RADIANS_F;
        Vector3f motion = projectile.getMotion();
        const float x = motion.x * std::cos(radians) - motion.z * std::sin(radians);
        const float z = motion.x * std::sin(radians) + motion.z * std::cos(radians);
        motion.x = x;
        motion.z = z;
        projectile.setMotion(motion);
        owner.sendActorMotion(projectile);
    }

    // multishot fires three at -10, 0 and 10 degrees for one ammunition; the weapon wears by one
    void shoot(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item,
               int32_t elapsedTicks, const Ammunition *charged) {
        const float power = shooterPower(item, elapsedTicks);
        if (power < MINIMUM_POWER)
            return;

        Ammunition ammo;
        if (charged != nullptr)
            ammo = *charged;
        else if (!findAmmunition(owner, player, item, ammo))
            return;

        ProjectileSource source;
        if (!projectileOf(identifierOf(ammo.mStack), source))
            return;

        const ItemStack weapon = player.getInventory().getItemInHand();
        owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::BOW, player.getPosition(), "minecraft:player");

        const bool multishot = ItemEnchantments::getLevel(weapon, EnchantmentIds::MULTISHOT) > 0;
        const int32_t shots = multishot ? 3 : 1;
        for (int32_t index = 0; index < shots; ++index) {
            ServerActor *projectile = launch(owner, player, source.mEntity, power, source.mMinimumCriticalPower, 0.0f);
            if (projectile == nullptr)
                continue;

            if (multishot)
                turnMotion(owner, *projectile, (float) index * MULTISHOT_SPREAD_DEGREES - MULTISHOT_SPREAD_DEGREES);

            ProjectileData &data = projectile->getProjectileData();
            if (dynamic_cast<ArrowActor *>(projectile) != nullptr) {
                ItemStack pickup = ammo.mStack;
                pickup.mCount = 1;
                data.mBaseDamage = ARROW_BASE_DAMAGE;
                data.mPotionId = pickup.mDamage - 1;
                data.mPickupItem = pickup;
                data.mPickupCreativeOnly = player.isCreative();
            }
            applyEnchantments(weapon, data);
        }

        owner.damagePlayerHeldItem(player, 1);
        if (!item.mChargeOnDraw)
            consumeAmmunition(owner, player, ammo,
                              ItemEnchantments::getLevel(weapon, EnchantmentIds::INFINITY_ENCHANTMENT) > 0);
    }

    const Tag *chargedItemOf(const ItemStack &stack) {
        if (stack.mTag.getType() != Tag::Type::Compound)
            return nullptr;
        const Tag *charged = stack.mTag.get(CHARGED_ITEM_TAG);
        return charged != nullptr && charged->getType() == Tag::Type::Compound ? charged : nullptr;
    }

    void setHeld(ServerPlayer &player, ItemStack stack) {
        PlayerInventory &inventory = player.getInventory();
        inventory.setItemInHand(std::move(stack));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }

    // the charge is the ammunition itself, saved on the weapon as the game saves a stack
    void charge(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item) {
        Ammunition ammo;
        if (!findAmmunition(owner, player, item, ammo))
            return;

        const ItemStack weapon = player.getInventory().getItemInHand();
        consumeAmmunition(owner, player, ammo,
                          ItemEnchantments::getLevel(weapon, EnchantmentIds::INFINITY_ENCHANTMENT) > 0);

        ItemStack loaded = player.getInventory().getItemInHand();
        ItemStack chargedItem = ammo.mStack;
        chargedItem.mCount = 1;
        if (loaded.mTag.getType() != Tag::Type::Compound)
            loaded.mTag = Tag::ofCompound();
        loaded.mTag.put(CHARGED_ITEM_TAG, ItemStackNbt::write(chargedItem));
        setHeld(player, std::move(loaded));
        owner._broadcastEntityEvent(player, ACTOR_EVENT_CHARGED, 0);
    }

    void fireCharge(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item,
                    const Tag &charged) {
        Ammunition ammo;
        ammo.mStack = ItemStackNbt::read(charged, owner.getCodecContext());
        if (!ammo.mStack.isAir())
            shoot(owner, player, item, shooterUseDuration(item, player.getInventory().getItemInHand()), &ammo);

        ItemStack fired = player.getInventory().getItemInHand();
        if (fired.isAir() || chargedItemOf(fired) == nullptr)
            return;
        fired.mTag.remove(CHARGED_ITEM_TAG);
        setHeld(player, std::move(fired));
    }

    bool drawsThrow(const CustomItemDefinition &item) {
        return item.mHasThrowable && item.mHasProjectile && item.mMaxDrawDuration > 0.0f;
    }

    // a zero max_draw_duration divides by zero in the game: the draw counts as full
    float throwPower(const CustomItemDefinition &item, int32_t elapsedTicks) {
        float ratio = 1.0f;
        const int32_t drawTicks = (int32_t) (item.mMaxDrawDuration * 20.0f);
        if (item.mScalePowerByDraw && drawTicks > 0)
            ratio = std::min(1.0f, drawRatio(elapsedTicks, drawTicks));
        return std::min(ratio * item.mLaunchPowerScale, item.mMaxLaunchPower);
    }

    void throwItem(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item, float power) {
        if (launch(owner, player, entityOf(item.mProjectileEntity), power, item.mMinimumCriticalPower,
                   ServerNetworkHandler::THROWN_PROJECTILE_DROP)
            == nullptr)
            return;
        owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(),
                             "minecraft:player");
        player.consumeOneHeldItem();
    }
}

PackShooter::Use PackShooter::use(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item,
                                  int32_t &duration) {
    const ItemStack held = player.getInventory().getItemInHand();
    if (item.mHasShooter) {
        const Tag *charged = item.mChargeOnDraw ? chargedItemOf(held) : nullptr;
        if (charged != nullptr) {
            fireCharge(owner, player, item, *charged);
            return Use::Done;
        }

        // no ammunition: survival does not draw; creative or an empty list still does
        Ammunition ammo;
        if (player.isCreative() || item.mAmmunition.empty() || findAmmunition(owner, player, item, ammo)) {
            if (std::min(MAX_DRAW_TICKS, item.mShooterMaxDrawDuration * 20.0f) <= 0.0f) {
                shoot(owner, player, item, shooterUseDuration(item, held), nullptr);
                return Use::Done;
            }
            duration = shooterUseDuration(item, held);
            return Use::Drawing;
        }
    }

    if (!item.mHasThrowable || !item.mHasProjectile)
        return Use::None;

    if (drawsThrow(item)) {
        duration = maxUseDuration(item);
        return Use::Drawing;
    }

    throwItem(owner, player, item, throwPower(item, maxUseDuration(item)));
    return Use::Done;
}

bool PackShooter::release(ServerNetworkHandler &owner, ServerPlayer &player, const CustomItemDefinition &item,
                          int32_t elapsedTicks) {
    const bool drawnThrow = drawsThrow(item);
    if (!item.mHasShooter && !drawnThrow)
        return false;

    const ItemStack held = player.getInventory().getItemInHand();
    if (item.mHasShooter) {
        const int32_t duration = shooterUseDuration(item, held);
        const int32_t elapsed = std::max(0, std::min(elapsedTicks, duration));
        // a draw under one tick divides by zero in the game: any held tick charges
        const int32_t drawTicks = shooterDrawTicks(item);
        if (!item.mChargeOnDraw)
            shoot(owner, player, item, elapsed, nullptr);
        else if (drawTicks > 0 ? drawRatio(elapsed, drawTicks) >= 1.0f : elapsed > 0)
            charge(owner, player, item);
    }

    if (drawnThrow) {
        const int32_t elapsed = std::max(0, std::min(elapsedTicks, maxUseDuration(item)));
        const float power = throwPower(item, elapsed);
        if (elapsed >= (int32_t) (item.mMinDrawDuration * 20.0f) && power >= MINIMUM_POWER)
            throwItem(owner, player, item, power);
    }
    return true;
}
