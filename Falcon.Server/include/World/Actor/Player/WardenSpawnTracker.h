#pragma once

#include <algorithm>
#include <cstdint>

class Tag;

// a player's warden threat: shriekers raise it, it falls back one level every ten minutes
struct WardenSpawnTracker {
    static constexpr int32_t MAX_LEVEL = 4;
    static constexpr int32_t DECREASE_TICKS = 12000;
    static constexpr int32_t INCREASE_COOLDOWN_TICKS = 200;

    int32_t mLevel = 0;
    int32_t mCooldown = 0;
    int32_t mDecreaseTimer = 0;

    void tick() {
        if (mLevel > 0 && (mDecreaseTimer <= 0 || --mDecreaseTimer == 0)) {
            mLevel = std::min(mLevel - 1, MAX_LEVEL);
            mDecreaseTimer = DECREASE_TICKS;
        }
        if (mCooldown > 0)
            --mCooldown;
    }

    bool onCooldown() const {
        return mCooldown > 0;
    }

    void increase() {
        mLevel = std::min(std::max(mLevel, -1) + 1, MAX_LEVEL);
        mCooldown = INCREASE_COOLDOWN_TICKS;
        mDecreaseTimer = DECREASE_TICKS;
    }

    void save(Tag &data) const;

    void load(const Tag &data);
};
