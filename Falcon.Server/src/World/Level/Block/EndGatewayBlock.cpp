#include "World/Level/Block/EndGatewayBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/EndGatewayBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(EndGatewayBlock, 6);

bool EndGatewayBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:end_gateway";
}

bool EndGatewayBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    // entities walk into the gateway, which has no collision box in the game
    shape = AxisAlignedBB(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    return true;
}

void EndGatewayBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                    const BlockState &state) const {
    (void) state;

    // a gateway placed without its block actor gets the one the game would have loaded without an exit
    BlockActorStore &blockActors = owner.getLevelFor(actor).getBlockActors();
    if (blockActors.find<EndGatewayBlockActor>(position) != nullptr)
        return;

    blockActors.getOrCreate<EndGatewayBlockActor>(position).requestExit();
}
