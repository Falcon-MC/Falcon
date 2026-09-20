#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MobDrop {
    std::string mItemIdentifier;
    int32_t mCount;
};

struct LootEntry {
    const char *mItemIdentifier;
    const char *mBurntItemIdentifier;
    int32_t mMinCount;
    int32_t mMaxCount;
    float mChance;
    float mLootingChancePerLevel = 0.01f;
};

namespace MobLoot {

std::vector<MobDrop> roll(const std::vector<LootEntry> &entries, bool onFire, int32_t lootingLevel);

}
