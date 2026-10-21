#include "World/Actor/Movement/Systems/PlayerLiquidCurrentSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

using namespace MovementPhysics;

void PlayerLiquidCurrentSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &contact = context.contact;

    if (contact.waterTravel || contact.lavaTravel) {
        const float current = contact.waterTravel ? 0.014f : 0.0035f;
        state.velocity.x += contact.liquid.flow.x * current;
        state.velocity.y += contact.liquid.flow.y * current;
        state.velocity.z += contact.liquid.flow.z * current;
    }
}
