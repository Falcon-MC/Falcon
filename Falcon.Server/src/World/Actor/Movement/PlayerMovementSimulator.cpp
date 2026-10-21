#include "World/Actor/Movement/PlayerMovementSimulator.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Movement/PlayerMovementPipeline.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

void PlayerMovementSimulator::apply(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                    const PlayerAuthInputPacket &packet, Vector3f &feetPosition, bool sprinting) {
    PlayerMovementContext context{owner, id, player, owner.getLevelFor(player), {}, {packet}, {}, {},
                                  {feetPosition, player.getMovementValidator()}, {}};
    context.abilities.sprinting = sprinting;
    PlayerMovementPipeline::tick(context);
}
