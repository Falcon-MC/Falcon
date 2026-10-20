#include "World/Level/LevelGen/Density/DensityBase3dNoise.h"

#include "World/Level/LevelGen/Noise/OctavePerlinNoiseSampler.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace {

    constexpr double MAIN_NOISE_DIVISOR = 10.0;
    constexpr double LIMIT_NOISE_DIVISOR = 512.0;
    constexpr double RESULT_DIVISOR = 128.0;
    constexpr double BASE_SCALE = 684.412;

    double clampValue(double value, double min, double max) {
        return value < min ? min : std::min(value, max);
    }

    double lerpValue(double first, double second, double alpha) {
        return first + alpha * (second - first);
    }

    std::vector<int32_t> descendingOctaves(int32_t minInclusive, int32_t maxInclusive) {
        std::vector<int32_t> octaves;
        for (int32_t octave = minInclusive; octave <= maxInclusive; octave++) {
            octaves.push_back(octave);
        }

        std::sort(octaves.begin(), octaves.end(), [](int32_t a, int32_t b) {
            return b < a;
        });
        return octaves;
    }

    const std::vector<int32_t> &limitOctaves() {
        static const std::vector<int32_t> octaves = descendingOctaves(-15, 0);
        return octaves;
    }

    const std::vector<int32_t> &mainOctaves() {
        static const std::vector<int32_t> octaves = descendingOctaves(-7, 0);
        return octaves;
    }

    class OldBlendedNoise : public DensityFunction {
    public:
        OldBlendedNoise(OctavePerlinNoiseSamplerPtr minLimitNoise, OctavePerlinNoiseSamplerPtr maxLimitNoise,
                        OctavePerlinNoiseSamplerPtr mainNoise, double xzScale, double yScale, double xzFactor,
                        double yFactor, double smearScaleMultiplier)
                : mMinLimitNoise(std::move(minLimitNoise)), mMaxLimitNoise(std::move(maxLimitNoise)),
                  mMainNoise(std::move(mainNoise)), mXzScale(xzScale), mYScale(yScale), mXzFactor(xzFactor),
                  mYFactor(yFactor), mSmearScaleMultiplier(smearScaleMultiplier) {}

        double compute(FunctionContext &context) override {
            double scaledXZ = BASE_SCALE * mXzScale;
            double scaledY = BASE_SCALE * mYScale;
            double mainScaledXZ = scaledXZ / mXzFactor;
            double mainScaledY = scaledY / mYFactor;
            double smearScale = scaledY * mSmearScaleMultiplier;

            double x = context.blockX();
            double y = context.blockY();
            double z = context.blockZ();

            double mainValue = 0.0;
            double frequency = 1.0;
            for (int32_t octave = 0; octave < mMainNoise->getCount(); octave++) {
                const PerlinNoiseSampler *sampler = mMainNoise->getOctave(octave);
                if (sampler != nullptr) {
                    mainValue += sampler->sample(
                            x * mainScaledXZ * frequency,
                            y * mainScaledY * frequency,
                            z * mainScaledXZ * frequency,
                            smearScale * frequency,
                            y * mainScaledY * frequency
                    ) / frequency;
                }
                frequency /= 2.0;
            }

            double blend = clampValue(mainValue / MAIN_NOISE_DIVISOR + 1.0, 0.0, 2.0) * 0.5;
            bool useOnlyMax = blend >= 1.0;
            bool useOnlyMin = blend <= 0.0;

            double minValue = 0.0;
            double maxValue = 0.0;
            frequency = 1.0;

            int32_t octaveCount = std::min(mMinLimitNoise->getCount(), mMaxLimitNoise->getCount());
            for (int32_t octave = 0; octave < octaveCount; octave++) {
                double sampleX = x * scaledXZ * frequency;
                double sampleY = y * scaledY * frequency;
                double sampleZ = z * scaledXZ * frequency;
                double sampleSmear = smearScale * frequency;

                if (!useOnlyMax) {
                    const PerlinNoiseSampler *sampler = mMinLimitNoise->getOctave(octave);
                    if (sampler != nullptr) {
                        minValue += sampler->sample(sampleX, sampleY, sampleZ, sampleSmear, sampleY) / frequency;
                    }
                }

                if (!useOnlyMin) {
                    const PerlinNoiseSampler *sampler = mMaxLimitNoise->getOctave(octave);
                    if (sampler != nullptr) {
                        maxValue += sampler->sample(sampleX, sampleY, sampleZ, sampleSmear, sampleY) / frequency;
                    }
                }

                frequency /= 2.0;
            }

            double lower = minValue / LIMIT_NOISE_DIVISOR;
            double upper = maxValue / LIMIT_NOISE_DIVISOR;
            return lerpValue(lower, upper, blend) / RESULT_DIVISOR;
        }

        double minValue() const override {
            return -1.5;
        }

        double maxValue() const override {
            return 1.5;
        }

    private:
        OctavePerlinNoiseSamplerPtr mMinLimitNoise;
        OctavePerlinNoiseSamplerPtr mMaxLimitNoise;
        OctavePerlinNoiseSamplerPtr mMainNoise;
        double mXzScale;
        double mYScale;
        double mXzFactor;
        double mYFactor;
        double mSmearScaleMultiplier;
    };

}

DensityFunctionPtr DensityBase3dNoise::oldBlendedNoise(IRandom &random, double xzScale, double yScale,
                                                       double xzFactor, double yFactor,
                                                       double smearScaleMultiplier) {
    IRandomPtr minLimitRandom = random.fork();
    OctavePerlinNoiseSamplerPtr minLimitNoise = std::make_shared<OctavePerlinNoiseSampler>(*minLimitRandom,
                                                                                          limitOctaves());
    IRandomPtr maxLimitRandom = random.fork();
    OctavePerlinNoiseSamplerPtr maxLimitNoise = std::make_shared<OctavePerlinNoiseSampler>(*maxLimitRandom,
                                                                                          limitOctaves());
    IRandomPtr mainRandom = random.fork();
    OctavePerlinNoiseSamplerPtr mainNoise = std::make_shared<OctavePerlinNoiseSampler>(*mainRandom, mainOctaves());

    return std::make_shared<OldBlendedNoise>(std::move(minLimitNoise), std::move(maxLimitNoise), std::move(mainNoise),
                                             xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier);
}

DensityFunctionPtr DensityBase3dNoise::overworld(IRandom &random) {
    return oldBlendedNoise(random, 0.25, 0.125, 80.0, 160.0, 8.0);
}
