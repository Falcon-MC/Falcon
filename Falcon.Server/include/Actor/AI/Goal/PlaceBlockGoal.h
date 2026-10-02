#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Block/BlockState.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <memory>
#include <vector>

class Level;

class PlaceBlockGoal : public Goal {
public:
    struct Entry {
        BlockState mState;
        std::shared_ptr<json::Value> mFilter;
    };

    struct Range {
        int32_t mMin = 0;
        int32_t mMax = 0;
    };

    PlaceBlockGoal(std::vector<Entry> entries, std::shared_ptr<json::Value> canPlace, float chance, Range xzRange,
                   Range yRange, bool affectedByGriefingRule, std::shared_ptr<json::Value> onPlace);

    static std::unique_ptr<Goal> create(const json::Value &component);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _findPlacement(ServerNetworkHandler &owner, Level &level, MobActor &mob);

    std::vector<Entry> mEntries;
    std::shared_ptr<json::Value> mCanPlace;
    float mChance;
    Range mXzRange;
    Range mYRange;
    bool mAffectedByGriefingRule;
    std::shared_ptr<json::Value> mOnPlace;
    Vector3i mPosition;
    BlockState mState;
};
