#pragma once

#include "World/Level/LevelGen/Noise/SimplexNoiseSampler.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <memory>
#include <string>
#include <vector>

class SimplexNoise {
public:
    SimplexNoise(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes);

    float getValue(double x, double y, double z) const;

    float getMax() const;

    static float wrap(double value);

    static int32_t javaStringHashCode(const std::string &text);

    static std::string javaIntegerToString(int32_t value);

private:
    float _edgeValue(double input) const;

    std::vector<float> mAmplitudes;
    std::vector<std::shared_ptr<SimplexNoiseSampler>> mNoiseLevels;
    int32_t mLevels;
    float mLowestFreqValueFactor;
    float mLowestFreqInputFactor;
    float mMaxValue;
};
