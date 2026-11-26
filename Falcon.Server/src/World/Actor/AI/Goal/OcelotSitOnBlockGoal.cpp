#include "World/Actor/AI/Goal/OcelotSitOnBlockGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

// a pet told to sit stays where its owner left it
bool OcelotSitOnBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mob.isTamed() && !mob.isSitting() && BaseMoveToBlockGoal::canUse(owner, mob);
}

bool OcelotSitOnBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mob.isSitting() && BaseMoveToBlockGoal::canContinueToUse(owner, mob);
}

void OcelotSitOnBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::stop(owner, mob);
    // an order to sit took over: its pose stays
    if (!mob.isSitting())
        mob.setSittingFlag(owner, false);
}

// the sitting pose only shows: the pet is not told to stay
void OcelotSitOnBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::tick(owner, mob);
    mob.setSittingFlag(owner, isReached());
}

bool OcelotSitOnBlockGoal::isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) {
    Level &level = owner.getLevelFor(mob);
    const BlockState *above = level.peekBlockPtr(position.x, position.y + 1, position.z);
    if (above == nullptr || above->mName != BlockNames::AIR)
        return false;

    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
    return block != nullptr && block->seatsPet(level, position, *state);
}
