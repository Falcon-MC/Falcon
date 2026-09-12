#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MobDrop {
    std::string mItemIdentifier;
    int32_t mCount;
};

namespace MobLootTable {

/**
 * Rolls the drops of a mob. A positive looting level raises the maximum count of every drop whose
 * count varies by that level, and the chance of every uncertain drop by that drop's per-level bonus,
 * one percentage point unless the table sets another. Drops with a fixed count are never multiplied.
 */
std::vector<MobDrop> getMobDrops(const std::string &identifier, bool onFire, int32_t lootingLevel = 0);

bool hasMobDrops(const std::string &identifier);

}
