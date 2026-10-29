#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"

#include <vector>

class Level;
class LevelChunk;
class TheEndNoiseHolder;

class TheEndFeatureBuilder {
public:
    static void apply(Level &level, LevelChunk &chunk, const TheEndNoiseHolder &noiseHolder,
                      std::vector<GeneratedBlockChange> *overflow = nullptr);
};
