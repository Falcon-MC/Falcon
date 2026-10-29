#pragma once

#include "World/Level/LevelGen/Noise/PerlinNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <memory>
#include <vector>

class NormalNoise {
public:
    NormalNoise(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes);

    float getValue(double x, double y, double z) const;

    double getMax() const;

private:
    static double _expectedDeviation(int32_t octaveSpan);

    std::shared_ptr<PerlinNoise> mFirst;
    std::shared_ptr<PerlinNoise> mSecond;
    double mValueFactor;
    double mMaxValue;
};

using NormalNoisePtr = std::shared_ptr<NormalNoise>;
