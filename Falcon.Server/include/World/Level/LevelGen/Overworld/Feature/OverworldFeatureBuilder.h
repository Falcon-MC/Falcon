#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"

#include <vector>

class Level;
class LevelChunk;
class OverworldClimate;

class OverworldFeatureBuilder {
public:
    static void apply(Level &level, LevelChunk &chunk, const OverworldClimate &climate,
                      std::vector<GeneratedBlockChange> *overflow = nullptr);
};
