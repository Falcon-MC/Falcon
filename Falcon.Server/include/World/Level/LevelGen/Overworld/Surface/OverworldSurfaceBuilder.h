#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

class LevelChunk;

class OverworldSurfaceBuilder {
public:
    explicit OverworldSurfaceBuilder(const XoroshiroPositionalFactory &noiseFactory);

    void apply(LevelChunk &chunk) const;

private:
    NormalNoisePtr mNoise;
};
