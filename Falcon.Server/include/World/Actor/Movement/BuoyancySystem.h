#pragma once

#include "World/Actor/Movement/BuoyancySettings.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <random>

class BlockState;
class Level;

// CheckAndAddBuoyancyFloatRequest: where the actor stands in one of its liquids
struct BuoyancyContact {
    // at the surface: the liquid holds the feet below its top and nothing of it is above
    bool mSurface = false;
    // under the surface: the liquid continues above the feet
    bool mSubmerged = false;

    bool floats() const {
        return mSurface || mSubmerged;
    }
};

// minecraft:buoyant
class BuoyancySystem {
public:
    static BuoyancySettings read(const json::Value *component);

    static BuoyancyContact contact(Level &level, const Vector3f &position, const BuoyancySettings &settings);

    // IncreaseBuoyancyTimer
    static double advanceTimer(double timer, const BuoyancySettings &settings, const Vector3f &velocity,
                               std::mt19937 &random);

    // BuoyancyFloat: the vertical speed of an actor floating in one of its liquids
    // leashed: a rising leashed actor keeps part of its own speed
    static float floatVelocity(float velocityY, float positionY, double timer, const BuoyancySettings &settings,
                               const BuoyancyContact &contact, bool leashed);

private:
    static bool _isFloatLiquid(const BuoyancySettings &settings, const BlockState &state);
};
