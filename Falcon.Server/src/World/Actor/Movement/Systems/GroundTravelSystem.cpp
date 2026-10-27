#include "World/Actor/Movement/Systems/GroundTravelSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Block.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"

#include <algorithm>

using namespace MovementPhysics;

namespace MovementPhysics {
    float blockFriction(const Block *ground) {
        if (ground == nullptr)
            return DEFAULT_BLOCK_FRICTION;
        const float friction = ground->getFrictionFactor();
        return friction > 0.0f ? friction : DEFAULT_BLOCK_FRICTION;
    }
}

void GroundTravelSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    const ItemStack &boots = context.player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    if (!abilities.flying && !context.waterTravel.active && !context.liquidTravel.lava && state.onGround && !abilities.gliding) {
        const Block *ground = blockAt(context.level, state.start.x, state.start.y - GROUND_PROBE_DEPTH, state.start.z);
        const float groundFriction = blockFriction(ground);
        travel.friction *= groundFriction;
        float accelerationMultiplier = ground == nullptr ? 1.0f : ground->getAccelerationFrictionMultiplier();
        if (ItemEnchantments::getLevel(boots, EnchantmentIds::SOUL_SPEED) > 0)
            accelerationMultiplier = 1.0f;
        // Ground acceleration scales with the cube of (default friction / block friction), so
        // slippery blocks such as ice accelerate slowly but keep their momentum.
        const float accelerationFriction = (groundFriction * accelerationMultiplier) * AIR_FRICTION;
        travel.acceleration = PlayerMovementSimulator::groundAcceleration(context.player, accelerationFriction,
                                                                          abilities.sprinting);
    }
}
