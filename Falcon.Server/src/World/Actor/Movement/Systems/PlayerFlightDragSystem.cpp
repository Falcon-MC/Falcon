#include "World/Actor/Movement/Systems/PlayerFlightDragSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerFlightDragSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;

    if (abilities.flying)
        state.velocity.y *= 0.6f;
}
