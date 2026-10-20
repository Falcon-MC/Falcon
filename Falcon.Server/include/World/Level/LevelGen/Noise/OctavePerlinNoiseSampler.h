#pragma once

#include "World/Level/LevelGen/Noise/PerlinNoiseSampler.h"

#include <memory>
#include <vector>

class OctavePerlinNoiseSampler {
public:
    OctavePerlinNoiseSampler(IRandom &rand, const std::vector<int32_t> &octaves);

    int32_t getCount() const;

    const PerlinNoiseSampler *getOctave(int32_t octave) const;

    double sample(double x, double y, double z) const;

    double sample(double x, double y, double z, double yAmplification, double minY, bool useDefaultY) const;

    static double maintainPrecision(double value);

    double getPersistence() const { return mPersistence; }

    double getLacunarity() const { return mLacunarity; }

private:
    static void _skipOctave(IRandom &rand);

    std::vector<std::shared_ptr<PerlinNoiseSampler>> mOctaveSamplers;
    double mLacunarity;
    double mPersistence;
    int32_t mOctaveSamplersCount;
};

using OctavePerlinNoiseSamplerPtr = std::shared_ptr<OctavePerlinNoiseSampler>;
