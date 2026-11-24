#pragma once

#include "World/Level/LevelGen/End/Noise/TheEndImprovedNoise.h"

#include <cstdint>
#include <random>
#include <vector>

class TheEndOctaveNoise {
public:
    TheEndOctaveNoise(std::mt19937 &random, int32_t octaves);

    void generate(float *noiseArray, const float *position, int32_t xSize, int32_t ySize, int32_t zSize, float xzScale,
                  float yScale) const;

    // single point form: each octave halves the frequency and doubles the amplitude, the sum is normalised
    float sample(float x, float y, float z) const;

private:
    std::vector<TheEndImprovedNoise> mOctaves;
};
