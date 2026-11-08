#include "World/Level/LevelGen/Noise/NormalNoise.h"

#include <algorithm>
#include <limits>

namespace {

// float 0x3F8251FB in the game (dword_14F279BE0)
constexpr float INPUT_FACTOR = 1.01812684535980224609375f;

}

NormalNoise::NormalNoise(IRandom &random, int32_t firstOctave, const std::vector<float> &amplitudes)
    : mFirst(nullptr), mSecond(nullptr), mValueFactor(0.0f), mMaxValue(0.0) {
    int32_t min = 0;
    int32_t max = 0;
    const std::vector<double> octaveAmplitudes = _octaveAmplitudes(amplitudes, min, max);

    mFirst = std::make_shared<PerlinNoise>(random, firstOctave, octaveAmplitudes);
    mSecond = std::make_shared<PerlinNoise>(random, firstOctave, octaveAmplitudes);
    mValueFactor = _valueFactor(min, max);
    mMaxValue = (mFirst->maxValue() + mSecond->maxValue()) * mValueFactor;
}

NormalNoise::NormalNoise(std::mt19937 &random, int32_t firstOctave, const std::vector<float> &amplitudes)
    : mFirst(nullptr), mSecond(nullptr), mValueFactor(0.0f), mMaxValue(0.0) {
    int32_t min = 0;
    int32_t max = 0;
    const std::vector<double> octaveAmplitudes = _octaveAmplitudes(amplitudes, min, max);

    mFirst = std::make_shared<PerlinNoise>(random, firstOctave, octaveAmplitudes);
    mSecond = std::make_shared<PerlinNoise>(random, firstOctave, octaveAmplitudes);
    mValueFactor = _valueFactor(min, max);
    mMaxValue = (mFirst->maxValue() + mSecond->maxValue()) * mValueFactor;
}

NormalNoise::NormalNoise(std::shared_ptr<PerlinNoise> first, std::shared_ptr<PerlinNoise> second, float valueFactor)
    : mFirst(std::move(first)), mSecond(std::move(second)), mValueFactor(valueFactor), mMaxValue(0.0) {
    mMaxValue = (mFirst->maxValue() + mSecond->maxValue()) * mValueFactor;
}

std::shared_ptr<NormalNoise> NormalNoise::create(const XoroshiroPositionalFactory &factory, const std::string &name,
                                                 int32_t firstOctave, const std::vector<float> &amplitudes) {
    XoroshiroRandom random = factory.hashOf(name);
    return create(random, firstOctave, amplitudes);
}

std::shared_ptr<NormalNoise> NormalNoise::create(XoroshiroRandom &random, int32_t firstOctave,
                                                 const std::vector<float> &amplitudes) {
    int32_t min = 0;
    int32_t max = 0;
    const std::vector<double> octaveAmplitudes = _octaveAmplitudes(amplitudes, min, max);

    // order matters: first stack drawn before the second
    const XoroshiroPositionalFactory firstFactory = random.forkPositional();
    const XoroshiroPositionalFactory secondFactory = random.forkPositional();

    auto first = std::make_shared<PerlinNoise>(firstFactory, firstOctave, octaveAmplitudes);
    auto second = std::make_shared<PerlinNoise>(secondFactory, firstOctave, octaveAmplitudes);
    const float valueFactor = _valueFactor(min, max);
    return std::shared_ptr<NormalNoise>(new NormalNoise(std::move(first), std::move(second), valueFactor));
}

// second stack sampled at a slightly scaled position: the lattices never line up; scale and normalisation stay
// exact, or seeds give other terrain
float NormalNoise::getValue(double x, double y, double z) const {
    // stacks summed separately, then added (sub_145C2C790)
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float fz = static_cast<float>(z);
    const float first = mFirst->getValue(fx, fy, fz);
    const float second = mSecond->getValue(fx * INPUT_FACTOR, fy * INPUT_FACTOR, fz * INPUT_FACTOR);
    return (first + second) * mValueFactor;
}

double NormalNoise::getMax() const {
    return mMaxValue;
}

// the indices of the first and last non-zero amplitudes, min above max when there is none
std::vector<double> NormalNoise::_octaveAmplitudes(const std::vector<float> &amplitudes, int32_t &min, int32_t &max) {
    std::vector<double> octaveAmplitudes(amplitudes.size(), 0.0);
    min = std::numeric_limits<int32_t>::max();
    max = std::numeric_limits<int32_t>::min();

    for (size_t i = 0; i < amplitudes.size(); ++i) {
        const double amplitude = static_cast<double>(amplitudes[i]);
        octaveAmplitudes[i] = amplitude;
        if (amplitude != 0.0) {
            const int32_t index = static_cast<int32_t>(i);
            min = std::min(min, index);
            max = std::max(max, index);
        }
    }
    return octaveAmplitudes;
}

// float, in the game's order (sub_1471326A0, sub_14D20B390); span term is 2 with no non-zero amplitude
float NormalNoise::_valueFactor(int32_t minOctave, int32_t maxOctave) {
    const float span = minOctave > maxOctave ? 2.0f : static_cast<float>(maxOctave - minOctave) + 1.0f;
    return 0.16666667f / ((1.0f / span + 1.0f) * 0.1f);
}
