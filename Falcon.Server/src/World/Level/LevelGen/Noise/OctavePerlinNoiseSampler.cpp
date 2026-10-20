#include "World/Level/LevelGen/Noise/OctavePerlinNoiseSampler.h"

#include "World/Level/LevelGen/Random/LinearCongruentialGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    int64_t javaDoubleToLong(double value) {
        if (std::isnan(value)) {
            return 0;
        }

        if (value >= 9223372036854775808.0) {
            return std::numeric_limits<int64_t>::max();
        }

        if (value <= -9223372036854775808.0) {
            return std::numeric_limits<int64_t>::min();
        }

        return (int64_t) value;
    }

    bool containsOctave(const std::vector<int32_t> &octaves, int32_t value) {
        return std::find(octaves.begin(), octaves.end(), value) != octaves.end();
    }
}

void OctavePerlinNoiseSampler::_skipOctave(IRandom &rand) {
    rand.setSeed(LinearCongruentialGenerator::skip262().nextSeed(rand.getSeed()));
}

double OctavePerlinNoiseSampler::maintainPrecision(double value) {
    return value - std::floor(value / 3.3554432E7 + 0.5) * 3.3554432E7;
}

OctavePerlinNoiseSampler::OctavePerlinNoiseSampler(IRandom &rand, const std::vector<int32_t> &octaves)
        : mLacunarity(0.0), mPersistence(0.0), mOctaveSamplersCount(0) {
    std::vector<int32_t> sortedOctaves(octaves);
    std::sort(sortedOctaves.begin(), sortedOctaves.end());

    const int32_t start = -sortedOctaves.front();
    const int32_t end = sortedOctaves.back();
    const int32_t length = start + end + 1;

    const std::shared_ptr<PerlinNoiseSampler> perlin = std::make_shared<PerlinNoiseSampler>(rand);

    mOctaveSamplers.assign((size_t) length, nullptr);

    if (end >= 0 && end < length && containsOctave(sortedOctaves, 0)) {
        mOctaveSamplers[(size_t) end] = perlin;
    }

    for (int32_t index = end + 1; index < length; ++index) {
        if (index >= 0 && containsOctave(sortedOctaves, end - index)) {
            mOctaveSamplers[(size_t) index] = std::make_shared<PerlinNoiseSampler>(rand);
        } else {
            _skipOctave(rand);
        }
    }

    if (end > 0) {
        const double sampled = perlin->sample(0.0, 0.0, 0.0, 0.0, 0.0);
        const int64_t noiseSeed = javaDoubleToLong(sampled * 9.223372036854776E18);
        rand.setSeed(noiseSeed);

        for (int32_t index = end - 1; index >= 0; --index) {
            if (index < length && containsOctave(sortedOctaves, end - index)) {
                mOctaveSamplers[(size_t) index] = std::make_shared<PerlinNoiseSampler>(rand);
            } else {
                _skipOctave(rand);
            }
        }
    }

    mPersistence = std::pow(2.0, (double) end);
    mLacunarity = 1.0 / (std::pow(2.0, (double) length) - 1.0);
    mOctaveSamplersCount = (int32_t) mOctaveSamplers.size();
}

int32_t OctavePerlinNoiseSampler::getCount() const {
    return (int32_t) mOctaveSamplers.size();
}

const PerlinNoiseSampler *OctavePerlinNoiseSampler::getOctave(int32_t octave) const {
    return mOctaveSamplers[(size_t) octave].get();
}

double OctavePerlinNoiseSampler::sample(double x, double y, double z) const {
    return sample(x, y, z, 0.0, 0.0, false);
}

double OctavePerlinNoiseSampler::sample(double x, double y, double z, double yAmplification, double minY,
                                        bool useDefaultY) const {
    double noise = 0.0;
    double persistence = mPersistence;
    double lacunarity = mLacunarity;

    for (int32_t index = 0; index < mOctaveSamplersCount; index++) {
        const PerlinNoiseSampler *sampler = mOctaveSamplers[(size_t) index].get();

        if (sampler != nullptr) {
            const double sampleY = useDefaultY
                                   ? -sampler->getOriginY()
                                   : maintainPrecision(y * persistence);
            const double sampled = sampler->sample(
                    maintainPrecision(x * persistence),
                    sampleY,
                    maintainPrecision(z * persistence),
                    yAmplification * persistence,
                    minY * persistence
            ) * lacunarity;
            noise += 1.0 * sampled;
        }

        persistence /= 2.0;
        lacunarity *= 2.0;
    }

    return noise;
}
