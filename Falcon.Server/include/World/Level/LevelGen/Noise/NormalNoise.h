#pragma once

#include "World/Level/LevelGen/Noise/PerlinNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

class NormalNoise {
public:
    NormalNoise(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes);

    // both legacy stacks drawn from the game's Mersenne twister Random, as the geode feature builds its noise
    NormalNoise(std::mt19937 &random, int32_t firstOctave, const std::vector<float> &amplitudes);

    static std::shared_ptr<NormalNoise> create(const XoroshiroPositionalFactory &factory, const std::string &name,
                                               int32_t firstOctave, const std::vector<float> &amplitudes);

    static std::shared_ptr<NormalNoise> create(XoroshiroRandom &random, int32_t firstOctave,
                                               const std::vector<float> &amplitudes);

    // narrowed to float once, before scaling: the game samples float coordinates
    float getValue(double x, double y, double z) const;

    double getMax() const;

private:
    NormalNoise(std::shared_ptr<PerlinNoise> first, std::shared_ptr<PerlinNoise> second, float valueFactor);

    static std::vector<double> _octaveAmplitudes(const std::vector<float> &amplitudes, int32_t &min, int32_t &max);

    static float _valueFactor(int32_t minOctave, int32_t maxOctave);

    std::shared_ptr<PerlinNoise> mFirst;
    std::shared_ptr<PerlinNoise> mSecond;
    float mValueFactor;
    double mMaxValue;
};

using NormalNoisePtr = std::shared_ptr<NormalNoise>;
