#include "World/Actor/Movement/Systems/PlayerTraversalSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Block.h"
#include "World/Inventory/PlayerInventory.h"

#include <algorithm>

using namespace MovementPhysics;

void PlayerTraversalSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    const ItemStack &boots = context.player.getInventory().getArmor(PlayerInventory::ARMOR_FEET);
    const Block *inside = blockAt(context.level, state.start.x, state.start.y, state.start.z);
    BlockTraversal traversal = inside == nullptr ? BlockTraversal::None : inside->getTraversal();
    if (traversal == BlockTraversal::None && context.player.hasSupportingBlock()) {
        const Vector3i &support = context.player.getSupportingBlock();
        const Block *supporting = blockAt(context.level, (float) support.x, (float) support.y, (float) support.z);
        if (supporting != nullptr && supporting->getTraversal() == BlockTraversal::Scaffolding)
            traversal = BlockTraversal::Scaffolding;
    }
    context.autoClimb.scaffoldDescend = false;
    if (abilities.flying || abilities.gliding || context.waterTravel.active || context.liquidTravel.lava)
        traversal = BlockTraversal::None;
    if (traversal == BlockTraversal::Scaffolding) {
        if (input.pressingSneak) {
            state.velocity.y = -TRAVERSAL_SPEED;
            context.autoClimb.scaffoldDescend = true;
        } else if (input.jumpHeld) {
            state.velocity.y = TRAVERSAL_SPEED;
        }
    } else if (traversal == BlockTraversal::PowderSnow) {
        if (input.pressingSneak)
            state.velocity.y = -TRAVERSAL_SPEED;
        else if (input.jumpHeld && !boots.isAir() && boots.mDefinition->getIdentifier() == LEATHER_BOOTS)
            state.velocity.y = POWDER_SNOW_ASCEND_SPEED;
    }

    if (!abilities.flying && !abilities.gliding && inside != nullptr && inside->isClimbable() && traversal == BlockTraversal::None) {
        state.velocity.y = std::max(state.velocity.y, -CLIMB_SPEED);
        if (input.jumpHeld || context.player.hasHorizontalCollision())
            state.velocity.y = CLIMB_SPEED;
        if (context.pose.mSneaking && state.velocity.y < 0.0f)
            state.velocity.y = 0.0f;
    }
}
