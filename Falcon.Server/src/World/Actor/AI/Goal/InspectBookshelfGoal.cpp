#include "World/Actor/AI/Goal/InspectBookshelfGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

bool InspectBookshelfGoal::isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) {
    const BlockState *state = owner.getLevelFor(mob).peekBlockPtr(position.x, position.y, position.z);
    return state != nullptr && state->mName == BlockNames::BOOKSHELF;
}

bool InspectBookshelfGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return isValidTarget(owner, mob, getTarget());
}

void InspectBookshelfGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::tick(owner, mob);
    const Vector3i &target = getTarget();
    mob.getLookControl().setLookAt(
            Vector3f((float) target.x + 0.5f, (float) target.y + 0.5f, (float) target.z + 0.5f));
}
