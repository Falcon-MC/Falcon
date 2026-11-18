#include "World/Actor/Movement/Systems/SwimControlSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void SwimControlSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    // the game's view requires WasInWater, excludes MobIsJumping
    if (context.waterTravel.active && abilities.swimming && !input.jumpHeld) {
        const float lookY = mojangSinDegrees(-input.getPacket().mRotation.x);
        const float factor = lookY < -0.2f ? 0.085f : 0.06f;
        if (lookY <= 0.0f || contact.liquid.swimProbeInWater)
            state.velocity.y += (lookY - state.velocity.y) * factor;
        else
            state.velocity.y = 0.0f;
    }
}
