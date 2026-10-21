#include "World/Actor/Movement/Systems/PlayerFlightSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

#include <algorithm>
#include <cmath>

using namespace MovementPhysics;

void PlayerFlightSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &travel = context.travel;

    if (abilities.flying) {
        // Same fly speed as the ability advertised by LoginHandler; BDS doubles it when sprinting.
        travel.acceleration = 0.05f * (abilities.sprinting ? 2.0f : 1.0f);
        const bool ascend = MovementPhysics::input(input.packet, PlayerAuthInputData::Ascend);
        const bool descend = MovementPhysics::input(input.packet, PlayerAuthInputData::Descend);
        if (input.packet.mInputMode == PlayerInputMode::Mouse
            && std::max(std::fabs(input.packet.mMotionX), std::fabs(input.packet.mMotionY)) < 0.01f
            && !ascend && !descend && !input.jumpHeld && !input.pressingSneak)
            state.velocity.y *= 0.375f;
        if (ascend && descend) {
            state.velocity.y = 0.0f;
        } else {
            state.velocity.y += (ascend ? 0.15f : 0.0f) + (input.jumpHeld ? 0.05f : 0.0f)
                          - (descend ? 0.22f : 0.0f) - (input.pressingSneak ? 0.15f : 0.0f);
        }
    }
}
