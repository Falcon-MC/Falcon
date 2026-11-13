#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"

#include <cstdint>
#include <string>
#include <vector>

class BlockState;
class LevelChunk;

// minecraft:noise_gradient builder, vtable
class NoiseGradientSurfaceBuilder {
public:
    struct Range {
        float mMin;
        float mMax;
        const BlockState *mBlock;
    };

    NoiseGradientSurfaceBuilder(int64_t levelSeed, const std::string &noiseName, int32_t firstOctave,
                                const std::vector<float> &amplitudes, std::vector<Range> ranges,
                                std::vector<const BlockState *> nonReplaceable);

    static NoiseGradientSurfaceBuilder sulfurCaves(int64_t levelSeed);

    // nullptr between ranges
    const BlockState *blockAt(int32_t x, int32_t y, int32_t z) const;

    // fromY to toY - 1, capped by the column height
    void build(LevelChunk &chunk, int32_t x, int32_t z, int32_t fromY, int32_t toY) const;

private:
    NormalNoisePtr mNoise;
    std::vector<Range> mRanges;
    std::vector<const BlockState *> mNonReplaceable;
};
