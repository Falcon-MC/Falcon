#include "World/Actor/Player/WardenSpawnTracker.h"

#include "Core/NBT/Tag.h"

namespace {
    const char *const TAG_LEVEL = "WardenThreatLevel";
    const char *const TAG_COOLDOWN = "WardenThreatLevelIncreaseCooldown";
    const char *const TAG_DECREASE_TIMER = "WardenThreatDecreaseTimer";
}

void WardenSpawnTracker::save(Tag &data) const {
    data.putInt(TAG_LEVEL, mLevel);
    data.putInt(TAG_COOLDOWN, mCooldown);
    data.putInt(TAG_DECREASE_TIMER, mDecreaseTimer);
}

void WardenSpawnTracker::load(const Tag &data) {
    mDecreaseTimer = data.getInt(TAG_DECREASE_TIMER, mDecreaseTimer);
    mLevel = data.getInt(TAG_LEVEL, mLevel);
    mCooldown = data.getInt(TAG_COOLDOWN, mCooldown);
}
