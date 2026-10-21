#include "World/Actor/Movement/Systems/PlayerTravelPreparationSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerTravelPreparationSystem::tick(PlayerMovementContext &context) {
    auto &abilities = context.abilities;
    auto &travel = context.travel;

    travel.friction = AIR_FRICTION;
    travel.acceleration = abilities.sprinting ? SPRINT_AIR_SPEED : WALK_AIR_SPEED;
}
