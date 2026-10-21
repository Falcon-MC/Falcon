#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Movement/PlayerMovementPipeline.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

void PlayerMovementSimulator::apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                    const PlayerAuthInputPacket &packet, Vector3f &feetPosition, bool sprinting) {
    auto &registry = owner.getPlayerMovementRegistry();
    const auto entity = player.getRuntimeId();
    registry.prepare(entity, packet, feetPosition, player.getMovementValidator(), sprinting);
    PlayerMovementContext context{owner, id, player, owner.getLevelFor(player), registry, entity};
    PlayerMovementPipeline::tick(context);
}
