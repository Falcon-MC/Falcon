#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <cstdint>

class OreVeinifier {
public:
    static const int32_t MIN_VEIN_Y = -60;
    static const int32_t MAX_VEIN_Y = 50;

    OreVeinifier(DensityFunctionPtr veinToggle, DensityFunctionPtr veinRidged, DensityFunctionPtr veinGap,
                 int64_t seed);

    const BlockState *calculate(FunctionContext &context);

private:
    struct VeinType {
        const BlockState *mOre;
        const BlockState *mRawOreBlock;
        const BlockState *mFiller;
        int32_t mMinY;
        int32_t mMaxY;
    };

    static VeinType _copperVein();

    static VeinType _ironVein();

    static double _clampedMap(double value, double inMin, double inMax, double outMin, double outMax);

    static int64_t _mixSeed(int64_t seed, int32_t x, int32_t y, int32_t z);

    static constexpr float VEININESS_THRESHOLD = 0.4F;
    static constexpr int32_t EDGE_ROUNDOFF_BEGIN = 20;
    static constexpr double MAX_EDGE_ROUNDOFF = 0.2;
    static constexpr float VEIN_SOLIDNESS = 0.7F;
    static constexpr float MIN_RICHNESS = 0.1F;
    static constexpr float MAX_RICHNESS = 0.3F;
    static constexpr float MAX_RICHNESS_THRESHOLD = 0.6F;
    static constexpr float CHANCE_OF_RAW_ORE_BLOCK = 0.02F;
    static constexpr float SKIP_ORE_IF_GAP_NOISE_IS_BELOW = -0.3F;

    DensityFunctionPtr mVeinToggle;
    DensityFunctionPtr mVeinRidged;
    DensityFunctionPtr mVeinGap;
    int64_t mOreVeinSeed;
    SimpleRandom mRandom;
};
