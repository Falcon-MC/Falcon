#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"

#include <cmath>

namespace {

    int64_t floorDoubleLong(double value) {
        const int64_t truncated = (int64_t) value;
        return value < (double) truncated ? truncated - 1LL : truncated;
    }

}

TheEndOctaveNoise::TheEndOctaveNoise(IRandom &random, int32_t octaves)
        : mGeneratorCollection(), mOctaves(octaves) {
    mGeneratorCollection.reserve((size_t) octaves);

    for (int32_t i = 0; i < octaves; ++i)
        mGeneratorCollection.push_back(std::unique_ptr<TheEndImprovedNoise>(new TheEndImprovedNoise(random)));
}

std::vector<double> TheEndOctaveNoise::generateNoiseOctaves(int32_t xOffset, int32_t yOffset, int32_t zOffset,
                                                            int32_t xSize, int32_t ySize, int32_t zSize,
                                                            double xScale, double yScale, double zScale) const {
    std::vector<double> noiseArray((size_t) (xSize * ySize * zSize), 0.0);

    double d3 = 1.0;

    for (int32_t j = 0; j < mOctaves; ++j) {
        double d0 = (double) xOffset * d3 * xScale;
        const double d1 = (double) yOffset * d3 * yScale;
        double d2 = (double) zOffset * d3 * zScale;
        int64_t k = floorDoubleLong(d0);
        int64_t l = floorDoubleLong(d2);
        d0 = d0 - (double) k;
        d2 = d2 - (double) l;
        k = k % 16777216LL;
        l = l % 16777216LL;
        d0 = d0 + (double) k;
        d2 = d2 + (double) l;
        mGeneratorCollection[(size_t) j]->populateNoiseArray(noiseArray, d0, d1, d2, xSize, ySize, zSize,
                                                            xScale * d3, yScale * d3, zScale * d3, d3);
        d3 /= 2.0;
    }

    return noiseArray;
}
