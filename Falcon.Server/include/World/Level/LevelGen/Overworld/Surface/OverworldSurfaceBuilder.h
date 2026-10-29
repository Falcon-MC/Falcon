#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

class LevelChunk;

class OverworldSurfaceBuilder {
public:
    explicit OverworldSurfaceBuilder(IRandom &random);

    void apply(LevelChunk &chunk) const;

private:
    NormalNoisePtr mNoise;
};
