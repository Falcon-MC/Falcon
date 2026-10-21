#include "World/Actor/Movement/Systems/PlayerLiquidCurrentSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerLiquidCurrentSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &contact = context.contact;

    if (context.waterTravel.active || context.liquidTravel.lava) {
        const float current = context.waterTravel.active ? 0.014f : 0.0035f;
        state.velocity.x += contact.liquid.flow.x * current;
        state.velocity.y += contact.liquid.flow.y * current;
        state.velocity.z += contact.liquid.flow.z * current;
    }
}
