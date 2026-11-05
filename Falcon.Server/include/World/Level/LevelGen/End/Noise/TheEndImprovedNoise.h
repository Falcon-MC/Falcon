#pragma once

#include <array>
#include <cstdint>
#include <random>

// one octave of the game's legacy perlin noise, float from start to end (sub_145C2A210)
class TheEndImprovedNoise {
public:
    explicit TheEndImprovedNoise(std::mt19937 &random);

    // adds an octave to an array ordered x, z, y
    void addOctave(float *noiseArray, const float *position, int32_t xSize, int32_t ySize, int32_t zSize,
                   const float *scale, float weight) const;

private:
    std::array<int32_t, 512> mPermutations;
    float mXCoord;
    float mYCoord;
    float mZCoord;
};
