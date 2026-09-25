#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Level;

class EatBlockGoal : public Goal {
public:
    EatBlockGoal(std::vector<std::pair<std::string, std::string>> pairs, std::string chance, int32_t eatTicks,
                 std::string event);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    const std::pair<std::string, std::string> *_findPair(Level &level, const MobActor &mob,
                                                           Vector3i &position) const;

    float _chance(const MobActor &mob) const;

    std::vector<std::pair<std::string, std::string>> mPairs;
    std::string mChance;
    int32_t mEatTicks;
    std::string mEvent;
    int32_t mTimer = 0;
};
