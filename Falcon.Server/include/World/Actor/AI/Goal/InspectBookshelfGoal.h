#pragma once

#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"

// minecraft:behavior.inspect_bookshelf: a librarian walks to a nearby bookshelf and looks it over
class InspectBookshelfGoal final : public BaseMoveToBlockGoal {
public:
    using BaseMoveToBlockGoal::BaseMoveToBlockGoal;

    // the game asks only that the bookshelf is still there: another goal or the schedule ends it
    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) override;
};
