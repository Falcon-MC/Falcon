#include "World/Actor/Movement/Systems/PlayerGroundJumpSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>

using namespace MovementPhysics;

namespace MovementPhysics {
    float jumpFactor(Level &level, const Vector3f &feet) {
        const Block *inside = blockAt(level, feet.x, feet.y, feet.z);
        if (inside != nullptr && inside->getJumpFactor() != 1.0f)
            return inside->getJumpFactor();
        const Block *below = blockAt(level, feet.x, feet.y - JUMP_PROBE_DEPTH, feet.z);
        return below == nullptr ? 1.0f : below->getJumpFactor();
    }
}

void PlayerGroundJumpSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    const float swimAmount = context.player.getSwimAmount().mCurrent;
    if (swimAmount > 0.0f && swimAmount < 1.0f)
        return;

    if (!abilities.flying && !abilities.gliding && !context.waterTravel.active && !context.liquidTravel.lava && input.jumpHeld && state.onGround && state.jumpDelay <= 0) {
        const MobEffectInstance *jumpBoost = context.player.getEffects().get(MobEffectId::JumpBoost);
        const float jumpBonus = jumpBoost == nullptr ? 0.0f : JUMP_BOOST_PER_LEVEL * (float) jumpBoost->level();
        const float jumpHeight = JUMP_VELOCITY + jumpBonus;
        state.velocity.y = std::max(jumpHeight * jumpFactor(context.level, state.start), state.velocity.y);
        state.jumpDelay = JUMP_DELAY_TICKS;
        if (abilities.sprinting) {
            // BDS 0x140EAFAF0 indexes the sine table directly in degrees.
            state.velocity.x -= mojangSinDegrees(input.getPacket().mRotation.y) * 0.2f;
            state.velocity.z += mojangCosDegrees(input.getPacket().mRotation.y) * 0.2f;
        }
    }
}
