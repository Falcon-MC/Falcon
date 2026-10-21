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
    if (!context.player.takeKnockback(state.velocity))
        state.velocity = context.player.hasSimulatedVelocity() ? context.player.getSimulatedVelocity() : context.player.getMotion();
    state.onGround = context.player.isOnGround();
    state.jumpDelay = context.player.getJumpDelay();
    input.jumpHeld = input.packet.hasInputFlag((int32_t) PlayerAuthInputData::Jumping)
                          || input.packet.hasInputFlag((int32_t) PlayerAuthInputData::StartJumping);
    input.startJumping = input.packet.hasInputFlag((int32_t) PlayerAuthInputData::StartJumping);
    if (!input.jumpHeld)
        state.jumpDelay = 0;
}
