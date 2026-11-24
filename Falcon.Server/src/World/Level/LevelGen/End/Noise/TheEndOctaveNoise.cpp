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

float TheEndOctaveNoise::sample(float x, float y, float z) const {
    float frequency = 1.0f;
    float total = 0.0f;
    float amplitudeSum = 0.0f;

    for (const TheEndImprovedNoise &octave: mOctaves) {
        total += octave.sample(x * frequency + octave.getXCoord(), y * frequency + octave.getYCoord(),
                               z * frequency + octave.getZCoord())
                 / frequency;
        amplitudeSum += 1.0f / frequency;
        frequency *= 0.5f;
    }

    return total * (1.0f / amplitudeSum);
}
