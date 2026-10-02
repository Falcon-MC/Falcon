#include "Level/Generator/Noise/NormalNoise.h"

#include <limits>

namespace {

constexpr double INPUT_FACTOR = 1.0181268882175227;

}

NormalNoise::NormalNoise(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes)
    : mFirst(nullptr), mSecond(nullptr), mValueFactor(0.0), mMaxValue(0.0) {
    std::vector<double> octaveAmplitudes(amplitudes.size(), 0.0);
    int32_t min = std::numeric_limits<int32_t>::max();
    int32_t max = std::numeric_limits<int32_t>::min();

    for (size_t i = 0; i < amplitudes.size(); ++i) {
        const double amplitude = static_cast<double>(amplitudes[i]);
        octaveAmplitudes[i] = amplitude;

        if (amplitude != 0.0) {
            const int32_t index = static_cast<int32_t>(i);

            if (index < min) {
                min = index;
            }

            if (index > max) {
                max = index;
            }
        }
    }

    mFirst = std::make_shared<PerlinNoise>(random, firstOctave, octaveAmplitudes);
    mSecond = std::make_shared<PerlinNoise>(random, firstOctave, octaveAmplitudes);
    const int32_t octaveSpan = static_cast<int32_t>(static_cast<uint32_t>(max) - static_cast<uint32_t>(min));
    mValueFactor = 1.0 / 6.0 / _expectedDeviation(octaveSpan);
    mMaxValue = (mFirst->maxValue() + mSecond->maxValue()) * mValueFactor;
}

/**
 * Sum of two independent Perlin stacks, the second sampled at a slightly scaled position so
 * their lattices never line up; this smooths out grid artefacts and gives a roughly normal
 * value distribution. The scale factor and normalisation must stay exact for seeds to
 * produce the expected terrain.
 */
float NormalNoise::getValue(double x, double y, double z) const {
    const double x2 = x * INPUT_FACTOR;
    const double y2 = y * INPUT_FACTOR;
    const double z2 = z * INPUT_FACTOR;
    return static_cast<float>((mFirst->getValue(x, y, z) + mSecond->getValue(x2, y2, z2)) * mValueFactor);
}

double NormalNoise::getMax() const {
    return mMaxValue;
}

double NormalNoise::_expectedDeviation(int32_t octaveSpan) {
    return 0.1 * (1.0 + 1.0 / (static_cast<double>(octaveSpan) + 1.0));
}
