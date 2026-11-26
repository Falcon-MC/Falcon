#pragma once

#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"
#include "World/Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// minecraft:behavior.drop_item_for: in its time of day the mob goes to a spot near a nearby entity and may drop a
// roll of its loot table there, as the cat's morning gift; every try ends with on_drop_attempt
class DropItemForGoal : public BaseMoveToBlockGoal {
public:
    struct Offer {
        int32_t mCooldownTicks = 0;
        float mDropChance = 1.0f;
        // the spot's squared block distance to the entity is kept closest to this, as the game compares it
        float mOfferingDistance = 1.0f;
        // a squared distance too: farther from the spot, the mob is put on it
        float mMinimumTeleportDistance = 2.0f;
        int32_t mPickupDelayTicks = 0;
        float mTimeOfDayMin = 0.0f;
        float mTimeOfDayMax = 1.0f;
        Vector3f mTargetRange;
        Vector3f mTeleportOffset;
        std::vector<NearestAttackableTargetGoal::Entry> mEntries;
        std::string mLootTable;
        std::shared_ptr<json::Value> mOnDropAttempt;
    };

    DropItemForGoal(Settings settings, Offer offer);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) override;

private:
    Actor *_findReceiver(ServerNetworkHandler &owner, MobActor &mob) const;

    bool _findSpotNear(ServerNetworkHandler &owner, MobActor &mob, const Actor &receiver);

    void _drop(ServerNetworkHandler &owner, MobActor &mob);

    Settings mSettings;
    Offer mOffer;
    int32_t mCooldownLeft = 0;
    uint64_t mReceiverId = 0;
    bool mDropped = false;
};
