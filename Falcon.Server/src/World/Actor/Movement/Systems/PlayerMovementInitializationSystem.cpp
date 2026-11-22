#include "World/Actor/Movement/Systems/PlayerMovementInitializationSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

using namespace MovementPhysics;

void PlayerMovementInitializationSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &prediction = context.prediction;

    prediction.strict = context.owner.getProperties().getServerAuthoritativeMovementStrict();
    if (!context.player.takeKnockback(state.velocity)
        && !context.player.takeServerMotion(input.getPacket().mTick, state.velocity))
        state.velocity = context.player.hasSimulatedVelocity() ? context.player.getSimulatedVelocity()
                                                               : context.player.getMotion();
    state.onGround = context.player.hasSimulatedGround() ? context.player.isSimulatedOnGround()
                                                         : context.player.isOnGround();
    state.jumpDelay = context.player.getJumpDelay();
    input.jumpHeld = input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::Jumping)
                     || input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::StartJumping)
                     || input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::AutoJumpingInWater)
                     || input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::AscendBlock);
    // movement (2) or jump (6) locked: no jump in the replay; a client that jumps anyway is corrected back
    if (context.player.isInputLocked(2) || context.player.isInputLocked(6))
        input.jumpHeld = false;
    // DecrementNoJumpDelaySystem runs before the jump systems
    if (state.jumpDelay > 0)
        --state.jumpDelay;
    if (!input.jumpHeld)
        state.jumpDelay = 0;
}
