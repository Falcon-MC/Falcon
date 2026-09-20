#include "Actor/Mob/MobLoot.h"

#include <algorithm>
#include <random>

namespace MobLoot {

namespace {

const uint32_t LOOT_SEED = 0x9E3779B9u;

std::mt19937 &getRandom() {
    static std::mt19937 random(LOOT_SEED);
    return random;
}

float nextChanceRoll() {
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(getRandom());
}

int32_t nextCount(int32_t minCount, int32_t maxCount) {
    if (minCount >= maxCount)
        return minCount;

    std::uniform_int_distribution<int32_t> distribution(minCount, maxCount);
    return distribution(getRandom());
}

}

std::vector<MobDrop> roll(const std::vector<LootEntry> &entries, bool onFire, int32_t lootingLevel) {
    std::vector<MobDrop> drops;
    const int32_t looting = std::max(lootingLevel, 0);

    for (const LootEntry &entry: entries) {
        if (entry.mChance < 1.0f) {
            const float chance = std::min(1.0f, entry.mChance + entry.mLootingChancePerLevel * (float) looting);
            if (nextChanceRoll() >= chance)
                continue;
        }

        const int32_t maxCount = entry.mMaxCount > entry.mMinCount ? entry.mMaxCount + looting : entry.mMaxCount;
        const int32_t count = nextCount(entry.mMinCount, maxCount);
        if (count <= 0)
            continue;

        const char *itemIdentifier = entry.mItemIdentifier;
        if (onFire && entry.mBurntItemIdentifier != nullptr)
            itemIdentifier = entry.mBurntItemIdentifier;

        MobDrop drop;
        drop.mItemIdentifier = itemIdentifier;
        drop.mCount = count;
        drops.push_back(drop);
    }

    return drops;
}

}
