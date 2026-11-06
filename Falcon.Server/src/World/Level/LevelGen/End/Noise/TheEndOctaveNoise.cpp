#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"

TheEndOctaveNoise::TheEndOctaveNoise(std::mt19937 &random, int32_t octaves) {
    mOctaves.reserve((size_t) octaves);

    for (int32_t i = 0; i < octaves; i++)
        mOctaves.emplace_back(random);
}

void TheEndOctaveNoise::generate(float *noiseArray, const float *position, int32_t xSize, int32_t ySize, int32_t zSize,
                                 float xzScale, float yScale) const {
    float weight = 1.0f;

    for (const TheEndImprovedNoise &octave: mOctaves) {
        const float scale[3] = {weight * xzScale, weight * yScale, weight * xzScale};
        octave.addOctave(noiseArray, position, xSize, ySize, zSize, scale, weight);
        weight *= 0.5f;
    }
}
