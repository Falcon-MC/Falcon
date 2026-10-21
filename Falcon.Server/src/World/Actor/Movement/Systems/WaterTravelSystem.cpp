#include "World/Actor/Movement/Systems/WaterTravelSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"

#include <algorithm>

using namespace MovementPhysics;

void WaterTravelSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    const ItemStack &boots = context.player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    if (context.waterTravel.active) {
        const float depth = (float) std::clamp(ItemEnchantments::getLevel(boots, EnchantmentIds::DEPTH_STRIDER), 0, 3)
                            * (state.onGround ? 1.0f : 0.5f);
        const float movement = std::max(0.0f, context.player.getAttributes().get("minecraft:movement"))
                               * (abilities.sprinting ? SPRINT_SPEED_MULTIPLIER : 1.0f);
        travel.acceleration = std::max(0.0f, context.player.getAttributes().get("minecraft:underwater_movement"));
        travel.acceleration += (movement - travel.acceleration) * depth / 3.0f;
        travel.friction = abilities.sprinting ? 0.9f : 0.8f;
        travel.friction += depth / 3.0f * (0.54600006f - travel.friction);
    }
}
