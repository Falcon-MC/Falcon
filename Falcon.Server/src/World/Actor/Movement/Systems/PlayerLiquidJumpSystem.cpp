#include "World/Actor/Movement/Systems/PlayerLiquidJumpSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerLiquidJumpSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;

    if (contact.waterTravel || contact.lavaTravel) {
        // MobJumpSystem (0x140EB1110) suppresses the water jump while swimming with the head out.
        if (input.jumpHeld) {
            if (contact.waterTravel && abilities.swimming && !contact.liquid.eyeSubmerged)
                state.velocity.y = 0.0f;
            else
                state.velocity.y += 0.04f;
        }
    }
}
