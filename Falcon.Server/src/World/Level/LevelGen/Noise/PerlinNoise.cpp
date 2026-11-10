#include "World/Level/LevelGen/Noise/PerlinNoise.h"

#include "World/Level/LevelGen/Random/LinearCongruentialGenerator.h"

#include <cmath>
#include <string>

PerlinNoise::PerlinNoise(IRandom &random, int32_t firstOctave, const std::vector<double> &amplitudes)
        : mNoiseLevels(), mAmplitudes(amplitudes), mLowestFreqValueFactor(0.0), mMaxValue(0.0) {
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

    _computeOctaveFactors(firstOctave);
    mLowestFreqValueFactor = std::pow(2.0, static_cast<double>(octaves - 1))
                             / (std::pow(2.0, static_cast<double>(octaves)) - 1.0);
    mMaxValue = _edgeValue(2.0);
}

PerlinNoise::PerlinNoise(const XoroshiroPositionalFactory &factory, int32_t firstOctave,
                         const std::vector<double> &amplitudes)
        : mNoiseLevels(), mAmplitudes(amplitudes), mLowestFreqValueFactor(0.0), mMaxValue(0.0) {
    const int32_t octaves = static_cast<int32_t>(amplitudes.size());
    mNoiseLevels.assign(static_cast<size_t>(octaves), nullptr);

    for (int32_t i = 0; i < octaves; ++i) {
        if (mAmplitudes[static_cast<size_t>(i)] != 0.0) {
            XoroshiroRandom octaveRandom = factory.hashOf("octave_" + std::to_string(firstOctave + i));
            mNoiseLevels[static_cast<size_t>(i)] = std::make_shared<PerlinNoiseSampler>(octaveRandom);
        }
    }

    _computeOctaveFactors(firstOctave);
    mLowestFreqValueFactor = std::pow(2.0, static_cast<double>(octaves - 1))
                             / (std::pow(2.0, static_cast<double>(octaves)) - 1.0);
    mMaxValue = _edgeValue(2.0);
}

// sub_145C2AF90 with its legacy mode
PerlinNoise::PerlinNoise(std::mt19937 &random, int32_t firstOctave, const std::vector<double> &amplitudes)
        : mNoiseLevels(), mAmplitudes(amplitudes), mLowestFreqValueFactor(0.0), mMaxValue(0.0) {
    const int32_t octaves = static_cast<int32_t>(amplitudes.size());
    const int32_t zeroOctaveIndex = -firstOctave;

    mNoiseLevels.assign(static_cast<size_t>(octaves), nullptr);

    std::shared_ptr<PerlinNoiseSampler> zeroOctave = std::make_shared<PerlinNoiseSampler>(random);
    if (zeroOctaveIndex >= 0 && zeroOctaveIndex < octaves && mAmplitudes[static_cast<size_t>(zeroOctaveIndex)] != 0.0)
        mNoiseLevels[static_cast<size_t>(zeroOctaveIndex)] = zeroOctave;

    for (int32_t ix = zeroOctaveIndex - 1; ix >= 0; --ix) {
        if (ix < octaves && mAmplitudes[static_cast<size_t>(ix)] != 0.0)
            mNoiseLevels[static_cast<size_t>(ix)] = std::make_shared<PerlinNoiseSampler>(random);
        else
            random.discard(262);
    }

    _computeOctaveFactors(firstOctave);
    mLowestFreqValueFactor = std::pow(2.0, static_cast<double>(octaves - 1))
                             / (std::pow(2.0, static_cast<double>(octaves)) - 1.0);
    mMaxValue = _edgeValue(2.0);
}

void PerlinNoise::_skipOctave(IRandom &random) {
    random.setSeed(LinearCongruentialGenerator::skip262().nextSeed(random.getSeed()));
}

// per-octave factors kept in float like the game (sub_145C2AF90)
void PerlinNoise::_computeOctaveFactors(int32_t firstOctave) {
    const int32_t octaves = static_cast<int32_t>(mAmplitudes.size());
    const float totalShare = static_cast<float>(static_cast<int32_t>(~(~0u << octaves)));
    mInputFactors.assign(static_cast<size_t>(octaves), 0.0f);
    mValueFactors.assign(static_cast<size_t>(octaves), 0.0f);

    for (int32_t i = 0; i < octaves; ++i) {
        const float share = static_cast<float>(1 << (octaves - 1 - i)) / totalShare;
        mInputFactors[static_cast<size_t>(i)] = static_cast<float>(std::ldexp(1.0, firstOctave + i));
        mValueFactors[static_cast<size_t>(i)] = share * static_cast<float>(mAmplitudes[static_cast<size_t>(i)]);
    }
}

// float sum, no coordinate wrapping (sub_145C2C790)
float PerlinNoise::getValue(float x, float y, float z) const {
    float value = 0.0f;

    for (size_t i = 0; i < mNoiseLevels.size(); ++i) {
        const std::shared_ptr<PerlinNoiseSampler> &noise = mNoiseLevels[i];

        if (noise != nullptr) {
            const float factor = mInputFactors[i];
            value += noise->sampleFloat(x * factor, y * factor, z * factor) * mValueFactors[i];
        }
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
