#include "World/Actor/Movement/Systems/FlyingPlayerStuckOnGroundWorkaroundSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

#include <limits>

void FlyingPlayerStuckOnGroundWorkaroundSystem::tick(PlayerMovementContext &context) {
    // BDS uses float bits 1 to release the retained ground contact without a visible lift.
    if (context.abilities.flying && context.state.onGround && context.state.velocity.y == 0.0f)
        context.state.velocity.y = std::numeric_limits<float>::denorm_min();
}
