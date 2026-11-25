#pragma once

#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"

// minecraft:behavior.ocelot_sit_on_block: a tamed cat or ocelot walks to a closed chest, a furnace or the foot of a
// bed and sits on it until the walk or the stay runs out
class OcelotSitOnBlockGoal final : public BaseMoveToBlockGoal {
public:
    using BaseMoveToBlockGoal::BaseMoveToBlockGoal;

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) override;
};
