#pragma once

#include "World/Level/LevelGen/Noise/PerlinNoiseSampler.h"
#include "World/Level/LevelGen/Random/IRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <memory>
#include <vector>

class PerlinNoise {
public:
    PerlinNoise(IRandom &random, int32_t firstOctave, const std::vector<double> &amplitudes);

    PerlinNoise(const XoroshiroPositionalFactory &factory, int32_t firstOctave, const std::vector<double> &amplitudes);

    float getValue(float x, float y, float z) const;

    double maxValue() const;

private:
    static void _skipOctave(IRandom &random);

    void _computeOctaveFactors(int32_t firstOctave);

    double _edgeValue(double noiseValue) const;

    std::vector<std::shared_ptr<PerlinNoiseSampler>> mNoiseLevels;
    std::vector<double> mAmplitudes;
    std::vector<float> mInputFactors;
    std::vector<float> mValueFactors;
    double mLowestFreqValueFactor;
    double mMaxValue;
};
