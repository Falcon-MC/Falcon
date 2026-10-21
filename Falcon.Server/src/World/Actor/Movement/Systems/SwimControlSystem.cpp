#include "World/Actor/Movement/Systems/SwimControlSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void SwimControlSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    // BDS SwimControlSystem (0x140F4FB60) requires Swimming and excludes MobIsJumping,
    // but does not require WasInWater: control also runs on the surface-exit tick.
    if (abilities.swimming && !input.jumpHeld) {
        const float lookY = mojangSinDegrees(-input.packet.mRotation.x);
        const float factor = lookY < -0.2f ? 0.085f : 0.06f;
        if (lookY <= 0.0f || contact.liquid.swimProbeInWater)
            state.velocity.y += (lookY - state.velocity.y) * factor;
        else
            state.velocity.y = 0.0f;
    }
}
