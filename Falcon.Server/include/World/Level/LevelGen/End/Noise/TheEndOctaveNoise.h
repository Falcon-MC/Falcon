#pragma once

#include "World/Level/LevelGen/End/Noise/TheEndImprovedNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>
#include <memory>
#include <vector>

class TheEndOctaveNoise {
public:
    TheEndOctaveNoise(IRandom &random, int32_t octaves);

    std::vector<double> generateNoiseOctaves(int32_t xOffset, int32_t yOffset, int32_t zOffset, int32_t xSize,
                                             int32_t ySize, int32_t zSize, double xScale, double yScale,
                                             double zScale) const;

private:
    std::vector<std::unique_ptr<TheEndImprovedNoise>> mGeneratorCollection;
    int32_t mOctaves;
};
