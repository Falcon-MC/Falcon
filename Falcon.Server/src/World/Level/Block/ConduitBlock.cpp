#include "World/Level/Block/ConduitBlock.h"

#include "World/Level/Block/Actor/ConduitBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(ConduitBlock, 200);

bool ConduitBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::CONDUIT;
}

const char *ConduitBlock::getBlockActorId() const {
    return ConduitBlockActor::BLOCK_ACTOR_ID;
}

void ConduitBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
    (void) owner;
    (void) state;
    level.getBlockActors().getOrCreate<ConduitBlockActor>(position);
}
