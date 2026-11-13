#include "World/Level/Block/SculkCatalystBlock.h"

#include "World/Level/Block/Actor/SculkCatalystBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(SculkCatalystBlock, 352);

bool SculkCatalystBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SCULK_CATALYST;
}

const char *SculkCatalystBlock::getBlockActorId() const {
    return SculkCatalystBlockActor::BLOCK_ACTOR_ID;
}

// the spreader lives in the block actor
void SculkCatalystBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    (void) owner;
    (void) state;
    level.getBlockActors().getOrCreate<SculkCatalystBlockActor>(position);
}
