#include "World/Actor/Movement/Systems/PlayerMoveRelativeSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerMoveRelativeSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    if (!abilities.gliding || contact.waterTravel || contact.lavaTravel)
        moveRelative(state.velocity, input.sideways, input.forward,
                     travel.acceleration, input.packet.mRotation.y);
}
