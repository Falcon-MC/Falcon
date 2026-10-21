#include "World/Actor/Movement/Systems/SwimControlSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void SwimControlSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    if (contact.waterTravel || contact.lavaTravel) {
        // BDS SwimControlSystem (0x140F4FB60), before movement and water drag.
        // Its ECS view excludes MobIsJumpingFlagComponent (0x140F54910).
        if (contact.waterTravel && abilities.swimming && !input.jumpHeld) {
            const float lookY = -mojangSin(input.packet.mRotation.x * MOJANG_PI / 180.0f);
            const float factor = lookY < -0.2f ? 0.085f : 0.06f;
            if (lookY <= 0.0f || contact.liquid.swimProbeInWater)
                state.velocity.y += (lookY - state.velocity.y) * factor;
            else
                state.velocity.y = 0.0f;
        }
    }
}
