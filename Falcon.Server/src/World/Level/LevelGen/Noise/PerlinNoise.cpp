#include "World/Level/LevelGen/Noise/PerlinNoise.h"

#include "World/Level/LevelGen/Noise/NoiseMath.h"
#include "World/Level/LevelGen/Random/LinearCongruentialGenerator.h"

#include <cmath>

PerlinNoise::PerlinNoise(IRandom &random, int32_t firstOctave, const std::vector<double> &amplitudes)
    : mNoiseLevels(),
      mAmplitudes(amplitudes),
      mLowestFreqValueFactor(0.0),
      mLowestFreqInputFactor(0.0),
      mMaxValue(0.0) {
    const int32_t octaves = static_cast<int32_t>(amplitudes.size());
    const int32_t zeroOctaveIndex = -firstOctave;

    mNoiseLevels.assign(static_cast<size_t>(octaves), nullptr);

    std::shared_ptr<PerlinNoiseSampler> zeroOctave = std::make_shared<PerlinNoiseSampler>(random);

    if (zeroOctaveIndex >= 0 && zeroOctaveIndex < octaves) {
        const double amplitude = mAmplitudes[static_cast<size_t>(zeroOctaveIndex)];

        if (amplitude != 0.0) {
            mNoiseLevels[static_cast<size_t>(zeroOctaveIndex)] = zeroOctave;
        }
    }

    for (int32_t ix = zeroOctaveIndex - 1; ix >= 0; --ix) {
        if (ix < octaves) {
            if (mAmplitudes[static_cast<size_t>(ix)] != 0.0) {
                mNoiseLevels[static_cast<size_t>(ix)] = std::make_shared<PerlinNoiseSampler>(random);
            } else {
                _skipOctave(random);
            }
        } else {
            _skipOctave(random);
        }
    }

    mLowestFreqInputFactor = std::pow(2.0, static_cast<double>(-zeroOctaveIndex));
    mLowestFreqValueFactor = std::pow(2.0, static_cast<double>(octaves - 1)) / (std::pow(2.0, static_cast<double>(octaves)) - 1.0);
    mMaxValue = _edgeValue(2.0);
}

void PerlinNoise::_skipOctave(IRandom &random) {
    random.setSeed(LinearCongruentialGenerator::skip262().nextSeed(random.getSeed()));
}

double PerlinNoise::getValue(double x, double y, double z) const {
    double value = 0.0;
    double factor = mLowestFreqInputFactor;
    double valueFactor = mLowestFreqValueFactor;

    for (size_t i = 0; i < mNoiseLevels.size(); ++i) {
        const std::shared_ptr<PerlinNoiseSampler> &noise = mNoiseLevels[i];

        if (noise != nullptr) {
            const double noiseValue = noise->sample(
                NoiseMath::maintainPrecision(x * factor),
                NoiseMath::maintainPrecision(y * factor),
                NoiseMath::maintainPrecision(z * factor),
                0.0,
                0.0
            );
            value += mAmplitudes[i] * noiseValue * valueFactor;
        }

        factor *= 2.0;
        valueFactor /= 2.0;
    }

    return value;
}

double PerlinNoise::_edgeValue(double noiseValue) const {
    double value = 0.0;
    double valueFactor = mLowestFreqValueFactor;

    for (size_t i = 0; i < mNoiseLevels.size(); ++i) {
        if (mNoiseLevels[i] != nullptr) {
            value += mAmplitudes[i] * noiseValue * valueFactor;
        }

        valueFactor /= 2.0;
    }

    return value;
}

double PerlinNoise::maxValue() const {
    return mMaxValue;
}
