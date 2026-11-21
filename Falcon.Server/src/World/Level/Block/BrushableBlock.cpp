#include "World/Level/Block/BrushableBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(BrushableBlock, 185);

bool BrushableBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SUSPICIOUS_SAND || identifier == BlockNames::SUSPICIOUS_GRAVEL;
}

const char *BrushableBlock::getBlockActorId() const {
    return BrushableBlockActor::BLOCK_ACTOR_ID;
}

// the block makes its actor with the empty table
void BrushableBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) player;
    (void) usedItem;
    (void) blockFace;
    Level &level = owner.getLevelFor(player);
    level.getBlockActors().getOrCreate<BrushableBlockActor>(position).setType(state.mName);
}

// the fall check runs from the falling block system
void BrushableBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) state;
    BrushableBlockActor *actor = level.getBlockActors().find<BrushableBlockActor>(position);
    if (actor == nullptr)
        return;

    bool changed = false;
    const int32_t delay = actor->checkReset(owner.getCurrentTick(), changed);
    if (delay != 0)
        level.scheduleUpdate(position, delay);
    if (changed)
        actor->sync(owner, level);
}
