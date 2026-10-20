#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <array>
#include <cstdint>
#include <vector>

class TheEndImprovedNoise {
public:
    explicit TheEndImprovedNoise(IRandom &random);

    void populateNoiseArray(std::vector<double> &noiseArray, double xOffset, double yOffset, double zOffset,
                            int32_t xSize, int32_t ySize, int32_t zSize, double xScale, double yScale, double zScale,
                            double noiseScale) const;

private:
    static double _lerp(double delta, double start, double end);

    double _grad2(int32_t hash, double x, double z) const;

    double _grad(int32_t hash, double x, double y, double z) const;

    std::array<int32_t, 512> mPermutations;
    double mXCoord;
    double mYCoord;
    double mZCoord;
};
