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
    state.onGround = context.player.hasSimulatedGround() ? context.player.isSimulatedOnGround()
                                                        : context.player.isOnGround();
    state.jumpDelay = context.player.getJumpDelay();
    input.jumpHeld = input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::Jumping)
                          || input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::StartJumping)
                          || input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::AutoJumpingInWater)
                          || input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::AscendBlock);
    // BDS DecrementNoJumpDelaySystem (0x140EB0DA0) runs before the jump systems.
    if (state.jumpDelay > 0)
        --state.jumpDelay;
    if (!input.jumpHeld)
        state.jumpDelay = 0;
}
