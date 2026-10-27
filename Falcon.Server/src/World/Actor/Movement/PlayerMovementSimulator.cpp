#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Movement/PlayerMovementPipeline.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>

float PlayerMovementSimulator::groundAcceleration(const ServerPlayer &player, float friction, bool sprinting) {
    using namespace MovementPhysics;
    const float speed = std::max(0.0f, player.getAttributes().get("minecraft:movement"));
    const float ratio = (AIR_FRICTION * DEFAULT_BLOCK_FRICTION) / friction;
    const float movement = sprinting ? speed * SPRINT_SPEED_MULTIPLIER : speed;
    return ((movement * ratio) * ratio) * ratio;
}

void PlayerMovementSimulator::apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                    const PlayerAuthInputPacket &packet, Vector3f &feetPosition, bool sprinting) {
    auto &registry = owner.getPlayerMovementRegistry();
    const auto entity = player.getRuntimeId();
    registry.prepare(entity, packet, feetPosition, player.getMovementValidator(), sprinting);
    PlayerMovementContext context{owner, id, player, owner.getLevelFor(player), registry, entity};
    PlayerMovementPipeline::tick(context);
}
