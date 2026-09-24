#include "Item/Items/TridentItem.h"

#include "Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM_CUSTOM(TridentItem, 62,
                            [](const std::string &identifier) {
                                return identifier == "minecraft:trident";
                            },
                            [](const Item &item) -> std::unique_ptr<Item> {
                                return std::make_unique<TridentItem>(item);
                            });

#include "Actor/ServerActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Core/Math/MathConstants.h"
#include "Level/Level.h"
#include "Actor/ServerPlayer.h"
#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"
#include "Item/Items/RangedWeaponHelpers.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <cmath>

using namespace RangedWeaponHelpers;

namespace {
    const char *TRIDENT_ACTOR = "minecraft:thrown_trident";

    const float TRIDENT_MAX_FORCE = 2.5f;
    const float TRIDENT_BASE_DAMAGE = 8.0f;
    const int32_t TRIDENT_MINIMUM_TICKS = 5;
    const float RIPTIDE_GROUND_LIFT = 1.2f;
    const int32_t SPIN_ATTACK_TICKS = 20;
}

TridentItem::TridentItem(const Item &base) : Item(base) {
}

bool TridentItem::applyRiptide(ServerNetworkHandler &owner, ServerPlayer &player, int32_t level) const {
    Level &world = owner.getLevelFor(player);
    const bool raining = world.hasSkyLight() && owner.getLevel().isRaining();
    if (!LiquidBlocksFetch::at(world, player.getPosition()).water && !raining)
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
    owner.playLevelSound(owner.getLevelFor(player), sound, player.getPosition(), "minecraft:player");

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

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::TRIDENT_THROW, player.getPosition(),
                         "minecraft:player");

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
