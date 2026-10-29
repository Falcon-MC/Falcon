#include "World/Level/LevelGen/Noise/SimplexNoise.h"

#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <cmath>

SimplexNoise::SimplexNoise(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes)
    : mAmplitudes(amplitudes),
      mNoiseLevels(),
      mLevels(0),
      mLowestFreqValueFactor(0.0f),
      mLowestFreqInputFactor(0.0f),
      mMaxValue(0.0f) {
    mLevels = static_cast<int32_t>(mAmplitudes.size());
    mNoiseLevels.assign(static_cast<size_t>(mLevels), nullptr);
    mLowestFreqInputFactor = static_cast<float>(std::pow(2.0, static_cast<double>(firstOctave)));
    mLowestFreqValueFactor = static_cast<float>(std::pow(2.0, static_cast<double>(mLevels - 1)) / (std::pow(2.0, static_cast<double>(mLevels)) - 1.0));

    for (int32_t i = 0; i < mLevels; ++i) {
        if (mAmplitudes[static_cast<size_t>(i)] != 0.0f) {
            const int32_t octave = static_cast<int32_t>(static_cast<uint32_t>(firstOctave) + static_cast<uint32_t>(i));
            const int32_t hash = javaStringHashCode("octave_" + javaIntegerToString(octave));
            const int64_t seed = static_cast<int64_t>(static_cast<uint64_t>(random.nextLong()) + static_cast<uint64_t>(static_cast<int64_t>(hash)));
            SimpleRandom octaveRandom(seed);
            mNoiseLevels[static_cast<size_t>(i)] = std::make_shared<SimplexNoiseSampler>(octaveRandom);
        }
    }

    mMaxValue = _edgeValue(2.0);
}

float SimplexNoise::getValue(double x, double y, double z) const {
    float d0 = 0.0f;
    float d1 = mLowestFreqInputFactor;
    float d2 = mLowestFreqValueFactor;

    for (size_t i = 0; i < mNoiseLevels.size(); ++i) {
        const std::shared_ptr<SimplexNoiseSampler> &noise = mNoiseLevels[i];

        if (noise != nullptr) {
            const float d3 = static_cast<float>(noise->sample3D(
                static_cast<double>(wrap(x * static_cast<double>(d1))),
                static_cast<double>(wrap(y * static_cast<double>(d1))),
                static_cast<double>(wrap(z * static_cast<double>(d1)))
            ));
            d0 = static_cast<float>(d0 + static_cast<float>(static_cast<float>(mAmplitudes[i] * d3) * d2));
        }

        d1 = static_cast<float>(static_cast<double>(d1) * 2.0);
        d2 = static_cast<float>(static_cast<double>(d2) / 2.0);
    }

    return d0;
}

float SimplexNoise::wrap(double value) {
    return static_cast<float>(value - std::floor(value / 3.3554432E7 + 0.5) * 3.3554432E7);
}

float SimplexNoise::_edgeValue(double input) const {
    float cumulativeSum = 0.0f;
    float octaveContributionFactor = mLowestFreqValueFactor;

    for (size_t i = 0; i < mNoiseLevels.size(); ++i) {
        if (mNoiseLevels[i] != nullptr) {
            const double contribution = static_cast<double>(mAmplitudes[i]) * input * static_cast<double>(octaveContributionFactor);
            cumulativeSum = static_cast<float>(static_cast<double>(cumulativeSum) + contribution);
        }

        octaveContributionFactor = octaveContributionFactor / 2.0f;
    }

    return cumulativeSum;
}

float SimplexNoise::getMax() const {
    return mMaxValue;
}

int32_t SimplexNoise::javaStringHashCode(const std::string &text) {
    uint32_t h = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        h = h * 31u + static_cast<uint32_t>(static_cast<unsigned char>(text[i]));
    }

    return static_cast<int32_t>(h);
}

std::string SimplexNoise::javaIntegerToString(int32_t value) {
    if (value == 0) {
        return "0";
    }

    const bool negative = value < 0;
    int64_t magnitude = static_cast<int64_t>(value);

    if (negative) {
        magnitude = -magnitude;
    }

    std::string digits;

    while (magnitude > 0) {
        digits.insert(digits.begin(), static_cast<char>('0' + static_cast<int32_t>(magnitude % 10)));
        magnitude /= 10;
    }

    if (negative) {
        digits.insert(digits.begin(), '-');
    }

    return digits;
}
