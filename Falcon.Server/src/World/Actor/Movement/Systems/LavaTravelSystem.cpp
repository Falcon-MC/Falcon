#include "World/Actor/Movement/Systems/LavaTravelSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>

using namespace MovementPhysics;

void LavaTravelSystem::tick(PlayerMovementContext &context) {
    auto &contact = context.contact;
    auto &travel = context.travel;

    if (contact.lavaTravel) {
        travel.acceleration = std::max(0.0f, context.player.getAttributes().get("minecraft:lava_movement"));
        travel.friction = 0.5f;
    }
}
