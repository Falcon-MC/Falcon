#pragma once

#include <cstdint>

enum class BuoyancyMovement : uint8_t {
    None,
    Waves,
    Bobbing
};

// minecraft:buoyant as read from the definition
struct BuoyancySettings {
    bool mEnabled = false;
    float mBaseBuoyancy = 1.0f;
    bool mApplyGravity = true;
    BuoyancyMovement mMovement = BuoyancyMovement::Waves;
    float mBigWaveProbability = 0.03f;
    float mBigWaveSpeed = 10.0f;
    bool mWater = true;
    bool mLava = false;
};
