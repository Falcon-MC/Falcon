#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <random>

struct LiquidContact;

enum class BuoyancyMovement : uint8_t {
    None,
    Waves,
    Bobbing
};

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

// minecraft:buoyant
class BuoyancySystem {
public:
    static BuoyancySettings read(const json::Value *component);

    static bool floats(const BuoyancySettings &settings, const LiquidContact &feet);

    // IncreaseBuoyancyTimer
    static double advanceTimer(double timer, const BuoyancySettings &settings, const Vector3f &velocity,
                               std::mt19937 &random);

    // BuoyancyFloat: the vertical speed of an actor floating in one of its liquids
    static float floatVelocity(float velocityY, float positionY, double timer, const BuoyancySettings &settings);
};
