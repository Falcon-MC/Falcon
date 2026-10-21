#include "World/Actor/Movement/Systems/PlayerFrictionSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerFrictionSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &abilities = context.abilities;
    auto &contact = context.contact;
    auto &travel = context.travel;

    if (!abilities.gliding || context.waterTravel.active || context.liquidTravel.lava) {
        state.velocity.x *= travel.friction * travel.frictionModifier;
        state.velocity.z *= travel.friction * travel.frictionModifier;
    }
}
