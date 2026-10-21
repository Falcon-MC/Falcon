#include "World/Actor/Movement/Systems/BubbleColumnMovementSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"

#include <algorithm>

using namespace MovementPhysics;

void BubbleColumnMovementSystem::tick(PlayerMovementContext &context) {
    auto &state = context.state;
    auto &contact = context.contact;

    if (context.waterTravel.active && contact.liquid.bubble) {
        state.velocity.y = contact.liquid.dragDown ? std::max(-0.3f, state.velocity.y - 0.03f)
                                    : std::min(0.7f, state.velocity.y + 0.06f);
    }
}
